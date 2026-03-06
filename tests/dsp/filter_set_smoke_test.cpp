#include "CppUTest/TestHarness.h"
#include "definitions_cxx.hpp"
#include "dsp/filter/filter_set.h"
#include <cstring>

using namespace deluge::dsp::filter;

static constexpr int NUM_SAMPLES = 64;

TEST_GROUP(FilterSetSmokeTest) {
	FilterSet fs;
	q31_t samples[NUM_SAMPLES];
	q31_t stereoSamples[NUM_SAMPLES * 2];

	void setup() override {
		fs.reset();
		memset(samples, 0, sizeof(samples));
		memset(stereoSamples, 0, sizeof(stereoSamples));
	}

	void fillImpulse(q31_t* buf, int count, q31_t amp = ONE_Q31 / 256) {
		memset(buf, 0, count * sizeof(q31_t));
		buf[0] = amp;
	}

	void fillDC(q31_t* buf, int count, q31_t amp = ONE_Q31 / 256) {
		for (int i = 0; i < count; i++) {
			buf[i] = amp;
		}
	}

	q31_t configLPF(FilterMode mode, q31_t freq, q31_t reso = 0, q31_t morph = 0) {
		q31_t gain = ONE_Q31;
		q31_t oscAmp = ONE_Q31;
		return fs.setConfig(freq, reso, mode, morph,
		                    0, 0, FilterMode::OFF, 0,
		                    gain, FilterRoute::HIGH_TO_LOW, false, &oscAmp);
	}

	q31_t configHPF(FilterMode mode, q31_t freq, q31_t reso = 0, q31_t morph = 0) {
		q31_t gain = ONE_Q31;
		q31_t oscAmp = ONE_Q31;
		return fs.setConfig(0, 0, FilterMode::OFF, 0,
		                    freq, reso, mode, morph,
		                    gain, FilterRoute::HIGH_TO_LOW, false, &oscAmp);
	}

	q31_t configBoth(FilterMode lpfMode, q31_t lpfFreq, FilterMode hpfMode, q31_t hpfFreq,
	                 FilterRoute route = FilterRoute::HIGH_TO_LOW) {
		q31_t gain = ONE_Q31;
		q31_t oscAmp = ONE_Q31;
		return fs.setConfig(lpfFreq, 0, lpfMode, 0,
		                    hpfFreq, 0, hpfMode, 0,
		                    gain, route, false, &oscAmp);
	}
};

// --- Filter mode switching mid-stream ---

TEST(FilterSetSmokeTest, switchLPFModes) {
	FilterMode modes[] = {
		FilterMode::TRANSISTOR_24DB,
		FilterMode::TRANSISTOR_12DB,
		FilterMode::SVF_BAND,
		FilterMode::SVF_NOTCH,
	};

	for (FilterMode mode : modes) {
		fs.reset();
		configLPF(mode, ONE_Q31 / 4);
		fillImpulse(samples, NUM_SAMPLES);
		fs.renderLong(samples, samples + NUM_SAMPLES, NUM_SAMPLES);

		bool hasOutput = false;
		for (int i = 0; i < NUM_SAMPLES; i++) {
			if (samples[i] != 0) { hasOutput = true; break; }
		}
		CHECK(hasOutput);
	}
}

TEST(FilterSetSmokeTest, switchLPFModesMidStream) {
	// Start with transistor 24dB
	configLPF(FilterMode::TRANSISTOR_24DB, ONE_Q31 / 4);
	fillImpulse(samples, NUM_SAMPLES);
	fs.renderLong(samples, samples + NUM_SAMPLES, NUM_SAMPLES);

	// Switch to SVF band without reset
	configLPF(FilterMode::SVF_BAND, ONE_Q31 / 4);
	fillImpulse(samples, NUM_SAMPLES);
	fs.renderLong(samples, samples + NUM_SAMPLES, NUM_SAMPLES);

	// Switch to transistor 12dB
	configLPF(FilterMode::TRANSISTOR_12DB, ONE_Q31 / 4);
	fillImpulse(samples, NUM_SAMPLES);
	fs.renderLong(samples, samples + NUM_SAMPLES, NUM_SAMPLES);
}

// --- Frequency sweep ---

TEST(FilterSetSmokeTest, lpfFrequencySweep) {
	q31_t freqs[] = {ONE_Q31 / 64, ONE_Q31 / 16, ONE_Q31 / 4, ONE_Q31 / 2};

	for (q31_t freq : freqs) {
		fs.reset();
		configLPF(FilterMode::TRANSISTOR_24DB, freq);
		fillDC(samples, NUM_SAMPLES, ONE_Q31 / 256);
		fs.renderLong(samples, samples + NUM_SAMPLES, NUM_SAMPLES);
	}
}

TEST(FilterSetSmokeTest, hpfFrequencySweep) {
	q31_t freqs[] = {ONE_Q31 / 64, ONE_Q31 / 16, ONE_Q31 / 4, ONE_Q31 / 2};

	for (q31_t freq : freqs) {
		fs.reset();
		configHPF(FilterMode::HPLADDER, freq);
		fillDC(samples, NUM_SAMPLES, ONE_Q31 / 256);
		fs.renderLong(samples, samples + NUM_SAMPLES, NUM_SAMPLES);
	}
}

// --- Resonance sweep ---

TEST(FilterSetSmokeTest, resonanceSweep) {
	q31_t resos[] = {0, ONE_Q31 / 8, ONE_Q31 / 4, ONE_Q31 / 2, ONE_Q31 - 1};

	for (q31_t reso : resos) {
		fs.reset();
		q31_t gain = ONE_Q31;
		q31_t oscAmp = ONE_Q31;
		fs.setConfig(ONE_Q31 / 4, reso, FilterMode::TRANSISTOR_24DB, 0,
		             0, 0, FilterMode::OFF, 0,
		             gain, FilterRoute::HIGH_TO_LOW, false, &oscAmp);
		fillImpulse(samples, NUM_SAMPLES);
		fs.renderLong(samples, samples + NUM_SAMPLES, NUM_SAMPLES);
	}
}

