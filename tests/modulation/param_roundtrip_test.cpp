// Comprehensive round-trip tests for paramNameForFile <-> fileStringToParam
// and edge cases for param classification functions (bipolar, pan, pitch, stutter, LPF).

#include "CppUTest/TestHarness.h"
#include "modulation/params/param.h"

namespace params = deluge::modulation::params;

// ── Helper: verify round-trip for a single param ────────────────────────────

static void checkRoundTrip(params::Kind kind, int32_t param, const char* expectedName, bool allowPatched = true) {
	const char* name = params::paramNameForFile(kind, param);
	STRCMP_EQUAL(expectedName, name);
	params::ParamType recovered = params::fileStringToParam(kind, name, allowPatched);
	CHECK_EQUAL(param, recovered);
}

// ── Local patched params ────────────────────────────────────────────────────

TEST_GROUP(ParamRoundTripLocal) {};

TEST(ParamRoundTripLocal, oscAVolume) {
	checkRoundTrip(params::Kind::PATCHED, params::LOCAL_OSC_A_VOLUME, "oscAVolume");
}
TEST(ParamRoundTripLocal, oscBVolume) {
	checkRoundTrip(params::Kind::PATCHED, params::LOCAL_OSC_B_VOLUME, "oscBVolume");
}
TEST(ParamRoundTripLocal, volume) {
	checkRoundTrip(params::Kind::PATCHED, params::LOCAL_VOLUME, "volume");
}
TEST(ParamRoundTripLocal, noiseVolume) {
	checkRoundTrip(params::Kind::PATCHED, params::LOCAL_NOISE_VOLUME, "noiseVolume");
}
TEST(ParamRoundTripLocal, mod0Volume) {
	checkRoundTrip(params::Kind::PATCHED, params::LOCAL_MODULATOR_0_VOLUME, "modulator1Volume");
}
TEST(ParamRoundTripLocal, mod1Volume) {
	checkRoundTrip(params::Kind::PATCHED, params::LOCAL_MODULATOR_1_VOLUME, "modulator2Volume");
}
TEST(ParamRoundTripLocal, fold) {
	checkRoundTrip(params::Kind::PATCHED, params::LOCAL_FOLD, "waveFold");
}
TEST(ParamRoundTripLocal, mod0Feedback) {
	checkRoundTrip(params::Kind::PATCHED, params::LOCAL_MODULATOR_0_FEEDBACK, "modulator1Feedback");
}
TEST(ParamRoundTripLocal, mod1Feedback) {
	checkRoundTrip(params::Kind::PATCHED, params::LOCAL_MODULATOR_1_FEEDBACK, "modulator2Feedback");
}
TEST(ParamRoundTripLocal, carrier0Feedback) {
	checkRoundTrip(params::Kind::PATCHED, params::LOCAL_CARRIER_0_FEEDBACK, "carrier1Feedback");
}
TEST(ParamRoundTripLocal, carrier1Feedback) {
	checkRoundTrip(params::Kind::PATCHED, params::LOCAL_CARRIER_1_FEEDBACK, "carrier2Feedback");
}
TEST(ParamRoundTripLocal, lpfResonance) {
	checkRoundTrip(params::Kind::PATCHED, params::LOCAL_LPF_RESONANCE, "lpfResonance");
}
TEST(ParamRoundTripLocal, hpfResonance) {
	checkRoundTrip(params::Kind::PATCHED, params::LOCAL_HPF_RESONANCE, "hpfResonance");
}
TEST(ParamRoundTripLocal, env0Sustain) {
	checkRoundTrip(params::Kind::PATCHED, params::LOCAL_ENV_0_SUSTAIN, "env1Sustain");
}
TEST(ParamRoundTripLocal, env1Sustain) {
	checkRoundTrip(params::Kind::PATCHED, params::LOCAL_ENV_1_SUSTAIN, "env2Sustain");
}
TEST(ParamRoundTripLocal, env2Sustain) {
	checkRoundTrip(params::Kind::PATCHED, params::LOCAL_ENV_2_SUSTAIN, "env3Sustain");
}
TEST(ParamRoundTripLocal, env3Sustain) {
	checkRoundTrip(params::Kind::PATCHED, params::LOCAL_ENV_3_SUSTAIN, "env4Sustain");
}
TEST(ParamRoundTripLocal, lpfMorph) {
	checkRoundTrip(params::Kind::PATCHED, params::LOCAL_LPF_MORPH, "lpfMorph");
}
TEST(ParamRoundTripLocal, hpfMorph) {
	checkRoundTrip(params::Kind::PATCHED, params::LOCAL_HPF_MORPH, "hpfMorph");
}
TEST(ParamRoundTripLocal, oscAPhaseWidth) {
	checkRoundTrip(params::Kind::PATCHED, params::LOCAL_OSC_A_PHASE_WIDTH, "oscAPhaseWidth");
}
TEST(ParamRoundTripLocal, oscBPhaseWidth) {
	checkRoundTrip(params::Kind::PATCHED, params::LOCAL_OSC_B_PHASE_WIDTH, "oscBPhaseWidth");
}
TEST(ParamRoundTripLocal, oscAWaveIndex) {
	checkRoundTrip(params::Kind::PATCHED, params::LOCAL_OSC_A_WAVE_INDEX, "oscAWavetablePosition");
}
TEST(ParamRoundTripLocal, oscBWaveIndex) {
	checkRoundTrip(params::Kind::PATCHED, params::LOCAL_OSC_B_WAVE_INDEX, "oscBWavetablePosition");
}
TEST(ParamRoundTripLocal, pan) {
	checkRoundTrip(params::Kind::PATCHED, params::LOCAL_PAN, "pan");
}
TEST(ParamRoundTripLocal, lpfFreq) {
	checkRoundTrip(params::Kind::PATCHED, params::LOCAL_LPF_FREQ, "lpfFrequency");
}
TEST(ParamRoundTripLocal, pitchAdjust) {
	checkRoundTrip(params::Kind::PATCHED, params::LOCAL_PITCH_ADJUST, "pitch");
}
TEST(ParamRoundTripLocal, oscAPitch) {
	checkRoundTrip(params::Kind::PATCHED, params::LOCAL_OSC_A_PITCH_ADJUST, "oscAPitch");
}
TEST(ParamRoundTripLocal, oscBPitch) {
	checkRoundTrip(params::Kind::PATCHED, params::LOCAL_OSC_B_PITCH_ADJUST, "oscBPitch");
}
TEST(ParamRoundTripLocal, mod0Pitch) {
	checkRoundTrip(params::Kind::PATCHED, params::LOCAL_MODULATOR_0_PITCH_ADJUST, "modulator1Pitch");
}
TEST(ParamRoundTripLocal, mod1Pitch) {
	checkRoundTrip(params::Kind::PATCHED, params::LOCAL_MODULATOR_1_PITCH_ADJUST, "modulator2Pitch");
}
TEST(ParamRoundTripLocal, hpfFreq) {
	checkRoundTrip(params::Kind::PATCHED, params::LOCAL_HPF_FREQ, "hpfFrequency");
}
TEST(ParamRoundTripLocal, lfoLocalFreq1) {
	checkRoundTrip(params::Kind::PATCHED, params::LOCAL_LFO_LOCAL_FREQ_1, "lfo2Rate");
}
TEST(ParamRoundTripLocal, lfoLocalFreq2) {
	checkRoundTrip(params::Kind::PATCHED, params::LOCAL_LFO_LOCAL_FREQ_2, "lfo4Rate");
}
TEST(ParamRoundTripLocal, env0Attack) {
	checkRoundTrip(params::Kind::PATCHED, params::LOCAL_ENV_0_ATTACK, "env1Attack");
}
TEST(ParamRoundTripLocal, env1Attack) {
	checkRoundTrip(params::Kind::PATCHED, params::LOCAL_ENV_1_ATTACK, "env2Attack");
}
TEST(ParamRoundTripLocal, env2Attack) {
	checkRoundTrip(params::Kind::PATCHED, params::LOCAL_ENV_2_ATTACK, "env3Attack");
}
TEST(ParamRoundTripLocal, env3Attack) {
	checkRoundTrip(params::Kind::PATCHED, params::LOCAL_ENV_3_ATTACK, "env4Attack");
}
TEST(ParamRoundTripLocal, env0Decay) {
	checkRoundTrip(params::Kind::PATCHED, params::LOCAL_ENV_0_DECAY, "env1Decay");
}
TEST(ParamRoundTripLocal, env1Decay) {
	checkRoundTrip(params::Kind::PATCHED, params::LOCAL_ENV_1_DECAY, "env2Decay");
}
TEST(ParamRoundTripLocal, env2Decay) {
	checkRoundTrip(params::Kind::PATCHED, params::LOCAL_ENV_2_DECAY, "env3Decay");
}
TEST(ParamRoundTripLocal, env3Decay) {
	checkRoundTrip(params::Kind::PATCHED, params::LOCAL_ENV_3_DECAY, "env4Decay");
}
TEST(ParamRoundTripLocal, env0Release) {
	checkRoundTrip(params::Kind::PATCHED, params::LOCAL_ENV_0_RELEASE, "env1Release");
}
TEST(ParamRoundTripLocal, env1Release) {
	checkRoundTrip(params::Kind::PATCHED, params::LOCAL_ENV_1_RELEASE, "env2Release");
}
TEST(ParamRoundTripLocal, env2Release) {
	checkRoundTrip(params::Kind::PATCHED, params::LOCAL_ENV_2_RELEASE, "env3Release");
}
TEST(ParamRoundTripLocal, env3Release) {
	checkRoundTrip(params::Kind::PATCHED, params::LOCAL_ENV_3_RELEASE, "env4Release");
}

