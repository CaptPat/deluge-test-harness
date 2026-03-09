// Tests for model/fx/stutterer.cpp — stutter effect (record/playback buffer)
#include "CppUTest/TestHarness.h"
#include "dsp/stereo_sample.h"
#include "model/fx/stutterer.h"
#include "modulation/params/param_collection_summary.h"
#include "modulation/params/param_manager.h"
#include "modulation/params/param_set.h"
#include <cstring>

namespace params = deluge::modulation::params;

// Helper to set up a ParamManagerForTimeline with an UnpatchedParamSet
struct StutterTestHelper {
	ParamCollectionSummary summary{};
	UnpatchedParamSet unpatchedParams{&summary};
	ParamManagerForTimeline paramManager;

	StutterTestHelper() {
		summary.paramCollection = &unpatchedParams;
		paramManager.summaries[0] = summary;
	}
	~StutterTestHelper() {
		// Prevent real destructor from delugeDealloc on stack objects
		paramManager.summaries[0].paramCollection = nullptr;
	}
};

TEST_GROUP(StutterTest) {
	Stutterer stut;
	StutterTestHelper helper;

	void setup() override {
		stut = Stutterer{};
	}
};

TEST(StutterTest, defaultConstruction) {
	// isStuttering(nullptr) is true when not stuttering — stutterSource starts null
	CHECK_TRUE(stut.isStuttering(nullptr));
	CHECK_FALSE(stut.isStuttering((void*)0x1234));
}

TEST(StutterTest, initParamsDoesNotCrash) {
	Stutterer::initParams(&helper.paramManager);
	int32_t val = helper.unpatchedParams.getValue(params::UNPATCHED_STUTTER_RATE);
	CHECK_EQUAL(0, val);
}

TEST(StutterTest, beginStutterDefaultConfig) {
	Stutterer::initParams(&helper.paramManager);
	StutterConfig sc{};
	void* source = (void*)0xBEEF;

	Error err = stut.beginStutter(source, &helper.paramManager, sc, 7, 1 << 24);
	CHECK_TRUE(err == Error::NONE);
	CHECK_TRUE(stut.isStuttering(source));
	CHECK_FALSE(stut.isStuttering(nullptr));
}

TEST(StutterTest, endStutterClearsState) {
	Stutterer::initParams(&helper.paramManager);
	StutterConfig sc{};
	void* source = (void*)0xBEEF;

	stut.beginStutter(source, &helper.paramManager, sc, 7, 1 << 24);
	stut.endStutter(&helper.paramManager);
	CHECK_FALSE(stut.isStuttering(source));
}

TEST(StutterTest, endStutterWithoutParamManager) {
	Stutterer::initParams(&helper.paramManager);
	StutterConfig sc{};
	void* source = (void*)0xBEEF;

	stut.beginStutter(source, &helper.paramManager, sc, 7, 1 << 24);
	stut.endStutter(nullptr);
	CHECK_FALSE(stut.isStuttering(source));
}

TEST(StutterTest, processRecordingPhase) {
	Stutterer::initParams(&helper.paramManager);
	StutterConfig sc{.quantized = false};
	void* source = (void*)0xBEEF;

	stut.beginStutter(source, &helper.paramManager, sc, 7, 1 << 24);

	StereoSample buf[32];
	for (auto& s : buf) {
		s.l = 1 << 20;
		s.r = 1 << 20;
	}
	stut.processStutter({buf, 32}, &helper.paramManager, 7, 1 << 24);
	CHECK_TRUE(stut.isStuttering(source));

	stut.endStutter(&helper.paramManager);
}

TEST(StutterTest, quantizedStutterSnapsToPreset) {
	Stutterer::initParams(&helper.paramManager);
	helper.unpatchedParams.params[params::UNPATCHED_STUTTER_RATE]
	    .setCurrentValueBasicForSetup(0);

	StutterConfig sc{.quantized = true};
	void* source = (void*)0xBEEF;

	Error err = stut.beginStutter(source, &helper.paramManager, sc, 7, 1 << 24);
	CHECK_TRUE(err == Error::NONE);

	stut.endStutter(&helper.paramManager);
}

TEST(StutterTest, pingPongConfig) {
	Stutterer::initParams(&helper.paramManager);
	StutterConfig sc{.quantized = false, .pingPong = true};
	void* source = (void*)0xBEEF;

	Error err = stut.beginStutter(source, &helper.paramManager, sc, 7, 1 << 24);
	CHECK_TRUE(err == Error::NONE);

	stut.endStutter(&helper.paramManager);
}

TEST(StutterTest, reversedConfig) {
	Stutterer::initParams(&helper.paramManager);
	StutterConfig sc{.quantized = false, .reversed = true};
	void* source = (void*)0xBEEF;

	Error err = stut.beginStutter(source, &helper.paramManager, sc, 7, 1 << 24);
	CHECK_TRUE(err == Error::NONE);

	stut.endStutter(&helper.paramManager);
}

TEST(StutterTest, processMultipleBlocksTransitionsToPlayback) {
	Stutterer::initParams(&helper.paramManager);
	StutterConfig sc{.quantized = false};
	void* source = (void*)0xBEEF;

	stut.beginStutter(source, &helper.paramManager, sc, 7, 1 << 24);

	StereoSample buf[64];
	for (int block = 0; block < 50; block++) {
		for (auto& s : buf) {
			s.l = (1 << 18) * ((block % 2) ? 1 : -1);
			s.r = (1 << 18) * ((block % 2) ? -1 : 1);
		}
		stut.processStutter({buf, 64}, &helper.paramManager, 7, 1 << 24);
	}

	CHECK_TRUE(stut.isStuttering(source));
	stut.endStutter(&helper.paramManager);
}

TEST(StutterTest, globalStuttererExists) {
	// Global stutterer exists and has no active source
	CHECK_FALSE(stutterer.isStuttering((void*)0x1));
}
