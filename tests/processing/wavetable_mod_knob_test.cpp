// Regression tests for Sound::applyWavetableModKnobDefaults() — issue #61.
//
// The bug: loading a new wavetable file on an oscillator that was already a
// wavetable overwrote user mod knob mappings with defaults. The fix guards
// the default assignment with wasAlreadyWavetable.
//
// Extracted from SampleBrowser::doLoadAsWaveTable() so the logic can be
// tested without UI/file dependencies.

#include "CppUTest/TestHarness.h"
#include "processing/sound/sound_instrument.h"

namespace params = deluge::modulation::params;

TEST_GROUP(WavetableModKnob){};

// ── First-time wavetable assignment (wasAlreadyWavetable = false) ───────

TEST(WavetableModKnob, OscAFirstTimeSetsMappings) {
	SoundInstrument si;

	// Before: modKnobs[7] has defaults from constructor (SRR / bitcrushing)
	int32_t origP0 = si.modKnobs[7][0].paramDescriptor.getJustTheParam();
	int32_t origP1 = si.modKnobs[7][1].paramDescriptor.getJustTheParam();
	CHECK(origP1 != params::LOCAL_OSC_A_WAVE_INDEX); // not already wavetable mapped

	si.applyWavetableModKnobDefaults(0, false);

	// After: [7][1] = OSC_A_WAVE_INDEX, [7][0] = OSC_A_WAVE_INDEX with LFO source
	CHECK_EQUAL(params::LOCAL_OSC_A_WAVE_INDEX, si.modKnobs[7][1].paramDescriptor.getJustTheParam());
	// [7][0] should be set to OSC_A_WAVE_INDEX + LFO_LOCAL_1 source (since it wasn't OSC_B_WAVE_INDEX)
	CHECK_EQUAL(params::LOCAL_OSC_A_WAVE_INDEX, si.modKnobs[7][0].paramDescriptor.getJustTheParam());
}

TEST(WavetableModKnob, OscBFirstTimeSetsMappings) {
	SoundInstrument si;

	si.applyWavetableModKnobDefaults(1, false);

	// After: [7][0] = OSC_B_WAVE_INDEX
	CHECK_EQUAL(params::LOCAL_OSC_B_WAVE_INDEX, si.modKnobs[7][0].paramDescriptor.getJustTheParam());
}

TEST(WavetableModKnob, OscAPreservesOscBMapping) {
	SoundInstrument si;

	// Set up: [7][0] already mapped to OSC_B_WAVE_INDEX (Osc B is already a wavetable)
	si.modKnobs[7][0].paramDescriptor.setToHaveParamOnly(params::LOCAL_OSC_B_WAVE_INDEX);

	si.applyWavetableModKnobDefaults(0, false);

	// [7][1] should be set to OSC_A_WAVE_INDEX
	CHECK_EQUAL(params::LOCAL_OSC_A_WAVE_INDEX, si.modKnobs[7][1].paramDescriptor.getJustTheParam());
	// [7][0] should STAY as OSC_B_WAVE_INDEX (not overwritten)
	CHECK_EQUAL(params::LOCAL_OSC_B_WAVE_INDEX, si.modKnobs[7][0].paramDescriptor.getJustTheParam());
}

// ── Already-wavetable case (wasAlreadyWavetable = true) — THE BUG FIX ──

TEST(WavetableModKnob, AlreadyWavetablePreservesUserMappingsOscA) {
	SoundInstrument si;

	// User has custom mappings on modKnobs[7]
	si.modKnobs[7][0].paramDescriptor.setToHaveParamOnly(params::LOCAL_LPF_RESONANCE);
	si.modKnobs[7][1].paramDescriptor.setToHaveParamOnly(params::LOCAL_LPF_FREQ);

	// Load a new wavetable file on an already-wavetable oscillator
	si.applyWavetableModKnobDefaults(0, true);

	// User mappings should be preserved — this is the bug fix
	CHECK_EQUAL(params::LOCAL_LPF_RESONANCE, si.modKnobs[7][0].paramDescriptor.getJustTheParam());
	CHECK_EQUAL(params::LOCAL_LPF_FREQ, si.modKnobs[7][1].paramDescriptor.getJustTheParam());
}

TEST(WavetableModKnob, AlreadyWavetablePreservesUserMappingsOscB) {
	SoundInstrument si;

	si.modKnobs[7][0].paramDescriptor.setToHaveParamOnly(params::GLOBAL_REVERB_AMOUNT);
	si.modKnobs[7][1].paramDescriptor.setToHaveParamOnly(params::GLOBAL_DELAY_RATE);

	si.applyWavetableModKnobDefaults(1, true);

	CHECK_EQUAL(params::GLOBAL_REVERB_AMOUNT, si.modKnobs[7][0].paramDescriptor.getJustTheParam());
	CHECK_EQUAL(params::GLOBAL_DELAY_RATE, si.modKnobs[7][1].paramDescriptor.getJustTheParam());
}

// ── Edge cases ─────────────────────────────────────────────────────────

TEST(WavetableModKnob, OtherModKnobsUntouched) {
	SoundInstrument si;

	// Record all non-[7] mod knobs
	int32_t before[7][2];
	for (int f = 0; f < 7; f++) {
		before[f][0] = si.modKnobs[f][0].paramDescriptor.getJustTheParam();
		before[f][1] = si.modKnobs[f][1].paramDescriptor.getJustTheParam();
	}

	si.applyWavetableModKnobDefaults(0, false);

	// All non-[7] mod knobs should be unchanged
	for (int f = 0; f < 7; f++) {
		CHECK_EQUAL(before[f][0], si.modKnobs[f][0].paramDescriptor.getJustTheParam());
		CHECK_EQUAL(before[f][1], si.modKnobs[f][1].paramDescriptor.getJustTheParam());
	}
}

TEST(WavetableModKnob, DoubleApplyFirstTimeSameResult) {
	SoundInstrument si;

	si.applyWavetableModKnobDefaults(0, false);
	int32_t p0 = si.modKnobs[7][0].paramDescriptor.getJustTheParam();
	int32_t p1 = si.modKnobs[7][1].paramDescriptor.getJustTheParam();

	// Applying again as not-already-wavetable should produce the same result
	si.applyWavetableModKnobDefaults(0, false);
	CHECK_EQUAL(p0, si.modKnobs[7][0].paramDescriptor.getJustTheParam());
	CHECK_EQUAL(p1, si.modKnobs[7][1].paramDescriptor.getJustTheParam());
}
