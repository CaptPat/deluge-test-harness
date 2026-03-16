// Regression tests for CC64/CC66/CC67 sustain pedal behavior.
// Tests the real Voice::noteOff() sustain/sostenuto logic.
//
// CC64 sustain: noteOff deferred while pedal held, released on pedal up
// CC66 sostenuto: only holds notes captured when pedal went down
// CC67 soft pedal: velocity reduction (tested in note_lifecycle_test.cpp)

#include "CppUTest/TestHarness.h"
#include "model/model_stack.h"
#include "model/voice/voice.h"
#include "modulation/params/param.h"
#include "modulation/params/param_manager.h"
#include "modulation/params/param_set.h"
#include "processing/engines/audio_engine.h"
#include "processing/sound/sound_instrument.h"

namespace params = deluge::modulation::params;

namespace {

// Heap-allocated fixture (same pattern as note_lifecycle_test)
struct PedalTestFixture {
	ParamCollectionSummary* summaries;
	PatchedParamSet* patchedParamSet;
	UnpatchedParamSet* unpatchedParamSet;
	ParamManagerForTimeline* paramManager;
	SoundInstrument* si;
	char* modelStackMemory;

	PedalTestFixture() {
		summaries = new ParamCollectionSummary[PARAM_COLLECTIONS_STORAGE_NUM]{};
		patchedParamSet = new PatchedParamSet{&summaries[1]};
		unpatchedParamSet = new UnpatchedParamSet{&summaries[0]};
		paramManager = new ParamManagerForTimeline;
		si = new SoundInstrument;
		modelStackMemory = new char[1024]{};

		paramManager->summaries[0].paramCollection = unpatchedParamSet;
		paramManager->summaries[1].paramCollection = patchedParamSet;
		unpatchedParamSet->kind = params::Kind::UNPATCHED_SOUND;
		patchedParamSet->params[params::LOCAL_OSC_A_VOLUME].setCurrentValueBasicForSetup(2147483647);

		// All pedals off by default
		unpatchedParamSet->params[params::UNPATCHED_SUSTAIN_PEDAL].setCurrentValueBasicForSetup(-2147483648);
		unpatchedParamSet->params[params::UNPATCHED_SOSTENUTO_PEDAL].setCurrentValueBasicForSetup(-2147483648);
		unpatchedParamSet->params[params::UNPATCHED_SOFT_PEDAL].setCurrentValueBasicForSetup(-2147483648);
		unpatchedParamSet->params[params::UNPATCHED_PORTAMENTO].setCurrentValueBasicForSetup(-2147483648);
	}

	~PedalTestFixture() {
		si->killAllVoices();
		// Leak si to avoid MinGW GMA destruction crash
	}

	ModelStackWithSoundFlags* getSoundFlagsModelStack() {
		auto* ms = (ModelStackWithSoundFlags*)modelStackMemory;
		ms->song = nullptr;
		ms->setTimelineCounter(nullptr);
		ms->setNoteRow(nullptr);
		ms->noteRowId = 0;
		ms->modControllable = si;
		ms->paramManager = paramManager;
		for (int i = 0; i < NUM_SOUND_FLAGS; i++) {
			ms->soundFlags[i] = FLAG_TBD;
		}
		return ms;
	}

	ModelStackWithThreeMainThings* getModelStack() {
		auto* ms = (ModelStackWithThreeMainThings*)modelStackMemory;
		ms->song = nullptr;
		ms->setTimelineCounter(nullptr);
		ms->setNoteRow(nullptr);
		ms->noteRowId = 0;
		ms->modControllable = si;
		ms->paramManager = paramManager;
		return ms;
	}

	// Create a voice via noteOn
	void playNote(int32_t note = 60, int32_t velocity = 100) {
		int16_t mpe[3] = {0, 0, 0};
		si->noteOn(getModelStack(), si->getArp(), note, mpe, 0, 0, 0, velocity, 16);
	}

	void setSustainPedal(bool on) {
		unpatchedParamSet->params[params::UNPATCHED_SUSTAIN_PEDAL].setCurrentValueBasicForSetup(
		    on ? 2147483647 : -2147483648);
	}

	void setSostenutoPedal(bool on) {
		unpatchedParamSet->params[params::UNPATCHED_SOSTENUTO_PEDAL].setCurrentValueBasicForSetup(
		    on ? 2147483647 : -2147483648);
	}

