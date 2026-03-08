#include "CppUTest/TestHarness.h"
#include "dsp/granular/GranularProcessor.h"
#include "dsp/stereo_sample.h"
#include <cstring>

// Extended granular tests: fill the 65536-sample buffer to actually trigger
// grain initialization and synthesis (setupGrainsIfNeeded, processOneGrainSample).
// The existing smoke tests only do 5×32 = 160 samples, never reaching grainInitialized.

namespace {

static constexpr int kBufSize = 128; // larger blocks for faster fill
// Need 65536 / 128 = 512 blocks to fill buffer, plus some for grain processing
static constexpr int kWarmupBlocks = 520;

TEST_GROUP(GranularRender) {
	GranularProcessor proc;
	StereoSample buf[kBufSize];
	int32_t postFXVolume;

	void setup() override {
		memset(buf, 0, sizeof(buf));
		postFXVolume = 1 << 27;
	}

	void fillDC(int32_t amplitude = 1 << 20) {
		for (auto& s : buf) {
			s.l = amplitude;
			s.r = amplitude;
		}
	}

	// Fill buffer until grainInitialized
	void warmup() {
		for (int i = 0; i < kWarmupBlocks; i++) {
			fillDC(1 << 18);
			postFXVolume = 1 << 27;
			proc.processGrainFX({buf, kBufSize}, 1 << 24, 1 << 24, 1 << 24, 0, &postFXVolume, true, 120.0f, 0);
		}
	}

	bool isBounded(int limit = INT32_MAX / 2) {
		for (int i = 0; i < kBufSize; i++) {
			if (buf[i].l > limit || buf[i].l < -limit)
				return false;
			if (buf[i].r > limit || buf[i].r < -limit)
				return false;
		}
		return true;
	}
};

// --- Grain initialization after buffer fill ---

TEST(GranularRender, grainInitAfterBufferFill) {
	warmup();
	// After warmup, processing should trigger grain synthesis
	fillDC(1 << 18);
	postFXVolume = 1 << 27;
	proc.processGrainFX({buf, kBufSize}, 1 << 24, 1 << 24, 1 << 24, 0, &postFXVolume, true, 120.0f, 0);
	CHECK(isBounded());
}

// --- Different grain rates after init ---

TEST(GranularRender, lowRateAfterInit) {
	warmup();
	fillDC(1 << 18);
	postFXVolume = 1 << 27;
	proc.processGrainFX({buf, kBufSize}, 1 << 20, 1 << 24, 1 << 24, 0, &postFXVolume, true, 120.0f, 0);
	CHECK(isBounded());
}

TEST(GranularRender, highRateAfterInit) {
	warmup();
	fillDC(1 << 18);
	postFXVolume = 1 << 27;
	proc.processGrainFX({buf, kBufSize}, 1 << 29, 1 << 24, 1 << 24, 0, &postFXVolume, true, 120.0f, 0);
	CHECK(isBounded());
}

// --- Pitch randomness after init ---

TEST(GranularRender, pitchRandomnessAfterInit) {
	warmup();
	fillDC(1 << 18);
	postFXVolume = 1 << 27;
	proc.processGrainFX({buf, kBufSize}, 1 << 24, 1 << 24, 1 << 24, INT32_MAX, &postFXVolume, true, 120.0f, 0);
	CHECK(isBounded());
}

TEST(GranularRender, moderatePitchRandomness) {
	warmup();
	fillDC(1 << 18);
	postFXVolume = 1 << 27;
	proc.processGrainFX({buf, kBufSize}, 1 << 24, 1 << 24, 1 << 24, 1 << 28, &postFXVolume, true, 120.0f, 0);
	CHECK(isBounded());
}

// --- Different densities after init ---

TEST(GranularRender, lowDensityAfterInit) {
	warmup();
	fillDC(1 << 18);
	postFXVolume = 1 << 27;
	proc.processGrainFX({buf, kBufSize}, 1 << 24, 1 << 24, 1 << 16, 0, &postFXVolume, true, 120.0f, 0);
	CHECK(isBounded());
}

TEST(GranularRender, highDensityAfterInit) {
	warmup();
	fillDC(1 << 18);
	postFXVolume = 1 << 27;
	proc.processGrainFX({buf, kBufSize}, 1 << 24, 1 << 24, 1 << 30, 0, &postFXVolume, true, 120.0f, 0);
	CHECK(isBounded());
}

// --- Mix levels ---

TEST(GranularRender, fullWetAfterInit) {
	warmup();
	fillDC(1 << 18);
	postFXVolume = 1 << 27;
	proc.processGrainFX({buf, kBufSize}, 1 << 24, INT32_MAX, 1 << 24, 0, &postFXVolume, true, 120.0f, 0);
	CHECK(isBounded());
}

TEST(GranularRender, fullDryAfterInit) {
	warmup();
	fillDC(1 << 18);
	postFXVolume = 1 << 27;
	proc.processGrainFX({buf, kBufSize}, 1 << 24, 0, 1 << 24, 0, &postFXVolume, true, 120.0f, 0);
	CHECK(isBounded());
}

// --- Sustained processing (multiple blocks after init) ---

TEST(GranularRender, sustainedProcessingStable) {
	warmup();
	for (int block = 0; block < 20; block++) {
		fillDC(1 << 18);
		postFXVolume = 1 << 27;
		proc.processGrainFX({buf, kBufSize}, 1 << 24, 1 << 24, 1 << 24, 0, &postFXVolume, true, 120.0f, 0);
		CHECK(isBounded());
	}
}

// --- Parameter changes mid-stream (after init) ---

TEST(GranularRender, paramChangeMidStream) {
	warmup();
	// Normal processing
	for (int block = 0; block < 5; block++) {
		fillDC(1 << 18);
		postFXVolume = 1 << 27;
		proc.processGrainFX({buf, kBufSize}, 1 << 24, 1 << 24, 1 << 24, 0, &postFXVolume, true, 120.0f, 0);
	}
	// Switch to very different params
	fillDC(1 << 18);
	postFXVolume = 1 << 27;
	proc.processGrainFX({buf, kBufSize}, 1 << 28, 1 << 28, 1 << 28, 1 << 28, &postFXVolume, true, 60.0f, 0);
	CHECK(isBounded());
}

// --- No input after init ---

TEST(GranularRender, silenceAfterInitStable) {
	warmup();
	memset(buf, 0, sizeof(buf));
	postFXVolume = 1 << 27;
	proc.processGrainFX({buf, kBufSize}, 1 << 24, 1 << 24, 1 << 24, 0, &postFXVolume, false, 120.0f, 0);
	CHECK(isBounded());
}

// --- Clear and re-warm ---

TEST(GranularRender, clearAndReWarmup) {
	warmup();
	proc.clearGrainFXBuffer();
	// Re-fill buffer
	for (int i = 0; i < kWarmupBlocks; i++) {
		fillDC(1 << 18);
		postFXVolume = 1 << 27;
		proc.processGrainFX({buf, kBufSize}, 1 << 24, 1 << 24, 1 << 24, 0, &postFXVolume, true, 120.0f, 0);
	}
	CHECK(isBounded());
}

// --- Reverb amount ---

TEST(GranularRender, withReverbAmount) {
	warmup();
	fillDC(1 << 18);
	postFXVolume = 1 << 27;
	proc.processGrainFX({buf, kBufSize}, 1 << 24, 1 << 24, 1 << 24, 0, &postFXVolume, true, 120.0f, 1 << 24);
	CHECK(isBounded());
}

} // namespace
