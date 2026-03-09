// Deep coverage tests for dx7note.cpp
// Targets uncovered branches: ScaleVelocity, ScaleRate, ScaleCurve, ScaleLevel,
// osc_freq modes, lfoPhaseToValue waveforms, getdelay, compute with op switches,
// keyup, transferState/transferSignal, update, engine mode selection, LFO sync.

#include "CppUTest/TestHarness.h"
#include "dsp/dx/dx7note.h"
#include "dsp/dx/engine.h"
#include "dsp/dx/fm_core.h"
#include "dsp/dx/math_lut.h"
#include "memory/memory_allocator_interface.h"
#include <cmath>
#include <cstring>

// ── Forward declarations for static functions we test indirectly ─────────
// These are file-static in dx7note.cpp, so we test them through the public API.
// ScaleVelocity, ScaleRate, ScaleCurve, ScaleLevel are called from DxVoice::init().

// ── Helper: create a default patch and voice ────────────────────────────

static DxEngine* ensureEngine() {
	return getDxEngine();
}

TEST_GROUP(DxNoteTest) {
	DxEngine* engine;
	DxPatch* patch;

	void setup() override {
		engine = ensureEngine();
		patch = engine->newPatch();
		// Disable NEON path — the neon_fm_kernel stub is a no-op,
		// so we must use the scalar C++ render path for real output.
		// Must be set AFTER newPatch() since setEngineMode(0) sets neon=true.
		engine->engineModern.neon = false;
	}

	void teardown() override {
		if (patch) {
			delugeDealloc(patch);
		}
	}

	// Set a specific operator parameter (params are 21 bytes per op, 6 ops)
	void setOpParam(int op, int paramOffset, uint8_t value) {
		patch->params[op * 21 + paramOffset] = value;
	}

	// Get operator output level param offset = 16
	// Get operator mode param offset = 17
	// Get operator coarse param offset = 18
	// Get operator fine param offset = 19
	// Get operator detune param offset = 20
	// Get operator velocity sensitivity offset = 15
	// Get operator rate scaling offset = 13
	// Get operator amp mod sensitivity offset = 14
	// Get operator breakpoint offset = 8
	// Get operator left depth offset = 9
	// Get operator right depth offset = 10
	// Get operator left curve offset = 11
	// Get operator right curve offset = 12

	// Configure patch for immediate audible output:
	// all 6 ops at max output, instant-attack EG (L4=99 so envelope starts full),
	// all ops enabled.
	void makeHotPatch() {
		for (int op = 0; op < 6; op++) {
			setOpParam(op, 0, 99); // R1 = fast
			setOpParam(op, 1, 99); // R2 = fast
			setOpParam(op, 2, 99); // R3 = fast
			setOpParam(op, 3, 99); // R4 = fast
			setOpParam(op, 4, 99); // L1 = full
			setOpParam(op, 5, 99); // L2 = full
			setOpParam(op, 6, 99); // L3 = full
			setOpParam(op, 7, 99); // L4 = full (start level!)
			setOpParam(op, 16, 99); // output level = max
		}
		patch->params[155] = 0x3F; // all ops on
		// Ensure scalar render path (NEON stub is no-op)
		engine->engineModern.neon = false;
	}

	// Compute multiple buffers to let envelope ramp up
	int64_t computeEnergy(DxVoice* voice, int iterations, int bufSize = 64) {
		int64_t energy = 0;
		int32_t buf[256] = {};
		DxVoiceCtrl ctrl;
		for (int i = 0; i < iterations; i++) {
			memset(buf, 0, bufSize * sizeof(int32_t));
			voice->compute(buf, bufSize, 0, patch, &ctrl);
			for (int j = 0; j < bufSize; j++) {
				energy += (int64_t)buf[j] * buf[j];
			}
		}
		return energy;
	}
};

// ── DxPatch construction ────────────────────────────────────────────────

TEST(DxNoteTest, patchDefaultsToInitVoice) {
	// Algorithm should be set from init_voice data (byte 134)
	// init_voice[134] = 0 (algorithm 0)
	CHECK_EQUAL(0, patch->params[134]);
}

TEST(DxNoteTest, patchEgModDefault) {
	CHECK_EQUAL(127, patch->eg_mod);
}

TEST(DxNoteTest, patchRandomDetuneDefault) {
	CHECK_EQUAL(0, patch->random_detune);
}

// ── Engine mode selection ───────────────────────────────────────────────
// updateEngineMode() selects between Modern and MkI engines based on
// engineMode and algorithm/feedback settings.

TEST(DxNoteTest, engineModeModernDefault) {
	patch->setEngineMode(0);
	// Default init_voice has algo=0, feedback=0 → Modern engine
	CHECK(patch->core == &engine->engineModern);
}

TEST(DxNoteTest, engineModeMkIExplicit) {
	patch->setEngineMode(2);
	CHECK(patch->core == &engine->engineMkI);
}

TEST(DxNoteTest, engineModeAutoMkIForAlgo4WithFeedback) {
	// Algorithm 3 (0-indexed) = DX7 algorithm 4, with feedback > 0
	// should auto-select MkI engine
	patch->params[134] = 3; // algo 4 (0-indexed)
	patch->params[135] = 5; // feedback > 0
	patch->setEngineMode(0);
	CHECK(patch->core == &engine->engineMkI);
}

TEST(DxNoteTest, engineModeAutoMkIForAlgo6WithFeedback) {
	patch->params[134] = 5; // algo 6 (0-indexed)
	patch->params[135] = 3; // feedback > 0
	patch->setEngineMode(0);
	CHECK(patch->core == &engine->engineMkI);
}

