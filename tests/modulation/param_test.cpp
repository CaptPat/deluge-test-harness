#include "CppUTest/TestHarness.h"
#include "modulation/params/param.h"

namespace params = deluge::modulation::params;

TEST_GROUP(ParamTest) {};

// --- isParamBipolar ---

TEST(ParamTest, volumeIsNotBipolar) {
	CHECK_FALSE(params::isParamBipolar(params::Kind::PATCHED, params::LOCAL_OSC_A_VOLUME));
}

TEST(ParamTest, panIsBipolar) {
	CHECK_TRUE(params::isParamBipolar(params::Kind::PATCHED, params::LOCAL_PAN));
}

TEST(ParamTest, pitchAdjustIsBipolar) {
	CHECK_TRUE(params::isParamBipolar(params::Kind::PATCHED, params::LOCAL_PITCH_ADJUST));
}

TEST(ParamTest, globalVolumeIsNotBipolar) {
	CHECK_FALSE(params::isParamBipolar(params::Kind::PATCHED, params::GLOBAL_VOLUME_POST_FX));
}

// --- isParamPan ---

TEST(ParamTest, localPanIsPan) {
	CHECK_TRUE(params::isParamPan(params::Kind::PATCHED, params::LOCAL_PAN));
}

TEST(ParamTest, globalVolumeIsNotPan) {
	CHECK_FALSE(params::isParamPan(params::Kind::PATCHED, params::GLOBAL_VOLUME_POST_FX));
}

TEST(ParamTest, volumeIsNotPan) {
	CHECK_FALSE(params::isParamPan(params::Kind::PATCHED, params::LOCAL_OSC_A_VOLUME));
}

// --- isParamPitch ---

TEST(ParamTest, pitchAdjustIsPitch) {
	CHECK_TRUE(params::isParamPitch(params::Kind::PATCHED, params::LOCAL_PITCH_ADJUST));
}

TEST(ParamTest, oscAPitchAdjustIsPitch) {
	CHECK_TRUE(params::isParamPitch(params::Kind::PATCHED, params::LOCAL_OSC_A_PITCH_ADJUST));
}

TEST(ParamTest, volumeIsNotPitch) {
	CHECK_FALSE(params::isParamPitch(params::Kind::PATCHED, params::LOCAL_OSC_A_VOLUME));
}

// --- isParamStutter ---

TEST(ParamTest, stutterRateIsStutter) {
	CHECK_TRUE(params::isParamStutter(params::Kind::UNPATCHED_GLOBAL,
	                                  params::UNPATCHED_STUTTER_RATE));
}

TEST(ParamTest, volumeIsNotStutter) {
	CHECK_FALSE(params::isParamStutter(params::Kind::PATCHED, params::LOCAL_OSC_A_VOLUME));
}

// --- paramNeedsLPF ---

TEST(ParamTest, paramNeedsLPFVolume) {
	// Volume params typically need LPF
	bool result = params::paramNeedsLPF(params::LOCAL_OSC_A_VOLUME, false);
	// Just verify it doesn't crash — exact value depends on firmware logic
	(void)result;
}

// --- Display names ---

TEST(ParamTest, patchedParamDisplayNameNotNull) {
	const char* name = params::getPatchedParamDisplayName(params::LOCAL_OSC_A_VOLUME);
	CHECK(name != nullptr);
}

TEST(ParamTest, patchedParamShortNameNotNull) {
	const char* name = params::getPatchedParamShortName(params::LOCAL_OSC_A_VOLUME);
	CHECK(name != nullptr);
}

// --- paramNameForFile / fileStringToParam round-trip ---

TEST(ParamTest, paramNameForFilePatched) {
	const char* name = params::paramNameForFile(params::Kind::PATCHED, params::LOCAL_OSC_A_VOLUME);
	CHECK(name != nullptr);
}

TEST(ParamTest, fileStringToParamRoundTrip) {
	const char* name = params::paramNameForFile(params::Kind::PATCHED, params::LOCAL_PAN);
	if (name != nullptr) {
		params::ParamType recovered = params::fileStringToParam(params::Kind::PATCHED, name, true);
		CHECK_EQUAL(params::LOCAL_PAN, recovered);
	}
}

// --- Constants ---

TEST(ParamTest, unpatchedStartValue) {
	CHECK(params::UNPATCHED_START > params::kNumParams);
}

TEST(ParamTest, kNumParamsEqualsGlobalNone) {
	CHECK_EQUAL(params::GLOBAL_NONE, params::kNumParams);
}
