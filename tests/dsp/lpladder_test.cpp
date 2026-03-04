#include "CppUTest/TestHarness.h"
#include "dsp/filter/lpladder.h"

#include <cstring>

using namespace deluge::dsp::filter;

static constexpr int32_t NUM_SAMPLES = 64;

static int64_t totalEnergy(const q31_t* buf, int count) {
	int64_t energy = 0;
	for (int i = 0; i < count; i++) {
		energy += (int64_t)buf[i] * buf[i] >> 31;
	}
	return energy;
}

TEST_GROUP(LpLadderTest) {
	LpLadderFilter filter;
	q31_t samples[NUM_SAMPLES];

	void setup() override {
		filter.reset();
		filter.dryFade = 0; // bypass blend path for deterministic testing
		memset(samples, 0, sizeof(samples));
	}
};

// --- Configuration ---

TEST(LpLadderTest, setConfig12dB) {
	q31_t gain = filter.configure(ONE_Q31 / 4, ONE_Q31 / 8, FilterMode::TRANSISTOR_12DB, 0, ONE_Q31);
	CHECK(gain != 0);
}

TEST(LpLadderTest, setConfig24dB) {
	q31_t gain = filter.configure(ONE_Q31 / 4, ONE_Q31 / 8, FilterMode::TRANSISTOR_24DB, 0, ONE_Q31);
	CHECK(gain != 0);
}

TEST(LpLadderTest, setConfigDrive) {
	q31_t gain = filter.configure(ONE_Q31 / 4, ONE_Q31 / 8, FilterMode::TRANSISTOR_24DB_DRIVE, 0, ONE_Q31);
	CHECK(gain != 0);
}

// --- Filtering basics ---

TEST(LpLadderTest, resetProducesZeros12dB) {
	filter.configure(ONE_Q31 / 4, ONE_Q31 / 8, FilterMode::TRANSISTOR_12DB, 0, ONE_Q31);
	filter.filterMono(samples, samples + NUM_SAMPLES);
	for (int i = 0; i < NUM_SAMPLES; i++) {
		CHECK_EQUAL(0, samples[i]);
	}
}

TEST(LpLadderTest, resetProducesZeros24dB) {
	filter.configure(ONE_Q31 / 4, ONE_Q31 / 8, FilterMode::TRANSISTOR_24DB, 0, ONE_Q31);
	filter.filterMono(samples, samples + NUM_SAMPLES);
	for (int i = 0; i < NUM_SAMPLES; i++) {
		CHECK_EQUAL(0, samples[i]);
	}
}

TEST(LpLadderTest, filterMonoImpulse12dB) {
	filter.configure(ONE_Q31 / 4, ONE_Q31 / 8, FilterMode::TRANSISTOR_12DB, 0, ONE_Q31);
	samples[0] = ONE_Q31 / 4;
	filter.filterMono(samples, samples + NUM_SAMPLES);

	bool hasNonZero = false;
	for (int i = 1; i < NUM_SAMPLES; i++) {
		if (samples[i] != 0) {
			hasNonZero = true;
			break;
		}
	}
	CHECK(hasNonZero);
}

TEST(LpLadderTest, filterMonoImpulse24dB) {
	filter.configure(ONE_Q31 / 4, ONE_Q31 / 8, FilterMode::TRANSISTOR_24DB, 0, ONE_Q31);
	samples[0] = ONE_Q31 / 4;
	filter.filterMono(samples, samples + NUM_SAMPLES);

	bool hasNonZero = false;
	for (int i = 1; i < NUM_SAMPLES; i++) {
		if (samples[i] != 0) {
			hasNonZero = true;
			break;
		}
	}
	CHECK(hasNonZero);
}

TEST(LpLadderTest, filterMonoDrive) {
	filter.configure(ONE_Q31 / 4, ONE_Q31 / 8, FilterMode::TRANSISTOR_24DB_DRIVE, 0, ONE_Q31);
	samples[0] = ONE_Q31 / 4;
	filter.filterMono(samples, samples + NUM_SAMPLES);

	bool hasNonZero = false;
	for (int i = 0; i < NUM_SAMPLES; i++) {
		if (samples[i] != 0) {
			hasNonZero = true;
			break;
		}
	}
	CHECK(hasNonZero);
}

