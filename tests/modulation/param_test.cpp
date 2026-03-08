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

// --- isParamArpRhythm ---

TEST(ParamTest, arpRhythmIsArpRhythm) {
	CHECK_TRUE(params::isParamArpRhythm(params::Kind::UNPATCHED_SOUND, params::UNPATCHED_ARP_RHYTHM));
}

TEST(ParamTest, volumeIsNotArpRhythm) {
	CHECK_FALSE(params::isParamArpRhythm(params::Kind::PATCHED, params::LOCAL_OSC_A_VOLUME));
}

TEST(ParamTest, wrongKindNotArpRhythm) {
	CHECK_FALSE(params::isParamArpRhythm(params::Kind::UNPATCHED_GLOBAL, params::UNPATCHED_ARP_RHYTHM));
}

// --- isParamPitchBend ---

TEST(ParamTest, pitchBendIsPitchBend) {
	CHECK_TRUE(params::isParamPitchBend(params::Kind::EXPRESSION, X_PITCH_BEND));
}

TEST(ParamTest, volumeIsNotPitchBend) {
	CHECK_FALSE(params::isParamPitchBend(params::Kind::PATCHED, params::LOCAL_OSC_A_VOLUME));
}

TEST(ParamTest, wrongKindNotPitchBend) {
	CHECK_FALSE(params::isParamPitchBend(params::Kind::PATCHED, X_PITCH_BEND));
}

// --- isParamPitch (additional branches) ---

TEST(ParamTest, unpatchedGlobalPitchIsPitch) {
	CHECK_TRUE(params::isParamPitch(params::Kind::UNPATCHED_GLOBAL, params::UNPATCHED_PITCH_ADJUST));
}

TEST(ParamTest, oscBPitchIsPitch) {
	CHECK_TRUE(params::isParamPitch(params::Kind::PATCHED, params::LOCAL_OSC_B_PITCH_ADJUST));
}

TEST(ParamTest, mod0PitchIsPitch) {
	CHECK_TRUE(params::isParamPitch(params::Kind::PATCHED, params::LOCAL_MODULATOR_0_PITCH_ADJUST));
}

TEST(ParamTest, mod1PitchIsPitch) {
	CHECK_TRUE(params::isParamPitch(params::Kind::PATCHED, params::LOCAL_MODULATOR_1_PITCH_ADJUST));
}

TEST(ParamTest, expressionKindIsNotPitch) {
	CHECK_FALSE(params::isParamPitch(params::Kind::EXPRESSION, 0));
}

// --- isParamBipolar (additional branches) ---

TEST(ParamTest, patchCableIsBipolar) {
	CHECK_TRUE(params::isParamBipolar(params::Kind::PATCH_CABLE, 0));
}

TEST(ParamTest, pitchBendIsBipolar) {
	CHECK_TRUE(params::isParamBipolar(params::Kind::EXPRESSION, X_PITCH_BEND));
}

// --- isVibratoPatchCableShortcut ---

TEST(ParamTest, vibratoShortcutCorrectPos) {
	CHECK_TRUE(params::isVibratoPatchCableShortcut(6, 2));
}

TEST(ParamTest, vibratoShortcutWrongPos) {
	CHECK_FALSE(params::isVibratoPatchCableShortcut(5, 2));
	CHECK_FALSE(params::isVibratoPatchCableShortcut(6, 3));
}

// --- isSidechainPatchCableShortcut ---

TEST(ParamTest, sidechainShortcutCorrectPos) {
	CHECK_TRUE(params::isSidechainPatchCableShortcut(10, 2));
}

TEST(ParamTest, sidechainShortcutWrongPos) {
	CHECK_FALSE(params::isSidechainPatchCableShortcut(9, 2));
	CHECK_FALSE(params::isSidechainPatchCableShortcut(10, 1));
}

// --- isPatchCableShortcut ---

TEST(ParamTest, patchCableShortcutVibrato) {
	CHECK_TRUE(params::isPatchCableShortcut(6, 2));
}

TEST(ParamTest, patchCableShortcutSidechain) {
	CHECK_TRUE(params::isPatchCableShortcut(10, 2));
}

TEST(ParamTest, patchCableShortcutNeither) {
	CHECK_FALSE(params::isPatchCableShortcut(0, 0));
	CHECK_FALSE(params::isPatchCableShortcut(7, 2));
}

// --- expressionParamFromShortcut ---

TEST(ParamTest, expressionShortcutPitchBend) {
	CHECK_EQUAL(X_PITCH_BEND, params::expressionParamFromShortcut(14, 7));
}

TEST(ParamTest, expressionShortcutPressure) {
	CHECK_EQUAL(Z_PRESSURE, params::expressionParamFromShortcut(15, 0));
}

TEST(ParamTest, expressionShortcutSlideTimbre) {
	CHECK_EQUAL(Y_SLIDE_TIMBRE, params::expressionParamFromShortcut(15, 7));
}

TEST(ParamTest, expressionShortcutNoMatch) {
	CHECK_EQUAL(params::kNoParamID, params::expressionParamFromShortcut(0, 0));
}

// --- paramNeedsLPF branch coverage ---

TEST(ParamTest, paramNeedsLPFVolumeNotAutomation) {
	CHECK_TRUE(params::paramNeedsLPF(params::LOCAL_OSC_A_VOLUME, false));
}

TEST(ParamTest, paramNeedsLPFVolumeFromAutomation) {
	CHECK_FALSE(params::paramNeedsLPF(params::LOCAL_OSC_A_VOLUME, true));
}

TEST(ParamTest, paramNeedsLPFAlwaysTrue) {
	// Modulator volume always needs LPF regardless of automation
	CHECK_TRUE(params::paramNeedsLPF(params::LOCAL_MODULATOR_0_VOLUME, false));
	CHECK_TRUE(params::paramNeedsLPF(params::LOCAL_MODULATOR_0_VOLUME, true));
}

TEST(ParamTest, paramNeedsLPFDefault) {
	// Attack param falls into default case → false
	CHECK_FALSE(params::paramNeedsLPF(params::LOCAL_ENV_0_ATTACK, false));
}

// --- isParamPan (unpatched) ---

TEST(ParamTest, unpatchedPanIsPan) {
	CHECK_TRUE(params::isParamPan(params::Kind::UNPATCHED_GLOBAL, params::UNPATCHED_PAN));
}

TEST(ParamTest, wrongKindNotPan) {
	CHECK_FALSE(params::isParamPan(params::Kind::UNPATCHED_SOUND, params::LOCAL_PAN));
}