// ── Global patched params ───────────────────────────────────────────────────

TEST_GROUP(ParamRoundTripGlobal) {};

TEST(ParamRoundTripGlobal, volumePostFX) {
	checkRoundTrip(params::Kind::PATCHED, params::GLOBAL_VOLUME_POST_FX, "volumePostFX");
}
TEST(ParamRoundTripGlobal, volumePostReverbSend) {
	checkRoundTrip(params::Kind::PATCHED, params::GLOBAL_VOLUME_POST_REVERB_SEND, "volumePostReverbSend");
}
TEST(ParamRoundTripGlobal, reverbAmount) {
	checkRoundTrip(params::Kind::PATCHED, params::GLOBAL_REVERB_AMOUNT, "reverbAmount");
}
TEST(ParamRoundTripGlobal, modFXDepth) {
	checkRoundTrip(params::Kind::PATCHED, params::GLOBAL_MOD_FX_DEPTH, "modFXDepth");
}
TEST(ParamRoundTripGlobal, delayFeedback) {
	checkRoundTrip(params::Kind::PATCHED, params::GLOBAL_DELAY_FEEDBACK, "delayFeedback");
}
TEST(ParamRoundTripGlobal, delayRate) {
	checkRoundTrip(params::Kind::PATCHED, params::GLOBAL_DELAY_RATE, "delayRate");
}
TEST(ParamRoundTripGlobal, modFXRate) {
	checkRoundTrip(params::Kind::PATCHED, params::GLOBAL_MOD_FX_RATE, "modFXRate");
}
TEST(ParamRoundTripGlobal, lfoFreq1) {
	checkRoundTrip(params::Kind::PATCHED, params::GLOBAL_LFO_FREQ_1, "lfo1Rate");
}
TEST(ParamRoundTripGlobal, lfoFreq2) {
	checkRoundTrip(params::Kind::PATCHED, params::GLOBAL_LFO_FREQ_2, "lfo3Rate");
}
TEST(ParamRoundTripGlobal, arpRate) {
	checkRoundTrip(params::Kind::PATCHED, params::GLOBAL_ARP_RATE, "arpRate");
}

