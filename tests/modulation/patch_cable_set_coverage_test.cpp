// Coverage-focused tests for modulation/patch/patch_cable_set.cpp
//
// Targets methods and branches NOT exercised by existing tests:
//   - getModifiedPatchCableAmount for all pitch params
//   - getDestinationForParam with local/global ranges
//   - isAnySourcePatchedToParamVolumeInspecific additional branches
//   - isSourcePatchedToDestinationDescriptorVolumeInspecific edge cases
//   - paramValueToKnobPos/knobPosToParamValue boundary values
//   - writePatchCablesToFile with depth-controlling cables
//   - beenCloned deeper paths
//   - grabVelocityToLevelFromMIDIInput (LearnedMIDI path)
//   - dissectParamId exhaustive coverage
//   - isSourcePatchedToSomething all source bits
//   - getParam creation and lookup edge cases

#include "CppUTest/TestHarness.h"
#include "definitions_cxx.hpp"
#include "io/midi/learned_midi.h"
#include "io/midi/midi_device.h"
#include "modulation/params/param_manager.h"
#include "modulation/params/param_set.h"
#include "modulation/patch/patch_cable_set.h"
#include "modulation/patch/patcher.h"
#include "model/model_stack.h"
#include "string_serializer.h"
#include <cstring>
#include <string>

namespace params = deluge::modulation::params;

// ── Helper ──────────────────────────────────────────────────────────────
struct CovHelper {
	ParamManager pm;

	CovHelper() { pm.setupWithPatching(); }

	PatchCableSet* cs() { return static_cast<PatchCableSet*>(pm.summaries[2].paramCollection); }
};

static void addCableCov(PatchCableSet* cs, PatchSource src, int32_t destParam, int32_t amount = 536870912,
                        Polarity pol = Polarity::BIPOLAR) {
	uint8_t idx = cs->numPatchCables;
	cs->patchCables[idx].from = src;
	cs->patchCables[idx].destinationParamDescriptor.setToHaveParamOnly(destParam);
	cs->patchCables[idx].param.setCurrentValueBasicForSetup(amount);
	cs->patchCables[idx].polarity = pol;
	cs->numPatchCables = idx + 1;
}

// ═══════════════════════════════════════════════════════════════════════
TEST_GROUP(PatchCableSetCoverage){};

// ═══════════════════════════════════════════════════════════════════════
// getModifiedPatchCableAmount — all pitch-related params
// ═══════════════════════════════════════════════════════════════════════

TEST(PatchCableSetCoverage, modAmountOscBPitch) {
	CovHelper h;
	PatchCableSet* cs = h.cs();
	addCableCov(cs, PatchSource::LFO_GLOBAL_1, params::LOCAL_OSC_B_PITCH_ADJUST, 536870912);

	int32_t linear = cs->getModifiedPatchCableAmount(0, params::LOCAL_VOLUME);
	int32_t squared = cs->getModifiedPatchCableAmount(0, params::LOCAL_OSC_B_PITCH_ADJUST);
	CHECK(squared != linear);
}

TEST(PatchCableSetCoverage, modAmountMod0Pitch) {
	CovHelper h;
	PatchCableSet* cs = h.cs();
	addCableCov(cs, PatchSource::LFO_GLOBAL_1, params::LOCAL_MODULATOR_0_PITCH_ADJUST, 536870912);

	int32_t linear = cs->getModifiedPatchCableAmount(0, params::LOCAL_VOLUME);
	int32_t squared = cs->getModifiedPatchCableAmount(0, params::LOCAL_MODULATOR_0_PITCH_ADJUST);
	CHECK(squared != linear);
}

TEST(PatchCableSetCoverage, modAmountMod1Pitch) {
	CovHelper h;
	PatchCableSet* cs = h.cs();
	addCableCov(cs, PatchSource::LFO_GLOBAL_1, params::LOCAL_MODULATOR_1_PITCH_ADJUST, 536870912);

	int32_t linear = cs->getModifiedPatchCableAmount(0, params::LOCAL_VOLUME);
	int32_t squared = cs->getModifiedPatchCableAmount(0, params::LOCAL_MODULATOR_1_PITCH_ADJUST);
	CHECK(squared != linear);
}

TEST(PatchCableSetCoverage, modAmountPositivePitchSquared) {
	CovHelper h;
	PatchCableSet* cs = h.cs();
	addCableCov(cs, PatchSource::LFO_GLOBAL_1, params::LOCAL_PITCH_ADJUST, 1073741824);

	int32_t result = cs->getModifiedPatchCableAmount(0, params::LOCAL_PITCH_ADJUST);
	CHECK(result > 0);
}

TEST(PatchCableSetCoverage, modAmountNegativeDelayRate) {
	CovHelper h;
	PatchCableSet* cs = h.cs();
	addCableCov(cs, PatchSource::LFO_GLOBAL_1, params::GLOBAL_DELAY_RATE, -536870912);

	int32_t result = cs->getModifiedPatchCableAmount(0, params::GLOBAL_DELAY_RATE);
	CHECK(result < 0);
}

