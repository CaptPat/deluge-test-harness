// Monte Carlo stress test using realistic Q31 parameter values extracted from
// community preset pack XMLs. Exercises ModFX, delay, filter, and reverb paths
// with real-world parameter distributions rather than synthetic edge cases.
//
// Strategy: sweep combinations of parameter values drawn from actual presets,
// verifying no crashes, no unbounded output, and stable multi-block processing.

#include "CppUTest/TestHarness.h"
#include "definitions_cxx.hpp"
#include "dsp/delay/delay.h"
#include "dsp/reverb/base.hpp"
#include "dsp/stereo_sample.h"
#include "model/mod_controllable/ModFXProcessor.h"
#include "modulation/params/param_collection_summary.h"
#include "modulation/params/param_set.h"
#include <cmath>
#include <cstring>
#include <span>

using namespace deluge::modulation::params;

// --- Q31 values extracted from community preset XMLs ---
// These represent the actual distribution of parameter values in the wild.
static constexpr int32_t kPresetValues[] = {
    // Common preset levels (volume, depth, feedback, etc.)
    0x7FFFFFFF,  // Maximum (very common for volume)
    0x4CCCCCA8,  // ~60% (moderate depth/feedback)
    0x3333332A,  // ~40%
    0x19999998,  // ~20%
    0x0CCCCCCA,  // ~10%
    0x00000000,  // Zero/center
    static_cast<int32_t>(0x80000000),  // Minimum (fully left pan, etc.)
    static_cast<int32_t>(0xC0000000),  // -50%
    static_cast<int32_t>(0xE6666666),  // ~-20%
    static_cast<int32_t>(0xF3333334),  // ~-10%
    // Specific values seen in FM/subtractive presets
    0x7FFFFF00,  // Near-max (common for oscillator volume)
    0x40000000,  // 50% (very common default)
    0x20000000,  // 25%
    0x10000000,  // 12.5%
    0x08000000,  // 6.25%
    0x04000000,  // 3.125%
    // Negative patch cable amounts (inverted modulation)
    static_cast<int32_t>(0xEA3D70A3),  // ~-17%
    static_cast<int32_t>(0xECCCCCCC),  // ~-15%
    static_cast<int32_t>(0xF851EB85),  // ~-6%
};
static constexpr int kNumPresetValues = sizeof(kPresetValues) / sizeof(kPresetValues[0]);

// --- Helpers ---

namespace {

constexpr int kBufSize = 64;

bool isBounded(const StereoSample* buf, int n) {
	for (int i = 0; i < n; i++) {
		if (buf[i].l == INT32_MIN || buf[i].r == INT32_MIN)
			return false;
	}
	return true;
}

void fillDC(StereoSample* buf, int n, int32_t val) {
	for (int i = 0; i < n; i++) {
		buf[i].l = val;
		buf[i].r = val;
	}
}

// Simple LCG PRNG for deterministic "random" test sequences
struct Rng {
	uint32_t state;
	explicit Rng(uint32_t seed) : state(seed) {}
	uint32_t next() {
		state = state * 1664525u + 1013904223u;
		return state;
	}
	int32_t presetValue() { return kPresetValues[next() % kNumPresetValues]; }
	uint32_t range(uint32_t lo, uint32_t hi) { return lo + (next() % (hi - lo + 1)); }
};

struct TestParamContext {
	ParamCollectionSummary summary{};
	UnpatchedParamSet params;
	TestParamContext() : params(&summary) {
		params.params[UNPATCHED_MOD_FX_OFFSET].setCurrentValueBasicForSetup(0);
		params.params[UNPATCHED_MOD_FX_FEEDBACK].setCurrentValueBasicForSetup(0);
	}
};

} // namespace

// ═══════════════════════════════════════════════════════════════════════
// ModFX Monte Carlo
// ═══════════════════════════════════════════════════════════════════════

TEST_GROUP(PresetModFXStress) {
	ModFXProcessor proc;
	StereoSample buffer[kBufSize];
	int32_t postFXVolume;

	void setup() {
		proc = ModFXProcessor();
		proc.setupBuffer();
		postFXVolume = 0x40000000;
		memset(buffer, 0, sizeof(buffer));
	}
	void teardown() { proc.disableBuffer(); }
};

TEST(PresetModFXStress, flangerWithPresetValues) {
	Rng rng(0xDEADBEEF);
	TestParamContext ctx;

	for (int trial = 0; trial < 50; trial++) {
		int32_t depth = kPresetValues[trial % kNumPresetValues];
		int32_t rate = kPresetValues[(trial * 7 + 3) % kNumPresetValues];
		int32_t inputLevel = kPresetValues[(trial * 13 + 5) % kNumPresetValues];
		int32_t feedback = rng.presetValue();

		ctx.params.params[UNPATCHED_MOD_FX_FEEDBACK].setCurrentValueBasicForSetup(feedback);
		fillDC(buffer, kBufSize, inputLevel >> 2); // Scale down to avoid overflow
		postFXVolume = 0x40000000;

		proc.processModFX(std::span<StereoSample>(buffer, kBufSize), ModFXType::FLANGER,
		                  rate, depth, &postFXVolume, &ctx.params, inputLevel != 0);
		CHECK(isBounded(buffer, kBufSize));
	}
}

