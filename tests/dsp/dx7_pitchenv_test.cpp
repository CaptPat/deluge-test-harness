#include "CppUTest/TestHarness.h"
#include "dsp/dx/pitchenv.h"

TEST_GROUP(DX7PitchEnvTest) {
	PitchEnv penv;
	EnvParams params;

	void setup() override {
		PitchEnv::init(44100.0);
		params.rates[0] = 80;
		params.rates[1] = 50;
		params.rates[2] = 50;
		params.rates[3] = 60;
		params.levels[0] = 75; // above center (50)
		params.levels[1] = 60;
		params.levels[2] = 50; // center = no pitch change
		params.levels[3] = 50;
	}
};

TEST(DX7PitchEnvTest, setSetsLevel) {
	penv.set(params);
	// After set, level is initialized from levels[3] via pitchenv_tab
	// pitchenv_tab[50] = 0, so level should be 0 << 19 = 0
	int32_t sample = penv.getsample(params, 0);
	CHECK_EQUAL(0, sample);
}

TEST(DX7PitchEnvTest, getSampleAdvances) {
	penv.set(params);
	int32_t first = penv.getsample(params, 1);
	// Run for many samples
	int32_t later = first;
	for (int i = 0; i < 500; i++) {
		later = penv.getsample(params, 64);
	}
	// With non-center target levels, pitch should move
	CHECK(later != first);
}

TEST(DX7PitchEnvTest, keydownTriggers) {
	penv.set(params);
	for (int i = 0; i < 100; i++) {
		penv.getsample(params, 64);
	}
	// Release
	penv.keydown(params, false);
	int32_t releaseVal = penv.getsample(params, 64);
	// Re-trigger
	penv.keydown(params, true);
	int32_t retriggerVal = penv.getsample(params, 64);
	// Just verify no crash
	(void)releaseVal;
	(void)retriggerVal;
}

TEST(DX7PitchEnvTest, keyupRelease) {
	penv.set(params);
	// Advance
	for (int i = 0; i < 200; i++) {
		penv.getsample(params, 64);
	}
	CHECK(penv.isDown());
	penv.keydown(params, false);
	CHECK_FALSE(penv.isDown());
}

TEST(DX7PitchEnvTest, getPositionReturnsIx) {
	penv.set(params);
	char step = -1;
	penv.getPosition(&step);
	CHECK(step >= 0);
	CHECK(step <= 3);
}

TEST(DX7PitchEnvTest, pitchenvTabCenter) {
	// pitchenv_tab[50] should be 0 (center = no pitch change)
	CHECK_EQUAL(0, pitchenv_tab[50]);
}

TEST(DX7PitchEnvTest, pitchenvTabExtremes) {
	// Tab values at extremes
	CHECK_EQUAL(-128, pitchenv_tab[0]);
	CHECK_EQUAL(127, pitchenv_tab[99]);
}