// --- Routing modes ---

TEST(FilterSetSmokeTest, allRoutingModes) {
	FilterRoute routes[] = {
		FilterRoute::HIGH_TO_LOW,
		FilterRoute::LOW_TO_HIGH,
		FilterRoute::PARALLEL,
	};

	for (FilterRoute route : routes) {
		fs.reset();
		q31_t gain = ONE_Q31;
		q31_t oscAmp = ONE_Q31;
		fs.setConfig(ONE_Q31 / 4, 0, FilterMode::TRANSISTOR_24DB, 0,
		             ONE_Q31 / 4, 0, FilterMode::HPLADDER, 0,
		             gain, route, false, &oscAmp);
		fillImpulse(samples, NUM_SAMPLES);
		fs.renderLong(samples, samples + NUM_SAMPLES, NUM_SAMPLES);

		bool hasOutput = false;
		for (int i = 0; i < NUM_SAMPLES; i++) {
			if (samples[i] != 0) { hasOutput = true; break; }
		}
		CHECK(hasOutput);
	}
}

// --- Switch routing mid-stream ---

TEST(FilterSetSmokeTest, switchRoutingMidStream) {
	configBoth(FilterMode::TRANSISTOR_24DB, ONE_Q31 / 4, FilterMode::HPLADDER, ONE_Q31 / 8,
	           FilterRoute::HIGH_TO_LOW);
	fillImpulse(samples, NUM_SAMPLES);
	fs.renderLong(samples, samples + NUM_SAMPLES, NUM_SAMPLES);

	configBoth(FilterMode::TRANSISTOR_24DB, ONE_Q31 / 4, FilterMode::HPLADDER, ONE_Q31 / 8,
	           FilterRoute::PARALLEL);
	fillImpulse(samples, NUM_SAMPLES);
	fs.renderLong(samples, samples + NUM_SAMPLES, NUM_SAMPLES);

	configBoth(FilterMode::TRANSISTOR_24DB, ONE_Q31 / 4, FilterMode::HPLADDER, ONE_Q31 / 8,
	           FilterRoute::LOW_TO_HIGH);
	fillImpulse(samples, NUM_SAMPLES);
	fs.renderLong(samples, samples + NUM_SAMPLES, NUM_SAMPLES);
}

// --- Stereo processing ---

TEST(FilterSetSmokeTest, stereoMultipleBlocks) {
	configLPF(FilterMode::TRANSISTOR_24DB, ONE_Q31 / 4);

	for (int block = 0; block < 10; block++) {
		for (int i = 0; i < NUM_SAMPLES * 2; i += 2) {
			stereoSamples[i] = ONE_Q31 / 256;     // L
			stereoSamples[i + 1] = ONE_Q31 / 512;  // R
		}
		fs.renderLongStereo(stereoSamples, stereoSamples + NUM_SAMPLES * 2);
	}
}

// --- Enable → disable → re-enable ---

TEST(FilterSetSmokeTest, resetAndReconfigure) {
	configLPF(FilterMode::TRANSISTOR_24DB, ONE_Q31 / 4);
	fillImpulse(samples, NUM_SAMPLES);
	fs.renderLong(samples, samples + NUM_SAMPLES, NUM_SAMPLES);

	// reset() zeroes filter memory but does NOT clear LPFOn/HPFOn flags
	// (those are only set by setConfig). So we reconfigure to OFF to disable.
	q31_t gain = ONE_Q31;
	q31_t oscAmp = ONE_Q31;
	fs.setConfig(0, 0, FilterMode::OFF, 0, 0, 0, FilterMode::OFF, 0,
	             gain, FilterRoute::HIGH_TO_LOW, false, &oscAmp);
	CHECK_FALSE(fs.isOn());

	// Re-enable with different mode
	fs.reset(); // clear internal state before new mode
	configLPF(FilterMode::SVF_BAND, ONE_Q31 / 3);
	CHECK(fs.isOn());
	fillImpulse(samples, NUM_SAMPLES);
	fs.renderLong(samples, samples + NUM_SAMPLES, NUM_SAMPLES);
}

// --- Morph parameter ---

TEST(FilterSetSmokeTest, morphSweepSVF) {
	q31_t morphs[] = {0, ONE_Q31 / 4, ONE_Q31 / 2, ONE_Q31 - 1};

	for (q31_t morph : morphs) {
		fs.reset();
		configLPF(FilterMode::SVF_BAND, ONE_Q31 / 4, 0, morph);
		fillImpulse(samples, NUM_SAMPLES);
		fs.renderLong(samples, samples + NUM_SAMPLES, NUM_SAMPLES);
	}
}

// --- Large amplitude (saturation test) ---

TEST(FilterSetSmokeTest, largeAmplitudeDoesNotExplode) {
	configLPF(FilterMode::TRANSISTOR_24DB, ONE_Q31 / 4, ONE_Q31 / 2);

	// Feed in very hot signal
	fillDC(samples, NUM_SAMPLES, ONE_Q31 / 2);
	fs.renderLong(samples, samples + NUM_SAMPLES, NUM_SAMPLES);

	// Check output doesn't exceed input by an unreasonable margin
	// (resonance can amplify, but shouldn't explode to INT32_MAX)
	for (int i = 0; i < NUM_SAMPLES; i++) {
		CHECK(samples[i] < ONE_Q31);
		CHECK(samples[i] > -ONE_Q31);
	}
}
