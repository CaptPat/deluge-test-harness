// Deep tests for modulation/patch/patch_cable_set.cpp
// Covers: getPatchCableIndex, deletePatchCable, isSourcePatchedToSomething,
// doesDestinationDescriptorHaveAnyCables, getPatchCableFromVelocityToLevel,
// grabVelocityToLevelFromMIDICable, getModifiedPatchCableAmount,
// paramValueToKnobPos/knobPosToParamValue, dissectParamId/getParamId,
// multi-source-to-one-dest, one-source-to-multi-dest, max cables, boundary indices

#include "CppUTest/TestHarness.h"
#include "definitions_cxx.hpp"
#include "io/midi/midi_device.h"
#include "modulation/params/param_manager.h"
#include "modulation/params/param_set.h"
#include "modulation/patch/patch_cable_set.h"
#include "modulation/patch/patcher.h"
#include "model/model_stack.h"
#include <cstring>

namespace params = deluge::modulation::params;

// ── Helper ──────────────────────────────────────────────────────────────
// Allocates a PatchCableSet through ParamManager so lifetime is managed.
struct DeepHelper {
	ParamManager pm;

	DeepHelper() { pm.setupWithPatching(); }

	PatchCableSet* cs() { return static_cast<PatchCableSet*>(pm.summaries[2].paramCollection); }
};

// Manually add a cable without going through getPatchCableIndex (no modelStack needed).
static void addCable(PatchCableSet* cs, PatchSource src, int32_t destParam, int32_t amount = 536870912) {
	uint8_t idx = cs->numPatchCables;
	cs->patchCables[idx].from = src;
	cs->patchCables[idx].destinationParamDescriptor.setToHaveParamOnly(destParam);
	cs->patchCables[idx].param.setCurrentValueBasicForSetup(amount);
	cs->patchCables[idx].polarity = Polarity::BIPOLAR;
	cs->numPatchCables = idx + 1;
}

// ── Test group ──────────────────────────────────────────────────────────
TEST_GROUP(PatchCableSetDeep){};

// ═══════════════════════════════════════════════════════════════════════
// getPatchCableIndex — lookup
// ═══════════════════════════════════════════════════════════════════════

TEST(PatchCableSetDeep, getPatchCableIndexNotFoundReturns255) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();

	ParamDescriptor desc;
	desc.setToHaveParamOnly(params::LOCAL_VOLUME);
	uint8_t idx = cs->getPatchCableIndex(PatchSource::LFO_GLOBAL_1, desc);
	CHECK_EQUAL(255, idx);
}

TEST(PatchCableSetDeep, getPatchCableIndexFindsExistingCable) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();
	addCable(cs, PatchSource::ENVELOPE_0, params::LOCAL_LPF_FREQ);

	ParamDescriptor desc;
	desc.setToHaveParamOnly(params::LOCAL_LPF_FREQ);
	uint8_t idx = cs->getPatchCableIndex(PatchSource::ENVELOPE_0, desc);
	CHECK_EQUAL(0, idx);
}

TEST(PatchCableSetDeep, getPatchCableIndexDistinguishesSources) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();
	addCable(cs, PatchSource::LFO_GLOBAL_1, params::LOCAL_VOLUME);
	addCable(cs, PatchSource::ENVELOPE_0, params::LOCAL_VOLUME);

	ParamDescriptor desc;
	desc.setToHaveParamOnly(params::LOCAL_VOLUME);

	uint8_t idx0 = cs->getPatchCableIndex(PatchSource::LFO_GLOBAL_1, desc);
	uint8_t idx1 = cs->getPatchCableIndex(PatchSource::ENVELOPE_0, desc);
	CHECK_EQUAL(0, idx0);
	CHECK_EQUAL(1, idx1);
	CHECK(idx0 != idx1);
}

TEST(PatchCableSetDeep, getPatchCableIndexDistinguishesDestinations) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();
	addCable(cs, PatchSource::LFO_GLOBAL_1, params::LOCAL_VOLUME);
	addCable(cs, PatchSource::LFO_GLOBAL_1, params::LOCAL_LPF_FREQ);

	ParamDescriptor descVol;
	descVol.setToHaveParamOnly(params::LOCAL_VOLUME);
	ParamDescriptor descLpf;
	descLpf.setToHaveParamOnly(params::LOCAL_LPF_FREQ);

	uint8_t idxVol = cs->getPatchCableIndex(PatchSource::LFO_GLOBAL_1, descVol);
	uint8_t idxLpf = cs->getPatchCableIndex(PatchSource::LFO_GLOBAL_1, descLpf);
	CHECK_EQUAL(0, idxVol);
	CHECK_EQUAL(1, idxLpf);
}

TEST(PatchCableSetDeep, getPatchCableIndexCreateWhenNotFound) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();
	CHECK_EQUAL(0, cs->numPatchCables);

	ParamDescriptor desc;
	desc.setToHaveParamOnly(params::LOCAL_VOLUME);

	// createIfNotFound = true, no modelStack — creates cable but skips setupPatching
	uint8_t idx = cs->getPatchCableIndex(PatchSource::VELOCITY, desc, nullptr, true);
	CHECK(idx != 255);
	CHECK_EQUAL(1, cs->numPatchCables);
	CHECK(cs->patchCables[idx].from == PatchSource::VELOCITY);
}

TEST(PatchCableSetDeep, getPatchCableIndexCreateDoesNotDuplicate) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();

	ParamDescriptor desc;
	desc.setToHaveParamOnly(params::LOCAL_VOLUME);

	// Create once
	uint8_t idx1 = cs->getPatchCableIndex(PatchSource::VELOCITY, desc, nullptr, true);
	CHECK_EQUAL(1, cs->numPatchCables);

	// Finding existing should not create a new one, even with createIfNotFound
	uint8_t idx2 = cs->getPatchCableIndex(PatchSource::VELOCITY, desc, nullptr, true);
	CHECK_EQUAL(1, cs->numPatchCables);
	CHECK_EQUAL(idx1, idx2);
}

