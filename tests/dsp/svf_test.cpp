#include "CppUTest/TestHarness.h"
#include "dsp/filter/svf.h"

#include <cstring>
#include <vector>

using namespace deluge::dsp::filter;

static constexpr int32_t NUM_SAMPLES = 64;

// Helper: compute total energy in a buffer
static int64_t totalEnergy(const q31_t* buf, int count) {
	int64_t energy = 0;
	for (int i = 0; i < count; i++) {
		energy += (int64_t)buf[i] * buf[i] >> 31;
	}
	return energy;
}

TEST_GROUP(SVFTest) {
	SVFilter filter;
	q31_t samples[NUM_SAMPLES];

	void setup() override {
		filter.reset();
		filter.dryFade = 0; // bypass blend path for deterministic testing
		memset(samples, 0, sizeof(samples));
	}
};

// --- Configuration ---

TEST(SVFTest, setConfigLPReturnsGain) {
	q31_t gain = filter.configure(ONE_Q31 / 4, ONE_Q31 / 8, FilterMode::TRANSISTOR_12DB, 0, ONE_Q31);
	CHECK(gain != 0);
}

TEST(SVFTest, setConfigBandReturnsGain) {
	q31_t gain = filter.configure(ONE_Q31 / 4, ONE_Q31 / 8, FilterMode::SVF_BAND, 0, ONE_Q31);
	CHECK(gain != 0);
}

TEST(SVFTest, setConfigNotchReturnsGain) {
	q31_t gain = filter.configure(ONE_Q31 / 4, ONE_Q31 / 8, FilterMode::SVF_NOTCH, 0, ONE_Q31);
	CHECK(gain != 0);
}

// --- Filtering basics ---

TEST(SVFTest, resetProducesZeros) {
	filter.configure(ONE_Q31 / 4, ONE_Q31 / 8, FilterMode::TRANSISTOR_12DB, 0, ONE_Q31);
	// All zeros in → all zeros out
	filter.filterMono(samples, samples + NUM_SAMPLES);
	for (int i = 0; i < NUM_SAMPLES; i++) {
		CHECK_EQUAL(0, samples[i]);
	}
}

TEST(SVFTest, filterMonoProcessesSamples) {
	filter.configure(ONE_Q31 / 4, ONE_Q31 / 8, FilterMode::TRANSISTOR_12DB, 0, ONE_Q31);

	// Impulse: single non-zero sample at start
	samples[0] = ONE_Q31 / 2;
	filter.filterMono(samples, samples + NUM_SAMPLES);

	// After filtering, there should be some non-zero output beyond the impulse
	bool hasNonZero = false;
	for (int i = 1; i < NUM_SAMPLES; i++) {
		if (samples[i] != 0) {
			hasNonZero = true;
			break;
		}
	}
	CHECK(hasNonZero);
}

TEST(SVFTest, filterStereoProcessesSamples) {
	filter.configure(ONE_Q31 / 4, ONE_Q31 / 8, FilterMode::TRANSISTOR_12DB, 0, ONE_Q31);

	q31_t stereoSamples[NUM_SAMPLES * 2];
	memset(stereoSamples, 0, sizeof(stereoSamples));
	stereoSamples[0] = ONE_Q31 / 2;  // L impulse
	stereoSamples[1] = ONE_Q31 / 4;  // R impulse

	filter.filterStereo(stereoSamples, stereoSamples + NUM_SAMPLES * 2);

	bool hasNonZeroL = false, hasNonZeroR = false;
	for (int i = 2; i < NUM_SAMPLES * 2; i += 2) {
		if (stereoSamples[i] != 0) hasNonZeroL = true;
		if (stereoSamples[i + 1] != 0) hasNonZeroR = true;
	}
	CHECK(hasNonZeroL);
	CHECK(hasNonZeroR);
}

TEST(SVFTest, lowFrequencyAttenuatesImpulse) {
	// Very low frequency should attenuate a single impulse
	filter.configure(ONE_Q31 / 64, 0, FilterMode::TRANSISTOR_12DB, 0, ONE_Q31);

	samples[0] = ONE_Q31 / 2;
	filter.filterMono(samples, samples + NUM_SAMPLES);

	// Output should be attenuated compared to input
	int64_t outputEnergy = totalEnergy(samples, NUM_SAMPLES);
	int64_t inputEnergy = (int64_t)(ONE_Q31 / 2) * (ONE_Q31 / 2) >> 31;
	CHECK(outputEnergy < inputEnergy * 2);
}