TEST(PresetModFXStress, chorusWithPresetValues) {
	Rng rng(0xCAFEBABE);
	TestParamContext ctx;

	for (int trial = 0; trial < 50; trial++) {
		int32_t depth = rng.presetValue();
		int32_t rate = rng.presetValue();
		int32_t inputLevel = rng.presetValue();

		fillDC(buffer, kBufSize, inputLevel >> 2);
		postFXVolume = 0x40000000;

		proc.processModFX(std::span<StereoSample>(buffer, kBufSize), ModFXType::CHORUS,
		                  rate, depth, &postFXVolume, &ctx.params, true);
		CHECK(isBounded(buffer, kBufSize));
	}
}

TEST(PresetModFXStress, phaserWithPresetValues) {
	Rng rng(0x12345678);
	TestParamContext ctx;

	for (int trial = 0; trial < 50; trial++) {
		int32_t depth = rng.presetValue();
		int32_t rate = rng.presetValue();
		int32_t feedback = rng.presetValue();

		ctx.params.params[UNPATCHED_MOD_FX_FEEDBACK].setCurrentValueBasicForSetup(feedback);
		fillDC(buffer, kBufSize, rng.presetValue() >> 2);
		postFXVolume = 0x40000000;

		proc.processModFX(std::span<StereoSample>(buffer, kBufSize), ModFXType::PHASER,
		                  rate, depth, &postFXVolume, &ctx.params, true);
		CHECK(isBounded(buffer, kBufSize));
	}
}

TEST(PresetModFXStress, allTypesRandomSweep) {
	Rng rng(0xFEEDFACE);
	TestParamContext ctx;

	ModFXType types[] = {
	    ModFXType::FLANGER, ModFXType::CHORUS, ModFXType::PHASER,
	    ModFXType::CHORUS_STEREO, ModFXType::WARBLE, ModFXType::DIMENSION,
	};

	for (int trial = 0; trial < 100; trial++) {
		ModFXType type = types[rng.range(0, 5)];
		int32_t depth = rng.presetValue();
		int32_t rate = rng.presetValue();
		int32_t feedback = rng.presetValue();
		int32_t offset = rng.presetValue();
		int32_t inputLevel = rng.presetValue();

		ctx.params.params[UNPATCHED_MOD_FX_FEEDBACK].setCurrentValueBasicForSetup(feedback);
		ctx.params.params[UNPATCHED_MOD_FX_OFFSET].setCurrentValueBasicForSetup(offset);
		fillDC(buffer, kBufSize, inputLevel >> 3);
		postFXVolume = 0x40000000;

		proc.processModFX(std::span<StereoSample>(buffer, kBufSize), type,
		                  rate, depth, &postFXVolume, &ctx.params, inputLevel != 0);
		CHECK(isBounded(buffer, kBufSize));
	}
}

TEST(PresetModFXStress, multiBlockAccumulationAllTypes) {
	Rng rng(0xABCD1234);
	TestParamContext ctx;

	ModFXType types[] = {
	    ModFXType::FLANGER, ModFXType::CHORUS, ModFXType::PHASER,
	    ModFXType::WARBLE, ModFXType::DIMENSION,
	};

	for (ModFXType type : types) {
		proc = ModFXProcessor();
		proc.setupBuffer();
		int32_t depth = rng.presetValue();
		int32_t rate = rng.presetValue();

		// Process 20 blocks to test state accumulation stability
		for (int block = 0; block < 20; block++) {
			fillDC(buffer, kBufSize, rng.presetValue() >> 3);
			postFXVolume = 0x40000000;
			proc.processModFX(std::span<StereoSample>(buffer, kBufSize), type,
			                  rate, depth, &postFXVolume, &ctx.params, true);
			CHECK(isBounded(buffer, kBufSize));
		}
		proc.disableBuffer();
	}
}

// ═══════════════════════════════════════════════════════════════════════
// Delay Monte Carlo
// ═══════════════════════════════════════════════════════════════════════

TEST_GROUP(PresetDelayStress) {
	Delay delay;
	StereoSample buf[kBufSize];

	void setup() override { memset(buf, 0, sizeof(buf)); }
	void teardown() override { delay.discardBuffers(); }
};

TEST(PresetDelayStress, presetFeedbackLevelsStable) {
	// Feed each preset-derived feedback level through delay processing
	for (int i = 0; i < kNumPresetValues; i++) {
		Delay d;
		d.syncLevel = SYNC_LEVEL_NONE;
		d.analog = false;

		int32_t feedback = kPresetValues[i];
		// Feedback must be > 256 for delay to activate
		if (feedback <= 256 && feedback >= -256)
			continue;

		d.informWhetherActive(true, 44100);
		Delay::State state{};
		state.doDelay = true;
		state.userDelayRate = 44100;
		state.delayFeedbackAmount = feedback;
		d.userRateLastTime = 44100;
		d.countCyclesWithoutChange = 0;

		for (int block = 0; block < 5; block++) {
			fillDC(buf, kBufSize, kPresetValues[(i + block) % kNumPresetValues] >> 3);
			d.process({buf, kBufSize}, state);
		}
		d.discardBuffers();
	}
}