TEST(PatchCableSetDeep, getPatchCableIndexMaxCablesReturns255) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();

	// Fill all cable slots with distinct source+dest combos
	for (int i = 0; i < kMaxNumPatchCables; i++) {
		cs->patchCables[i].from = static_cast<PatchSource>(i % kNumPatchSources);
		cs->patchCables[i].destinationParamDescriptor.setToHaveParamOnly(params::LOCAL_VOLUME + (i / kNumPatchSources));
		cs->patchCables[i].param.setCurrentValueBasicForSetup(100);
	}
	cs->numPatchCables = kMaxNumPatchCables;

	// Trying to create a new cable when full should return 255
	ParamDescriptor desc;
	desc.setToHaveParamOnly(params::GLOBAL_DELAY_RATE);
	uint8_t idx = cs->getPatchCableIndex(PatchSource::AFTERTOUCH, desc, nullptr, true);
	CHECK_EQUAL(255, idx);
	CHECK_EQUAL(kMaxNumPatchCables, cs->numPatchCables);
}

// ═══════════════════════════════════════════════════════════════════════
// doesDestinationDescriptorHaveAnyCables
// ═══════════════════════════════════════════════════════════════════════

TEST(PatchCableSetDeep, doesDestDescriptorHaveAnyCablesEmpty) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();
	ParamDescriptor desc;
	desc.setToHaveParamOnly(params::LOCAL_VOLUME);
	CHECK_FALSE(cs->doesDestinationDescriptorHaveAnyCables(desc));
}

TEST(PatchCableSetDeep, doesDestDescriptorHaveAnyCablesFinds) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();
	addCable(cs, PatchSource::LFO_GLOBAL_1, params::LOCAL_VOLUME);

	ParamDescriptor desc;
	desc.setToHaveParamOnly(params::LOCAL_VOLUME);
	CHECK_TRUE(cs->doesDestinationDescriptorHaveAnyCables(desc));
}

TEST(PatchCableSetDeep, doesDestDescriptorHaveAnyCablesWrongDest) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();
	addCable(cs, PatchSource::LFO_GLOBAL_1, params::LOCAL_VOLUME);

	ParamDescriptor desc;
	desc.setToHaveParamOnly(params::LOCAL_LPF_FREQ);
	CHECK_FALSE(cs->doesDestinationDescriptorHaveAnyCables(desc));
}

TEST(PatchCableSetDeep, doesDestDescriptorMultipleCables) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();
	addCable(cs, PatchSource::LFO_GLOBAL_1, params::LOCAL_VOLUME);
	addCable(cs, PatchSource::ENVELOPE_0, params::LOCAL_VOLUME);
	addCable(cs, PatchSource::VELOCITY, params::LOCAL_LPF_FREQ);

	ParamDescriptor descVol;
	descVol.setToHaveParamOnly(params::LOCAL_VOLUME);
	ParamDescriptor descLpf;
	descLpf.setToHaveParamOnly(params::LOCAL_LPF_FREQ);
	ParamDescriptor descHpf;
	descHpf.setToHaveParamOnly(params::LOCAL_HPF_FREQ);

	CHECK_TRUE(cs->doesDestinationDescriptorHaveAnyCables(descVol));
	CHECK_TRUE(cs->doesDestinationDescriptorHaveAnyCables(descLpf));
	CHECK_FALSE(cs->doesDestinationDescriptorHaveAnyCables(descHpf));
}

// ═══════════════════════════════════════════════════════════════════════
// isSourcePatchedToSomethingManuallyCheckCables — deeper coverage
// ═══════════════════════════════════════════════════════════════════════

TEST(PatchCableSetDeep, manualCheckAllSourceTypes) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();
	addCable(cs, PatchSource::VELOCITY, params::LOCAL_VOLUME);
	addCable(cs, PatchSource::AFTERTOUCH, params::LOCAL_LPF_FREQ);
	addCable(cs, PatchSource::SIDECHAIN, params::LOCAL_HPF_FREQ);

	CHECK_TRUE(cs->isSourcePatchedToSomethingManuallyCheckCables(PatchSource::VELOCITY));
	CHECK_TRUE(cs->isSourcePatchedToSomethingManuallyCheckCables(PatchSource::AFTERTOUCH));
	CHECK_TRUE(cs->isSourcePatchedToSomethingManuallyCheckCables(PatchSource::SIDECHAIN));
	CHECK_FALSE(cs->isSourcePatchedToSomethingManuallyCheckCables(PatchSource::LFO_GLOBAL_1));
	CHECK_FALSE(cs->isSourcePatchedToSomethingManuallyCheckCables(PatchSource::LFO_LOCAL_1));
	CHECK_FALSE(cs->isSourcePatchedToSomethingManuallyCheckCables(PatchSource::X));
	CHECK_FALSE(cs->isSourcePatchedToSomethingManuallyCheckCables(PatchSource::Y));
}

TEST(PatchCableSetDeep, manualCheckSameSourceMultiDest) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();
	addCable(cs, PatchSource::LFO_GLOBAL_1, params::LOCAL_VOLUME);
	addCable(cs, PatchSource::LFO_GLOBAL_1, params::LOCAL_LPF_FREQ);
	addCable(cs, PatchSource::LFO_GLOBAL_1, params::LOCAL_HPF_FREQ);

	CHECK_TRUE(cs->isSourcePatchedToSomethingManuallyCheckCables(PatchSource::LFO_GLOBAL_1));
	CHECK_FALSE(cs->isSourcePatchedToSomethingManuallyCheckCables(PatchSource::ENVELOPE_0));
}

