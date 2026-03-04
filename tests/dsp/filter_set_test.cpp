#include "CppUTest/TestHarness.h"
#include "definitions_cxx.hpp"
#include "dsp/filter/filter_set.h"
#include <cstring>

using namespace deluge::dsp::filter;

static constexpr int NUM_SAMPLES = 128;

TEST_GROUP(FilterSetTest) {
	FilterSet fs;
	q31_t samples[NUM_SAMPLES];
	q31_t stereoSamples[NUM_SAMPLES * 2];

	void setup() override {
		fs.reset();
		memset(samples, 0, sizeof(samples));
		memset(stereoSamples, 0, sizeof(stereoSamples));
	}
};

TEST(FilterSetTest, resetTurnsOff) {
	fs.reset();
	CHECK_FALSE(fs.isLPFOn());
	CHECK_FALSE(fs.isHPFOn());
	CHECK_FALSE(fs.isOn());
}

TEST(FilterSetTest, setConfigLPFOnly) {
	q31_t gain = ONE_Q31;
	q31_t oscAmp = ONE_Q31;
	q31_t result = fs.setConfig(ONE_Q31 / 4, 0, FilterMode::TRANSISTOR_24DB, 0, 0, 0, FilterMode::OFF, 0, gain,
	                            FilterRoute::HIGH_TO_LOW, false, &oscAmp);
	CHECK(fs.isLPFOn());
	CHECK_FALSE(fs.isHPFOn());
	CHECK(result != 0);
}

TEST(FilterSetTest, setConfigHPFOnly) {
	q31_t gain = ONE_Q31;
	q31_t oscAmp = ONE_Q31;
	q31_t result = fs.setConfig(0, 0, FilterMode::OFF, 0, ONE_Q31 / 4, 0, FilterMode::HPLADDER, 0, gain,
	                            FilterRoute::HIGH_TO_LOW, false, &oscAmp);
	CHECK_FALSE(fs.isLPFOn());
	CHECK(fs.isHPFOn());
	CHECK(result != 0);
}

TEST(FilterSetTest, setConfigBothFilters) {
	q31_t gain = ONE_Q31;
	q31_t oscAmp = ONE_Q31;
	fs.setConfig(ONE_Q31 / 4, 0, FilterMode::TRANSISTOR_24DB, 0, ONE_Q31 / 4, 0, FilterMode::HPLADDER, 0, gain,
	             FilterRoute::HIGH_TO_LOW, false, &oscAmp);
	CHECK(fs.isLPFOn());
	CHECK(fs.isHPFOn());
	CHECK(fs.isOn());
}

TEST(FilterSetTest, renderLongH2L) {
	q31_t gain = ONE_Q31;
	q31_t oscAmp = ONE_Q31;
	fs.setConfig(ONE_Q31 / 4, 0, FilterMode::TRANSISTOR_24DB, 0, ONE_Q31 / 4, 0, FilterMode::HPLADDER, 0, gain,
	             FilterRoute::HIGH_TO_LOW, false, &oscAmp);

	// Put an impulse in
	samples[0] = ONE_Q31 / 256;
	fs.renderLong(samples, samples + NUM_SAMPLES, NUM_SAMPLES);

	// Filter should have processed the signal (output may differ from input)
	// Just verify it doesn't crash and produces some output
	bool hasNonZero = false;
	for (int i = 0; i < NUM_SAMPLES; i++) {
		if (samples[i] != 0) {
			hasNonZero = true;
			break;
		}
	}
	CHECK(hasNonZero);
}

TEST(FilterSetTest, renderLongL2H) {
	q31_t gain = ONE_Q31;
	q31_t oscAmp = ONE_Q31;
	fs.setConfig(ONE_Q31 / 4, 0, FilterMode::TRANSISTOR_24DB, 0, ONE_Q31 / 4, 0, FilterMode::HPLADDER, 0, gain,
	             FilterRoute::LOW_TO_HIGH, false, &oscAmp);

	samples[0] = ONE_Q31 / 256;
	fs.renderLong(samples, samples + NUM_SAMPLES, NUM_SAMPLES);

	bool hasNonZero = false;
	for (int i = 0; i < NUM_SAMPLES; i++) {
		if (samples[i] != 0) {
			hasNonZero = true;
			break;
		}
	}
	CHECK(hasNonZero);
}

TEST(FilterSetTest, renderLongParallel) {
	q31_t gain = ONE_Q31;
	q31_t oscAmp = ONE_Q31;
	fs.setConfig(ONE_Q31 / 4, 0, FilterMode::TRANSISTOR_24DB, 0, ONE_Q31 / 4, 0, FilterMode::HPLADDER, 0, gain,
	             FilterRoute::PARALLEL, false, &oscAmp);

	samples[0] = ONE_Q31 / 256;
	fs.renderLong(samples, samples + NUM_SAMPLES, NUM_SAMPLES);

	bool hasNonZero = false;
	for (int i = 0; i < NUM_SAMPLES; i++) {
		if (samples[i] != 0) {
			hasNonZero = true;
			break;
		}
	}
	CHECK(hasNonZero);
}

TEST(FilterSetTest, renderLongStereo) {
	q31_t gain = ONE_Q31;
	q31_t oscAmp = ONE_Q31;
	fs.setConfig(ONE_Q31 / 4, 0, FilterMode::TRANSISTOR_24DB, 0, 0, 0, FilterMode::OFF, 0, gain,
	             FilterRoute::HIGH_TO_LOW, false, &oscAmp);

	stereoSamples[0] = ONE_Q31 / 256; // L
	stereoSamples[1] = ONE_Q31 / 256; // R
	fs.renderLongStereo(stereoSamples, stereoSamples + NUM_SAMPLES * 2);

	bool hasNonZero = false;
	for (int i = 0; i < NUM_SAMPLES * 2; i++) {
		if (stereoSamples[i] != 0) {
			hasNonZero = true;
			break;
		}
	}
	CHECK(hasNonZero);
}

TEST(FilterSetTest, svfModeDispatch) {
	q31_t gain = ONE_Q31;
	q31_t oscAmp = ONE_Q31;
	fs.setConfig(ONE_Q31 / 4, 0, FilterMode::SVF_BAND, ONE_Q31 / 4, 0, 0, FilterMode::OFF, 0, gain,
	             FilterRoute::HIGH_TO_LOW, false, &oscAmp);

	CHECK(fs.isLPFOn());
	samples[0] = ONE_Q31 / 256;
	fs.renderLong(samples, samples + NUM_SAMPLES, NUM_SAMPLES);
	// SVF path should process without crash
}

TEST(FilterSetTest, filterGainReduction) {
	q31_t gain = ONE_Q31;
	q31_t oscAmp = ONE_Q31;
	// The setConfig applies a resonance-based gain reduction (multiply by 1720000000/2^31)
	q31_t result = fs.setConfig(ONE_Q31 / 4, 0, FilterMode::TRANSISTOR_24DB, 0, 0, 0, FilterMode::OFF, 0, gain,
	                            FilterRoute::HIGH_TO_LOW, false, &oscAmp);
	// Result should be less than original gain due to the 1720000000 multiply
	CHECK(result < gain);
	CHECK(result > 0);
}