TEST(PresetDelayStress, analogWithPresetSaturation) {
	Rng rng(0x55AA55AA);

	for (int trial = 0; trial < 20; trial++) {
		Delay d;
		d.analog = true;
		d.syncLevel = SYNC_LEVEL_NONE;
		d.informWhetherActive(true, 44100);

		Delay::State state{};
		state.doDelay = true;
		state.userDelayRate = 44100;
		state.delayFeedbackAmount = rng.presetValue();
		state.analog_saturation = static_cast<int32_t>(rng.range(1, 16));
		if (state.delayFeedbackAmount <= 256 && state.delayFeedbackAmount >= -256) {
			d.discardBuffers();
			continue;
		}
		d.userRateLastTime = 44100;
		d.countCyclesWithoutChange = 0;

		for (int block = 0; block < 5; block++) {
			fillDC(buf, kBufSize, rng.presetValue() >> 3);
			d.process({buf, kBufSize}, state);
		}
		d.discardBuffers();
	}
}

TEST(PresetDelayStress, syncedDelayWithPresetRates) {
	Rng rng(0xBEEFCAFE);
	SyncType types[] = {SYNC_TYPE_EVEN, SYNC_TYPE_TRIPLET, SYNC_TYPE_DOTTED};

	for (int trial = 0; trial < 30; trial++) {
		Delay d;
		d.syncLevel = static_cast<SyncLevel>(rng.range(1, 9));
		d.syncType = types[rng.range(0, 2)];
		d.pingPong = (rng.next() & 1);
		d.analog = (rng.next() & 1);

		Delay::State state{};
		state.userDelayRate = static_cast<int32_t>(rng.range(1 << 16, 1 << 24));
		state.delayFeedbackAmount = rng.presetValue();
		if (state.delayFeedbackAmount <= 256 && state.delayFeedbackAmount >= -256) {
			state.delayFeedbackAmount = 1 << 29; // Ensure it activates
		}
		state.analog_saturation = 8;

		d.setupWorkingState(state, 1 << 24);
		if (state.doDelay) {
			d.userRateLastTime = state.userDelayRate;
			d.countCyclesWithoutChange = 0;
			for (int block = 0; block < 5; block++) {
				fillDC(buf, kBufSize, rng.presetValue() >> 4);
				d.process({buf, kBufSize}, state);
			}
		}
		d.discardBuffers();
	}
}

// ═══════════════════════════════════════════════════════════════════════
// Reverb filter cutoff Monte Carlo
// ═══════════════════════════════════════════════════════════════════════

TEST_GROUP(PresetReverbStress) {};

TEST(PresetReverbStress, filterCutoffMonotonicityLPF) {
	// Verify monotonicity across 200 evenly-spaced input values
	float prev = 0.0f;
	for (int i = 0; i <= 200; i++) {
		float f = static_cast<float>(i) / 200.0f;
		float wc = deluge::dsp::reverb::Base::calcFilterCutoff<deluge::dsp::reverb::Base::FilterType::LowPass>(f);
		CHECK(wc >= prev);
		CHECK(wc >= 0.0f);
		CHECK(wc <= 1.0f);
		prev = wc;
	}
}

TEST(PresetReverbStress, filterCutoffMonotonicityHPF) {
	float prev = 0.0f;
	for (int i = 0; i <= 200; i++) {
		float f = static_cast<float>(i) / 200.0f;
		float wc = deluge::dsp::reverb::Base::calcFilterCutoff<deluge::dsp::reverb::Base::FilterType::HighPass>(f);
		CHECK(wc >= prev);
		CHECK(wc >= 0.0f);
		CHECK(wc <= 1.0f);
		prev = wc;
	}
}

TEST(PresetReverbStress, lpfCutoffAtPresetKnobPositions) {
	// Typical knob positions seen in presets: 0, 0.25, 0.5, 0.75, 1.0
	float positions[] = {0.0f, 0.1f, 0.25f, 0.33f, 0.5f, 0.67f, 0.75f, 0.9f, 1.0f};
	float prevWc = 0.0f;
	for (float f : positions) {
		float wc = deluge::dsp::reverb::Base::calcFilterCutoff<deluge::dsp::reverb::Base::FilterType::LowPass>(f);
		CHECK(wc >= prevWc);
		CHECK(std::isfinite(wc));
		prevWc = wc;
	}
}

TEST(PresetReverbStress, hpfCutoffAtPresetKnobPositions) {
	float positions[] = {0.0f, 0.1f, 0.25f, 0.33f, 0.5f, 0.67f, 0.75f, 0.9f, 1.0f};
	float prevWc = 0.0f;
	for (float f : positions) {
		float wc = deluge::dsp::reverb::Base::calcFilterCutoff<deluge::dsp::reverb::Base::FilterType::HighPass>(f);
		CHECK(wc >= prevWc);
		CHECK(std::isfinite(wc));
		prevWc = wc;
	}
}