TEST(PatchCableSetCoverage, modAmountVelocityToMasterPitch) {
	CovHelper h;
	PatchCableSet* cs = h.cs();
	addCableCov(cs, PatchSource::VELOCITY, params::LOCAL_PITCH_ADJUST, 1073741824);

	int32_t velocityResult = cs->getModifiedPatchCableAmount(0, params::LOCAL_PITCH_ADJUST);

	cs->patchCables[0].from = PatchSource::LFO_GLOBAL_1;
	int32_t lfoResult = cs->getModifiedPatchCableAmount(0, params::LOCAL_PITCH_ADJUST);

	CHECK(velocityResult != lfoResult);
}

TEST(PatchCableSetCoverage, modAmountNonVelocityToOscAPitch) {
	CovHelper h;
	PatchCableSet* cs = h.cs();
	addCableCov(cs, PatchSource::VELOCITY, params::LOCAL_OSC_A_PITCH_ADJUST, 536870912);

	int32_t oscAPitchResult = cs->getModifiedPatchCableAmount(0, params::LOCAL_OSC_A_PITCH_ADJUST);
	int32_t masterPitchResult = cs->getModifiedPatchCableAmount(0, params::LOCAL_PITCH_ADJUST);

	CHECK(oscAPitchResult != masterPitchResult);
}

TEST(PatchCableSetCoverage, modAmountSmallPositivePitch) {
	CovHelper h;
	PatchCableSet* cs = h.cs();
	addCableCov(cs, PatchSource::LFO_GLOBAL_1, params::LOCAL_PITCH_ADJUST, 1000);

	int32_t result = cs->getModifiedPatchCableAmount(0, params::LOCAL_PITCH_ADJUST);
	CHECK(result >= 0);
	CHECK(result < 1000);
}

TEST(PatchCableSetCoverage, modAmountSmallNegativePitch) {
	CovHelper h;
	PatchCableSet* cs = h.cs();
	addCableCov(cs, PatchSource::LFO_GLOBAL_1, params::LOCAL_PITCH_ADJUST, -1000);

	int32_t result = cs->getModifiedPatchCableAmount(0, params::LOCAL_PITCH_ADJUST);
	CHECK(result <= 0);
}

TEST(PatchCableSetCoverage, modAmountNonPitchPassthrough) {
	CovHelper h;
	PatchCableSet* cs = h.cs();

	int32_t amount = 536870912;
	addCableCov(cs, PatchSource::LFO_GLOBAL_1, params::LOCAL_LPF_FREQ, amount);

	CHECK_EQUAL(amount, cs->getModifiedPatchCableAmount(0, params::LOCAL_LPF_FREQ));
	CHECK_EQUAL(amount, cs->getModifiedPatchCableAmount(0, params::LOCAL_HPF_FREQ));
	CHECK_EQUAL(amount, cs->getModifiedPatchCableAmount(0, params::LOCAL_PAN));
	CHECK_EQUAL(amount, cs->getModifiedPatchCableAmount(0, params::LOCAL_OSC_A_VOLUME));
	CHECK_EQUAL(amount, cs->getModifiedPatchCableAmount(0, params::LOCAL_OSC_B_VOLUME));
	CHECK_EQUAL(amount, cs->getModifiedPatchCableAmount(0, params::LOCAL_LPF_RESONANCE));
	CHECK_EQUAL(amount, cs->getModifiedPatchCableAmount(0, params::LOCAL_HPF_RESONANCE));
	CHECK_EQUAL(amount, cs->getModifiedPatchCableAmount(0, params::GLOBAL_REVERB_AMOUNT));
}

// ═══════════════════════════════════════════════════════════════════════
// getDestinationForParam — local vs global routing
// ═══════════════════════════════════════════════════════════════════════

TEST(PatchCableSetCoverage, getDestForParamLocalVsGlobal) {
	CovHelper h;
	PatchCableSet* cs = h.cs();

	CHECK(cs->getDestinationForParam(params::LOCAL_VOLUME) == nullptr);
	CHECK(cs->getDestinationForParam(params::GLOBAL_VOLUME_POST_FX) == nullptr);
	CHECK(cs->getDestinationForParam(params::LOCAL_LPF_FREQ) == nullptr);
	CHECK(cs->getDestinationForParam(params::GLOBAL_DELAY_RATE) == nullptr);
	CHECK(cs->getDestinationForParam(params::LOCAL_HPF_FREQ) == nullptr);
	CHECK(cs->getDestinationForParam(params::GLOBAL_REVERB_AMOUNT) == nullptr);
}

TEST(PatchCableSetCoverage, doesParamHaveSomethingVariousParams) {
	CovHelper h;
	PatchCableSet* cs = h.cs();

	CHECK_FALSE(cs->doesParamHaveSomethingPatchedToIt(params::LOCAL_VOLUME));
	CHECK_FALSE(cs->doesParamHaveSomethingPatchedToIt(params::GLOBAL_VOLUME_POST_FX));
	CHECK_FALSE(cs->doesParamHaveSomethingPatchedToIt(params::LOCAL_PITCH_ADJUST));
	CHECK_FALSE(cs->doesParamHaveSomethingPatchedToIt(params::GLOBAL_DELAY_RATE));
	CHECK_FALSE(cs->doesParamHaveSomethingPatchedToIt(params::LOCAL_PAN));
}

// ═══════════════════════════════════════════════════════════════════════
// isAnySourcePatchedToParamVolumeInspecific — additional branches
// ═══════════════════════════════════════════════════════════════════════