TEST(LpLadderTest, filterStereo24dB) {
	filter.configure(ONE_Q31 / 4, ONE_Q31 / 8, FilterMode::TRANSISTOR_24DB, 0, ONE_Q31);

	q31_t stereo[NUM_SAMPLES * 2];
	memset(stereo, 0, sizeof(stereo));
	stereo[0] = ONE_Q31 / 4;  // L
	stereo[1] = ONE_Q31 / 8;  // R

	filter.filterStereo(stereo, stereo + NUM_SAMPLES * 2);

	bool hasNonZeroL = false, hasNonZeroR = false;
	for (int i = 2; i < NUM_SAMPLES * 2; i += 2) {
		if (stereo[i] != 0) hasNonZeroL = true;
		if (stereo[i + 1] != 0) hasNonZeroR = true;
	}
	CHECK(hasNonZeroL);
	CHECK(hasNonZeroR);
}

TEST(LpLadderTest, lowFrequencyAttenuates) {
	q31_t lowOut[NUM_SAMPLES], highOut[NUM_SAMPLES];
	memset(lowOut, 0, sizeof(lowOut));
	memset(highOut, 0, sizeof(highOut));
	lowOut[0] = ONE_Q31 / 4;
	highOut[0] = ONE_Q31 / 4;

	LpLadderFilter low, high;
	low.reset();
	high.reset();
	low.dryFade = 0;
	high.dryFade = 0;

	low.configure(ONE_Q31 / 256, 0, FilterMode::TRANSISTOR_24DB, 0, ONE_Q31);
	high.configure(ONE_Q31 / 32, 0, FilterMode::TRANSISTOR_24DB, 0, ONE_Q31);

	low.filterMono(lowOut, lowOut + NUM_SAMPLES);
	high.filterMono(highOut, highOut + NUM_SAMPLES);

	int64_t lowEnergy = totalEnergy(lowOut, NUM_SAMPLES);
	int64_t highEnergy = totalEnergy(highOut, NUM_SAMPLES);

	CHECK(highEnergy > lowEnergy);
}

TEST(LpLadderTest, resonanceAffectsOutput) {
	q31_t noRes[NUM_SAMPLES], withRes[NUM_SAMPLES];
	memset(noRes, 0, sizeof(noRes));
	memset(withRes, 0, sizeof(withRes));
	noRes[0] = ONE_Q31 / 4;
	withRes[0] = ONE_Q31 / 4;

	LpLadderFilter lr, hr;
	lr.reset();
	hr.reset();
	lr.dryFade = 0;
	hr.dryFade = 0;

	lr.configure(ONE_Q31 / 4, 0, FilterMode::TRANSISTOR_24DB, 0, ONE_Q31);
	hr.configure(ONE_Q31 / 4, ONE_Q31 / 4, FilterMode::TRANSISTOR_24DB, 0, ONE_Q31);

	lr.filterMono(noRes, noRes + NUM_SAMPLES);
	hr.filterMono(withRes, withRes + NUM_SAMPLES);

	bool differ = false;
	for (int i = 0; i < NUM_SAMPLES; i++) {
		if (noRes[i] != withRes[i]) {
			differ = true;
			break;
		}
	}
	CHECK(differ);
}

TEST(LpLadderTest, resetClearsState) {
	filter.configure(ONE_Q31 / 4, ONE_Q31 / 8, FilterMode::TRANSISTOR_24DB, 0, ONE_Q31);
	samples[0] = ONE_Q31 / 4;
	filter.filterMono(samples, samples + NUM_SAMPLES);

	filter.reset();
	memset(samples, 0, sizeof(samples));
	filter.filterMono(samples, samples + NUM_SAMPLES);

	for (int i = 0; i < NUM_SAMPLES; i++) {
		CHECK_EQUAL(0, samples[i]);
	}
}

TEST(LpLadderTest, modes12vs24ProduceDifferentOutput) {
	q31_t out12[NUM_SAMPLES], out24[NUM_SAMPLES];
	memset(out12, 0, sizeof(out12));
	memset(out24, 0, sizeof(out24));
	out12[0] = ONE_Q31 / 4;
	out24[0] = ONE_Q31 / 4;

	LpLadderFilter f12, f24;
	f12.reset();
	f24.reset();
	f12.dryFade = 0;
	f24.dryFade = 0;

	f12.configure(ONE_Q31 / 4, ONE_Q31 / 8, FilterMode::TRANSISTOR_12DB, 0, ONE_Q31);
	f24.configure(ONE_Q31 / 4, ONE_Q31 / 8, FilterMode::TRANSISTOR_24DB, 0, ONE_Q31);

	f12.filterMono(out12, out12 + NUM_SAMPLES);
	f24.filterMono(out24, out24 + NUM_SAMPLES);

	bool differ = false;
	for (int i = 0; i < NUM_SAMPLES; i++) {
		if (out12[i] != out24[i]) {
			differ = true;
			break;
		}
	}
	CHECK(differ);
}
