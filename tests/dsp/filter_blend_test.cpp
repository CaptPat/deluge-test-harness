// Filter blend/fade path tests — exercises the dry/wet crossfade in filter.h
// that activates when dryFade >= 0.001 (e.g., after filter mode switch from OFF).

#include "CppUTest/TestHarness.h"
#include "definitions_cxx.hpp"
#include "dsp/filter/filter_set.h"
#include "dsp/filter/lpladder.h"
#include "dsp/filter/svf.h"
#include <cmath>
#include <cstring>

using namespace deluge::dsp::filter;

static constexpr int NUM_SAMPLES = 64;

// --- Direct filter blend tests (mono + stereo) ---

TEST_GROUP(FilterBlendTest) {
	LpLadderFilter ladder;
	SVFilter svf;
	q31_t samples[NUM_SAMPLES];
	q31_t stereoSamples[NUM_SAMPLES * 2];

	void setup() override {
		memset(samples, 0, sizeof(samples));
		memset(stereoSamples, 0, sizeof(stereoSamples));
	}

	void fillDC(q31_t* buf, int count, q31_t amp = ONE_Q31 / 256) {
		for (int i = 0; i < count; i++) {
			buf[i] = amp;
		}
	}

	void configureLadder(q31_t freq = ONE_Q31 / 4, q31_t reso = 0) {
		ladder.configure(freq, reso, FilterMode::TRANSISTOR_24DB, 0, ONE_Q31);
	}

	void configureSvf(q31_t freq = ONE_Q31 / 4, q31_t reso = 0) {
		svf.configure(freq, reso, FilterMode::SVF_BAND, 0, ONE_Q31);
	}
};

TEST(FilterBlendTest, resetFadeSetsBlendState) {
	ladder.dryFade = 0;
	ladder.wetLevel = ONE_Q31;

	ladder.reset(true); // fade = true
	DOUBLES_EQUAL(1.0, ladder.dryFade, 0.001);
	CHECK_EQUAL(0, ladder.wetLevel);
}

TEST(FilterBlendTest, resetNoFadeDoesNotSetBlend) {
	ladder.dryFade = 0;
	ladder.wetLevel = ONE_Q31;

	ladder.reset(false);
	DOUBLES_EQUAL(0.0, ladder.dryFade, 0.001);
	CHECK_EQUAL(ONE_Q31, ladder.wetLevel);
}

TEST(FilterBlendTest, updateBlendDecaysDryFade) {
	ladder.dryFade = 1.0;
	ladder.updateBlend();

	// After one step: dryFade *= 0.99
	DOUBLES_EQUAL(0.99, ladder.dryFade, 0.001);
	// wetLevel should be close to ONE_Q31 * 0.01
	CHECK(ladder.wetLevel > 0);
	CHECK(ladder.wetLevel < ONE_Q31);
}

TEST(FilterBlendTest, updateBlendConvergesToFullWet) {
	ladder.dryFade = 1.0;
	// 500 iterations of 0.99 decay: 0.99^500 ≈ 0.0066
	for (int i = 0; i < 500; i++) {
		ladder.updateBlend();
	}
	CHECK(ladder.dryFade < 0.01);
	// wetLevel should be very close to ONE_Q31
	CHECK(ladder.wetLevel > (ONE_Q31 - ONE_Q31 / 100));
}

TEST(FilterBlendTest, monoBlendPathProducesOutput) {
	configureLadder();
	ladder.reset(true); // activate blend: dryFade=1, wetLevel=0
	CHECK(ladder.dryFade >= 0.999); // confirm blend is active

	fillDC(samples, NUM_SAMPLES);
	ladder.filterMono(samples, samples + NUM_SAMPLES);

	// dryFade should have decayed from updateBlend calls inside filterMono
	CHECK(ladder.dryFade < 0.999);

	// Output should be non-zero (mix of dry and wet)
	bool hasOutput = false;
	for (int i = 0; i < NUM_SAMPLES; i++) {
		if (samples[i] != 0) {
			hasOutput = true;
			break;
		}
	}
	CHECK(hasOutput);
}

TEST(FilterBlendTest, monoBlendStartsDry) {
	configureLadder();
	ladder.reset(true); // dryFade=1, wetLevel=0

	q31_t original[NUM_SAMPLES];
	fillDC(samples, NUM_SAMPLES);
	memcpy(original, samples, sizeof(samples));

	ladder.filterMono(samples, samples + NUM_SAMPLES);

	// First sample should be nearly identical to original (wetLevel starts at 0)
	// But updateBlend runs per sample so it ramps quickly
	// At least verify: first sample is closer to dry than last sample
	int32_t firstDiff = abs(samples[0] - original[0]);
	int32_t lastDiff = abs(samples[NUM_SAMPLES - 1] - original[NUM_SAMPLES - 1]);
	// First should deviate less from dry than last (which is more wet)
	CHECK(firstDiff <= lastDiff || firstDiff < ONE_Q31 / 1000);
}