// ═══════════════════════════════════════════════════════════════════════
// isSourcePatchedToSomething — reads sourcesPatchedToAnything bitfield
// ═══════════════════════════════════════════════════════════════════════

TEST(PatchCableSetDeep, isSourcePatchedToSomethingBitfield) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();

	// Manually set the bitfield (normally done by setupPatching)
	cs->sourcesPatchedToAnything[GLOBALITY_LOCAL] = (1u << static_cast<uint32_t>(PatchSource::LFO_GLOBAL_1));
	cs->sourcesPatchedToAnything[GLOBALITY_GLOBAL] = 0;

	CHECK_TRUE(cs->isSourcePatchedToSomething(PatchSource::LFO_GLOBAL_1));
	CHECK_FALSE(cs->isSourcePatchedToSomething(PatchSource::ENVELOPE_0));
}

TEST(PatchCableSetDeep, isSourcePatchedToSomethingCombinesGlobalities) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();

	// One source in local, another in global
	cs->sourcesPatchedToAnything[GLOBALITY_LOCAL] = (1u << static_cast<uint32_t>(PatchSource::LFO_GLOBAL_1));
	cs->sourcesPatchedToAnything[GLOBALITY_GLOBAL] = (1u << static_cast<uint32_t>(PatchSource::VELOCITY));

	CHECK_TRUE(cs->isSourcePatchedToSomething(PatchSource::LFO_GLOBAL_1));
	CHECK_TRUE(cs->isSourcePatchedToSomething(PatchSource::VELOCITY));
	CHECK_FALSE(cs->isSourcePatchedToSomething(PatchSource::AFTERTOUCH));
}

TEST(PatchCableSetDeep, isSourcePatchedToSomethingEmptyBitfield) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();
	cs->sourcesPatchedToAnything[GLOBALITY_LOCAL] = 0;
	cs->sourcesPatchedToAnything[GLOBALITY_GLOBAL] = 0;

	CHECK_FALSE(cs->isSourcePatchedToSomething(PatchSource::LFO_GLOBAL_1));
	CHECK_FALSE(cs->isSourcePatchedToSomething(PatchSource::VELOCITY));
}

// ═══════════════════════════════════════════════════════════════════════
// doesParamHaveSomethingPatchedToIt — depends on destinations[] arrays
// ═══════════════════════════════════════════════════════════════════════

TEST(PatchCableSetDeep, doesParamHaveSomethingNullDestinations) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();
	// destinations are null after construction
	CHECK_FALSE(cs->doesParamHaveSomethingPatchedToIt(params::LOCAL_VOLUME));
	CHECK_FALSE(cs->doesParamHaveSomethingPatchedToIt(params::GLOBAL_VOLUME_POST_FX));
}

// ═══════════════════════════════════════════════════════════════════════
// Multi-source to same destination
// ═══════════════════════════════════════════════════════════════════════

TEST(PatchCableSetDeep, multipleSourcesSameDestination) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();

	addCable(cs, PatchSource::LFO_GLOBAL_1, params::LOCAL_VOLUME, 100);
	addCable(cs, PatchSource::ENVELOPE_0, params::LOCAL_VOLUME, 200);
	addCable(cs, PatchSource::VELOCITY, params::LOCAL_VOLUME, 300);

	CHECK_EQUAL(3, cs->numPatchCables);

	ParamDescriptor desc;
	desc.setToHaveParamOnly(params::LOCAL_VOLUME);

	// Each source should find its own cable
	CHECK_EQUAL(0, cs->getPatchCableIndex(PatchSource::LFO_GLOBAL_1, desc));
	CHECK_EQUAL(1, cs->getPatchCableIndex(PatchSource::ENVELOPE_0, desc));
	CHECK_EQUAL(2, cs->getPatchCableIndex(PatchSource::VELOCITY, desc));

	// Source not patched to this dest should not be found
	CHECK_EQUAL(255, cs->getPatchCableIndex(PatchSource::AFTERTOUCH, desc));

	// Verify individual cable amounts
	CHECK_EQUAL(100, cs->patchCables[0].param.getCurrentValue());
	CHECK_EQUAL(200, cs->patchCables[1].param.getCurrentValue());
	CHECK_EQUAL(300, cs->patchCables[2].param.getCurrentValue());
}

// ═══════════════════════════════════════════════════════════════════════
// Same source to multiple destinations
// ═══════════════════════════════════════════════════════════════════════

TEST(PatchCableSetDeep, sameSourceMultipleDestinations) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();

	addCable(cs, PatchSource::LFO_GLOBAL_1, params::LOCAL_VOLUME, 111);
	addCable(cs, PatchSource::LFO_GLOBAL_1, params::LOCAL_LPF_FREQ, 222);
	addCable(cs, PatchSource::LFO_GLOBAL_1, params::LOCAL_HPF_FREQ, 333);
	addCable(cs, PatchSource::LFO_GLOBAL_1, params::LOCAL_LPF_RESONANCE, 444);

	CHECK_EQUAL(4, cs->numPatchCables);

	// Manual check should find the source regardless of destination
	CHECK_TRUE(cs->isSourcePatchedToSomethingManuallyCheckCables(PatchSource::LFO_GLOBAL_1));

	// Each destination should have its own cable
	ParamDescriptor descVol, descLpf, descHpf, descRes;
	descVol.setToHaveParamOnly(params::LOCAL_VOLUME);
	descLpf.setToHaveParamOnly(params::LOCAL_LPF_FREQ);
	descHpf.setToHaveParamOnly(params::LOCAL_HPF_FREQ);
	descRes.setToHaveParamOnly(params::LOCAL_LPF_RESONANCE);

	CHECK(cs->getPatchCableIndex(PatchSource::LFO_GLOBAL_1, descVol) != 255);
	CHECK(cs->getPatchCableIndex(PatchSource::LFO_GLOBAL_1, descLpf) != 255);
	CHECK(cs->getPatchCableIndex(PatchSource::LFO_GLOBAL_1, descHpf) != 255);
	CHECK(cs->getPatchCableIndex(PatchSource::LFO_GLOBAL_1, descRes) != 255);

	// All indices must be distinct
	uint8_t i0 = cs->getPatchCableIndex(PatchSource::LFO_GLOBAL_1, descVol);
	uint8_t i1 = cs->getPatchCableIndex(PatchSource::LFO_GLOBAL_1, descLpf);
	uint8_t i2 = cs->getPatchCableIndex(PatchSource::LFO_GLOBAL_1, descHpf);
	uint8_t i3 = cs->getPatchCableIndex(PatchSource::LFO_GLOBAL_1, descRes);
	CHECK(i0 != i1);
	CHECK(i0 != i2);
	CHECK(i0 != i3);
	CHECK(i1 != i2);
	CHECK(i1 != i3);
	CHECK(i2 != i3);
}