// ── Unpatched shared params ─────────────────────────────────────────────────

TEST_GROUP(ParamRoundTripUnpatchedShared) {};

static void checkUnpatchedShared(params::ParamType rawEnum, const char* expectedName) {
	int32_t param = rawEnum + params::UNPATCHED_START;
	// Should work for both UNPATCHED_SOUND and UNPATCHED_GLOBAL kinds
	checkRoundTrip(params::Kind::UNPATCHED_SOUND, param, expectedName);
	checkRoundTrip(params::Kind::UNPATCHED_GLOBAL, param, expectedName);
}

TEST(ParamRoundTripUnpatchedShared, stutterRate) {
	checkUnpatchedShared(params::UNPATCHED_STUTTER_RATE, "stutterRate");
}
TEST(ParamRoundTripUnpatchedShared, bass) {
	checkUnpatchedShared(params::UNPATCHED_BASS, "bass");
}
TEST(ParamRoundTripUnpatchedShared, treble) {
	checkUnpatchedShared(params::UNPATCHED_TREBLE, "treble");
}
TEST(ParamRoundTripUnpatchedShared, bassFreq) {
	checkUnpatchedShared(params::UNPATCHED_BASS_FREQ, "bassFreq");
}
TEST(ParamRoundTripUnpatchedShared, trebleFreq) {
	checkUnpatchedShared(params::UNPATCHED_TREBLE_FREQ, "trebleFreq");
}
TEST(ParamRoundTripUnpatchedShared, sampleRateReduction) {
	checkUnpatchedShared(params::UNPATCHED_SAMPLE_RATE_REDUCTION, "sampleRateReduction");
}
TEST(ParamRoundTripUnpatchedShared, bitcrushing) {
	checkUnpatchedShared(params::UNPATCHED_BITCRUSHING, "bitcrushAmount");
}
TEST(ParamRoundTripUnpatchedShared, modFXOffset) {
	checkUnpatchedShared(params::UNPATCHED_MOD_FX_OFFSET, "modFXOffset");
}
TEST(ParamRoundTripUnpatchedShared, modFXFeedback) {
	checkUnpatchedShared(params::UNPATCHED_MOD_FX_FEEDBACK, "modFXFeedback");
}
TEST(ParamRoundTripUnpatchedShared, sidechainShape) {
	checkUnpatchedShared(params::UNPATCHED_SIDECHAIN_SHAPE, "compressorShape");
}
TEST(ParamRoundTripUnpatchedShared, compressorThreshold) {
	checkUnpatchedShared(params::UNPATCHED_COMPRESSOR_THRESHOLD, "compressorThreshold");
}
TEST(ParamRoundTripUnpatchedShared, arpGate) {
	checkUnpatchedShared(params::UNPATCHED_ARP_GATE, "arpGate");
}
TEST(ParamRoundTripUnpatchedShared, arpRhythm) {
	checkUnpatchedShared(params::UNPATCHED_ARP_RHYTHM, "rhythm");
}
TEST(ParamRoundTripUnpatchedShared, arpSequenceLength) {
	checkUnpatchedShared(params::UNPATCHED_ARP_SEQUENCE_LENGTH, "sequenceLength");
}
TEST(ParamRoundTripUnpatchedShared, arpChordPolyphony) {
	checkUnpatchedShared(params::UNPATCHED_ARP_CHORD_POLYPHONY, "chordPolyphony");
}
TEST(ParamRoundTripUnpatchedShared, arpRatchetAmount) {
	checkUnpatchedShared(params::UNPATCHED_ARP_RATCHET_AMOUNT, "ratchetAmount");
}
TEST(ParamRoundTripUnpatchedShared, noteProbability) {
	checkUnpatchedShared(params::UNPATCHED_NOTE_PROBABILITY, "noteProbability");
}
TEST(ParamRoundTripUnpatchedShared, reverseProbability) {
	checkUnpatchedShared(params::UNPATCHED_REVERSE_PROBABILITY, "reverseProbability");
}
TEST(ParamRoundTripUnpatchedShared, bassProbability) {
	checkUnpatchedShared(params::UNPATCHED_ARP_BASS_PROBABILITY, "bassProbability");
}
TEST(ParamRoundTripUnpatchedShared, swapProbability) {
	checkUnpatchedShared(params::UNPATCHED_ARP_SWAP_PROBABILITY, "swapProbability");
}
TEST(ParamRoundTripUnpatchedShared, glideProbability) {
	checkUnpatchedShared(params::UNPATCHED_ARP_GLIDE_PROBABILITY, "glideProbability");
}
TEST(ParamRoundTripUnpatchedShared, chordProbability) {
	checkUnpatchedShared(params::UNPATCHED_ARP_CHORD_PROBABILITY, "chordProbability");
}
TEST(ParamRoundTripUnpatchedShared, ratchetProbability) {
	checkUnpatchedShared(params::UNPATCHED_ARP_RATCHET_PROBABILITY, "ratchetProbability");
}
TEST(ParamRoundTripUnpatchedShared, spreadGate) {
	checkUnpatchedShared(params::UNPATCHED_ARP_SPREAD_GATE, "spreadGate");
}
TEST(ParamRoundTripUnpatchedShared, spreadOctave) {
	checkUnpatchedShared(params::UNPATCHED_ARP_SPREAD_OCTAVE, "spreadOctave");
}
TEST(ParamRoundTripUnpatchedShared, spreadVelocity) {
	checkUnpatchedShared(params::UNPATCHED_SPREAD_VELOCITY, "spreadVelocity");
}

