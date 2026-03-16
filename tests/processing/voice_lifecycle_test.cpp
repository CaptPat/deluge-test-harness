// Voice lifecycle stress tests: polyphony, voice limits, mono mode, rapid cycling.
// Exercises Sound::noteOnPostArpeggiator voice allocation and cleanup paths.

#include "CppUTest/TestHarness.h"
#include "model/model_stack.h"
#include "model/voice/voice.h"
#include "modulation/params/param_manager.h"
#include "modulation/params/param_set.h"
#include "processing/engines/audio_engine.h"
#include "processing/sound/sound_instrument.h"

namespace params = deluge::modulation::params;

namespace {

struct VoiceLifecycleFixture {
	ParamCollectionSummary* summaries;
	PatchedParamSet* patchedParamSet;
	UnpatchedParamSet* unpatchedParamSet;
	ParamManagerForTimeline* paramManager;
	SoundInstrument* si;
	char* modelStackMemory;

	VoiceLifecycleFixture() {
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

		// All pedals off
		unpatchedParamSet->params[params::UNPATCHED_SUSTAIN_PEDAL].setCurrentValueBasicForSetup(-2147483648);
		unpatchedParamSet->params[params::UNPATCHED_SOSTENUTO_PEDAL].setCurrentValueBasicForSetup(-2147483648);
		unpatchedParamSet->params[params::UNPATCHED_SOFT_PEDAL].setCurrentValueBasicForSetup(-2147483648);
		unpatchedParamSet->params[params::UNPATCHED_PORTAMENTO].setCurrentValueBasicForSetup(-2147483648);
	}

