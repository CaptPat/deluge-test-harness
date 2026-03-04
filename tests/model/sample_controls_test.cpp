// Phase 9C: SampleControls tests — interpolation buffer size, reversed state

#include "CppUTest/TestHarness.h"
#include "definitions_cxx.hpp"
#include "model/sample/sample_controls.h"
#include "processing/engines/audio_engine.h"

TEST_GROUP(SampleControlsTest){
    SampleControls ctrl;

void setup(){
    ctrl = SampleControls();
AudioEngine::cpuDireness = 0;
}
}
;

TEST(SampleControlsTest, constructorDefaults) {
	CHECK(ctrl.interpolationMode == InterpolationMode::SMOOTH);
	CHECK_FALSE(ctrl.pitchAndSpeedAreIndependent);
	CHECK_FALSE(ctrl.reversed);
	CHECK_FALSE(ctrl.invertReversed);
}

TEST(SampleControlsTest, getInterpolationBufferSizeSmooth) {
	// SMOOTH mode with no CPU direness → full interpolation buffer
	int32_t size = ctrl.getInterpolationBufferSize(1 << 24);
	CHECK_EQUAL(kInterpolationMaxNumSamples, size);
}

TEST(SampleControlsTest, getInterpolationBufferSizeLinear) {
	ctrl.interpolationMode = InterpolationMode::LINEAR;
	int32_t size = ctrl.getInterpolationBufferSize(1 << 24);
	CHECK_EQUAL(2, size);
}

TEST(SampleControlsTest, getInterpolationBufferSizeCpuDire) {
	// High CPU direness + high phase increment → falls back to linear (size 2)
	AudioEngine::cpuDireness = 14; // Max direness
	// phaseIncrement with magnitude >= 26 - (14>>2) = 26 - 3 = 23
	// getMagnitudeOld(1 << 25) = 32 - clz(1<<25) = 32 - 6 = 26, which is >= 23
	int32_t size = ctrl.getInterpolationBufferSize(1 << 25);
	CHECK_EQUAL(2, size);
}

TEST(SampleControlsTest, getInterpolationBufferSizeLowDireStaysSmooth) {
	AudioEngine::cpuDireness = 4;
	// getMagnitudeOld(1 << 20) = 32 - clz(1<<20) = 32 - 11 = 21
	// Threshold: 26 - (4>>2) = 26 - 1 = 25, so 21 < 25 → stays smooth
	int32_t size = ctrl.getInterpolationBufferSize(1 << 20);
	CHECK_EQUAL(kInterpolationMaxNumSamples, size);
}

TEST(SampleControlsTest, isCurrentlyReversedDefault) {
	CHECK_FALSE(ctrl.isCurrentlyReversed());
}

TEST(SampleControlsTest, isCurrentlyReversedTrue) {
	ctrl.reversed = true;
	CHECK(ctrl.isCurrentlyReversed());
}

TEST(SampleControlsTest, isCurrentlyReversedInverted) {
	// invertReversed=true, reversed=true → !reversed → false
	ctrl.reversed = true;
	ctrl.invertReversed = true;
	CHECK_FALSE(ctrl.isCurrentlyReversed());
}

TEST(SampleControlsTest, isCurrentlyReversedInvertedNotReversed) {
	// invertReversed=true, reversed=false → !false → true
	ctrl.invertReversed = true;
	CHECK(ctrl.isCurrentlyReversed());
}