// ── Unpatched sound-only params ─────────────────────────────────────────────

TEST_GROUP(ParamRoundTripUnpatchedSound) {};

TEST(ParamRoundTripUnpatchedSound, portamento) {
	int32_t param = params::UNPATCHED_PORTAMENTO + params::UNPATCHED_START;
	checkRoundTrip(params::Kind::UNPATCHED_SOUND, param, "portamento");
}

// Pedal params added by feature/midi-cc66-sostenuto-pedal (not on main yet)
#if __has_include("model/time_signature.h")
TEST(ParamRoundTripUnpatchedSound, sustainPedal) {
	int32_t param = params::UNPATCHED_SUSTAIN_PEDAL + params::UNPATCHED_START;
	checkRoundTrip(params::Kind::UNPATCHED_SOUND, param, "sustainPedal");
}

TEST(ParamRoundTripUnpatchedSound, sostenutoPedal) {
	int32_t param = params::UNPATCHED_SOSTENUTO_PEDAL + params::UNPATCHED_START;
	checkRoundTrip(params::Kind::UNPATCHED_SOUND, param, "sostenutoPedal");
}

TEST(ParamRoundTripUnpatchedSound, softPedal) {
	int32_t param = params::UNPATCHED_SOFT_PEDAL + params::UNPATCHED_START;
	checkRoundTrip(params::Kind::UNPATCHED_SOUND, param, "softPedal");
}
#endif

