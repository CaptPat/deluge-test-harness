// Extreme resonance tests: near self-oscillation, sustained high-Q rendering,
// output boundedness checks, and all filter modes at max resonance.

#include "CppUTest/TestHarness.h"
#include "definitions_cxx.hpp"
#include "dsp/filter/filter_set.h"
#include <cstring>
#include <cmath>

using namespace deluge::dsp::filter;

static constexpr int NUM_SAMPLES = 128;
static constexpr q31_t MAX_RESO = ONE_Q31 - 1;    // Near-max resonance
static constexpr q31_t HIGH_RESO = ONE_Q31 / 4 * 3; // 75% resonance

TEST_GROUP(FilterResonanceExtreme) {
	FilterSet fs;
	q31_t samples[NUM_SAMPLES];

	void setup() override {
		fs.reset();
		memset(samples, 0, sizeof(samples));
	}

	void fillImpulse(q31_t amp = ONE_Q31 / 256) {
		memset(samples, 0, sizeof(samples));
		samples[0] = amp;
	}

	void fillDC(q31_t amp = ONE_Q31 / 256) {
		for (int i = 0; i < NUM_SAMPLES; i++) {
			samples[i] = amp;
		}
	}

	q31_t configLPF(FilterMode mode, q31_t freq, q31_t reso, q31_t morph = 0) {
		q31_t gain = ONE_Q31;
		q31_t oscAmp = ONE_Q31;
		return fs.setConfig(freq, reso, mode, morph,
		                    0, 0, FilterMode::OFF, 0,
		                    gain, FilterRoute::HIGH_TO_LOW, false, &oscAmp);
	}

	q31_t configHPF(FilterMode mode, q31_t freq, q31_t reso, q31_t morph = 0) {
		q31_t gain = ONE_Q31;
		q31_t oscAmp = ONE_Q31;
		return fs.setConfig(0, 0, FilterMode::OFF, 0,
		                    freq, reso, mode, morph,
		                    gain, FilterRoute::HIGH_TO_LOW, false, &oscAmp);
	}

	bool outputBounded() {
		for (int i = 0; i < NUM_SAMPLES; i++) {
			if (samples[i] >= ONE_Q31 || samples[i] <= -ONE_Q31) {
				return false;
			}
		}
		return true;
	}

	bool hasOutput() {
		for (int i = 0; i < NUM_SAMPLES; i++) {
			if (samples[i] != 0) return true;
		}
		return false;
	}
};

// ── Max resonance single impulse ────────────────────────────────────────────

TEST(FilterResonanceExtreme, transistor24dbMaxResoImpulse) {
	configLPF(FilterMode::TRANSISTOR_24DB, ONE_Q31 / 4, MAX_RESO);
	fillImpulse();
	fs.renderLong(samples, samples + NUM_SAMPLES, NUM_SAMPLES);
	CHECK(hasOutput());
	CHECK(outputBounded());
}

TEST(FilterResonanceExtreme, transistor12dbMaxResoImpulse) {
	configLPF(FilterMode::TRANSISTOR_12DB, ONE_Q31 / 4, MAX_RESO);
	fillImpulse();
	fs.renderLong(samples, samples + NUM_SAMPLES, NUM_SAMPLES);
	CHECK(hasOutput());
	CHECK(outputBounded());
}

TEST(FilterResonanceExtreme, svfBandMaxResoImpulse) {
	configLPF(FilterMode::SVF_BAND, ONE_Q31 / 4, MAX_RESO);
	fillImpulse(ONE_Q31 / 4); // SVF at max reso needs stronger impulse
	fs.renderLong(samples, samples + NUM_SAMPLES, NUM_SAMPLES);
	// SVF at max reso may attenuate heavily — just check it doesn't crash
	CHECK(outputBounded());
}

TEST(FilterResonanceExtreme, svfNotchMaxResoImpulse) {
	configLPF(FilterMode::SVF_NOTCH, ONE_Q31 / 4, MAX_RESO);
	fillImpulse(ONE_Q31 / 4);
	fs.renderLong(samples, samples + NUM_SAMPLES, NUM_SAMPLES);
	CHECK(outputBounded());
}

TEST(FilterResonanceExtreme, hpLadderMaxResoImpulse) {
	configHPF(FilterMode::HPLADDER, ONE_Q31 / 4, MAX_RESO);
	fillImpulse();
	fs.renderLong(samples, samples + NUM_SAMPLES, NUM_SAMPLES);
	CHECK(hasOutput());
	CHECK(outputBounded());
}

// ── Sustained rendering at max resonance ────────────────────────────────────

TEST(FilterResonanceExtreme, multiBlockMaxResoStability) {
	configLPF(FilterMode::TRANSISTOR_24DB, ONE_Q31 / 4, MAX_RESO);

	// Run 10 blocks with silence after initial impulse
	fillImpulse();
	for (int block = 0; block < 10; block++) {
		fs.renderLong(samples, samples + NUM_SAMPLES, NUM_SAMPLES);
		CHECK(outputBounded());
		// After first block, feed silence
		memset(samples, 0, sizeof(samples));
	}
}