	int16_t defaultMPE[3] = {0, 0, 0};
};

} // namespace

TEST_GROUP(SustainPedal){};

// ── CC64 Sustain Pedal ─────────────────────────────────────────────────

TEST(SustainPedal, NoteOffWithoutSustainReleases) {
	PedalTestFixture f;
	f.playNote(60);
	CHECK(f.si->numActiveVoices() > 0);

	auto& voice = f.si->voices().front();
	CHECK(Voice::PedalState::None == voice->pedalState);

	// noteOff without sustain pedal — voice should enter release
	voice->noteOff(f.getSoundFlagsModelStack(), true, false);
	CHECK(EnvelopeStage::RELEASE == voice->envelopes[0].state);
	CHECK(Voice::PedalState::None == voice->pedalState);
}

TEST(SustainPedal, NoteOffWithSustainDefers) {
	PedalTestFixture f;
	f.playNote(60);
	f.setSustainPedal(true);

	auto& voice = f.si->voices().front();

	// noteOff with sustain held — should defer, not release
	voice->noteOff(f.getSoundFlagsModelStack(), true, false);
	CHECK((voice->pedalState & Voice::PedalState::SustainDeferred) != Voice::PedalState::None);
	// Envelope should NOT be in release (note-off was deferred)
	CHECK(EnvelopeStage::RELEASE != voice->envelopes[0].state);
}

TEST(SustainPedal, DeferredNoteReleasesWhenPedalLifted) {
	PedalTestFixture f;
	f.playNote(60);
	f.setSustainPedal(true);

	auto& voice = f.si->voices().front();
	voice->noteOff(f.getSoundFlagsModelStack(), true, false);
	CHECK((voice->pedalState & Voice::PedalState::SustainDeferred) != Voice::PedalState::None);

	// Lift sustain pedal and send noteOff again
	f.setSustainPedal(false);
	voice->noteOff(f.getSoundFlagsModelStack(), true, false);

	// Now it should release
	CHECK(EnvelopeStage::RELEASE == voice->envelopes[0].state);
	CHECK(Voice::PedalState::None == (voice->pedalState & Voice::PedalState::SustainDeferred));
}

TEST(SustainPedal, IgnoreSustainBypassesPedal) {
	PedalTestFixture f;
	f.playNote(60);
	f.setSustainPedal(true);

	auto& voice = f.si->voices().front();

	// ignoreSustain=true should bypass the pedal (used by allNotesOff/playback stop)
	voice->noteOff(f.getSoundFlagsModelStack(), true, true);
	CHECK(EnvelopeStage::RELEASE == voice->envelopes[0].state);
	CHECK(Voice::PedalState::None == voice->pedalState);
}

// ── CC66 Sostenuto Pedal ───────────────────────────────────────────────

TEST(SustainPedal, SostenutoOnlyCapturedNotesHeld) {
	PedalTestFixture f;
	f.playNote(60);

	auto& voice = f.si->voices().front();

	// Sostenuto pedal down — but note was NOT captured (no SostenutoCapture flag)
	f.setSostenutoPedal(true);
	voice->noteOff(f.getSoundFlagsModelStack(), true, false);

	// Without SostenutoCapture, sostenuto doesn't defer
	CHECK(EnvelopeStage::RELEASE == voice->envelopes[0].state);
}

TEST(SustainPedal, SostenutoCapturedNoteDefers) {
	PedalTestFixture f;
	f.playNote(60);

	auto& voice = f.si->voices().front();

	// Simulate sostenuto capture (pedal went down while note was playing)
	voice->pedalState = voice->pedalState | Voice::PedalState::SostenutoCapture;
	f.setSostenutoPedal(true);

	voice->noteOff(f.getSoundFlagsModelStack(), true, false);

	// Captured note should be deferred by sostenuto
	CHECK((voice->pedalState & Voice::PedalState::SostenutoDeferred) != Voice::PedalState::None);
	CHECK(EnvelopeStage::RELEASE != voice->envelopes[0].state);
}

// ── CC67 Soft Pedal velocity reduction ─────────────────────────────────