// ── Unpatched global-only params ────────────────────────────────────────────

TEST_GROUP(ParamRoundTripUnpatchedGlobal) {};

static void checkUnpatchedGlobal(params::ParamType rawEnum, const char* expectedName) {
	int32_t param = rawEnum + params::UNPATCHED_START;
	checkRoundTrip(params::Kind::UNPATCHED_GLOBAL, param, expectedName, false);
}

TEST(ParamRoundTripUnpatchedGlobal, modFXRate) {
	checkUnpatchedGlobal(params::UNPATCHED_MOD_FX_RATE, "modFXRate");
}
TEST(ParamRoundTripUnpatchedGlobal, modFXDepth) {
	checkUnpatchedGlobal(params::UNPATCHED_MOD_FX_DEPTH, "modFXDepth");
}
TEST(ParamRoundTripUnpatchedGlobal, delayRate) {
	checkUnpatchedGlobal(params::UNPATCHED_DELAY_RATE, "delayRate");
}
TEST(ParamRoundTripUnpatchedGlobal, delayAmount) {
	checkUnpatchedGlobal(params::UNPATCHED_DELAY_AMOUNT, "delayFeedback");
}
TEST(ParamRoundTripUnpatchedGlobal, arpRate) {
	checkUnpatchedGlobal(params::UNPATCHED_ARP_RATE, "arpRate");
}
TEST(ParamRoundTripUnpatchedGlobal, pan) {
	checkUnpatchedGlobal(params::UNPATCHED_PAN, "pan");
}
TEST(ParamRoundTripUnpatchedGlobal, lpfFreq) {
	checkUnpatchedGlobal(params::UNPATCHED_LPF_FREQ, "lpfFrequency");
}
TEST(ParamRoundTripUnpatchedGlobal, lpfRes) {
	checkUnpatchedGlobal(params::UNPATCHED_LPF_RES, "lpfResonance");
}
TEST(ParamRoundTripUnpatchedGlobal, lpfMorph) {
	checkUnpatchedGlobal(params::UNPATCHED_LPF_MORPH, "lpfMorph");
}
TEST(ParamRoundTripUnpatchedGlobal, hpfFreq) {
	checkUnpatchedGlobal(params::UNPATCHED_HPF_FREQ, "hpfFrequency");
}
TEST(ParamRoundTripUnpatchedGlobal, hpfRes) {
	checkUnpatchedGlobal(params::UNPATCHED_HPF_RES, "hpfResonance");
}
TEST(ParamRoundTripUnpatchedGlobal, hpfMorph) {
	checkUnpatchedGlobal(params::UNPATCHED_HPF_MORPH, "hpfMorph");
}
TEST(ParamRoundTripUnpatchedGlobal, reverbSendAmount) {
	checkUnpatchedGlobal(params::UNPATCHED_REVERB_SEND_AMOUNT, "reverbAmount");
}
TEST(ParamRoundTripUnpatchedGlobal, volume) {
	checkUnpatchedGlobal(params::UNPATCHED_VOLUME, "volume");
}
TEST(ParamRoundTripUnpatchedGlobal, sidechainVolume) {
	checkUnpatchedGlobal(params::UNPATCHED_SIDECHAIN_VOLUME, "sidechainCompressorVolume");
}
TEST(ParamRoundTripUnpatchedGlobal, pitchAdjust) {
	checkUnpatchedGlobal(params::UNPATCHED_PITCH_ADJUST, "pitchAdjust");
}