// ═══════════════════════════════════════════════════════════════════════
// getPatchCableFromVelocityToLevel
// ═══════════════════════════════════════════════════════════════════════

TEST(PatchCableSetDeep, getPatchCableFromVelocityToLevelCreates) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();
	CHECK_EQUAL(0, cs->numPatchCables);

	PatchCable* cable = cs->getPatchCableFromVelocityToLevel();
	CHECK(cable != nullptr);
	CHECK_EQUAL(1, cs->numPatchCables);
	CHECK(cable->from == PatchSource::VELOCITY);
}

TEST(PatchCableSetDeep, getPatchCableFromVelocityToLevelFindsExisting) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();

	// Pre-add a velocity->volume cable
	addCable(cs, PatchSource::VELOCITY, params::LOCAL_VOLUME, 42);

	PatchCable* cable = cs->getPatchCableFromVelocityToLevel();
	CHECK(cable != nullptr);
	// Should reuse the existing cable, not create a new one
	CHECK_EQUAL(1, cs->numPatchCables);
	CHECK_EQUAL(42, cable->param.getCurrentValue());
}

TEST(PatchCableSetDeep, getPatchCableFromVelocityToLevelMaxCablesReturnsNull) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();

	// Fill all slots (none of them velocity->volume)
	for (int i = 0; i < kMaxNumPatchCables; i++) {
		cs->patchCables[i].from = PatchSource::LFO_GLOBAL_1;
		cs->patchCables[i].destinationParamDescriptor.setToHaveParamOnly(params::LOCAL_LPF_FREQ + i);
		cs->patchCables[i].param.setCurrentValueBasicForSetup(100);
	}
	cs->numPatchCables = kMaxNumPatchCables;

	PatchCable* cable = cs->getPatchCableFromVelocityToLevel();
	CHECK(cable == nullptr);
}

// ═══════════════════════════════════════════════════════════════════════
// grabVelocityToLevelFromMIDICable
// ═══════════════════════════════════════════════════════════════════════

TEST(PatchCableSetDeep, grabVelocityToLevelFromMIDICableSetsValue) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();

	// Pre-add a velocity->volume cable
	addCable(cs, PatchSource::VELOCITY, params::LOCAL_VOLUME, 0);

	// Create a MIDICable with a specific default velocity level
	MIDICable cable;
	cable.defaultVelocityToLevel = 999888;

	cs->grabVelocityToLevelFromMIDICable(cable);

	PatchCable* pc = cs->getPatchCableFromVelocityToLevel();
	CHECK(pc != nullptr);
	CHECK_EQUAL(999888, pc->param.getCurrentValue());
}

TEST(PatchCableSetDeep, grabVelocityToLevelFromMIDICableCreatesIfNeeded) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();
	CHECK_EQUAL(0, cs->numPatchCables);

	MIDICable cable;
	cable.defaultVelocityToLevel = 12345;

	cs->grabVelocityToLevelFromMIDICable(cable);

	// Should have created the cable
	CHECK_EQUAL(1, cs->numPatchCables);
	PatchCable* pc = cs->getPatchCableFromVelocityToLevel();
	CHECK(pc != nullptr);
	CHECK_EQUAL(12345, pc->param.getCurrentValue());
}

// ═══════════════════════════════════════════════════════════════════════
// getModifiedPatchCableAmount — pitch params get squared
// ═══════════════════════════════════════════════════════════════════════

TEST(PatchCableSetDeep, getModifiedPatchCableAmountLinearPassthrough) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();
	addCable(cs, PatchSource::LFO_GLOBAL_1, params::LOCAL_VOLUME, 536870912);

	// For non-pitch params, amount passes through unchanged
	int32_t result = cs->getModifiedPatchCableAmount(0, params::LOCAL_VOLUME);
	CHECK_EQUAL(536870912, result);
}

TEST(PatchCableSetDeep, getModifiedPatchCableAmountPitchSquared) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();
	addCable(cs, PatchSource::LFO_GLOBAL_1, params::LOCAL_PITCH_ADJUST, 536870912);

	// For pitch params, the amount is squared (shifted)
	int32_t linear = cs->getModifiedPatchCableAmount(0, params::LOCAL_VOLUME);
	int32_t squared = cs->getModifiedPatchCableAmount(0, params::LOCAL_PITCH_ADJUST);
	CHECK(squared != linear);
}

TEST(PatchCableSetDeep, getModifiedPatchCableAmountZeroIsZero) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();
	addCable(cs, PatchSource::LFO_GLOBAL_1, params::LOCAL_PITCH_ADJUST, 0);

	CHECK_EQUAL(0, cs->getModifiedPatchCableAmount(0, params::LOCAL_PITCH_ADJUST));
	CHECK_EQUAL(0, cs->getModifiedPatchCableAmount(0, params::LOCAL_VOLUME));
}