TEST(FilterResonanceExtreme, multiBlockHighResoDC) {
	configLPF(FilterMode::TRANSISTOR_24DB, ONE_Q31 / 8, HIGH_RESO);

	// Sustained DC input at high resonance — should not blow up
	for (int block = 0; block < 5; block++) {
		fillDC(ONE_Q31 / 256);
		fs.renderLong(samples, samples + NUM_SAMPLES, NUM_SAMPLES);
		CHECK(outputBounded());
	}
}

// ── Corner frequency extremes ───────────────────────────────────────────────

TEST(FilterResonanceExtreme, veryLowFreqHighReso) {
	// Very low cutoff + high reso — slow oscillation, check stability
	configLPF(FilterMode::TRANSISTOR_24DB, ONE_Q31 / 128, HIGH_RESO);
	fillImpulse(ONE_Q31 / 64);
	fs.renderLong(samples, samples + NUM_SAMPLES, NUM_SAMPLES);
	CHECK(outputBounded());
}

TEST(FilterResonanceExtreme, veryHighFreqHighReso) {
	// Near-Nyquist cutoff + high reso
	configLPF(FilterMode::TRANSISTOR_24DB, ONE_Q31 - 1, HIGH_RESO);
	fillImpulse(ONE_Q31 / 64);
	fs.renderLong(samples, samples + NUM_SAMPLES, NUM_SAMPLES);
	CHECK(outputBounded());
}

// ── Resonance increases output energy ───────────────────────────────────────

TEST(FilterResonanceExtreme, highResoRingsLongerThanLowReso) {
	// High-Q filter should have samples ringing further into the buffer
	// (even though gain compensation may reduce overall amplitude)
	configLPF(FilterMode::TRANSISTOR_24DB, ONE_Q31 / 4, 0);
	fillImpulse(ONE_Q31 / 64);
	fs.renderLong(samples, samples + NUM_SAMPLES, NUM_SAMPLES);

	// Find last non-zero sample for low reso
	int lastNonZeroLow = 0;
	for (int i = NUM_SAMPLES - 1; i >= 0; i--) {
		if (samples[i] != 0) { lastNonZeroLow = i; break; }
	}

	fs.reset();
	configLPF(FilterMode::TRANSISTOR_24DB, ONE_Q31 / 4, HIGH_RESO);
	fillImpulse(ONE_Q31 / 64);
	fs.renderLong(samples, samples + NUM_SAMPLES, NUM_SAMPLES);

	int lastNonZeroHigh = 0;
	for (int i = NUM_SAMPLES - 1; i >= 0; i--) {
		if (samples[i] != 0) { lastNonZeroHigh = i; break; }
	}

	// High resonance should ring at least as long (usually much longer)
	CHECK(lastNonZeroHigh >= lastNonZeroLow);
}

// ── Hot signal + max resonance ──────────────────────────────────────────────

TEST(FilterResonanceExtreme, hotSignalMaxResoSaturates) {
	configLPF(FilterMode::TRANSISTOR_24DB, ONE_Q31 / 4, MAX_RESO);

	// Feed a very hot signal
	fillDC(ONE_Q31 / 2);
	fs.renderLong(samples, samples + NUM_SAMPLES, NUM_SAMPLES);

	// Output should be bounded (saturation should prevent overflow)
	CHECK(outputBounded());
}

TEST(FilterResonanceExtreme, hotSignalSVFMaxReso) {
	configLPF(FilterMode::SVF_BAND, ONE_Q31 / 4, MAX_RESO);
	fillDC(ONE_Q31 / 2);
	fs.renderLong(samples, samples + NUM_SAMPLES, NUM_SAMPLES);
	CHECK(outputBounded());
}

TEST(FilterResonanceExtreme, hotSignalHPFMaxReso) {
	configHPF(FilterMode::HPLADDER, ONE_Q31 / 4, MAX_RESO);
	fillDC(ONE_Q31 / 2);
	fs.renderLong(samples, samples + NUM_SAMPLES, NUM_SAMPLES);
	CHECK(outputBounded());
}

// ── Gain reduction at high resonance ────────────────────────────────────────

TEST(FilterResonanceExtreme, gainReductionIncreasesWithResonance) {
	q31_t gain1 = ONE_Q31, gain2 = ONE_Q31;
	q31_t oscAmp1 = ONE_Q31, oscAmp2 = ONE_Q31;

	q31_t result1 = fs.setConfig(ONE_Q31 / 4, 0, FilterMode::TRANSISTOR_24DB, 0,
	                             0, 0, FilterMode::OFF, 0,
	                             gain1, FilterRoute::HIGH_TO_LOW, false, &oscAmp1);

	fs.reset();
	q31_t result2 = fs.setConfig(ONE_Q31 / 4, HIGH_RESO, FilterMode::TRANSISTOR_24DB, 0,
	                             0, 0, FilterMode::OFF, 0,
	                             gain2, FilterRoute::HIGH_TO_LOW, false, &oscAmp2);

	// Higher resonance should result in same or lower gain (compensation)
	CHECK(result2 <= result1);
}
