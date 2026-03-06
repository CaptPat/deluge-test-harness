#include "CppUTest/TestHarness.h"
#include "dsp/granular/GranularProcessor.h"
#include "dsp/stereo_sample.h"
#include <cstring>

static constexpr int kBufSize = 32;

TEST_GROUP(GranularSmokeTest) {
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

	void fillImpulse(int32_t amplitude = 1 << 24) {
		memset(buf, 0, sizeof(buf));
		buf[0].l = amplitude;
		buf[0].r = amplitude;
	}
};

// --- Basic lifecycle ---

TEST(GranularSmokeTest, processDoesNotCrashFreshProcessor) {
	fillDC();
	proc.processGrainFX({buf, kBufSize},
	                    1 << 24,   // grainRate
	                    1 << 24,   // grainMix
	                    1 << 24,   // grainDensity
	                    0,         // pitchRandomness
	                    &postFXVolume,
	                    true,      // anySoundComingIn
	                    120.0f,    // tempoBPM
	                    0);        // reverbAmount
}

TEST(GranularSmokeTest, processMultipleBlocksBuildsBuffer) {
	// Feed several blocks so the grain buffer fills up
	for (int block = 0; block < 10; block++) {
		fillDC(1 << 18);
		postFXVolume = 1 << 27;
		proc.processGrainFX({buf, kBufSize},
		                    1 << 24, 1 << 24, 1 << 24,
		                    0, &postFXVolume, true, 120.0f, 0);
	}
}

// --- Parameter sweeps ---

TEST(GranularSmokeTest, rateParameterSweep) {
	// First fill the buffer with some signal
	for (int block = 0; block < 5; block++) {
		fillDC(1 << 18);
		postFXVolume = 1 << 27;
		proc.processGrainFX({buf, kBufSize},
		                    1 << 24, 1 << 24, 1 << 24,
		                    0, &postFXVolume, true, 120.0f, 0);
	}

	// Sweep the rate parameter
	int32_t rates[] = {1 << 20, 1 << 24, 1 << 28, 1 << 30};
	for (int32_t rate : rates) {
		fillDC(1 << 18);
		postFXVolume = 1 << 27;
		proc.processGrainFX({buf, kBufSize},
		                    rate, 1 << 24, 1 << 24,
		                    0, &postFXVolume, true, 120.0f, 0);
	}
}

TEST(GranularSmokeTest, mixParameterSweep) {
	for (int block = 0; block < 5; block++) {
		fillDC(1 << 18);
		postFXVolume = 1 << 27;
		proc.processGrainFX({buf, kBufSize},
		                    1 << 24, 1 << 24, 1 << 24,
		                    0, &postFXVolume, true, 120.0f, 0);
	}

	// Sweep mix from dry to fully wet
	int32_t mixes[] = {0, 1 << 20, 1 << 26, 1 << 30};
	for (int32_t mix : mixes) {
		fillDC(1 << 18);
		postFXVolume = 1 << 27;
		proc.processGrainFX({buf, kBufSize},
		                    1 << 24, mix, 1 << 24,
		                    0, &postFXVolume, true, 120.0f, 0);
	}
}

TEST(GranularSmokeTest, densityParameterSweep) {
	for (int block = 0; block < 5; block++) {
		fillDC(1 << 18);
		postFXVolume = 1 << 27;
		proc.processGrainFX({buf, kBufSize},
		                    1 << 24, 1 << 24, 1 << 24,
		                    0, &postFXVolume, true, 120.0f, 0);
	}

	int32_t densities[] = {0, 1 << 20, 1 << 26, 1 << 30};
	for (int32_t density : densities) {
		fillDC(1 << 18);
		postFXVolume = 1 << 27;
		proc.processGrainFX({buf, kBufSize},
		                    1 << 24, 1 << 24, density,
		                    0, &postFXVolume, true, 120.0f, 0);
	}
}