TEST(FilterBlendTest, stereoBlendPathProducesOutput) {
	configureLadder();
	ladder.reset(true);

	fillDC(stereoSamples, NUM_SAMPLES * 2);
	ladder.filterStereo(stereoSamples, stereoSamples + NUM_SAMPLES * 2);

	bool hasOutput = false;
	for (int i = 0; i < NUM_SAMPLES * 2; i++) {
		if (stereoSamples[i] != 0) {
			hasOutput = true;
			break;
		}
	}
	CHECK(hasOutput);
}

TEST(FilterBlendTest, svfMonoBlendPath) {
	configureSvf();
	svf.reset(true);

	fillDC(samples, NUM_SAMPLES);
	svf.filterMono(samples, samples + NUM_SAMPLES);

	bool hasOutput = false;
	for (int i = 0; i < NUM_SAMPLES; i++) {
		if (samples[i] != 0) {
			hasOutput = true;
			break;
		}
	}
	CHECK(hasOutput);
}

TEST(FilterBlendTest, svfStereoBlendPath) {
	configureSvf();
	svf.reset(true);

	fillDC(stereoSamples, NUM_SAMPLES * 2);
	svf.filterStereo(stereoSamples, stereoSamples + NUM_SAMPLES * 2);

	bool hasOutput = false;
	for (int i = 0; i < NUM_SAMPLES * 2; i++) {
		if (stereoSamples[i] != 0) {
			hasOutput = true;
			break;
		}
	}
	CHECK(hasOutput);
}

// --- FilterSet integration: trigger blend via mode switch from OFF ---

TEST_GROUP(FilterSetBlendTest) {
	FilterSet fs;
	q31_t samples[NUM_SAMPLES];
	q31_t stereoSamples[NUM_SAMPLES * 2];

	void setup() override {
		fs.reset();
		memset(samples, 0, sizeof(samples));
		memset(stereoSamples, 0, sizeof(stereoSamples));
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
};

TEST(FilterSetBlendTest, modeTransitionFromOffTriggersBlend) {
	// First call with OFF to set lastLPFMode_ = OFF
	q31_t gain = ONE_Q31;
	q31_t oscAmp = ONE_Q31;
	fs.setConfig(0, 0, FilterMode::OFF, 0,
	             0, 0, FilterMode::OFF, 0,
	             gain, FilterRoute::HIGH_TO_LOW, false, &oscAmp);

	// Now switch to ladder — should trigger reset(true) since lastLPFMode_==OFF
	configLPF(FilterMode::TRANSISTOR_24DB, ONE_Q31 / 4);

	fillDC(samples, NUM_SAMPLES);
	fs.renderLong(samples, samples + NUM_SAMPLES, NUM_SAMPLES);

	// Should produce output via blend path
	bool hasOutput = false;
	for (int i = 0; i < NUM_SAMPLES; i++) {
		if (samples[i] != 0) {
			hasOutput = true;
			break;
		}
	}
	CHECK(hasOutput);
}

TEST(FilterSetBlendTest, modeTransitionFromOffToSvfTriggersBlend) {
	q31_t gain = ONE_Q31;
	q31_t oscAmp = ONE_Q31;
	fs.setConfig(0, 0, FilterMode::OFF, 0,
	             0, 0, FilterMode::OFF, 0,
	             gain, FilterRoute::HIGH_TO_LOW, false, &oscAmp);

	configLPF(FilterMode::SVF_BAND, ONE_Q31 / 4);

	fillDC(samples, NUM_SAMPLES);
	fs.renderLong(samples, samples + NUM_SAMPLES, NUM_SAMPLES);

	bool hasOutput = false;
	for (int i = 0; i < NUM_SAMPLES; i++) {
		if (samples[i] != 0) {
			hasOutput = true;
			break;
		}
	}
	CHECK(hasOutput);
}

TEST(FilterSetBlendTest, stereoModeTransitionFromOffTriggersBlend) {
	q31_t gain = ONE_Q31;
	q31_t oscAmp = ONE_Q31;
	fs.setConfig(0, 0, FilterMode::OFF, 0,
	             0, 0, FilterMode::OFF, 0,
	             gain, FilterRoute::HIGH_TO_LOW, false, &oscAmp);

	configLPF(FilterMode::TRANSISTOR_24DB, ONE_Q31 / 4);

	fillDC(stereoSamples, NUM_SAMPLES * 2);
	fs.renderLongStereo(stereoSamples, stereoSamples + NUM_SAMPLES * 2);

	bool hasOutput = false;
	for (int i = 0; i < NUM_SAMPLES * 2; i++) {
		if (stereoSamples[i] != 0) {
			hasOutput = true;
			break;
		}
	}
	CHECK(hasOutput);
}