TEST(PatchCableSetCoverage, anySourceVolInspecPostReverbSendDirect) {
	CovHelper h;
	PatchCableSet* cs = h.cs();

	addCableCov(cs, PatchSource::LFO_GLOBAL_1, params::GLOBAL_VOLUME_POST_REVERB_SEND);

	ParamDescriptor desc;
	desc.setToHaveParamOnly(params::GLOBAL_VOLUME_POST_FX);
	CHECK_TRUE(cs->isAnySourcePatchedToParamVolumeInspecific(desc));
}

TEST(PatchCableSetCoverage, anySourceVolInspecPostFxDirect) {
	CovHelper h;
	PatchCableSet* cs = h.cs();

	addCableCov(cs, PatchSource::SIDECHAIN, params::GLOBAL_VOLUME_POST_FX);

	ParamDescriptor desc;
	desc.setToHaveParamOnly(params::GLOBAL_VOLUME_POST_FX);
	CHECK_TRUE(cs->isAnySourcePatchedToParamVolumeInspecific(desc));
}

TEST(PatchCableSetCoverage, anySourceVolInspecNonVolumeParam) {
	CovHelper h;
	PatchCableSet* cs = h.cs();

	addCableCov(cs, PatchSource::LFO_GLOBAL_1, params::LOCAL_LPF_FREQ);

	ParamDescriptor desc;
	desc.setToHaveParamOnly(params::LOCAL_LPF_FREQ);
	CHECK_TRUE(cs->isAnySourcePatchedToParamVolumeInspecific(desc));

	ParamDescriptor descOther;
	descOther.setToHaveParamOnly(params::LOCAL_HPF_FREQ);
	CHECK_FALSE(cs->isAnySourcePatchedToParamVolumeInspecific(descOther));
}

TEST(PatchCableSetCoverage, anySourceVolInspecNoMatch) {
	CovHelper h;
	PatchCableSet* cs = h.cs();

	addCableCov(cs, PatchSource::LFO_GLOBAL_1, params::LOCAL_LPF_FREQ);
	addCableCov(cs, PatchSource::ENVELOPE_0, params::LOCAL_HPF_FREQ);

	ParamDescriptor desc;
	desc.setToHaveParamOnly(params::GLOBAL_VOLUME_POST_FX);
	CHECK_FALSE(cs->isAnySourcePatchedToParamVolumeInspecific(desc));
}

// ═══════════════════════════════════════════════════════════════════════
// isSourcePatchedToDestinationDescriptorVolumeInspecific — edge cases
// ═══════════════════════════════════════════════════════════════════════

TEST(PatchCableSetCoverage, srcVolInspecPostFxDirectMatch) {
	CovHelper h;
	PatchCableSet* cs = h.cs();

	addCableCov(cs, PatchSource::SIDECHAIN, params::GLOBAL_VOLUME_POST_FX);

	ParamDescriptor desc;
	desc.setToHaveParamOnly(params::GLOBAL_VOLUME_POST_FX);
	CHECK_TRUE(cs->isSourcePatchedToDestinationDescriptorVolumeInspecific(PatchSource::SIDECHAIN, desc));
}

TEST(PatchCableSetCoverage, srcVolInspecPostReverbSendMatch) {
	CovHelper h;
	PatchCableSet* cs = h.cs();

	addCableCov(cs, PatchSource::SIDECHAIN, params::GLOBAL_VOLUME_POST_REVERB_SEND);

	ParamDescriptor desc;
	desc.setToHaveParamOnly(params::GLOBAL_VOLUME_POST_FX);
	CHECK_TRUE(cs->isSourcePatchedToDestinationDescriptorVolumeInspecific(PatchSource::SIDECHAIN, desc));
}

TEST(PatchCableSetCoverage, srcVolInspecLocalVolumeMatch) {
	CovHelper h;
	PatchCableSet* cs = h.cs();

	addCableCov(cs, PatchSource::SIDECHAIN, params::LOCAL_VOLUME);

	ParamDescriptor desc;
	desc.setToHaveParamOnly(params::GLOBAL_VOLUME_POST_FX);
	CHECK_TRUE(cs->isSourcePatchedToDestinationDescriptorVolumeInspecific(PatchSource::SIDECHAIN, desc));
}

TEST(PatchCableSetCoverage, srcVolInspecNoMatchAllThree) {
	CovHelper h;
	PatchCableSet* cs = h.cs();

	addCableCov(cs, PatchSource::SIDECHAIN, params::LOCAL_LPF_FREQ);

	ParamDescriptor desc;
	desc.setToHaveParamOnly(params::GLOBAL_VOLUME_POST_FX);
	CHECK_FALSE(cs->isSourcePatchedToDestinationDescriptorVolumeInspecific(PatchSource::SIDECHAIN, desc));
}

TEST(PatchCableSetCoverage, srcVolInspecWrongSource) {
	CovHelper h;
	PatchCableSet* cs = h.cs();

	addCableCov(cs, PatchSource::LFO_GLOBAL_1, params::GLOBAL_VOLUME_POST_FX);

	ParamDescriptor desc;
	desc.setToHaveParamOnly(params::GLOBAL_VOLUME_POST_FX);
	CHECK_FALSE(cs->isSourcePatchedToDestinationDescriptorVolumeInspecific(PatchSource::SIDECHAIN, desc));
	CHECK_TRUE(cs->isSourcePatchedToDestinationDescriptorVolumeInspecific(PatchSource::LFO_GLOBAL_1, desc));
}