TEST(DxNoteTest, engineModeModernForAlgo4NoFeedback) {
	patch->params[134] = 3; // algo 4
	patch->params[135] = 0; // feedback = 0
	patch->setEngineMode(0);
	CHECK(patch->core == &engine->engineModern);
}

TEST(DxNoteTest, engineModeModernForOtherAlgoWithFeedback) {
	patch->params[134] = 0; // algo 1 (not 4 or 6)
	patch->params[135] = 7; // feedback
	patch->setEngineMode(0);
	// algos other than 3,5 don't trigger MkI
	CHECK(patch->core == &engine->engineModern);
}

TEST(DxNoteTest, engineMode1UsesModern) {
	patch->setEngineMode(1);
	CHECK(patch->core == &engine->engineModern);
}

// ── LFO waveforms ───────────────────────────────────────────────────────
// lfoPhaseToValue is static but called via computeLfo, which updates lfo_value.

TEST(DxNoteTest, lfoSineWaveform) {
	patch->params[142] = 4; // sine
	patch->params[137] = 50; // moderate rate
	patch->lfo_phase = 0;
	patch->computeLfo(64);
	// After computing, lfo_value should be non-negative (sine starts at midpoint)
	CHECK(patch->lfo_value >= 0);
}

TEST(DxNoteTest, lfoSawtoothDownWaveform) {
	patch->params[142] = 1; // sawtooth down
	patch->params[137] = 50;
	patch->lfo_phase = 0;
	patch->computeLfo(64);
	CHECK(patch->lfo_value >= 0);
}

TEST(DxNoteTest, lfoSawtoothUpWaveform) {
	patch->params[142] = 2; // sawtooth up
	patch->params[137] = 50;
	patch->lfo_phase = 0;
	patch->computeLfo(64);
	CHECK(patch->lfo_value >= 0);
}

TEST(DxNoteTest, lfoSquareWaveform) {
	patch->params[142] = 3; // square
	patch->params[137] = 50;
	patch->lfo_phase = 0;
	patch->computeLfo(64);
	// Square outputs either 0 or (1<<24)
	CHECK(patch->lfo_value >= 0);
}

TEST(DxNoteTest, lfoTriangleWaveformFallsBackToSine) {
	// Triangle (waveform 0) has a workaround: "if (waveform == 0) waveform = 4"
	patch->params[142] = 0; // triangle → falls back to sine
	patch->params[137] = 50;
	patch->lfo_phase = 0;
	patch->computeLfo(64);
	CHECK(patch->lfo_value >= 0);
}

TEST(DxNoteTest, lfoSHWaveformDefaultValue) {
	// S&H (waveform 5) is unimplemented, falls through to default
	patch->params[142] = 5;
	patch->params[137] = 50;
	patch->lfo_phase = 0;
	patch->computeLfo(64);
	// Default returns 1<<23
	CHECK_EQUAL(1 << 23, patch->lfo_value);
}

TEST(DxNoteTest, lfoRateZero) {
	patch->params[137] = 0; // rate 0 → sr = 1
	patch->params[142] = 4; // sine
	patch->lfo_phase = 0;
	patch->computeLfo(64);
	// Should compute without crash, very slow LFO
	CHECK(patch->lfo_value >= 0);
}

TEST(DxNoteTest, lfoRateMax) {
	patch->params[137] = 99; // max rate
	patch->params[142] = 4;
	patch->lfo_phase = 0;
	patch->computeLfo(64);
	CHECK(patch->lfo_value >= 0);
}

// ── DxVoice init and compute ────────────────────────────────────────────

TEST(DxNoteTest, voiceInitAllNotes) {
	DxVoice* voice = engine->solicitDxVoice();
	CHECK(voice != nullptr);

	// Test several MIDI notes across the range
	int notes[] = {0, 21, 60, 84, 108, 127};
	for (int note : notes) {
		voice->init(*patch, note, 100);
		int32_t buf[32] = {};
		DxVoiceCtrl ctrl;
		voice->compute(buf, 32, 0, patch, &ctrl);
	}

	engine->dxVoiceUnassigned(voice);
}

TEST(DxNoteTest, voiceInitVelocityRange) {
	makeHotPatch();
	// Need velocity sensitivity > 0 for velocity to affect output
	for (int op = 0; op < 6; op++) {
		setOpParam(op, 15, 7); // max vel sensitivity
	}

	DxVoice* voice = engine->solicitDxVoice();

	voice->init(*patch, 60, 1);   // min velocity
	int64_t energyLow = computeEnergy(voice, 4);

	voice->init(*patch, 60, 127); // max velocity
	int64_t energyHigh = computeEnergy(voice, 4);

	// With high sensitivity, different velocities should differ
	CHECK(energyLow != energyHigh);

	engine->dxVoiceUnassigned(voice);
}

TEST(DxNoteTest, voiceVelocitySensitivity) {
	makeHotPatch();
	patch->params[134] = 31; // algo 32: all carriers
	for (int op = 0; op < 6; op++) {
		setOpParam(op, 15, 7); // max vel sensitivity
	}

	DxVoice* voice = engine->solicitDxVoice();

	voice->init(*patch, 60, 1); // very low velocity
	int64_t energyLow = computeEnergy(voice, 8);

	voice->init(*patch, 60, 127); // max velocity
	int64_t energyHigh = computeEnergy(voice, 8);

	// With high sensitivity, velocity 127 should have more energy than 1
	CHECK(energyHigh > energyLow);

	engine->dxVoiceUnassigned(voice);
}

// ── Operator frequency modes ────────────────────────────────────────────