// ── Edge cases ──────────────────────────────────────────────────────────────

TEST_GROUP(ParamRoundTripEdgeCases) {};

TEST(ParamRoundTripEdgeCases, unknownStringReturnsGlobalNone) {
	params::ParamType result = params::fileStringToParam(params::Kind::PATCHED, "nonExistentParam", true);
	CHECK_EQUAL(params::GLOBAL_NONE, result);
}

TEST(ParamRoundTripEdgeCases, rangeBackwardCompatibility) {
	params::ParamType result = params::fileStringToParam(params::Kind::PATCHED, "range", true);
	CHECK_EQUAL(params::PLACEHOLDER_RANGE, result);
}

TEST(ParamRoundTripEdgeCases, noneStringReturnsGlobalNone) {
	params::ParamType result = params::fileStringToParam(params::Kind::PATCHED, "none", true);
	CHECK_EQUAL(params::GLOBAL_NONE, result);
}

TEST(ParamRoundTripEdgeCases, globalNoneParamReturnsNone) {
	const char* name = params::paramNameForFile(params::Kind::PATCHED, params::GLOBAL_NONE);
	STRCMP_EQUAL("none", name);
}

TEST(ParamRoundTripEdgeCases, emptyStringReturnsGlobalNone) {
	params::ParamType result = params::fileStringToParam(params::Kind::PATCHED, "", true);
	CHECK_EQUAL(params::GLOBAL_NONE, result);
}