// ═══════════════════════════════════════════════════════════════════════
// beenCloned — deeper coverage
// ═══════════════════════════════════════════════════════════════════════

TEST(PatchCableSetCoverage, beenClonedWithZeroUsable) {
	CovHelper h;
	PatchCableSet* cs = h.cs();

	addCableCov(cs, PatchSource::LFO_GLOBAL_1, params::LOCAL_VOLUME, 111);
	cs->numUsablePatchCables = 0;

	cs->beenCloned(false, 0);

	CHECK_EQUAL(1, cs->numPatchCables);
}

TEST(PatchCableSetCoverage, beenClonedCopyAutomationFalse) {
	CovHelper h;
	PatchCableSet* cs = h.cs();

	addCableCov(cs, PatchSource::VELOCITY, params::LOCAL_VOLUME, 42);
	cs->numUsablePatchCables = 1;

	cs->beenCloned(false, 0);

	CHECK_EQUAL(1, cs->numPatchCables);
}

TEST(PatchCableSetCoverage, beenClonedMaxCables) {
	CovHelper h;
	PatchCableSet* cs = h.cs();

	for (int i = 0; i < kMaxNumPatchCables; i++) {
		addCableCov(cs, static_cast<PatchSource>(i % kNumPatchSources),
		            params::LOCAL_VOLUME + (i / kNumPatchSources), (i + 1) * 100);
	}
	cs->numUsablePatchCables = kMaxNumPatchCables;

	cs->beenCloned(true, 0);

	CHECK_EQUAL(kMaxNumPatchCables, (int)cs->numPatchCables);
}

TEST(PatchCableSetCoverage, beenClonedTwoCablesPreservesValues) {
	CovHelper h;
	PatchCableSet* cs = h.cs();

	addCableCov(cs, PatchSource::LFO_GLOBAL_1, params::LOCAL_VOLUME, 111);
	addCableCov(cs, PatchSource::ENVELOPE_0, params::LOCAL_LPF_FREQ, 222);
	cs->numUsablePatchCables = 2;

	cs->beenCloned(true, 0);

	CHECK_EQUAL(2, cs->numPatchCables);
	CHECK_EQUAL(111, cs->patchCables[0].param.getCurrentValue());
	CHECK_EQUAL(222, cs->patchCables[1].param.getCurrentValue());
}

// ═══════════════════════════════════════════════════════════════════════
// grabVelocityToLevelFromMIDIInput — LearnedMIDI paths
// ═══════════════════════════════════════════════════════════════════════

TEST(PatchCableSetCoverage, grabVelFromMIDIInputEmpty) {
	CovHelper h;
	PatchCableSet* cs = h.cs();

	LearnedMIDI learned;
	cs->grabVelocityToLevelFromMIDIInput(&learned);

	CHECK_EQUAL(0, cs->numPatchCables);
}

TEST(PatchCableSetCoverage, grabVelFromMIDIInputWithCable) {
	CovHelper h;
	PatchCableSet* cs = h.cs();

	MIDICable cable;
	cable.defaultVelocityToLevel = 777777;

	LearnedMIDI learned;
	learned.channelOrZone = 0;
	learned.cable = &cable;

	cs->grabVelocityToLevelFromMIDIInput(&learned);

	CHECK_EQUAL(1, cs->numPatchCables);
	PatchCable* pc = cs->getPatchCableFromVelocityToLevel();
	CHECK(pc != nullptr);
	CHECK_EQUAL(777777, pc->param.getCurrentValue());
}

TEST(PatchCableSetCoverage, grabVelFromMIDIInputNoDefaultVel) {
	CovHelper h;
	PatchCableSet* cs = h.cs();

	MIDICable cable;
	cable.defaultVelocityToLevel = 0;

	LearnedMIDI learned;
	learned.channelOrZone = 0;
	learned.cable = &cable;

	cs->grabVelocityToLevelFromMIDIInput(&learned);

	CHECK_EQUAL(0, cs->numPatchCables);
}

// ═══════════════════════════════════════════════════════════════════════
// grabVelocityToLevelFromMIDICable — cable full scenario
// ═══════════════════════════════════════════════════════════════════════

TEST(PatchCableSetCoverage, grabVelFromCableWhenFull) {
	CovHelper h;
	PatchCableSet* cs = h.cs();

	for (int i = 0; i < kMaxNumPatchCables; i++) {
		cs->patchCables[i].from = PatchSource::LFO_GLOBAL_1;
		cs->patchCables[i].destinationParamDescriptor.setToHaveParamOnly(params::LOCAL_LPF_FREQ + i);
		cs->patchCables[i].param.setCurrentValueBasicForSetup(100);
	}
	cs->numPatchCables = kMaxNumPatchCables;

	MIDICable cable;
	cable.defaultVelocityToLevel = 555;

	cs->grabVelocityToLevelFromMIDICable(cable);

	CHECK_EQUAL(kMaxNumPatchCables, (int)cs->numPatchCables);
}

// ═══════════════════════════════════════════════════════════════════════
// paramValueToKnobPos / knobPosToParamValue — boundary values
// ═══════════════════════════════════════════════════════════════════════

TEST(PatchCableSetCoverage, knobPosNegativeValue) {
	CovHelper h;
	PatchCableSet* cs = h.cs();
	CHECK_EQUAL(-128, cs->paramValueToKnobPos(-536870912, nullptr));
}