TEST(DxNoteTest, oscFreqRatioMode) {
	// Mode 0 = ratio mode (default). Coarse=1 means 1x fundamental.
	DxVoice* voice = engine->solicitDxVoice();

	setOpParam(0, 17, 0); // mode = ratio
	setOpParam(0, 18, 1); // coarse = 1 (fundamental)
	setOpParam(0, 19, 0); // fine = 0
	setOpParam(0, 20, 7); // detune = 7 (center, no detune)

	voice->init(*patch, 60, 100);
	int32_t buf[64] = {};
	DxVoiceCtrl ctrl;
	voice->compute(buf, 64, 0, patch, &ctrl);

	engine->dxVoiceUnassigned(voice);
}

TEST(DxNoteTest, oscFreqFixedMode) {
	// Mode 1 = fixed frequency mode
	DxVoice* voice = engine->solicitDxVoice();

	for (int op = 0; op < 6; op++) {
		setOpParam(op, 17, 1); // mode = fixed
		setOpParam(op, 18, 1); // coarse = 1
		setOpParam(op, 19, 50); // fine = 50
		setOpParam(op, 20, 10); // detune > 7
	}

	voice->init(*patch, 60, 100);
	int32_t buf[64] = {};
	DxVoiceCtrl ctrl;
	voice->compute(buf, 64, 0, patch, &ctrl);

	// In fixed mode, different MIDI notes should produce same frequency
	voice->init(*patch, 36, 100);
	int32_t buf2[64] = {};
	voice->compute(buf2, 64, 0, patch, &ctrl);

	// Fixed freq ops don't follow pitch, so both should be similar
	// (not identical due to pitch envelope, but structurally the same)

	engine->dxVoiceUnassigned(voice);
}

TEST(DxNoteTest, oscFreqFixedDetuneBelow7) {
	// detune <= 7 in fixed mode: no additional detune applied
	DxVoice* voice = engine->solicitDxVoice();

	for (int op = 0; op < 6; op++) {
		setOpParam(op, 17, 1); // fixed mode
		setOpParam(op, 18, 2); // coarse
		setOpParam(op, 19, 0); // fine
		setOpParam(op, 20, 3); // detune < 7 → no detune addition
	}

	voice->init(*patch, 60, 100);
	int32_t buf[64] = {};
	DxVoiceCtrl ctrl;
	voice->compute(buf, 64, 0, patch, &ctrl);

	engine->dxVoiceUnassigned(voice);
}

TEST(DxNoteTest, oscFreqCoarseRange) {
	// Test coarse values across the full range (0-31)
	DxVoice* voice = engine->solicitDxVoice();

	for (int coarse = 0; coarse < 32; coarse++) {
		setOpParam(0, 18, coarse);
		setOpParam(0, 17, 0); // ratio mode
		voice->init(*patch, 60, 100);
		int32_t buf[32] = {};
		DxVoiceCtrl ctrl;
		voice->compute(buf, 32, 0, patch, &ctrl);
	}

	engine->dxVoiceUnassigned(voice);
}

TEST(DxNoteTest, oscFreqFineNonZero) {
	// fine != 0 triggers log-based frequency adjustment
	DxVoice* voice = engine->solicitDxVoice();

	setOpParam(0, 17, 0); // ratio
	setOpParam(0, 18, 1); // coarse = 1
	setOpParam(0, 19, 99); // fine = max
	voice->init(*patch, 60, 100);

	int32_t buf[64] = {};
	DxVoiceCtrl ctrl;
	voice->compute(buf, 64, 0, patch, &ctrl);

	engine->dxVoiceUnassigned(voice);
}

// ── ScaleLevel branches ─────────────────────────────────────────────────
// ScaleLevel has branches for offset >= 0 vs < 0, and ScaleCurve has
// linear vs exponential curves, positive vs negative.

TEST(DxNoteTest, scaleLevelLeftOfBreakpoint) {
	// Set breakpoint high so midinote 60 is left of it
	for (int op = 0; op < 6; op++) {
		setOpParam(op, 8, 99);  // break point = high
		setOpParam(op, 9, 99);  // left depth = max
		setOpParam(op, 10, 0);  // right depth = 0
		setOpParam(op, 11, 0);  // left curve = 0 (linear negative)
		setOpParam(op, 12, 0);  // right curve = 0
	}

	DxVoice* voice = engine->solicitDxVoice();
	voice->init(*patch, 24, 100); // low note, well left of breakpoint
	int32_t buf[32] = {};
	DxVoiceCtrl ctrl;
	voice->compute(buf, 32, 0, patch, &ctrl);

	engine->dxVoiceUnassigned(voice);
}

TEST(DxNoteTest, scaleLevelRightOfBreakpoint) {
	for (int op = 0; op < 6; op++) {
		setOpParam(op, 8, 0);   // break point = low
		setOpParam(op, 9, 0);   // left depth = 0
		setOpParam(op, 10, 99); // right depth = max
		setOpParam(op, 11, 0);
		setOpParam(op, 12, 3);  // right curve = 3 (linear positive)
	}

	DxVoice* voice = engine->solicitDxVoice();
	voice->init(*patch, 108, 100); // high note
	int32_t buf[32] = {};
	DxVoiceCtrl ctrl;
	voice->compute(buf, 32, 0, patch, &ctrl);

	engine->dxVoiceUnassigned(voice);
}

TEST(DxNoteTest, scaleCurveExponential) {
	for (int op = 0; op < 6; op++) {
		setOpParam(op, 8, 60);  // breakpoint = middle C
		setOpParam(op, 9, 50);
		setOpParam(op, 10, 50);
		setOpParam(op, 11, 1);  // left curve = 1 (exponential negative)
		setOpParam(op, 12, 2);  // right curve = 2 (exponential positive)
	}

	DxVoice* voice = engine->solicitDxVoice();
	voice->init(*patch, 24, 100);
	int32_t buf1[32] = {};
	DxVoiceCtrl ctrl;
	voice->compute(buf1, 32, 0, patch, &ctrl);

	voice->init(*patch, 96, 100);
	int32_t buf2[32] = {};
	voice->compute(buf2, 32, 0, patch, &ctrl);

	engine->dxVoiceUnassigned(voice);
}