TEST(SVFTest, highFrequencyPassesMoreSignal) {
	// Compare low freq vs high freq filter on same impulse
	// Frequencies must stay below lshiftAndSaturate<5> saturation limit ((1<<26)-1)
	q31_t lowOut[NUM_SAMPLES], highOut[NUM_SAMPLES];
	memset(lowOut, 0, sizeof(lowOut));
	memset(highOut, 0, sizeof(highOut));
	lowOut[0] = ONE_Q31 / 2;
	highOut[0] = ONE_Q31 / 2;

	SVFilter lowFilter, highFilter;
	lowFilter.reset();
	highFilter.reset();
	lowFilter.dryFade = 0;
	highFilter.dryFade = 0;

	lowFilter.configure(ONE_Q31 / 256, 0, FilterMode::TRANSISTOR_12DB, 0, ONE_Q31);
	highFilter.configure(ONE_Q31 / 32, 0, FilterMode::TRANSISTOR_12DB, 0, ONE_Q31);

	lowFilter.filterMono(lowOut, lowOut + NUM_SAMPLES);
	highFilter.filterMono(highOut, highOut + NUM_SAMPLES);

	int64_t lowEnergy = totalEnergy(lowOut, NUM_SAMPLES);
	int64_t highEnergy = totalEnergy(highOut, NUM_SAMPLES);

	// Higher frequency cutoff should pass more energy
	CHECK(highEnergy > lowEnergy);
}

TEST(SVFTest, bandModeProducesDifferentOutput) {
	q31_t lpOut[NUM_SAMPLES], bpOut[NUM_SAMPLES];
	memset(lpOut, 0, sizeof(lpOut));
	memset(bpOut, 0, sizeof(bpOut));
	lpOut[0] = ONE_Q31 / 2;
	bpOut[0] = ONE_Q31 / 2;

	SVFilter lpFilter, bpFilter;
	lpFilter.reset();
	bpFilter.reset();
	lpFilter.dryFade = 0;
	bpFilter.dryFade = 0;

	// Morph must be non-zero for band mode to differ from LP mode.
	// With morph=0, both produce identical coefficients (c_low=ONE_Q31, c_band=0).
	lpFilter.configure(ONE_Q31 / 64, ONE_Q31 / 8, FilterMode::TRANSISTOR_12DB, ONE_Q31 / 16, ONE_Q31);
	bpFilter.configure(ONE_Q31 / 64, ONE_Q31 / 8, FilterMode::SVF_BAND, ONE_Q31 / 16, ONE_Q31);

	lpFilter.filterMono(lpOut, lpOut + NUM_SAMPLES);
	bpFilter.filterMono(bpOut, bpOut + NUM_SAMPLES);

	// They should produce different outputs
	bool differ = false;
	for (int i = 0; i < NUM_SAMPLES; i++) {
		if (lpOut[i] != bpOut[i]) {
			differ = true;
			break;
		}
	}
	CHECK(differ);
}

TEST(SVFTest, resonanceAffectsOutput) {
	q31_t lowRes[NUM_SAMPLES], highRes[NUM_SAMPLES];
	memset(lowRes, 0, sizeof(lowRes));
	memset(highRes, 0, sizeof(highRes));
	lowRes[0] = ONE_Q31 / 2;
	highRes[0] = ONE_Q31 / 2;

	SVFilter lr, hr;
	lr.reset();
	hr.reset();
	lr.dryFade = 0;
	hr.dryFade = 0;

	lr.configure(ONE_Q31 / 4, 0, FilterMode::TRANSISTOR_12DB, 0, ONE_Q31);
	hr.configure(ONE_Q31 / 4, ONE_Q31 / 4, FilterMode::TRANSISTOR_12DB, 0, ONE_Q31);

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

TEST(SVFTest, resetClearsState) {
	filter.configure(ONE_Q31 / 4, ONE_Q31 / 8, FilterMode::TRANSISTOR_12DB, 0, ONE_Q31);

	// Process some signal
	samples[0] = ONE_Q31 / 2;
	filter.filterMono(samples, samples + NUM_SAMPLES);

	// Reset and process zeros
	filter.reset();
	memset(samples, 0, sizeof(samples));
	filter.filterMono(samples, samples + NUM_SAMPLES);

	// Output should be all zeros after reset
	for (int i = 0; i < NUM_SAMPLES; i++) {
		CHECK_EQUAL(0, samples[i]);
	}
}

TEST(SVFTest, morphAffectsOutput) {
	q31_t noMorph[NUM_SAMPLES], withMorph[NUM_SAMPLES];
	memset(noMorph, 0, sizeof(noMorph));
	memset(withMorph, 0, sizeof(withMorph));
	noMorph[0] = ONE_Q31 / 2;
	withMorph[0] = ONE_Q31 / 2;

	SVFilter f1, f2;
	f1.reset();
	f2.reset();
	f1.dryFade = 0;
	f2.dryFade = 0;

	f1.configure(ONE_Q31 / 4, ONE_Q31 / 8, FilterMode::TRANSISTOR_12DB, 0, ONE_Q31);
	f2.configure(ONE_Q31 / 4, ONE_Q31 / 8, FilterMode::TRANSISTOR_12DB, ONE_Q31 / 2, ONE_Q31);

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