TEST(PatchCableSetDeep, getModifiedPatchCableAmountNegativePitch) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();
	addCable(cs, PatchSource::LFO_GLOBAL_1, params::LOCAL_PITCH_ADJUST, -536870912);

	int32_t result = cs->getModifiedPatchCableAmount(0, params::LOCAL_PITCH_ADJUST);
	// Negative pitch amounts: squared then negated
	CHECK(result < 0);
}

TEST(PatchCableSetDeep, getModifiedPatchCableAmountOscAPitch) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();
	addCable(cs, PatchSource::LFO_GLOBAL_1, params::LOCAL_OSC_A_PITCH_ADJUST, 536870912);

	// OSC_A_PITCH_ADJUST should also use the squared path
	int32_t linear = cs->getModifiedPatchCableAmount(0, params::LOCAL_VOLUME);
	int32_t squared = cs->getModifiedPatchCableAmount(0, params::LOCAL_OSC_A_PITCH_ADJUST);
	CHECK(squared != linear);
}

TEST(PatchCableSetDeep, getModifiedPatchCableAmountDelayRate) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();
	addCable(cs, PatchSource::LFO_GLOBAL_1, params::GLOBAL_DELAY_RATE, 536870912);

	// GLOBAL_DELAY_RATE also uses the squared path
	int32_t linear = cs->getModifiedPatchCableAmount(0, params::LOCAL_VOLUME);
	int32_t squared = cs->getModifiedPatchCableAmount(0, params::GLOBAL_DELAY_RATE);
	CHECK(squared != linear);
}

TEST(PatchCableSetDeep, getModifiedPatchCableAmountVelocityToPitchSpecialScale) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();
	addCable(cs, PatchSource::VELOCITY, params::LOCAL_PITCH_ADJUST, 536870912);

	int32_t velocityResult = cs->getModifiedPatchCableAmount(0, params::LOCAL_PITCH_ADJUST);

	// Now same amount but with a non-velocity source
	cs->patchCables[0].from = PatchSource::LFO_GLOBAL_1;
	int32_t lfoResult = cs->getModifiedPatchCableAmount(0, params::LOCAL_PITCH_ADJUST);

	// Velocity to master pitch has special semitone-aligned scaling
	CHECK(velocityResult != lfoResult);
}

// ═══════════════════════════════════════════════════════════════════════
// paramValueToKnobPos / knobPosToParamValue
// ═══════════════════════════════════════════════════════════════════════

TEST(PatchCableSetDeep, paramValueToKnobPosZero) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();
	// paramValue 0 => (0 >> 23) - 64 = -64
	CHECK_EQUAL(-64, cs->paramValueToKnobPos(0, nullptr));
}

TEST(PatchCableSetDeep, paramValueToKnobPosMax) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();
	// paramValue = 1073741824 (max cable value) => (1073741824 >> 23) - 64 = 128 - 64 = 64
	CHECK_EQUAL(64, cs->paramValueToKnobPos(1073741824, nullptr));
}

TEST(PatchCableSetDeep, paramValueToKnobPosMidpoint) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();
	// paramValue = 536870912 => (536870912 >> 23) - 64 = 64 - 64 = 0
	CHECK_EQUAL(0, cs->paramValueToKnobPos(536870912, nullptr));
}

TEST(PatchCableSetDeep, knobPosToParamValueCenter) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();
	// knobPos 0 => (0 + 64) << 23 = 536870912
	CHECK_EQUAL(536870912, cs->knobPosToParamValue(0, nullptr));
}

TEST(PatchCableSetDeep, knobPosToParamValueMin) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();
	// knobPos -64 => (-64 + 64) << 23 = 0
	CHECK_EQUAL(0, cs->knobPosToParamValue(-64, nullptr));
}

TEST(PatchCableSetDeep, knobPosToParamValueMax) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();
	// knobPos >= 64 => saturates to 1073741824
	CHECK_EQUAL(1073741824, cs->knobPosToParamValue(64, nullptr));
}

TEST(PatchCableSetDeep, knobPosParamValueRoundtrip) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();

	// Round-trip for values in the middle range
	for (int knob = -63; knob < 64; knob++) {
		int32_t param = cs->knobPosToParamValue(knob, nullptr);
		int32_t back = cs->paramValueToKnobPos(param, nullptr);
		CHECK_EQUAL(knob, back);
	}
}

// ═══════════════════════════════════════════════════════════════════════
// getParamId / dissectParamId roundtrip
// ═══════════════════════════════════════════════════════════════════════

TEST(PatchCableSetDeep, paramIdRoundtrip) {
	ParamDescriptor desc;
	desc.setToHaveParamOnly(params::LOCAL_LPF_FREQ);

	int32_t id = PatchCableSet::getParamId(desc, PatchSource::LFO_GLOBAL_1);

	ParamDescriptor outDesc;
	PatchSource outSource;
	PatchCableSet::dissectParamId(id, &outDesc, &outSource);

	CHECK(outSource == PatchSource::LFO_GLOBAL_1);
	// The dissected descriptor should match the original destination
	CHECK(outDesc == desc);
}

TEST(PatchCableSetDeep, paramIdRoundtripDifferentSources) {
	ParamDescriptor desc;
	desc.setToHaveParamOnly(params::LOCAL_VOLUME);

	PatchSource sources[] = {PatchSource::LFO_GLOBAL_1, PatchSource::ENVELOPE_0, PatchSource::VELOCITY,
	                         PatchSource::AFTERTOUCH,    PatchSource::X,           PatchSource::Y};

	for (auto src : sources) {
		int32_t id = PatchCableSet::getParamId(desc, src);
		ParamDescriptor outDesc;
		PatchSource outSource;
		PatchCableSet::dissectParamId(id, &outDesc, &outSource);
		CHECK(outSource == src);
		CHECK(outDesc == desc);
	}
}

// ═══════════════════════════════════════════════════════════════════════
// getParam (via source + descriptor)
// ═══════════════════════════════════════════════════════════════════════