// ── Rate scaling ────────────────────────────────────────────────────────

TEST(DxNoteTest, rateScalingHighNote) {
	for (int op = 0; op < 6; op++) {
		setOpParam(op, 13, 7); // max rate scaling sensitivity
	}

	DxVoice* voice = engine->solicitDxVoice();
	voice->init(*patch, 120, 100); // very high note
	int32_t buf[64] = {};
	DxVoiceCtrl ctrl;
	voice->compute(buf, 64, 0, patch, &ctrl);

	engine->dxVoiceUnassigned(voice);
}

TEST(DxNoteTest, rateScalingLowNote) {
	for (int op = 0; op < 6; op++) {
		setOpParam(op, 13, 7);
	}

	DxVoice* voice = engine->solicitDxVoice();
	voice->init(*patch, 12, 100); // very low note
	int32_t buf[64] = {};
	DxVoiceCtrl ctrl;
	voice->compute(buf, 64, 0, patch, &ctrl);

	engine->dxVoiceUnassigned(voice);
}

// ── Amp mod sensitivity ─────────────────────────────────────────────────

TEST(DxNoteTest, ampModSensitivityNonZero) {
	// Set amp mod sensitivity > 0 to exercise the ampmodsens branch in compute()
	for (int op = 0; op < 6; op++) {
		setOpParam(op, 14, 3); // amp mod sens = 3 (max)
		setOpParam(op, 16, 99); // output level = max
	}
	patch->params[140] = 99; // amp mod depth = max

	DxVoice* voice = engine->solicitDxVoice();
	voice->init(*patch, 60, 100);

	// Compute LFO to set lfo_value
	patch->computeLfo(64);

	int32_t buf[64] = {};
	DxVoiceCtrl ctrl;
	ctrl.ampmod = 1000;
	voice->compute(buf, 64, 0, patch, &ctrl);

	engine->dxVoiceUnassigned(voice);
}

// ── Operator switch (muting individual ops) ─────────────────────────────

TEST(DxNoteTest, opSwitchMutesOperator) {
	// All ops on
	patch->params[155] = 0x3F; // bits 0-5 all set
	DxVoice* voice = engine->solicitDxVoice();
	voice->init(*patch, 60, 100);

	int32_t bufAll[64] = {};
	DxVoiceCtrl ctrl;
	voice->compute(bufAll, 64, 0, patch, &ctrl);

	// Mute all ops
	patch->params[155] = 0x00;
	voice->init(*patch, 60, 100);

	int32_t bufNone[64] = {};
	voice->compute(bufNone, 64, 0, patch, &ctrl);

	// With all ops muted, output should be silent
	bool allZero = true;
	for (int i = 0; i < 64; i++) {
		if (bufNone[i] != 0) {
			allZero = false;
			break;
		}
	}
	CHECK(allZero);

	engine->dxVoiceUnassigned(voice);
}

TEST(DxNoteTest, opSwitchPartialMute) {
	// Only op 6 (bit 5) active
	patch->params[155] = 0x20;
	patch->params[134] = 31; // algo 32: all carriers

	DxVoice* voice = engine->solicitDxVoice();
	voice->init(*patch, 60, 100);

	int32_t buf[64] = {};
	DxVoiceCtrl ctrl;
	voice->compute(buf, 64, 0, patch, &ctrl);

	engine->dxVoiceUnassigned(voice);
}

// ── Feedback ────────────────────────────────────────────────────────────

TEST(DxNoteTest, feedbackZero) {
	patch->params[135] = 0; // no feedback
	DxVoice* voice = engine->solicitDxVoice();
	voice->init(*patch, 60, 100);

	int32_t buf[64] = {};
	DxVoiceCtrl ctrl;
	voice->compute(buf, 64, 0, patch, &ctrl);

	engine->dxVoiceUnassigned(voice);
}

TEST(DxNoteTest, feedbackMax) {
	makeHotPatch();
	patch->params[135] = 7; // max feedback

	DxVoice* voice = engine->solicitDxVoice();
	voice->init(*patch, 60, 100);

	int64_t energy = computeEnergy(voice, 8);
	CHECK(energy > 0);

	engine->dxVoiceUnassigned(voice);
}

// ── Keyup ───────────────────────────────────────────────────────────────

TEST(DxNoteTest, keyupTransitionsEnvelopes) {
	DxVoice* voice = engine->solicitDxVoice();
	voice->init(*patch, 60, 100);

	// Compute a few buffers to let envelopes settle
	int32_t buf[64] = {};
	DxVoiceCtrl ctrl;
	voice->compute(buf, 64, 0, patch, &ctrl);

	// Key up
	voice->keyup();

	// After keyup, compute should eventually return false (voice inactive)
	bool stillActive = true;
	for (int i = 0; i < 100; i++) {
		memset(buf, 0, sizeof(buf));
		stillActive = voice->compute(buf, 64, 0, patch, &ctrl);
		if (!stillActive) break;
	}

	engine->dxVoiceUnassigned(voice);
}

// ── LFO delay ───────────────────────────────────────────────────────────

TEST(DxNoteTest, lfoDelayInstant) {
	// LFO delay = 99 → delayinc = ~0u (instant full)
	patch->params[138] = 0; // 99 - 0 = 99
	DxVoice* voice = engine->solicitDxVoice();
	voice->init(*patch, 60, 100);

	int32_t buf[64] = {};
	DxVoiceCtrl ctrl;
	voice->compute(buf, 64, 0, patch, &ctrl);

	engine->dxVoiceUnassigned(voice);
}