TEST(GranularSmokeTest, pitchRandomnessSweep) {
	for (int block = 0; block < 5; block++) {
		fillDC(1 << 18);
		postFXVolume = 1 << 27;
		proc.processGrainFX({buf, kBufSize},
		                    1 << 24, 1 << 24, 1 << 24,
		                    0, &postFXVolume, true, 120.0f, 0);
	}

	int32_t randomness[] = {0, 1 << 20, 1 << 28, INT32_MAX};
	for (int32_t pr : randomness) {
		fillDC(1 << 18);
		postFXVolume = 1 << 27;
		proc.processGrainFX({buf, kBufSize},
		                    1 << 24, 1 << 24, 1 << 24,
		                    pr, &postFXVolume, true, 120.0f, 0);
	}
}

// --- Tempo changes ---

TEST(GranularSmokeTest, tempoChangeMidStream) {
	for (int block = 0; block < 5; block++) {
		fillDC(1 << 18);
		postFXVolume = 1 << 27;
		proc.processGrainFX({buf, kBufSize},
		                    1 << 24, 1 << 24, 1 << 24,
		                    0, &postFXVolume, true, 120.0f, 0);
	}

	// Halve tempo
	fillDC(1 << 18);
	postFXVolume = 1 << 27;
	proc.processGrainFX({buf, kBufSize},
	                    1 << 24, 1 << 24, 1 << 24,
	                    0, &postFXVolume, true, 60.0f, 0);

	// Double tempo
	fillDC(1 << 18);
	postFXVolume = 1 << 27;
	proc.processGrainFX({buf, kBufSize},
	                    1 << 24, 1 << 24, 1 << 24,
	                    0, &postFXVolume, true, 240.0f, 0);
}

// --- Buffer stolen mid-stream ---

// BUG 1: grainBufferStolen() sets grainBuffer=nullptr but does NOT reset
// wrapsToShutdown, so getSamplesToShutdown() returns stale value.
// BUG 2: processGrainFX after steal SEGFAULTs — it dereferences the null
// grainBuffer via processOneGrainSample → (*grainBuffer)[pos].
TEST(GranularSmokeTest, bufferStolenSetsNullButNotWraps) {
	for (int block = 0; block < 5; block++) {
		fillDC(1 << 18);
		postFXVolume = 1 << 27;
		proc.processGrainFX({buf, kBufSize},
		                    1 << 24, 1 << 24, 1 << 24,
		                    0, &postFXVolume, true, 120.0f, 0);
	}

	proc.grainBufferStolen();
	// wrapsToShutdown is NOT cleared — this documents the bug
	CHECK(proc.getSamplesToShutdown() > 0);
}

// --- No sound coming in → should still process grains from buffer ---

TEST(GranularSmokeTest, noInputStillProcesses) {
	// Build up the buffer first
	for (int block = 0; block < 10; block++) {
		fillDC(1 << 18);
		postFXVolume = 1 << 27;
		proc.processGrainFX({buf, kBufSize},
		                    1 << 24, 1 << 24, 1 << 24,
		                    0, &postFXVolume, true, 120.0f, 0);
	}

	// Now process with no input
	memset(buf, 0, sizeof(buf));
	postFXVolume = 1 << 27;
	proc.processGrainFX({buf, kBufSize},
	                    1 << 24, 1 << 24, 1 << 24,
	                    0, &postFXVolume, false, 120.0f, 0);
}

// --- Clear buffer then resume ---

TEST(GranularSmokeTest, clearAndResume) {
	for (int block = 0; block < 5; block++) {
		fillDC(1 << 18);
		postFXVolume = 1 << 27;
		proc.processGrainFX({buf, kBufSize},
		                    1 << 24, 1 << 24, 1 << 24,
		                    0, &postFXVolume, true, 120.0f, 0);
	}

	proc.clearGrainFXBuffer();

	// Resume processing after clear
	for (int block = 0; block < 5; block++) {
		fillDC(1 << 18);
		postFXVolume = 1 << 27;
		proc.processGrainFX({buf, kBufSize},
		                    1 << 24, 1 << 24, 1 << 24,
		                    0, &postFXVolume, true, 120.0f, 0);
	}
}