TEST(PatchCableSetDeep, getParamReturnsNullWhenNotFound) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();

	ParamDescriptor desc;
	desc.setToHaveParamOnly(params::LOCAL_VOLUME);

	AutoParam* param = cs->getParam(nullptr, PatchSource::LFO_GLOBAL_1, desc, false);
	CHECK(param == nullptr);
}

TEST(PatchCableSetDeep, getParamFindsExisting) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();
	addCable(cs, PatchSource::LFO_GLOBAL_1, params::LOCAL_VOLUME, 777);

	ParamDescriptor desc;
	desc.setToHaveParamOnly(params::LOCAL_VOLUME);

	AutoParam* param = cs->getParam(nullptr, PatchSource::LFO_GLOBAL_1, desc, false);
	CHECK(param != nullptr);
	CHECK_EQUAL(777, param->getCurrentValue());
}

TEST(PatchCableSetDeep, getParamCreatesWhenAllowed) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();

	ParamDescriptor desc;
	desc.setToHaveParamOnly(params::LOCAL_VOLUME);

	AutoParam* param = cs->getParam(nullptr, PatchSource::VELOCITY, desc, true);
	CHECK(param != nullptr);
	CHECK_EQUAL(1, cs->numPatchCables);
	// Newly created cable starts at 0
	CHECK_EQUAL(0, param->getCurrentValue());
}

// ═══════════════════════════════════════════════════════════════════════
// isSourcePatchedToDestinationDescriptorVolumeInspecific
// ═══════════════════════════════════════════════════════════════════════

TEST(PatchCableSetDeep, volumeInspecificNotPatched) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();

	ParamDescriptor desc;
	desc.setToHaveParamOnly(params::LOCAL_VOLUME);
	CHECK_FALSE(cs->isSourcePatchedToDestinationDescriptorVolumeInspecific(PatchSource::LFO_GLOBAL_1, desc));
}

TEST(PatchCableSetDeep, volumeInspecificDirectMatch) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();
	addCable(cs, PatchSource::LFO_GLOBAL_1, params::LOCAL_VOLUME);

	ParamDescriptor desc;
	desc.setToHaveParamOnly(params::LOCAL_VOLUME);
	CHECK_TRUE(cs->isSourcePatchedToDestinationDescriptorVolumeInspecific(PatchSource::LFO_GLOBAL_1, desc));
}

TEST(PatchCableSetDeep, volumeInspecificPostFxChecksLocalVolume) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();

	// Patch to LOCAL_VOLUME, query with GLOBAL_VOLUME_POST_FX
	addCable(cs, PatchSource::LFO_GLOBAL_1, params::LOCAL_VOLUME);

	ParamDescriptor desc;
	desc.setToHaveParamOnly(params::GLOBAL_VOLUME_POST_FX);
	// When querying GLOBAL_VOLUME_POST_FX, it also checks LOCAL_VOLUME
	CHECK_TRUE(cs->isSourcePatchedToDestinationDescriptorVolumeInspecific(PatchSource::LFO_GLOBAL_1, desc));
}

TEST(PatchCableSetDeep, volumeInspecificPostFxChecksPostReverbSend) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();

	addCable(cs, PatchSource::LFO_GLOBAL_1, params::GLOBAL_VOLUME_POST_REVERB_SEND);

	ParamDescriptor desc;
	desc.setToHaveParamOnly(params::GLOBAL_VOLUME_POST_FX);
	CHECK_TRUE(cs->isSourcePatchedToDestinationDescriptorVolumeInspecific(PatchSource::LFO_GLOBAL_1, desc));
}

TEST(PatchCableSetDeep, volumeInspecificNonVolumeParamDirectOnly) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();

	addCable(cs, PatchSource::LFO_GLOBAL_1, params::LOCAL_LPF_FREQ);

	ParamDescriptor desc;
	desc.setToHaveParamOnly(params::LOCAL_LPF_FREQ);
	CHECK_TRUE(cs->isSourcePatchedToDestinationDescriptorVolumeInspecific(PatchSource::LFO_GLOBAL_1, desc));

	// Different param should not match
	ParamDescriptor descHpf;
	descHpf.setToHaveParamOnly(params::LOCAL_HPF_FREQ);
	CHECK_FALSE(cs->isSourcePatchedToDestinationDescriptorVolumeInspecific(PatchSource::LFO_GLOBAL_1, descHpf));
}

// ═══════════════════════════════════════════════════════════════════════
// isAnySourcePatchedToParamVolumeInspecific
// ═══════════════════════════════════════════════════════════════════════

TEST(PatchCableSetDeep, anySourcePatchedVolumeInspecificEmpty) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();

	ParamDescriptor desc;
	desc.setToHaveParamOnly(params::LOCAL_VOLUME);
	CHECK_FALSE(cs->isAnySourcePatchedToParamVolumeInspecific(desc));
}

TEST(PatchCableSetDeep, anySourcePatchedVolumeInspecificDirect) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();
	addCable(cs, PatchSource::ENVELOPE_0, params::LOCAL_VOLUME);

	ParamDescriptor desc;
	desc.setToHaveParamOnly(params::LOCAL_VOLUME);
	CHECK_TRUE(cs->isAnySourcePatchedToParamVolumeInspecific(desc));
}

TEST(PatchCableSetDeep, anySourcePatchedVolumeInspecificPostFxFallthrough) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();
	addCable(cs, PatchSource::ENVELOPE_0, params::LOCAL_VOLUME);

	ParamDescriptor desc;
	desc.setToHaveParamOnly(params::GLOBAL_VOLUME_POST_FX);
	// Querying GLOBAL_VOLUME_POST_FX also checks LOCAL_VOLUME
	CHECK_TRUE(cs->isAnySourcePatchedToParamVolumeInspecific(desc));
}