TEST(PatchCableSetCoverage, knobPosSmallPositive) {
	CovHelper h;
	PatchCableSet* cs = h.cs();
	CHECK_EQUAL(-63, cs->paramValueToKnobPos(8388608, nullptr));
}

TEST(PatchCableSetCoverage, knobPosToParamVeryNeg) {
	CovHelper h;
	PatchCableSet* cs = h.cs();
	CHECK_EQUAL(-536870912, cs->knobPosToParamValue(-128, nullptr));
}

TEST(PatchCableSetCoverage, knobPosSaturatesAtMax) {
	CovHelper h;
	PatchCableSet* cs = h.cs();
	CHECK_EQUAL(1073741824, cs->knobPosToParamValue(100, nullptr));
	CHECK_EQUAL(1073741824, cs->knobPosToParamValue(127, nullptr));
}

TEST(PatchCableSetCoverage, knobPosJustBelow64) {
	CovHelper h;
	PatchCableSet* cs = h.cs();
	CHECK_EQUAL(1065353216, cs->knobPosToParamValue(63, nullptr));
}

// ═══════════════════════════════════════════════════════════════════════
// writePatchCablesToFile — branch coverage
// ═══════════════════════════════════════════════════════════════════════

TEST(PatchCableSetCoverage, writeSkipsDepthControllingCables) {
	CovHelper h;
	PatchCableSet* cs = h.cs();
	StringSerializer ser;

	cs->patchCables[0].from = PatchSource::LFO_GLOBAL_1;
	cs->patchCables[0].destinationParamDescriptor.setToHaveParamOnly(params::LOCAL_VOLUME);
	cs->patchCables[0].polarity = Polarity::BIPOLAR;
	cs->patchCables[0].param.setCurrentValueBasicForSetup(0x20000000);

	cs->patchCables[1].from = PatchSource::VELOCITY;
	cs->patchCables[1].destinationParamDescriptor.setToHaveParamAndSource(params::LOCAL_VOLUME,
	                                                                      PatchSource::LFO_GLOBAL_1);
	cs->patchCables[1].polarity = Polarity::BIPOLAR;
	cs->patchCables[1].param.setCurrentValueBasicForSetup(0x10000000);

	cs->numPatchCables = 2;

	cs->writePatchCablesToFile(ser, false);

	CHECK(ser.output.find("depthControlledBy") != std::string::npos);
	CHECK(ser.output.find("velocity") != std::string::npos);
	CHECK(ser.output.find("lfo1") != std::string::npos);
}

TEST(PatchCableSetCoverage, writeNoCableEmpty) {
	CovHelper h;
	PatchCableSet* cs = h.cs();
	StringSerializer ser;

	cs->numPatchCables = 0;
	cs->writePatchCablesToFile(ser, false);

	CHECK(ser.output.empty());
}

TEST(PatchCableSetCoverage, writeMultipleNormalCables) {
	CovHelper h;
	PatchCableSet* cs = h.cs();
	StringSerializer ser;

	addCableCov(cs, PatchSource::LFO_GLOBAL_1, params::LOCAL_VOLUME, 0x20000000);
	addCableCov(cs, PatchSource::ENVELOPE_0, params::LOCAL_LPF_FREQ, 0x30000000);
	addCableCov(cs, PatchSource::VELOCITY, params::LOCAL_HPF_FREQ, 0x10000000, Polarity::UNIPOLAR);

	cs->writePatchCablesToFile(ser, false);

	CHECK(ser.output.find("lfo1") != std::string::npos);
	CHECK(ser.output.find("envelope1") != std::string::npos);
	CHECK(ser.output.find("velocity") != std::string::npos);
	CHECK(ser.output.find("volume") != std::string::npos);
	CHECK(ser.output.find("lpfFrequency") != std::string::npos);
	CHECK(ser.output.find("hpfFrequency") != std::string::npos);
}

TEST(PatchCableSetCoverage, writeGlobalDest) {
	CovHelper h;
	PatchCableSet* cs = h.cs();
	StringSerializer ser;

	addCableCov(cs, PatchSource::LFO_GLOBAL_1, params::GLOBAL_DELAY_RATE, 0x20000000);

	cs->writePatchCablesToFile(ser, false);

	CHECK(ser.output.find("delayRate") != std::string::npos);
}

TEST(PatchCableSetCoverage, writeUnipolar) {
	CovHelper h;
	PatchCableSet* cs = h.cs();
	StringSerializer ser;

	addCableCov(cs, PatchSource::AFTERTOUCH, params::LOCAL_LPF_FREQ, 0x20000000, Polarity::UNIPOLAR);

	cs->writePatchCablesToFile(ser, false);

	CHECK(ser.output.find("unipolar") != std::string::npos);
}