TEST(SustainPedal, SoftPedalReducesVelocity) {
	PedalTestFixture f;

	// Play a note WITHOUT soft pedal — check stored velocity
	f.playNote(60, 127);
	auto& voiceNormal = f.si->voices().front();
	int32_t normalVelocitySource = voiceNormal->sourceValues[util::to_underlying(PatchSource::VELOCITY)];

	f.si->killAllVoices();

	// Play same note WITH soft pedal — velocity should be reduced
	f.unpatchedParamSet->params[params::UNPATCHED_SOFT_PEDAL].setCurrentValueBasicForSetup(2147483647);
	f.playNote(60, 127);
	auto& voiceSoft = f.si->voices().front();
	int32_t softVelocitySource = voiceSoft->sourceValues[util::to_underlying(PatchSource::VELOCITY)];

	// Soft pedal reduces velocity to ~2/3: velocity = max((127 * 2 + 1) / 3, 1) = 85
	// Normal: (127 - 64) * 33554432 = 2113929216
	// Soft:   (85 - 64) * 33554432 = 704643072
	CHECK(softVelocitySource < normalVelocitySource);

	// Verify the exact math: (127 * 2 + 1) / 3 = 85
	int32_t expectedSoftVelocity = 85;
	int32_t expectedSoftSource = ((int32_t)expectedSoftVelocity - 64) * 33554432;
	CHECK_EQUAL(expectedSoftSource, softVelocitySource);
}

TEST(SustainPedal, SoftPedalMinVelocityClamps) {
	PedalTestFixture f;

	// Soft pedal with velocity 1 — should clamp to 1, not 0
	f.unpatchedParamSet->params[params::UNPATCHED_SOFT_PEDAL].setCurrentValueBasicForSetup(2147483647);
	f.playNote(60, 1);
	auto& voice = f.si->voices().front();
	int32_t velSource = voice->sourceValues[util::to_underlying(PatchSource::VELOCITY)];
	// (1 * 2 + 1) / 3 = 1 → clamped to max(1, 1) = 1
	int32_t expectedSource = ((int32_t)1 - 64) * 33554432;
	CHECK_EQUAL(expectedSource, velSource);
}

TEST(SustainPedal, SoftPedalOffNoReduction) {
	PedalTestFixture f;

	// Soft pedal OFF (default) — velocity unchanged
	f.playNote(60, 100);
	auto& voice = f.si->voices().front();
	int32_t velSource = voice->sourceValues[util::to_underlying(PatchSource::VELOCITY)];
	int32_t expectedSource = ((int32_t)100 - 64) * 33554432;
	CHECK_EQUAL(expectedSource, velSource);
}

// ── CC66 additional sostenuto tests ────────────────────────────────────

TEST(SustainPedal, SostenutoDoesNotCaptureNewNotes) {
	PedalTestFixture f;

	// Play note A, press sostenuto (captures A)
	f.playNote(60);
	f.setSostenutoPedal(true);
	for (const auto& v : f.si->voices()) {
		if (v->envelopes[0].state < EnvelopeStage::RELEASE) {
			v->pedalState = v->pedalState | Voice::PedalState::SostenutoCapture;
		}
	}

	// Play note B AFTER sostenuto is down — should NOT be captured
	f.playNote(67);
	CHECK(f.si->numActiveVoices() >= 2);

	// The second voice should NOT have SostenutoCapture
	bool secondHasCapture = false;
	int count = 0;
	for (const auto& v : f.si->voices()) {
		if (count > 0) { // skip first voice
			if ((v->pedalState & Voice::PedalState::SostenutoCapture) != Voice::PedalState::None) {
				secondHasCapture = true;
			}
		}
		count++;
	}
	CHECK(!secondHasCapture);
}

// ── Edge cases ─────────────────────────────────────────────────────────

TEST(SustainPedal, SustainThenSostenutoInteraction) {
	PedalTestFixture f;
	f.playNote(60);

	auto& voice = f.si->voices().front();

	// Both pedals on, note captured by sostenuto
	f.setSustainPedal(true);
	f.setSostenutoPedal(true);
	voice->pedalState = voice->pedalState | Voice::PedalState::SostenutoCapture;

	// noteOff — sustain takes priority (checked first)
	voice->noteOff(f.getSoundFlagsModelStack(), true, false);
	CHECK((voice->pedalState & Voice::PedalState::SustainDeferred) != Voice::PedalState::None);
}

// ── End-to-end: noteOn → sustain hold → noteOff → pedal lift → release ──

