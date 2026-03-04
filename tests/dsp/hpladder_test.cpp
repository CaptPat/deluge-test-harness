#include "CppUTest/TestHarness.h"
#include "dsp/filter/hpladder.h"

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

TEST_GROUP(HpLadderTest) {
	HpLadderFilter filter;
	q31_t samples[NUM_SAMPLES];

	void setup() override {
		filter.reset();
		filter.dryFade = 0; // bypass blend path for deterministic testing
		memset(samples, 0, sizeof(samples));
	}
};

// --- Configuration ---

TEST(HpLadderTest, setConfigReturnsGain) {
	q31_t gain = filter.configure(ONE_Q31 / 4, ONE_Q31 / 8, FilterMode::HPLADDER, 0, ONE_Q31);
	CHECK(gain != 0);
}

// --- Filtering basics ---

TEST(HpLadderTest, resetProducesZeros) {
	filter.configure(ONE_Q31 / 4, ONE_Q31 / 8, FilterMode::HPLADDER, 0, ONE_Q31);
	filter.filterMono(samples, samples + NUM_SAMPLES);
	for (int i = 0; i < NUM_SAMPLES; i++) {
		CHECK_EQUAL(0, samples[i]);
	}
}

TEST(HpLadderTest, filterMonoProcessesSamples) {
	filter.configure(ONE_Q31 / 4, ONE_Q31 / 8, FilterMode::HPLADDER, 0, ONE_Q31);
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

TEST(HpLadderTest, filterStereoProcessesSamples) {
	filter.configure(ONE_Q31 / 4, ONE_Q31 / 8, FilterMode::HPLADDER, 0, ONE_Q31);

	q31_t stereo[NUM_SAMPLES * 2];
	memset(stereo, 0, sizeof(stereo));
	stereo[0] = ONE_Q31 / 4;
	stereo[1] = ONE_Q31 / 8;

	filter.filterStereo(stereo, stereo + NUM_SAMPLES * 2);

	bool hasNonZero = false;
	for (int i = 0; i < NUM_SAMPLES * 2; i++) {
		if (stereo[i] != 0) {
			hasNonZero = true;
			break;
		}
	}
	CHECK(hasNonZero);
}

TEST(HpLadderTest, highpassPassesImpulse) {
	// A single impulse contains all frequencies, so the HPF should pass some of it
	filter.configure(ONE_Q31 / 4, ONE_Q31 / 8, FilterMode::HPLADDER, 0, ONE_Q31);
	samples[0] = ONE_Q31 / 4;
	filter.filterMono(samples, samples + NUM_SAMPLES);

	int64_t energy = totalEnergy(samples, NUM_SAMPLES);
	CHECK(energy > 0);
}

TEST(HpLadderTest, frequencyAffectsOutput) {
	q31_t lowFreq[NUM_SAMPLES], highFreq[NUM_SAMPLES];
	memset(lowFreq, 0, sizeof(lowFreq));
	memset(highFreq, 0, sizeof(highFreq));
	lowFreq[0] = ONE_Q31 / 4;
	highFreq[0] = ONE_Q31 / 4;

	HpLadderFilter lf, hf;
	lf.reset();
	hf.reset();
	lf.dryFade = 0;
	hf.dryFade = 0;

	// Frequencies must stay below lshiftAndSaturate<5> saturation limit ((1<<26)-1)
	lf.configure(ONE_Q31 / 256, ONE_Q31 / 8, FilterMode::HPLADDER, 0, ONE_Q31);
	hf.configure(ONE_Q31 / 32, ONE_Q31 / 8, FilterMode::HPLADDER, 0, ONE_Q31);

	lf.filterMono(lowFreq, lowFreq + NUM_SAMPLES);
	hf.filterMono(highFreq, highFreq + NUM_SAMPLES);

	// Different frequency settings should produce different outputs
	bool differ = false;
	for (int i = 0; i < NUM_SAMPLES; i++) {
		if (lowFreq[i] != highFreq[i]) {
			differ = true;
			break;
		}
	}
	CHECK(differ);
}

TEST(HpLadderTest, resonanceAffectsOutput) {
	q31_t lowRes[NUM_SAMPLES], highRes[NUM_SAMPLES];
	memset(lowRes, 0, sizeof(lowRes));
	memset(highRes, 0, sizeof(highRes));
	lowRes[0] = ONE_Q31 / 4;
	highRes[0] = ONE_Q31 / 4;

	HpLadderFilter lr, hr;
	lr.reset();
	hr.reset();
	lr.dryFade = 0;
	hr.dryFade = 0;

	lr.configure(ONE_Q31 / 4, 0, FilterMode::HPLADDER, 0, ONE_Q31);
	hr.configure(ONE_Q31 / 4, ONE_Q31 / 4, FilterMode::HPLADDER, 0, ONE_Q31);

	lr.filterMono(lowRes, lowRes + NUM_SAMPLES);
	hr.filterMono(highRes, highRes + NUM_SAMPLES);

	bool differ = false;
	for (int i = 0; i < NUM_SAMPLES; i++) {
		if (lowRes[i] != highRes[i]) {
			differ = true;
			break;
		}
	}
	CHECK(differ);
}

TEST(HpLadderTest, resetClearsState) {
	filter.configure(ONE_Q31 / 4, ONE_Q31 / 8, FilterMode::HPLADDER, 0, ONE_Q31);
	samples[0] = ONE_Q31 / 4;
	filter.filterMono(samples, samples + NUM_SAMPLES);

	filter.reset();
	memset(samples, 0, sizeof(samples));
	filter.filterMono(samples, samples + NUM_SAMPLES);

	for (int i = 0; i < NUM_SAMPLES; i++) {
		CHECK_EQUAL(0, samples[i]);
	}
}

TEST(HpLadderTest, morphAffectsOutput) {
	q31_t noMorph[NUM_SAMPLES], withMorph[NUM_SAMPLES];
	memset(noMorph, 0, sizeof(noMorph));
	memset(withMorph, 0, sizeof(withMorph));
	// Use smaller impulse so input<<4 in doHPF doesn't overflow, making morph effect visible
	noMorph[0] = ONE_Q31 / 128;
	withMorph[0] = ONE_Q31 / 128;

	HpLadderFilter f1, f2;
	f1.reset();
	f2.reset();
	f1.dryFade = 0;
	f2.dryFade = 0;

	f1.configure(ONE_Q31 / 64, ONE_Q31 / 8, FilterMode::HPLADDER, 0, ONE_Q31);
	f2.configure(ONE_Q31 / 64, ONE_Q31 / 8, FilterMode::HPLADDER, ONE_Q31 / 2, ONE_Q31);

	f1.filterMono(noMorph, noMorph + NUM_SAMPLES);
	f2.filterMono(withMorph, withMorph + NUM_SAMPLES);

	bool differ = false;
	for (int i = 0; i < NUM_SAMPLES; i++) {
		if (noMorph[i] != withMorph[i]) {
			differ = true;
			break;
		}
	}
	CHECK(differ);
}