TEST(PatchCableSetCoverage, writeMultipleDepthCables) {
	CovHelper h;
	PatchCableSet* cs = h.cs();
	StringSerializer ser;

	cs->patchCables[0].from = PatchSource::LFO_GLOBAL_1;
	cs->patchCables[0].destinationParamDescriptor.setToHaveParamOnly(params::LOCAL_VOLUME);
	cs->patchCables[0].polarity = Polarity::BIPOLAR;
	cs->patchCables[0].param.setCurrentValueBasicForSetup(0x20000000);

	cs->patchCables[1].from = PatchSource::VELOCITY;
	cs->patchCables[1].destinationParamDescriptor.setToHaveParamAndSource(params::LOCAL_VOLUME,
	                                                                      PatchSource::LFO_GLOBAL_1);
	cs->patchCables[1].polarity = Polarity::BIPOLAR;
	cs->patchCables[1].param.setCurrentValueBasicForSetup(0x10000000);

	cs->patchCables[2].from = PatchSource::AFTERTOUCH;
	cs->patchCables[2].destinationParamDescriptor.setToHaveParamAndSource(params::LOCAL_VOLUME,
	                                                                      PatchSource::LFO_GLOBAL_1);
	cs->patchCables[2].polarity = Polarity::UNIPOLAR;
	cs->patchCables[2].param.setCurrentValueBasicForSetup(0x08000000);

	cs->numPatchCables = 3;

	cs->writePatchCablesToFile(ser, false);

	CHECK(ser.output.find("depthControlledBy") != std::string::npos);
	CHECK(ser.output.find("velocity") != std::string::npos);
	CHECK(ser.output.find("aftertouch") != std::string::npos);
}

TEST(PatchCableSetCoverage, writeWithAutomationFlag) {
	CovHelper h;
	PatchCableSet* cs = h.cs();
	StringSerializer ser;

	addCableCov(cs, PatchSource::LFO_GLOBAL_1, params::LOCAL_VOLUME, 0x20000000);

	cs->writePatchCablesToFile(ser, true);
	CHECK(!ser.output.empty());
	CHECK(ser.output.find("lfo1") != std::string::npos);
}

// ═══════════════════════════════════════════════════════════════════════
// dissectParamId — exhaustive coverage
// ═══════════════════════════════════════════════════════════════════════

TEST(PatchCableSetCoverage, dissectParamIdAllSources) {
	PatchSource allSources[] = {
	    PatchSource::LFO_GLOBAL_1, PatchSource::LFO_GLOBAL_2, PatchSource::LFO_LOCAL_1, PatchSource::LFO_LOCAL_2,
	    PatchSource::ENVELOPE_0,   PatchSource::ENVELOPE_1,   PatchSource::VELOCITY,    PatchSource::AFTERTOUCH,
	    PatchSource::NOTE,         PatchSource::SIDECHAIN,    PatchSource::RANDOM,      PatchSource::X,
	    PatchSource::Y,
	};
	int32_t allDests[] = {
	    params::LOCAL_VOLUME,       params::LOCAL_LPF_FREQ,    params::LOCAL_HPF_FREQ,
	    params::LOCAL_PITCH_ADJUST, params::LOCAL_PAN,         params::LOCAL_OSC_A_VOLUME,
	    params::LOCAL_OSC_B_VOLUME, params::GLOBAL_DELAY_RATE, params::GLOBAL_VOLUME_POST_FX,
	    params::GLOBAL_REVERB_AMOUNT,
	};

	for (auto src : allSources) {
		for (auto dest : allDests) {
			ParamDescriptor desc;
			desc.setToHaveParamOnly(dest);
			int32_t id = PatchCableSet::getParamId(desc, src);

			ParamDescriptor outDesc;
			PatchSource outSource;
			PatchCableSet::dissectParamId(id, &outDesc, &outSource);

			CHECK(outSource == src);
			CHECK(outDesc == desc);
		}
	}
}

// ═══════════════════════════════════════════════════════════════════════
// isSourcePatchedToSomething — all source bits
// ═══════════════════════════════════════════════════════════════════════

TEST(PatchCableSetCoverage, isSourcePatchedAllBits) {
	CovHelper h;
	PatchCableSet* cs = h.cs();

	for (int32_t s = 0; s < kNumPatchSources; s++) {
		auto source = static_cast<PatchSource>(s);
		if (source == PatchSource::NONE || source == PatchSource::NOT_AVAILABLE) {
			continue;
		}

		cs->sourcesPatchedToAnything[GLOBALITY_LOCAL] = (1u << s);
		cs->sourcesPatchedToAnything[GLOBALITY_GLOBAL] = 0;
		CHECK_TRUE(cs->isSourcePatchedToSomething(source));

		cs->sourcesPatchedToAnything[GLOBALITY_LOCAL] = 0;
		cs->sourcesPatchedToAnything[GLOBALITY_GLOBAL] = (1u << s);
		CHECK_TRUE(cs->isSourcePatchedToSomething(source));
	}
}

// ═══════════════════════════════════════════════════════════════════════
// doesDestinationDescriptorHaveAnyCables — at max
// ═══════════════════════════════════════════════════════════════════════

TEST(PatchCableSetCoverage, doesDestDescMaxCables) {
	CovHelper h;
	PatchCableSet* cs = h.cs();

	for (int i = 0; i < kMaxNumPatchCables; i++) {
		cs->patchCables[i].from = static_cast<PatchSource>(i % kNumPatchSources);
		cs->patchCables[i].destinationParamDescriptor.setToHaveParamOnly(params::LOCAL_VOLUME);
		cs->patchCables[i].param.setCurrentValueBasicForSetup(100);
	}
	cs->numPatchCables = kMaxNumPatchCables;

	ParamDescriptor desc;
	desc.setToHaveParamOnly(params::LOCAL_VOLUME);
	CHECK_TRUE(cs->doesDestinationDescriptorHaveAnyCables(desc));
}

// ═══════════════════════════════════════════════════════════════════════
// Construction
// ═══════════════════════════════════════════════════════════════════════

