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
