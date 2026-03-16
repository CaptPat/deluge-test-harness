// Sound method coverage tests: setSynthMode with voices, allNotesOff,
// source configuration, polyphony transitions.

#include "CppUTest/TestHarness.h"
#include "model/model_stack.h"
#include "model/voice/voice.h"
#include "modulation/params/param_manager.h"
#include "modulation/params/param_set.h"
#include "processing/engines/audio_engine.h"
#include "processing/sound/sound_instrument.h"

namespace params = deluge::modulation::params;

namespace {

struct SoundMethodFixture {
	ParamCollectionSummary* summaries;
	PatchedParamSet* patchedParamSet;
	UnpatchedParamSet* unpatchedParamSet;
	ParamManagerForTimeline* paramManager;
	SoundInstrument* si;
	char* modelStackMemory;

	SoundMethodFixture() {
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

	~SoundMethodFixture() {
		si->killAllVoices();
		// Leak si to avoid MinGW GMA destruction crash
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

TEST_GROUP(SoundMethod){};

// ── setSynthMode with active voices ─────────────────────────────────────

TEST(SoundMethod, SetSynthModeKillsVoices) {
	SoundMethodFixture f;

	f.playNote(60);
	f.playNote(64);
	CHECK_EQUAL(2, (int)f.si->numActiveVoices());

	// Switching synth mode should kill all voices
	f.si->setSynthMode(SynthMode::FM, nullptr);
	CHECK_EQUAL(0, (int)f.si->numActiveVoices());
	CHECK(SynthMode::FM == f.si->synthMode);
}

TEST(SoundMethod, SetSynthModeToSameDoesNotCrash) {
	SoundMethodFixture f;
	f.playNote(60);

	// Same mode — should still work (kills voices, sets mode)
	SynthMode before = f.si->synthMode;
	f.si->setSynthMode(before, nullptr);
	CHECK(before == f.si->synthMode);
}

TEST(SoundMethod, SetSynthModeThenPlayNewNotes) {
	SoundMethodFixture f;

	f.playNote(60);
	f.si->setSynthMode(SynthMode::FM, nullptr);
	CHECK_EQUAL(0, (int)f.si->numActiveVoices());

	// Should be able to play new notes after mode change
	f.playNote(67);
	CHECK_EQUAL(1, (int)f.si->numActiveVoices());
}

TEST(SoundMethod, RapidModeChangesWithVoices) {
	SoundMethodFixture f;

	// Rapid mode changes while playing — no crash
	for (int i = 0; i < 10; i++) {
		f.playNote(60 + i);
		f.si->setSynthMode(SynthMode::FM, nullptr);
		f.playNote(60 + i);
		f.si->setSynthMode(SynthMode::SUBTRACTIVE, nullptr);
	}
	// Should end with no voices (last setSynthMode kills them)
	CHECK_EQUAL(0, (int)f.si->numActiveVoices());
}

// ── allNotesOff ─────────────────────────────────────────────────────────

TEST(SoundMethod, AllNotesOffReleasesAll) {
	SoundMethodFixture f;

	f.playNote(60);
	f.playNote(64);
	f.playNote(67);
	CHECK_EQUAL(3, (int)f.si->numActiveVoices());

	f.si->allNotesOff(f.getSoundFlagsModelStack(), f.si->getArp());

	// Voices should be in release (not removed, just released)
	for (const auto& v : f.si->voices()) {
		CHECK(v->envelopes[0].state >= EnvelopeStage::RELEASE);
	}
}

TEST(SoundMethod, AllNotesOffWithNoVoicesNoCrash) {
	SoundMethodFixture f;
	CHECK_EQUAL(0, (int)f.si->numActiveVoices());

	// Should not crash with no voices
	f.si->allNotesOff(f.getSoundFlagsModelStack(), f.si->getArp());
	CHECK_EQUAL(0, (int)f.si->numActiveVoices());
}

// ── Source configuration ────────────────────────────────────────────────

TEST(SoundMethod, DefaultOscTypeIsAnalogSquare) {
	SoundMethodFixture f;
	CHECK(f.si->sources[0].oscType == OscType::ANALOG_SQUARE
	      || f.si->sources[0].oscType == OscType::SQUARE);
}

TEST(SoundMethod, SourceOscTypeChangeable) {
	SoundMethodFixture f;

	f.si->sources[0].oscType = OscType::SAW;
	CHECK(f.si->sources[0].oscType == OscType::SAW);

	f.si->sources[0].oscType = OscType::SINE;
	CHECK(f.si->sources[0].oscType == OscType::SINE);
}

TEST(SoundMethod, TwoSourcesIndependent) {
	SoundMethodFixture f;

	f.si->sources[0].oscType = OscType::SAW;
	f.si->sources[1].oscType = OscType::SINE;

	CHECK(f.si->sources[0].oscType == OscType::SAW);
	CHECK(f.si->sources[1].oscType == OscType::SINE);
}

// ── Polyphony mode transitions with active voices ───────────────────────

TEST(SoundMethod, PolyToMonoWithActiveVoices) {
	SoundMethodFixture f;

	f.playNote(60);
	f.playNote(64);
	f.playNote(67);
	CHECK_EQUAL(3, (int)f.si->numActiveVoices());

	// Switch to mono — existing voices stay, new notes replace
	f.si->polyphonic = PolyphonyMode::MONO;
	f.playNote(72);

	// The latest voice should be the new note
	CHECK_EQUAL(72, f.si->voices().back()->noteCodeAfterArpeggiation);
}

TEST(SoundMethod, MonoToPolyAllowsPolyphony) {
	SoundMethodFixture f;
	f.si->polyphonic = PolyphonyMode::MONO;

	f.playNote(60);
	f.playNote(64); // replaces in mono

	// Switch to poly
	f.si->polyphonic = PolyphonyMode::POLY;
	f.playNote(67);
	f.playNote(72);

	// Should have accumulated voices (mono leftovers + new poly notes)
	CHECK((int)f.si->numActiveVoices() >= 3);
}

// ── isDrum ──────────────────────────────────────────────────────────────

TEST(SoundMethod, SoundInstrumentIsNotDrum) {
	SoundMethodFixture f;
	CHECK(!f.si->isDrum());
}

// ── maxVoiceCount default ───────────────────────────────────────────────

TEST(SoundMethod, DefaultMaxVoiceCount) {
	SoundMethodFixture f;
	CHECK_EQUAL(8, (int)f.si->maxVoiceCount);
}

// ── Synth mode queries ──────────────────────────────────────────────────

TEST(SoundMethod, DefaultSynthModeIsSubtractive) {
	SoundMethodFixture f;
	CHECK(SynthMode::SUBTRACTIVE == f.si->synthMode);
}

TEST(SoundMethod, SynthModeRoundTrips) {
	SoundMethodFixture f;

	SynthMode modes[] = {SynthMode::SUBTRACTIVE, SynthMode::FM, SynthMode::RINGMOD};
	for (auto mode : modes) {
		f.si->setSynthMode(mode, nullptr);
		CHECK(mode == f.si->synthMode);
	}
}