TEST(PatchCableSetCoverage, constructionDestNull) {
	CovHelper h;
	PatchCableSet* cs = h.cs();

	CHECK(cs->destinations[GLOBALITY_LOCAL] == nullptr);
	CHECK(cs->destinations[GLOBALITY_GLOBAL] == nullptr);
}

TEST(PatchCableSetCoverage, constructionObjSize) {
	CovHelper h;
	PatchCableSet* cs = h.cs();
	CHECK_EQUAL((int32_t)sizeof(PatchCableSet), cs->objectSize);
}

// ═══════════════════════════════════════════════════════════════════════
// getParam creation edge cases
// ═══════════════════════════════════════════════════════════════════════

TEST(PatchCableSetCoverage, getParamCreateDiffSources) {
	CovHelper h;
	PatchCableSet* cs = h.cs();

	ParamDescriptor desc;
	desc.setToHaveParamOnly(params::LOCAL_VOLUME);

	AutoParam* p1 = cs->getParam(nullptr, PatchSource::LFO_GLOBAL_1, desc, true);
	AutoParam* p2 = cs->getParam(nullptr, PatchSource::ENVELOPE_0, desc, true);
	AutoParam* p3 = cs->getParam(nullptr, PatchSource::VELOCITY, desc, true);

	CHECK(p1 != nullptr);
	CHECK(p2 != nullptr);
	CHECK(p3 != nullptr);
	CHECK(p1 != p2);
	CHECK(p2 != p3);
	CHECK_EQUAL(3, cs->numPatchCables);
}

TEST(PatchCableSetCoverage, getParamCreateDiffDests) {
	CovHelper h;
	PatchCableSet* cs = h.cs();

	ParamDescriptor descVol, descLpf, descPitch;
	descVol.setToHaveParamOnly(params::LOCAL_VOLUME);
	descLpf.setToHaveParamOnly(params::LOCAL_LPF_FREQ);
	descPitch.setToHaveParamOnly(params::LOCAL_PITCH_ADJUST);

	AutoParam* p1 = cs->getParam(nullptr, PatchSource::LFO_GLOBAL_1, descVol, true);
	AutoParam* p2 = cs->getParam(nullptr, PatchSource::LFO_GLOBAL_1, descLpf, true);
	AutoParam* p3 = cs->getParam(nullptr, PatchSource::LFO_GLOBAL_1, descPitch, true);

	CHECK(p1 != nullptr);
	CHECK(p2 != nullptr);
	CHECK(p3 != nullptr);
	CHECK_EQUAL(3, cs->numPatchCables);
}

TEST(PatchCableSetCoverage, getParamCreateWhenFullNull) {
	CovHelper h;
	PatchCableSet* cs = h.cs();

	for (int i = 0; i < kMaxNumPatchCables; i++) {
		cs->patchCables[i].from = static_cast<PatchSource>(i % kNumPatchSources);
		cs->patchCables[i].destinationParamDescriptor.setToHaveParamOnly(params::LOCAL_VOLUME + (i / kNumPatchSources));
		cs->patchCables[i].param.setCurrentValueBasicForSetup(100);
	}
	cs->numPatchCables = kMaxNumPatchCables;

	ParamDescriptor desc;
	desc.setToHaveParamOnly(params::GLOBAL_DELAY_RATE);

	AutoParam* param = cs->getParam(nullptr, PatchSource::RANDOM, desc, true);
	CHECK(param == nullptr);
}

// ═══════════════════════════════════════════════════════════════════════
// Cable polarity preserved
// ═══════════════════════════════════════════════════════════════════════

TEST(PatchCableSetCoverage, cablePolarityPreserved) {
	CovHelper h;
	PatchCableSet* cs = h.cs();

	addCableCov(cs, PatchSource::AFTERTOUCH, params::LOCAL_VOLUME, 100, Polarity::UNIPOLAR);
	addCableCov(cs, PatchSource::LFO_GLOBAL_1, params::LOCAL_LPF_FREQ, 200, Polarity::BIPOLAR);

	CHECK(cs->patchCables[0].polarity == Polarity::UNIPOLAR);
	CHECK(cs->patchCables[1].polarity == Polarity::BIPOLAR);
}

// ═══════════════════════════════════════════════════════════════════════
// getPatchCableIndex — find after create
// ═══════════════════════════════════════════════════════════════════════

TEST(PatchCableSetCoverage, getPatchCableIndexFindAfterCreate) {
	CovHelper h;
	PatchCableSet* cs = h.cs();

	ParamDescriptor desc;
	desc.setToHaveParamOnly(params::LOCAL_VOLUME);

	uint8_t idx = cs->getPatchCableIndex(PatchSource::RANDOM, desc, nullptr, true);
	CHECK(idx != 255);

	uint8_t idx2 = cs->getPatchCableIndex(PatchSource::RANDOM, desc, nullptr, false);
	CHECK_EQUAL(idx, idx2);

	uint8_t idx3 = cs->getPatchCableIndex(PatchSource::NOTE, desc, nullptr, false);
	CHECK_EQUAL(255, idx3);
}

// ═══════════════════════════════════════════════════════════════════════
// getParamId — encoding uniqueness
// ═══════════════════════════════════════════════════════════════════════