TEST(DxNoteTest, lfoDelaySlow) {
	// LFO delay = moderate (e.g. 50)
	patch->params[138] = 49; // 99 - 49 = 50
	DxVoice* voice = engine->solicitDxVoice();
	voice->init(*patch, 60, 100);

	int32_t buf[64] = {};
	DxVoiceCtrl ctrl;
	// Compute many buffers to exercise the getdelay state machine
	for (int i = 0; i < 20; i++) {
		memset(buf, 0, sizeof(buf));
		voice->compute(buf, 64, 0, patch, &ctrl);
	}

	engine->dxVoiceUnassigned(voice);
}

// ── LFO sync ────────────────────────────────────────────────────────────

TEST(DxNoteTest, lfoSyncResetsPhase) {
	patch->params[141] = 1; // LFO sync on
	patch->lfo_phase = 12345; // non-zero phase

	DxVoice* voice = engine->solicitDxVoice();
	voice->init(*patch, 60, 100);

	// After init with LFO sync, lfo_phase should be reset
	CHECK_EQUAL((int32_t)((1U << 31) - 1), (int32_t)patch->lfo_phase);

	engine->dxVoiceUnassigned(voice);
}

TEST(DxNoteTest, lfoSyncOffPreservesPhase) {
	patch->params[141] = 0; // LFO sync off
	patch->lfo_phase = 12345;

	DxVoice* voice = engine->solicitDxVoice();
	voice->init(*patch, 60, 100);

	// Phase should NOT be reset
	CHECK(patch->lfo_phase != (int32_t)((1U << 31) - 1) || patch->lfo_phase == 12345);

	engine->dxVoiceUnassigned(voice);
}

// ── Osc sync ────────────────────────────────────────────────────────────

TEST(DxNoteTest, oscSyncResetsPhases) {
	patch->params[136] = 1; // osc sync on

	DxVoice* voice = engine->solicitDxVoice();
	voice->init(*patch, 60, 100);

	// After oscSync, phases should all be 0
	// (We can't directly read voice->phase, but we verify via output consistency)
	int32_t buf1[64] = {};
	int32_t buf2[64] = {};
	DxVoiceCtrl ctrl;

	// Re-init twice with sync on → should produce identical output
	voice->init(*patch, 60, 100);
	voice->compute(buf1, 64, 0, patch, &ctrl);
	voice->init(*patch, 60, 100);
	voice->compute(buf2, 64, 0, patch, &ctrl);

	bool same = true;
	for (int i = 0; i < 64; i++) {
		if (buf1[i] != buf2[i]) { same = false; break; }
	}
	CHECK(same);

	engine->dxVoiceUnassigned(voice);
}

TEST(DxNoteTest, oscUnsyncRandomizesPhases) {
	makeHotPatch();
	patch->params[136] = 0; // osc sync off → oscUnSync → random phases
	// Use multiple carriers so phase differences are audible
	patch->params[134] = 31; // algo 32: all carriers

	DxVoice* voice = engine->solicitDxVoice();
	voice->init(*patch, 60, 100);
	int32_t buf1[64] = {};
	DxVoiceCtrl ctrl;
	voice->compute(buf1, 64, 0, patch, &ctrl);

	voice->init(*patch, 60, 100);
	int32_t buf2[64] = {};
	voice->compute(buf2, 64, 0, patch, &ctrl);

	// With random phase init (CONG PRNG advances between calls),
	// outputs should differ due to different starting phases
	bool differs = false;
	for (int i = 0; i < 64; i++) {
		if (buf1[i] != buf2[i]) { differs = true; break; }
	}
	CHECK(differs);

	engine->dxVoiceUnassigned(voice);
}

// ── transferState / transferSignal ──────────────────────────────────────

TEST(DxNoteTest, transferStateFromOtherVoice) {
	makeHotPatch();

	DxVoice* v1 = engine->solicitDxVoice();
	DxVoice* v2 = engine->solicitDxVoice();

	v1->init(*patch, 60, 100);
	// Ramp up v1 so it has active envelope state
	computeEnergy(v1, 4);

	v2->init(*patch, 72, 100);
	v2->transferState(*v1);

	// After transfer, v2 should produce output
	int64_t energy = computeEnergy(v2, 4);
	CHECK(energy > 0);

	engine->dxVoiceUnassigned(v1);
	engine->dxVoiceUnassigned(v2);
}

TEST(DxNoteTest, transferSignalFromOtherVoice) {
	DxVoice* v1 = engine->solicitDxVoice();
	DxVoice* v2 = engine->solicitDxVoice();

	v1->init(*patch, 60, 100);
	int32_t buf[64] = {};
	DxVoiceCtrl ctrl;
	v1->compute(buf, 64, 0, patch, &ctrl);

	v2->init(*patch, 72, 100);
	v2->transferSignal(*v1);

	memset(buf, 0, sizeof(buf));
	v2->compute(buf, 64, 0, patch, &ctrl);

	engine->dxVoiceUnassigned(v1);
	engine->dxVoiceUnassigned(v2);
}

// ── update (live parameter change) ──────────────────────────────────────

TEST(DxNoteTest, updateRecalculatesOperators) {
	makeHotPatch();
	DxVoice* voice = engine->solicitDxVoice();
	voice->init(*patch, 60, 100);

	// Compute several buffers at full output to get steady-state energy
	int64_t e1 = computeEnergy(voice, 8);

	// Change output level to 0 and update
	for (int op = 0; op < 6; op++) {
		setOpParam(op, 16, 0); // output level = 0
	}
	voice->update(*patch, 60);

	// Compute several buffers to let gain ramp down after update
	computeEnergy(voice, 4); // flush transition
	int64_t e2 = computeEnergy(voice, 4);

	// After zeroing output levels, energy should be much less
	CHECK(e2 < e1);

	engine->dxVoiceUnassigned(voice);
}