TEST(SustainPedal, EndToEndSustainCycle) {
	PedalTestFixture f;

	// 1. Play a note
	f.playNote(60);
	CHECK(f.si->numActiveVoices() > 0);

	// 2. Press sustain pedal
	f.setSustainPedal(true);

	// 3. Release key (noteOff) — voice should be deferred
	auto& voice = f.si->voices().front();
	voice->noteOff(f.getSoundFlagsModelStack(), true, false);
	CHECK((voice->pedalState & Voice::PedalState::SustainDeferred) != Voice::PedalState::None);
	CHECK(EnvelopeStage::RELEASE != voice->envelopes[0].state);

	// 4. Lift sustain pedal
	f.setSustainPedal(false);

	// 5. Simulate releaseSustainedVoices — iterate voices and release deferred ones
	//    (calling the real method needs Song/ParamManager plumbing; we test the
	//    equivalent logic directly, which is what the method does)
	for (const auto& v : f.si->voices()) {
		if ((v->pedalState & Voice::PedalState::SustainDeferred) != Voice::PedalState::None) {
			v->noteOff(f.getSoundFlagsModelStack(), true, true); // ignoreSustain=true
		}
	}

	// 6. Voice should now be in release
	CHECK(EnvelopeStage::RELEASE == voice->envelopes[0].state);
	CHECK(Voice::PedalState::None == voice->pedalState);
}

TEST(SustainPedal, EndToEndMultipleNotesSustained) {
	PedalTestFixture f;

	// Play 3 notes
	f.playNote(60);
	f.playNote(64);
	f.playNote(67);
	CHECK(f.si->numActiveVoices() >= 3);

	// Press sustain
	f.setSustainPedal(true);

	// Release all keys
	for (const auto& v : f.si->voices()) {
		v->noteOff(f.getSoundFlagsModelStack(), true, false);
	}

	// All should be deferred
	for (const auto& v : f.si->voices()) {
		CHECK((v->pedalState & Voice::PedalState::SustainDeferred) != Voice::PedalState::None);
	}

	// Lift sustain — release all deferred
	f.setSustainPedal(false);
	for (const auto& v : f.si->voices()) {
		if ((v->pedalState & Voice::PedalState::SustainDeferred) != Voice::PedalState::None) {
			v->noteOff(f.getSoundFlagsModelStack(), true, true);
		}
	}

	// All should be in release
	for (const auto& v : f.si->voices()) {
		CHECK(EnvelopeStage::RELEASE == v->envelopes[0].state);
	}
}

TEST(SustainPedal, EndToEndSostenutoCaptureCycle) {
	PedalTestFixture f;

	// Play a note
	f.playNote(60);
	auto& voice = f.si->voices().front();

	// Press sostenuto pedal — captures the held note
	f.setSostenutoPedal(true);
	voice->pedalState = voice->pedalState | Voice::PedalState::SostenutoCapture;

	// Release key — should be deferred by sostenuto
	voice->noteOff(f.getSoundFlagsModelStack(), true, false);
	CHECK((voice->pedalState & Voice::PedalState::SostenutoDeferred) != Voice::PedalState::None);

	// Lift sostenuto pedal — release deferred
	f.setSostenutoPedal(false);
	// Clear capture+deferred flags and release (what releaseSostenutoVoices does)
	voice->pedalState = voice->pedalState & ~(Voice::PedalState::SostenutoCapture | Voice::PedalState::SostenutoDeferred);
	voice->noteOff(f.getSoundFlagsModelStack(), true, true);

	CHECK(EnvelopeStage::RELEASE == voice->envelopes[0].state);
}

TEST(SustainPedal, MultipleDeferredNoteOffsIdempotent) {
	PedalTestFixture f;
	f.playNote(60);
	f.setSustainPedal(true);

	auto& voice = f.si->voices().front();

	// Multiple noteOff calls while sustained — should be idempotent
	voice->noteOff(f.getSoundFlagsModelStack(), true, false);
	voice->noteOff(f.getSoundFlagsModelStack(), true, false);
	voice->noteOff(f.getSoundFlagsModelStack(), true, false);

	CHECK((voice->pedalState & Voice::PedalState::SustainDeferred) != Voice::PedalState::None);
	CHECK(EnvelopeStage::RELEASE != voice->envelopes[0].state);
}