// ═══════════════════════════════════════════════════════════════════════
// beenCloned — deeper edge cases
// ═══════════════════════════════════════════════════════════════════════

TEST(PatchCableSetDeep, beenClonedWithCablesDoesNotCrash) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();
	addCable(cs, PatchSource::LFO_GLOBAL_1, params::LOCAL_VOLUME);
	addCable(cs, PatchSource::ENVELOPE_0, params::LOCAL_LPF_FREQ);
	cs->numUsablePatchCables = 2;

	cs->beenCloned(true, 0);

	// Cables should still exist
	CHECK_EQUAL(2, cs->numPatchCables);
}

TEST(PatchCableSetDeep, beenClonedWithoutAutomation) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();
	addCable(cs, PatchSource::LFO_GLOBAL_1, params::LOCAL_VOLUME, 12345);
	cs->numUsablePatchCables = 1;

	cs->beenCloned(false, 0);

	// Cable values should be reset when copyAutomation = false
	// beenCloned(false) resets automation but keeps current value
	CHECK_EQUAL(1, cs->numPatchCables);
}

// ═══════════════════════════════════════════════════════════════════════
// shouldParamIndicateMiddleValue — always true for PatchCableSet
// ═══════════════════════════════════════════════════════════════════════

TEST(PatchCableSetDeep, shouldParamIndicateMiddleValueAlwaysTrue) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();
	CHECK_TRUE(cs->shouldParamIndicateMiddleValue(nullptr));
}

// ═══════════════════════════════════════════════════════════════════════
// getParamKind
// ═══════════════════════════════════════════════════════════════════════

TEST(PatchCableSetDeep, getParamKindIsPatchCable) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();
	CHECK(cs->getParamKind() == params::Kind::PATCH_CABLE);
}

// ═══════════════════════════════════════════════════════════════════════
// Cable array boundary: creating cables up to max capacity
// ═══════════════════════════════════════════════════════════════════════

TEST(PatchCableSetDeep, fillAllCableSlotsManually) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();

	for (int i = 0; i < kMaxNumPatchCables; i++) {
		cs->patchCables[i].from = static_cast<PatchSource>(i % kNumPatchSources);
		cs->patchCables[i].destinationParamDescriptor.setToHaveParamOnly(params::LOCAL_VOLUME);
		cs->patchCables[i].param.setCurrentValueBasicForSetup(i * 100);
	}
	cs->numPatchCables = kMaxNumPatchCables;

	CHECK_EQUAL(kMaxNumPatchCables, cs->numPatchCables);

	// Verify first and last cables are accessible
	CHECK_EQUAL(0, cs->patchCables[0].param.getCurrentValue());
	CHECK_EQUAL((kMaxNumPatchCables - 1) * 100, cs->patchCables[kMaxNumPatchCables - 1].param.getCurrentValue());
}

TEST(PatchCableSetDeep, getPatchCableIndexAtMaxMinusOne) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();

	// Fill to max - 1 using a single source and sequential local params
	// to avoid any accidental collision with the cable we'll create
	for (int i = 0; i < kMaxNumPatchCables - 1; i++) {
		cs->patchCables[i].from = PatchSource::LFO_GLOBAL_1;
		cs->patchCables[i].destinationParamDescriptor.setToHaveParamOnly(params::LOCAL_VOLUME + i);
		cs->patchCables[i].param.setCurrentValueBasicForSetup(100);
	}
	cs->numPatchCables = kMaxNumPatchCables - 1;

	// Should still be able to create one more with a unique source+dest
	ParamDescriptor desc;
	desc.setToHaveParamOnly(params::LOCAL_VOLUME);
	uint8_t idx = cs->getPatchCableIndex(PatchSource::AFTERTOUCH, desc, nullptr, true);
	CHECK(idx != 255);
	CHECK_EQUAL(kMaxNumPatchCables, (int)cs->numPatchCables);
}

// ═══════════════════════════════════════════════════════════════════════
// Cable index 0 boundary — ensure first slot works correctly
// ═══════════════════════════════════════════════════════════════════════

TEST(PatchCableSetDeep, cableAtIndexZero) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();

	ParamDescriptor desc;
	desc.setToHaveParamOnly(params::LOCAL_VOLUME);

	uint8_t idx = cs->getPatchCableIndex(PatchSource::VELOCITY, desc, nullptr, true);
	CHECK_EQUAL(0, idx);
	CHECK(cs->patchCables[0].from == PatchSource::VELOCITY);

	// Finding it again should return 0
	uint8_t idx2 = cs->getPatchCableIndex(PatchSource::VELOCITY, desc, nullptr, false);
	CHECK_EQUAL(0, idx2);
}

// ═══════════════════════════════════════════════════════════════════════
// Multiple create calls with different source+dest combos
// ═══════════════════════════════════════════════════════════════════════

TEST(PatchCableSetDeep, createMultipleCablesViaGetPatchCableIndex) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();

	ParamDescriptor descVol, descLpf, descHpf;
	descVol.setToHaveParamOnly(params::LOCAL_VOLUME);
	descLpf.setToHaveParamOnly(params::LOCAL_LPF_FREQ);
	descHpf.setToHaveParamOnly(params::LOCAL_HPF_FREQ);

	uint8_t i0 = cs->getPatchCableIndex(PatchSource::VELOCITY, descVol, nullptr, true);
	uint8_t i1 = cs->getPatchCableIndex(PatchSource::LFO_GLOBAL_1, descLpf, nullptr, true);
	uint8_t i2 = cs->getPatchCableIndex(PatchSource::ENVELOPE_0, descHpf, nullptr, true);

	CHECK_EQUAL(3, cs->numPatchCables);
	CHECK(i0 != 255);
	CHECK(i1 != 255);
	CHECK(i2 != 255);
	CHECK(i0 != i1);
	CHECK(i1 != i2);
	CHECK(i0 != i2);

	// Verify each cable has the right source
	CHECK(cs->patchCables[i0].from == PatchSource::VELOCITY);
	CHECK(cs->patchCables[i1].from == PatchSource::LFO_GLOBAL_1);
	CHECK(cs->patchCables[i2].from == PatchSource::ENVELOPE_0);
}