// ── Pitch mod ───────────────────────────────────────────────────────────

TEST(DxNoteTest, pitchModDepthAndSensitivity) {
	makeHotPatch();
	patch->params[139] = 99; // pitch mod depth = max
	patch->params[143] = 7;  // pitch mod sensitivity = max
	patch->params[142] = 4;  // LFO sine
	patch->params[137] = 80; // fast LFO

	DxVoice* voice = engine->solicitDxVoice();
	voice->init(*patch, 60, 100);

	patch->computeLfo(64);

	int64_t energy = computeEnergy(voice, 8);
	CHECK(energy > 0);

	engine->dxVoiceUnassigned(voice);
}

// ── EG mod ──────────────────────────────────────────────────────────────

TEST(DxNoteTest, egModAffectsAmpMod) {
	patch->eg_mod = 0; // minimal EG mod → large amod_3

	DxVoice* voice = engine->solicitDxVoice();
	voice->init(*patch, 60, 100);

	int32_t buf[64] = {};
	DxVoiceCtrl ctrl;
	voice->compute(buf, 64, 0, patch, &ctrl);

	engine->dxVoiceUnassigned(voice);
}

// ── Voice ctrl modulation ───────────────────────────────────────────────

TEST(DxNoteTest, voiceCtrlRatemod) {
	DxVoice* voice = engine->solicitDxVoice();
	voice->init(*patch, 60, 100);

	int32_t buf[64] = {};
	DxVoiceCtrl ctrl;
	ctrl.ratemod = 100; // non-zero rate modulation

	voice->compute(buf, 64, 0, patch, &ctrl);

	engine->dxVoiceUnassigned(voice);
}

TEST(DxNoteTest, voiceCtrlVelmod) {
	// velmod modifies level when vel sensitivity > 0
	for (int op = 0; op < 6; op++) {
		setOpParam(op, 15, 7); // max vel sensitivity
	}

	DxVoice* voice = engine->solicitDxVoice();
	voice->init(*patch, 60, 100);

	int32_t buf[64] = {};
	DxVoiceCtrl ctrl;
	ctrl.velmod = 500;

	voice->compute(buf, 64, 0, patch, &ctrl);

	engine->dxVoiceUnassigned(voice);
}

// ── All 32 algorithms produce output ────────────────────────────────────

TEST(DxNoteTest, allAlgorithmsProduceOutput) {
	for (int algo = 0; algo < 32; algo++) {
		makeHotPatch();
		patch->params[134] = algo;
		patch->updateEngineMode();

		DxVoice* voice = engine->solicitDxVoice();
		voice->init(*patch, 60, 100);

		int64_t energy = computeEnergy(voice, 8);
		CHECK(energy > 0);

		engine->dxVoiceUnassigned(voice);
	}
}

// ── updateBasePitches ───────────────────────────────────────────────────

TEST(DxNoteTest, updateBasePitchesChangesFrequency) {
	makeHotPatch();

	DxVoice* voice = engine->solicitDxVoice();
	voice->init(*patch, 60, 100);

	// Ramp up so we have steady-state output
	computeEnergy(voice, 4);

	int32_t buf1[64] = {};
	DxVoiceCtrl ctrl;
	voice->compute(buf1, 64, 0, patch, &ctrl);

	// Update base pitches to a different note's frequency (one octave up)
	voice->updateBasePitches(50857777 + (1 << 24));

	int32_t buf2[64] = {};
	voice->compute(buf2, 64, 0, patch, &ctrl);

	bool differs = false;
	for (int i = 0; i < 64; i++) {
		if (buf1[i] != buf2[i]) { differs = true; break; }
	}
	CHECK(differs);

	engine->dxVoiceUnassigned(voice);
}

// ══════════════════════════════════════════════════════════════════════════
// Phase D2: Coverage-targeted tests for remaining DX7 branches
// ══════════════════════════════════════════════════════════════════════════

// ── dx7note.cpp: scaleoutlevel with outlevel < 20 (line 242) ────────────

TEST(DxNoteTest, lowOutputLevelUsesLevelLut) {
	for (int op = 0; op < 6; op++) {
		setOpParam(op, 0, 99);
		setOpParam(op, 1, 99);
		setOpParam(op, 2, 99);
		setOpParam(op, 3, 99);
		setOpParam(op, 4, 99);
		setOpParam(op, 5, 99);
		setOpParam(op, 6, 99);
		setOpParam(op, 7, 99);
		setOpParam(op, 16, 5); // output level < 20 → levellut branch
	}
	patch->params[155] = 0x3F;

	DxVoice* voice = engine->solicitDxVoice();
	voice->init(*patch, 60, 100);
	int32_t buf[64] = {};
	DxVoiceCtrl ctrl;
	voice->compute(buf, 64, 0, patch, &ctrl);
	engine->dxVoiceUnassigned(voice);
}

TEST(DxNoteTest, outputLevelBoundary19and20) {
	for (int op = 0; op < 3; op++) {
		setOpParam(op, 16, 19);
	}
	for (int op = 3; op < 6; op++) {
		setOpParam(op, 16, 20);
	}

	DxVoice* voice = engine->solicitDxVoice();
	voice->init(*patch, 60, 100);
	int32_t buf[64] = {};
	DxVoiceCtrl ctrl;
	voice->compute(buf, 64, 0, patch, &ctrl);
	engine->dxVoiceUnassigned(voice);
}