TEST(PatchCableSetCoverage, getParamIdDistinctSources) {
	ParamDescriptor desc;
	desc.setToHaveParamOnly(params::LOCAL_VOLUME);

	int32_t id1 = PatchCableSet::getParamId(desc, PatchSource::LFO_GLOBAL_1);
	int32_t id2 = PatchCableSet::getParamId(desc, PatchSource::ENVELOPE_0);

	CHECK(id1 != id2);
}

TEST(PatchCableSetCoverage, getParamIdDistinctDests) {
	ParamDescriptor desc1, desc2;
	desc1.setToHaveParamOnly(params::LOCAL_VOLUME);
	desc2.setToHaveParamOnly(params::LOCAL_LPF_FREQ);

	int32_t id1 = PatchCableSet::getParamId(desc1, PatchSource::LFO_GLOBAL_1);
	int32_t id2 = PatchCableSet::getParamId(desc2, PatchSource::LFO_GLOBAL_1);

	CHECK(id1 != id2);
}

// ═══════════════════════════════════════════════════════════════════════
// shouldParamIndicateMiddleValue with cables
// ═══════════════════════════════════════════════════════════════════════

TEST(PatchCableSetCoverage, shouldParamMiddleWithCables) {
	CovHelper h;
	PatchCableSet* cs = h.cs();
	addCableCov(cs, PatchSource::LFO_GLOBAL_1, params::LOCAL_VOLUME);

	CHECK_TRUE(cs->shouldParamIndicateMiddleValue(nullptr));
}

// ═══════════════════════════════════════════════════════════════════════
// Destruction with null destinations — no crash
// ═══════════════════════════════════════════════════════════════════════

TEST(PatchCableSetCoverage, destructionNullDests) {
	{
		CovHelper h;
		PatchCableSet* cs = h.cs();
		CHECK(cs->destinations[0] == nullptr);
		CHECK(cs->destinations[1] == nullptr);
	}
}

// ═══════════════════════════════════════════════════════════════════════
// manualCheckCables — boundary at last cable
// ═══════════════════════════════════════════════════════════════════════

TEST(PatchCableSetCoverage, manualCheckLastCable) {
	CovHelper h;
	PatchCableSet* cs = h.cs();

	addCableCov(cs, PatchSource::LFO_GLOBAL_1, params::LOCAL_VOLUME);
	addCableCov(cs, PatchSource::ENVELOPE_0, params::LOCAL_LPF_FREQ);
	addCableCov(cs, PatchSource::VELOCITY, params::LOCAL_HPF_FREQ);
	addCableCov(cs, PatchSource::AFTERTOUCH, params::LOCAL_PAN);
	addCableCov(cs, PatchSource::RANDOM, params::LOCAL_PITCH_ADJUST);

	CHECK_TRUE(cs->isSourcePatchedToSomethingManuallyCheckCables(PatchSource::RANDOM));
	CHECK_FALSE(cs->isSourcePatchedToSomethingManuallyCheckCables(PatchSource::NOTE));
}

// ═══════════════════════════════════════════════════════════════════════
// swapCables — indirect verification
// ═══════════════════════════════════════════════════════════════════════

TEST(PatchCableSetCoverage, swapCablesIndirect) {
	CovHelper h;
	PatchCableSet* cs = h.cs();

	addCableCov(cs, PatchSource::LFO_GLOBAL_1, params::LOCAL_VOLUME, 111);
	addCableCov(cs, PatchSource::ENVELOPE_0, params::LOCAL_LPF_FREQ, 222);

	CHECK(cs->patchCables[0].from == PatchSource::LFO_GLOBAL_1);
	CHECK(cs->patchCables[1].from == PatchSource::ENVELOPE_0);
	CHECK_EQUAL(111, cs->patchCables[0].param.getCurrentValue());
	CHECK_EQUAL(222, cs->patchCables[1].param.getCurrentValue());

	ParamDescriptor descVol;
	descVol.setToHaveParamOnly(params::LOCAL_VOLUME);
	ParamDescriptor descLpf;
	descLpf.setToHaveParamOnly(params::LOCAL_LPF_FREQ);

	CHECK_EQUAL(0, cs->getPatchCableIndex(PatchSource::LFO_GLOBAL_1, descVol));
	CHECK_EQUAL(1, cs->getPatchCableIndex(PatchSource::ENVELOPE_0, descLpf));
}

// ═══════════════════════════════════════════════════════════════════════
// getParam find existing by source+desc
// ═══════════════════════════════════════════════════════════════════════

TEST(PatchCableSetCoverage, getParamFindsExistingBySourceDesc) {
	CovHelper h;
	PatchCableSet* cs = h.cs();

	addCableCov(cs, PatchSource::LFO_GLOBAL_1, params::LOCAL_VOLUME, 42);

	ParamDescriptor desc;
	desc.setToHaveParamOnly(params::LOCAL_VOLUME);

	AutoParam* param = cs->getParam(nullptr, PatchSource::LFO_GLOBAL_1, desc, false);
	CHECK(param != nullptr);
	CHECK_EQUAL(42, param->getCurrentValue());
}

TEST(PatchCableSetCoverage, getParamNotFoundNoCreate) {
	CovHelper h;
	PatchCableSet* cs = h.cs();

	ParamDescriptor desc;
	desc.setToHaveParamOnly(params::LOCAL_VOLUME);

	AutoParam* param = cs->getParam(nullptr, PatchSource::LFO_GLOBAL_1, desc, false);
	CHECK(param == nullptr);
}