// ── paramNeedsLPF coverage ──────────────────────────────────────────────────

TEST_GROUP(ParamNeedsLPF) {};

TEST(ParamNeedsLPF, volumeNeedsLPFWhenNotAutomation) {
	CHECK_TRUE(params::paramNeedsLPF(params::LOCAL_VOLUME, false));
}

TEST(ParamNeedsLPF, volumeNoLPFFromAutomation) {
	CHECK_FALSE(params::paramNeedsLPF(params::LOCAL_VOLUME, true));
}

TEST(ParamNeedsLPF, panNeedsLPFWhenNotAutomation) {
	CHECK_TRUE(params::paramNeedsLPF(params::LOCAL_PAN, false));
}

TEST(ParamNeedsLPF, panNoLPFFromAutomation) {
	CHECK_FALSE(params::paramNeedsLPF(params::LOCAL_PAN, true));
}

TEST(ParamNeedsLPF, mod0VolumeAlwaysNeedsLPF) {
	CHECK_TRUE(params::paramNeedsLPF(params::LOCAL_MODULATOR_0_VOLUME, true));
	CHECK_TRUE(params::paramNeedsLPF(params::LOCAL_MODULATOR_0_VOLUME, false));
}

TEST(ParamNeedsLPF, mod1VolumeAlwaysNeedsLPF) {
	CHECK_TRUE(params::paramNeedsLPF(params::LOCAL_MODULATOR_1_VOLUME, true));
}

TEST(ParamNeedsLPF, delayFeedbackAlwaysNeedsLPF) {
	CHECK_TRUE(params::paramNeedsLPF(params::GLOBAL_DELAY_FEEDBACK, true));
	CHECK_TRUE(params::paramNeedsLPF(params::GLOBAL_DELAY_FEEDBACK, false));
}

TEST(ParamNeedsLPF, pitchAdjustNeverNeedsLPF) {
	CHECK_FALSE(params::paramNeedsLPF(params::LOCAL_PITCH_ADJUST, false));
	CHECK_FALSE(params::paramNeedsLPF(params::LOCAL_PITCH_ADJUST, true));
}

TEST(ParamNeedsLPF, lpfFreqNeedsLPFWhenNotAutomation) {
	CHECK_TRUE(params::paramNeedsLPF(params::LOCAL_LPF_FREQ, false));
	CHECK_FALSE(params::paramNeedsLPF(params::LOCAL_LPF_FREQ, true));
}

TEST(ParamNeedsLPF, hpfFreqNeedsLPFWhenNotAutomation) {
	CHECK_TRUE(params::paramNeedsLPF(params::LOCAL_HPF_FREQ, false));
	CHECK_FALSE(params::paramNeedsLPF(params::LOCAL_HPF_FREQ, true));
}

TEST(ParamNeedsLPF, oscAWaveIndexNeedsLPFWhenNotAutomation) {
	CHECK_TRUE(params::paramNeedsLPF(params::LOCAL_OSC_A_WAVE_INDEX, false));
	CHECK_FALSE(params::paramNeedsLPF(params::LOCAL_OSC_A_WAVE_INDEX, true));
}

TEST(ParamNeedsLPF, modFXDepthAlwaysNeedsLPF) {
	CHECK_TRUE(params::paramNeedsLPF(params::GLOBAL_MOD_FX_DEPTH, true));
	CHECK_TRUE(params::paramNeedsLPF(params::GLOBAL_MOD_FX_DEPTH, false));
}

TEST(ParamNeedsLPF, carrier0FeedbackAlwaysNeedsLPF) {
	CHECK_TRUE(params::paramNeedsLPF(params::LOCAL_CARRIER_0_FEEDBACK, true));
}

TEST(ParamNeedsLPF, envAttackNeverNeedsLPF) {
	CHECK_FALSE(params::paramNeedsLPF(params::LOCAL_ENV_0_ATTACK, false));
}