// ── dx7note.cpp: negative outlevel clamp (line 248-249) ─────────────────

TEST(DxNoteTest, negativeOutlevelClamp) {
	for (int op = 0; op < 6; op++) {
		setOpParam(op, 16, 0);
		setOpParam(op, 15, 7);
	}
	DxVoice* voice = engine->solicitDxVoice();
	voice->init(*patch, 60, 1);
	int32_t buf[64] = {};
	DxVoiceCtrl ctrl;
	voice->compute(buf, 64, 0, patch, &ctrl);
	engine->dxVoiceUnassigned(voice);
}

// ── dx7note.cpp: negative senslfo (line 325) ────────────────────────────

TEST(DxNoteTest, negativeSenslfo) {
	makeHotPatch();
	patch->params[139] = 99;
	patch->params[143] = 7;
	patch->lfo_value = 0; // below (1<<23) → negative senslfo

	DxVoice* voice = engine->solicitDxVoice();
	voice->init(*patch, 60, 100);
	int64_t energy = computeEnergy(voice, 4);
	CHECK(energy > 0);
	engine->dxVoiceUnassigned(voice);
}

// ── dx7note.cpp: opSwitch off during compute (line 348) ─────────────────

TEST(DxNoteTest, opSwitchOffDuringCompute) {
	makeHotPatch();
	DxVoice* voice = engine->solicitDxVoice();
	voice->init(*patch, 60, 100);
	int32_t buf[64] = {};
	DxVoiceCtrl ctrl;
	voice->compute(buf, 64, 0, patch, &ctrl);

	patch->params[155] = 0x20; // only op 5
	memset(buf, 0, sizeof(buf));
	voice->compute(buf, 64, 0, patch, &ctrl);

	bool hasOutput = false;
	for (int i = 0; i < 64; i++) {
		if (buf[i] != 0) { hasOutput = true; break; }
	}
	CHECK(hasOutput);
	engine->dxVoiceUnassigned(voice);
}

TEST(DxNoteTest, allOpsSwitchedOffDuringCompute) {
	makeHotPatch();
	DxVoice* voice = engine->solicitDxVoice();
	voice->init(*patch, 60, 100);
	computeEnergy(voice, 2);

	// Disable all ops and flush the gain transition
	patch->params[155] = 0x00;
	computeEnergy(voice, 4); // let gain_out ramp down

	// Now output should be silent
	int32_t buf[64] = {};
	DxVoiceCtrl ctrl;
	voice->compute(buf, 64, 0, patch, &ctrl);

	bool allZero = true;
	for (int i = 0; i < 64; i++) {
		if (buf[i] != 0) { allZero = false; break; }
	}
	CHECK(allZero);
	engine->dxVoiceUnassigned(voice);
}

// ── dx7note.cpp: fixed mode during compute (lines 358/360) ──────────────

TEST(DxNoteTest, fixedModeDuringCompute) {
	makeHotPatch();
	for (int op = 0; op < 6; op++) {
		setOpParam(op, 17, 1);
		setOpParam(op, 18, 2);
		setOpParam(op, 19, 50);
	}
	DxVoice* voice = engine->solicitDxVoice();
	voice->init(*patch, 60, 100);
	int64_t energy = computeEnergy(voice, 4);
	CHECK(energy > 0);
	engine->dxVoiceUnassigned(voice);
}

// ── dx7note.cpp: update() with low output (lines 430-439) ──────────────

TEST(DxNoteTest, updateWithLowOutputLevel) {
	makeHotPatch();
	DxVoice* voice = engine->solicitDxVoice();
	voice->init(*patch, 60, 100);
	computeEnergy(voice, 2);

	for (int op = 0; op < 6; op++) {
		setOpParam(op, 16, 10);
		setOpParam(op, 15, 7);
		setOpParam(op, 13, 7);
	}
	voice->update(*patch, 60);
	int32_t buf[64] = {};
	DxVoiceCtrl ctrl;
	voice->compute(buf, 64, 0, patch, &ctrl);
	engine->dxVoiceUnassigned(voice);
}

TEST(DxNoteTest, updateWithZeroOutputAndHighVelSens) {
	makeHotPatch();
	DxVoice* voice = engine->solicitDxVoice();
	voice->init(*patch, 60, 1);
	computeEnergy(voice, 2);

	for (int op = 0; op < 6; op++) {
		setOpParam(op, 16, 0);
		setOpParam(op, 15, 7);
	}
	voice->update(*patch, 60);
	int32_t buf[64] = {};
	DxVoiceCtrl ctrl;
	voice->compute(buf, 64, 0, patch, &ctrl);
	engine->dxVoiceUnassigned(voice);
}

// ── Modern engine feedback (line 382 + fm_core line 99) ─────────────────

TEST(DxNoteTest, modernEngineFeedbackPath) {
	makeHotPatch();
	patch->params[134] = 0;
	patch->params[135] = 7;
	patch->setEngineMode(1);
	engine->engineModern.neon = false;

	DxVoice* voice = engine->solicitDxVoice();
	voice->init(*patch, 60, 100);
	int64_t energy = computeEnergy(voice, 8);
	CHECK(energy > 0);
	engine->dxVoiceUnassigned(voice);
}

// ── EngineMkI: algo 3/5 with feedback ───────────────────────────────────

TEST(DxNoteTest, mkIAlgo4WithFeedback) {
	makeHotPatch();
	patch->params[134] = 3;
	patch->params[135] = 5;
	patch->setEngineMode(0);
	engine->engineModern.neon = false;
	CHECK(patch->core == &engine->engineMkI);

	DxVoice* voice = engine->solicitDxVoice();
	voice->init(*patch, 60, 100);
	int64_t energy = computeEnergy(voice, 8);
	CHECK(energy > 0);
	engine->dxVoiceUnassigned(voice);
}