	~VoiceLifecycleFixture() {
		si->killAllVoices();
		std::erase(AudioEngine::sounds, static_cast<Sound*>(si));
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

	void playNote(int32_t note, int32_t velocity = 100) {
		int16_t mpe[3] = {0, 0, 0};
		si->noteOn(getModelStack(), si->getArp(), note, mpe, 0, 0, 0, velocity, 16);
	}

	void releaseNote(int32_t note) {
		si->noteOff(getModelStack(), si->getArp(), note);
	}
};

} // namespace

TEST_GROUP(VoiceLifecycle){};

// ── Polyphonic mode (default) ───────────────────────────────────────────

TEST(VoiceLifecycle, PolyModeAllowsMultipleVoices) {
	VoiceLifecycleFixture f;
	CHECK(f.si->polyphonic == PolyphonyMode::POLY);

	f.playNote(60);
	f.playNote(64);
	f.playNote(67);
	CHECK_EQUAL(3, (int)f.si->numActiveVoices());
}

TEST(VoiceLifecycle, PolyModeEachVoiceHasCorrectNote) {
	VoiceLifecycleFixture f;
	f.playNote(60);
	f.playNote(64);
	f.playNote(67);

	int notes[3] = {0, 0, 0};
	int i = 0;
	for (const auto& v : f.si->voices()) {
		if (i < 3) {
			notes[i++] = v->noteCodeAfterArpeggiation;
		}
	}
	// Notes should be 60, 64, 67 in order
	CHECK_EQUAL(60, notes[0]);
	CHECK_EQUAL(64, notes[1]);
	CHECK_EQUAL(67, notes[2]);
}

TEST(VoiceLifecycle, PolyModeScalesToEightVoices) {
	VoiceLifecycleFixture f;

	// Play a full octave of notes
	for (int n = 60; n < 68; n++) {
		f.playNote(n);
	}
	CHECK_EQUAL(8, (int)f.si->numActiveVoices());
}

// ── Mono mode ───────────────────────────────────────────────────────────

TEST(VoiceLifecycle, MonoModeOldVoiceFastReleased) {
	VoiceLifecycleFixture f;
	f.si->polyphonic = PolyphonyMode::MONO;

	f.playNote(60);
	CHECK_EQUAL(1, (int)f.si->numActiveVoices());

	f.playNote(64);
	// Mono: old voice enters fast release, new voice plays — both in list
	CHECK((int)f.si->numActiveVoices() >= 1);

	// The last voice should be playing the new note
	CHECK_EQUAL(64, f.si->voices().back()->noteCodeAfterArpeggiation);
}

TEST(VoiceLifecycle, MonoModeLatestNoteIsLast) {
	VoiceLifecycleFixture f;
	f.si->polyphonic = PolyphonyMode::MONO;

	for (int n = 60; n < 68; n++) {
		f.playNote(n);
	}
	// The most recent note should be the last voice in the list
	CHECK_EQUAL(67, f.si->voices().back()->noteCodeAfterArpeggiation);
	// There should be at least 1 voice
	CHECK((int)f.si->numActiveVoices() >= 1);
}

// ── Note off ────────────────────────────────────────────────────────────

TEST(VoiceLifecycle, NoteOffReleasesCorrectVoice) {
	VoiceLifecycleFixture f;

	f.playNote(60);
	f.playNote(64);
	f.playNote(67);
	CHECK_EQUAL(3, (int)f.si->numActiveVoices());

	// Release middle note
	f.releaseNote(64);

	// Voice should be in release but still active (release envelope)
	int releasedCount = 0;
	for (const auto& v : f.si->voices()) {
		if (v->envelopes[0].state >= EnvelopeStage::RELEASE) {
			releasedCount++;
		}
	}
	CHECK_EQUAL(1, releasedCount);
}

TEST(VoiceLifecycle, NoteOffAllNotesClears) {
	VoiceLifecycleFixture f;

	f.playNote(60);
	f.playNote(64);
	f.playNote(67);

	// Release all notes
	f.releaseNote(60);
	f.releaseNote(64);
	f.releaseNote(67);

	// All voices should be in release
	for (const auto& v : f.si->voices()) {
		CHECK(v->envelopes[0].state >= EnvelopeStage::RELEASE);
	}
}

// ── Kill all voices ─────────────────────────────────────────────────────

TEST(VoiceLifecycle, KillAllVoicesClearsCompletely) {
	VoiceLifecycleFixture f;

	f.playNote(60);
	f.playNote(64);
	f.playNote(67);
	CHECK_EQUAL(3, (int)f.si->numActiveVoices());

	f.si->killAllVoices();
	CHECK_EQUAL(0, (int)f.si->numActiveVoices());
	CHECK(!f.si->anyNoteIsOn());
}

// ── Rapid note cycling ──────────────────────────────────────────────────

TEST(VoiceLifecycle, RapidNoteOnOffCycle) {
	VoiceLifecycleFixture f;

	// Simulate rapid key presses — 50 note on/off cycles
	for (int i = 0; i < 50; i++) {
		int note = 48 + (i % 24); // span 2 octaves
		f.playNote(note);
		f.releaseNote(note);
	}

	// All voices should be in release (none stuck)
	for (const auto& v : f.si->voices()) {
		CHECK(v->envelopes[0].state >= EnvelopeStage::RELEASE);
	}
}

TEST(VoiceLifecycle, RapidNoteOnOffMonoMode) {
	VoiceLifecycleFixture f;
	f.si->polyphonic = PolyphonyMode::MONO;

	// 50 rapid notes in mono mode — last note should be active
	for (int i = 0; i < 50; i++) {
		f.playNote(48 + (i % 24));
	}

	// At least 1 voice, last one is the most recent note
	CHECK((int)f.si->numActiveVoices() >= 1);
	int lastNote = 48 + (49 % 24); // = 48 + 1 = 49
	CHECK_EQUAL(lastNote, f.si->voices().back()->noteCodeAfterArpeggiation);
}

// ── Voice state after noteOn ────────────────────────────────────────────

TEST(VoiceLifecycle, NewVoiceStartsInAttack) {
	VoiceLifecycleFixture f;
	f.playNote(60);

	auto& voice = f.si->voices().front();
	CHECK(EnvelopeStage::ATTACK == voice->envelopes[0].state);
	CHECK(Voice::PedalState::None == voice->pedalState);
}

TEST(VoiceLifecycle, VelocityStoredInSourceValues) {
	VoiceLifecycleFixture f;
	f.playNote(60, 127);

	auto& voice = f.si->voices().front();
	int32_t velSource = voice->sourceValues[util::to_underlying(PatchSource::VELOCITY)];
	// velocity 127: (127 - 64) * 33554432 = 2113929216
	int32_t expected = ((int32_t)127 - 64) * 33554432;
	CHECK_EQUAL(expected, velSource);
}

TEST(VoiceLifecycle, NoteCodeStoredCorrectly) {
	VoiceLifecycleFixture f;
	f.playNote(72, 100);

	auto& voice = f.si->voices().front();
	CHECK_EQUAL(72, voice->noteCodeAfterArpeggiation);
}

// ── anyNoteIsOn ─────────────────────────────────────────────────────────

TEST(VoiceLifecycle, AnyNoteIsOnReflectsState) {
	VoiceLifecycleFixture f;
	CHECK(!f.si->anyNoteIsOn());

	f.playNote(60);
	CHECK(f.si->anyNoteIsOn());

	f.si->killAllVoices();
	CHECK(!f.si->anyNoteIsOn());
}