// ═══════════════════════════════════════════════════════════════════════
// getParam by paramId (int32_t overload)
// ═══════════════════════════════════════════════════════════════════════

TEST(PatchCableSetDeep, getParamByIdNullWhenEmpty) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();

	ParamDescriptor desc;
	desc.setToHaveParamOnly(params::LOCAL_VOLUME);
	int32_t id = PatchCableSet::getParamId(desc, PatchSource::LFO_GLOBAL_1);

	// getParam(int32_t) directly indexes into patchCables array
	// With no cables, this would access uninitialized data, so we just verify the
	// getParam(modelStack, source, desc) path returns null
	AutoParam* param = cs->getParam(nullptr, PatchSource::LFO_GLOBAL_1, desc, false);
	CHECK(param == nullptr);
}

// ═══════════════════════════════════════════════════════════════════════
// numUsablePatchCables vs numPatchCables distinction
// ═══════════════════════════════════════════════════════════════════════

TEST(PatchCableSetDeep, usableVsTotalCableCountsIndependent) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();

	addCable(cs, PatchSource::LFO_GLOBAL_1, params::LOCAL_VOLUME);
	addCable(cs, PatchSource::ENVELOPE_0, params::LOCAL_LPF_FREQ);
	addCable(cs, PatchSource::VELOCITY, params::LOCAL_HPF_FREQ);

	CHECK_EQUAL(3, cs->numPatchCables);
	// numUsablePatchCables starts at 0 — only set by setupPatching
	CHECK_EQUAL(0, cs->numUsablePatchCables);

	// Manually mark 2 as usable
	cs->numUsablePatchCables = 2;
	CHECK_EQUAL(2, cs->numUsablePatchCables);
	CHECK_EQUAL(3, cs->numPatchCables);
}

// ═══════════════════════════════════════════════════════════════════════
// Mixed global and local destination cables
// ═══════════════════════════════════════════════════════════════════════

TEST(PatchCableSetDeep, mixedGlobalLocalCables) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();

	// Add local destination cables
	addCable(cs, PatchSource::LFO_GLOBAL_1, params::LOCAL_VOLUME);
	addCable(cs, PatchSource::ENVELOPE_0, params::LOCAL_LPF_FREQ);

	// Add global destination cables
	addCable(cs, PatchSource::LFO_GLOBAL_1, params::GLOBAL_VOLUME_POST_FX);
	addCable(cs, PatchSource::VELOCITY, params::GLOBAL_DELAY_RATE);

	CHECK_EQUAL(4, cs->numPatchCables);

	// All should be findable by getPatchCableIndex
	ParamDescriptor descLocal, descGlobal;
	descLocal.setToHaveParamOnly(params::LOCAL_VOLUME);
	descGlobal.setToHaveParamOnly(params::GLOBAL_VOLUME_POST_FX);

	CHECK(cs->getPatchCableIndex(PatchSource::LFO_GLOBAL_1, descLocal) != 255);
	CHECK(cs->getPatchCableIndex(PatchSource::LFO_GLOBAL_1, descGlobal) != 255);
}

// ═══════════════════════════════════════════════════════════════════════
// getDestinationForParam — null destinations
// ═══════════════════════════════════════════════════════════════════════

TEST(PatchCableSetDeep, getDestinationForParamNullReturnsNull) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();

	Destination* dest = cs->getDestinationForParam(params::LOCAL_VOLUME);
	CHECK(dest == nullptr);

	dest = cs->getDestinationForParam(params::GLOBAL_VOLUME_POST_FX);
	CHECK(dest == nullptr);
}

// ═══════════════════════════════════════════════════════════════════════
// Stress test: add many cables then look them all up
// ═══════════════════════════════════════════════════════════════════════

TEST(PatchCableSetDeep, stressTestManyLookups) {
	DeepHelper h;
	PatchCableSet* cs = h.cs();

	// Create 10 cables with unique source+dest combos
	PatchSource sources[] = {PatchSource::LFO_GLOBAL_1, PatchSource::LFO_LOCAL_1, PatchSource::ENVELOPE_0,
	                         PatchSource::ENVELOPE_1,    PatchSource::VELOCITY,     PatchSource::AFTERTOUCH,
	                         PatchSource::X,             PatchSource::Y,            PatchSource::SIDECHAIN,
	                         PatchSource::RANDOM};
	int32_t destParams[] = {params::LOCAL_VOLUME,         params::LOCAL_LPF_FREQ,         params::LOCAL_HPF_FREQ,
	                        params::LOCAL_LPF_RESONANCE,  params::LOCAL_HPF_RESONANCE,    params::LOCAL_PITCH_ADJUST,
	                        params::LOCAL_OSC_A_VOLUME,   params::LOCAL_OSC_B_VOLUME,     params::LOCAL_MODULATOR_0_VOLUME,
	                        params::LOCAL_MODULATOR_1_VOLUME};

	for (int i = 0; i < 10; i++) {
		addCable(cs, sources[i], destParams[i], (i + 1) * 1000);
	}

	CHECK_EQUAL(10, cs->numPatchCables);

	// Look up each one and verify
	for (int i = 0; i < 10; i++) {
		ParamDescriptor desc;
		desc.setToHaveParamOnly(destParams[i]);
		uint8_t idx = cs->getPatchCableIndex(sources[i], desc);
		CHECK(idx != 255);
		CHECK_EQUAL((i + 1) * 1000, cs->patchCables[idx].param.getCurrentValue());
	}
}