// ── isParamBipolar extended coverage ────────────────────────────────────────

TEST_GROUP(ParamBipolarExtended) {};

TEST(ParamBipolarExtended, patchCableKindIsBipolar) {
	CHECK_TRUE(params::isParamBipolar(params::Kind::PATCH_CABLE, 0));
}

TEST(ParamBipolarExtended, oscBPitchIsBipolar) {
	CHECK_TRUE(params::isParamBipolar(params::Kind::PATCHED, params::LOCAL_OSC_B_PITCH_ADJUST));
}

TEST(ParamBipolarExtended, mod0PitchIsBipolar) {
	CHECK_TRUE(params::isParamBipolar(params::Kind::PATCHED, params::LOCAL_MODULATOR_0_PITCH_ADJUST));
}

TEST(ParamBipolarExtended, mod1PitchIsBipolar) {
	CHECK_TRUE(params::isParamBipolar(params::Kind::PATCHED, params::LOCAL_MODULATOR_1_PITCH_ADJUST));
}

TEST(ParamBipolarExtended, unpatchedPanIsBipolar) {
	CHECK_TRUE(params::isParamBipolar(params::Kind::UNPATCHED_GLOBAL, params::UNPATCHED_PAN));
}

TEST(ParamBipolarExtended, expressionPitchBendIsBipolar) {
	CHECK_TRUE(params::isParamBipolar(params::Kind::EXPRESSION, X_PITCH_BEND));
}

TEST(ParamBipolarExtended, unpatchedPitchAdjustIsBipolar) {
	CHECK_TRUE(params::isParamBipolar(params::Kind::UNPATCHED_GLOBAL, params::UNPATCHED_PITCH_ADJUST));
}

TEST(ParamBipolarExtended, lpfResonanceIsNotBipolar) {
	CHECK_FALSE(params::isParamBipolar(params::Kind::PATCHED, params::LOCAL_LPF_RESONANCE));
}

TEST(ParamBipolarExtended, delayRateIsNotBipolar) {
	CHECK_FALSE(params::isParamBipolar(params::Kind::PATCHED, params::GLOBAL_DELAY_RATE));
}

// ── isParamPitch extended coverage ──────────────────────────────────────────

TEST_GROUP(ParamPitchExtended) {};

TEST(ParamPitchExtended, unpatchedPitchAdjustIsPitch) {
	CHECK_TRUE(params::isParamPitch(params::Kind::UNPATCHED_GLOBAL, params::UNPATCHED_PITCH_ADJUST));
}

TEST(ParamPitchExtended, unpatchedSoundIsNotPitch) {
	CHECK_FALSE(params::isParamPitch(params::Kind::UNPATCHED_SOUND, params::UNPATCHED_PORTAMENTO));
}

TEST(ParamPitchExtended, expressionIsNotPitch) {
	CHECK_FALSE(params::isParamPitch(params::Kind::EXPRESSION, X_PITCH_BEND));
}

// ── isParamStutter extended coverage ────────────────────────────────────────

TEST_GROUP(ParamStutterExtended) {};

TEST(ParamStutterExtended, unpatchedGlobalStutterIsStutter) {
	CHECK_TRUE(params::isParamStutter(params::Kind::UNPATCHED_GLOBAL, params::UNPATCHED_STUTTER_RATE));
}

TEST(ParamStutterExtended, unpatchedSoundStutterIsStutter) {
	CHECK_TRUE(params::isParamStutter(params::Kind::UNPATCHED_SOUND, params::UNPATCHED_STUTTER_RATE));
}

TEST(ParamStutterExtended, patchedIsNotStutter) {
	CHECK_FALSE(params::isParamStutter(params::Kind::PATCHED, params::UNPATCHED_STUTTER_RATE));
}

TEST(ParamStutterExtended, wrongParamIsNotStutter) {
	CHECK_FALSE(params::isParamStutter(params::Kind::UNPATCHED_GLOBAL, params::UNPATCHED_BASS));
}