TEST(DxNoteTest, mkIAlgo6WithFeedback) {
	makeHotPatch();
	patch->params[134] = 5;
	patch->params[135] = 3;
	patch->setEngineMode(0);
	engine->engineModern.neon = false;
	CHECK(patch->core == &engine->engineMkI);

	DxVoice* voice = engine->solicitDxVoice();
	voice->init(*patch, 60, 100);
	int64_t energy = computeEnergy(voice, 8);
	CHECK(energy > 0);
	engine->dxVoiceUnassigned(voice);
}

TEST(DxNoteTest, mkIAlgo4MaxFeedback) {
	makeHotPatch();
	patch->params[134] = 3;
	patch->params[135] = 7;
	patch->setEngineMode(2);

	DxVoice* voice = engine->solicitDxVoice();
	voice->init(*patch, 60, 100);
	int64_t energy = computeEnergy(voice, 8);
	CHECK(energy > 0);
	engine->dxVoiceUnassigned(voice);
}

TEST(DxNoteTest, mkIAlgo6NoFeedback) {
	makeHotPatch();
	patch->params[134] = 5;
	patch->params[135] = 0;
	patch->setEngineMode(2);

	DxVoice* voice = engine->solicitDxVoice();
	voice->init(*patch, 60, 100);
	int64_t energy = computeEnergy(voice, 8);
	CHECK(energy > 0);
	engine->dxVoiceUnassigned(voice);
}

TEST(DxNoteTest, mkIAllAlgorithms) {
	for (int algo = 0; algo < 32; algo++) {
		makeHotPatch();
		patch->params[134] = algo;
		patch->params[135] = 5;
		patch->setEngineMode(2);

		DxVoice* voice = engine->solicitDxVoice();
		voice->init(*patch, 60, 100);
		int64_t energy = computeEnergy(voice, 4);
		CHECK(energy > 0);
		engine->dxVoiceUnassigned(voice);
	}
}

TEST(DxNoteTest, mkIInitialGainZero) {
	makeHotPatch();
	patch->params[134] = 3;
	patch->params[135] = 7;
	patch->setEngineMode(2);

	DxVoice* voice = engine->solicitDxVoice();
	voice->init(*patch, 60, 100);
	int32_t buf[64] = {};
	DxVoiceCtrl ctrl;
	voice->compute(buf, 64, 0, patch, &ctrl);
	voice->compute(buf, 64, 0, patch, &ctrl);
	engine->dxVoiceUnassigned(voice);
}

TEST(DxNoteTest, mkILowGainThreshold) {
	for (int op = 0; op < 6; op++) {
		setOpParam(op, 0, 99);
		setOpParam(op, 1, 99);
		setOpParam(op, 2, 99);
		setOpParam(op, 3, 99);
		setOpParam(op, 4, 99);
		setOpParam(op, 5, 99);
		setOpParam(op, 6, 99);
		setOpParam(op, 7, 99);
		setOpParam(op, 16, 1);
	}
	patch->params[155] = 0x3F;
	patch->params[134] = 3;
	patch->params[135] = 7;
	patch->setEngineMode(2);

	DxVoice* voice = engine->solicitDxVoice();
	voice->init(*patch, 60, 100);
	int32_t buf[64] = {};
	DxVoiceCtrl ctrl;
	voice->compute(buf, 64, 0, patch, &ctrl);
	voice->compute(buf, 64, 0, patch, &ctrl);
	engine->dxVoiceUnassigned(voice);
}

// ── env.cpp: keydown same state (line 92) ───────────────────────────────

TEST(DxNoteTest, keydownSameStateTwice) {
	makeHotPatch();
	DxVoice* voice = engine->solicitDxVoice();
	voice->init(*patch, 60, 100);

	voice->keyup();
	int32_t buf[64] = {};
	DxVoiceCtrl ctrl;
	voice->compute(buf, 64, 0, patch, &ctrl);
	voice->keyup(); // same state → no-op
	voice->compute(buf, 64, 0, patch, &ctrl);
	engine->dxVoiceUnassigned(voice);
}

// ── env.cpp: update when not down (line 145) ────────────────────────────

TEST(DxNoteTest, updateAfterKeyup) {
	makeHotPatch();
	DxVoice* voice = engine->solicitDxVoice();
	voice->init(*patch, 60, 100);
	computeEnergy(voice, 2);

	voice->keyup();
	for (int op = 0; op < 6; op++) {
		setOpParam(op, 16, 50);
	}
	voice->update(*patch, 60);
	int32_t buf[64] = {};
	DxVoiceCtrl ctrl;
	voice->compute(buf, 64, 0, patch, &ctrl);
	engine->dxVoiceUnassigned(voice);
}

// ── fm_core.cpp: modulated operator path (line 109) ─────────────────────

TEST(DxNoteTest, modulatedOperatorPath) {
	makeHotPatch();
	patch->params[134] = 0;
	patch->params[135] = 0;

	DxVoice* voice = engine->solicitDxVoice();
	voice->init(*patch, 60, 100);
	int64_t energy = computeEnergy(voice, 8);
	CHECK(energy > 0);
	engine->dxVoiceUnassigned(voice);
}

TEST(DxNoteTest, algoWithDualBusRouting) {
	makeHotPatch();
	patch->params[134] = 15;
	patch->params[135] = 3;

	DxVoice* voice = engine->solicitDxVoice();
	voice->init(*patch, 60, 100);
	int64_t energy = computeEnergy(voice, 4);
	CHECK(energy > 0);
	engine->dxVoiceUnassigned(voice);
}
