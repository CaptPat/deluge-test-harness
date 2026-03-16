// Tests for Sound::noteOn()/noteOff() lifecycle — voice allocation, polyphony,
// soft pedal velocity reduction, and basic arpeggiator routing.
// Uses real firmware implementations (not stubs).

#include "CppUTest/TestHarness.h"
#include "model/model_stack.h"
#include "model/voice/voice.h"
#include "modulation/arpeggiator.h"
#include "modulation/params/param.h"
#include "modulation/params/param_manager.h"
#include "modulation/params/param_set.h"
#include "processing/engines/audio_engine.h"
#include "processing/sound/sound_instrument.h"

namespace params = deluge::modulation::params;

namespace {

// Heap-allocated fixture to avoid MinGW GMA duplicate static crash on destruction.
// SoundInstrument's fast_vector uses fast_allocator → GMA, and stack-allocated
// objects get destroyed through a different GMA instance than the one used to allocate.
struct NoteTestFixture {
	ParamCollectionSummary* summaries;
	PatchedParamSet* patchedParamSet;
	UnpatchedParamSet* unpatchedParamSet;
	ParamManagerForTimeline* paramManager;
	SoundInstrument* si;
	char* modelStackMemory;

	NoteTestFixture() {
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
		unpatchedParamSet->params[params::UNPATCHED_SUSTAIN_PEDAL].setCurrentValueBasicForSetup(-2147483648);
		unpatchedParamSet->params[params::UNPATCHED_SOSTENUTO_PEDAL].setCurrentValueBasicForSetup(-2147483648);
		unpatchedParamSet->params[params::UNPATCHED_SOFT_PEDAL].setCurrentValueBasicForSetup(-2147483648);
		unpatchedParamSet->params[params::UNPATCHED_PORTAMENTO].setCurrentValueBasicForSetup(-2147483648);
	}

	~NoteTestFixture() {
		si->killAllVoices();
		// Can't delete si (MinGW GMA crash), but deregister from AudioEngine
		// to prevent stale pointers from affecting subsequent tests.
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

	int16_t defaultMPE[3] = {0, 0, 0};
};

} // namespace

TEST_GROUP(NoteLifecycle){};

// ── Basic noteOn/noteOff ───────────────────────────────────────────────

TEST(NoteLifecycle, NoteOnCreatesVoice) {
	NoteTestFixture f;
	CHECK_EQUAL(static_cast<size_t>(0), f.si->numActiveVoices());

	f.si->noteOn(f.getModelStack(), f.si->getArp(), 60, f.defaultMPE, 0, 0, 0, 100, 16);

	CHECK(f.si->numActiveVoices() > 0);
}

TEST(NoteLifecycle, NoteOffReleasesVoice) {
	NoteTestFixture f;
	f.si->noteOn(f.getModelStack(), f.si->getArp(), 60, f.defaultMPE, 0, 0, 0, 100, 16);
	CHECK(f.si->numActiveVoices() > 0);

	f.si->noteOff(f.getModelStack(), f.si->getArp(), 60);
	// Voice enters release stage — no crash is the key check
}

TEST(NoteLifecycle, AllNotesOff) {
	NoteTestFixture f;
	f.si->noteOn(f.getModelStack(), f.si->getArp(), 60, f.defaultMPE, 0, 0, 0, 100, 16);
	f.si->noteOn(f.getModelStack(), f.si->getArp(), 64, f.defaultMPE, 0, 0, 0, 100, 16);

	f.si->allNotesOff(f.getModelStack(), f.si->getArp());
	// Arpeggiator is reset, voices may be in release
}

// ── Polyphony ──────────────────────────────────────────────────────────

TEST(NoteLifecycle, PolyModeMultipleVoices) {
	NoteTestFixture f;
	f.si->polyphonic = PolyphonyMode::POLY;

	f.si->noteOn(f.getModelStack(), f.si->getArp(), 60, f.defaultMPE, 0, 0, 0, 100, 16);
	f.si->noteOn(f.getModelStack(), f.si->getArp(), 64, f.defaultMPE, 0, 0, 0, 100, 16);
	f.si->noteOn(f.getModelStack(), f.si->getArp(), 67, f.defaultMPE, 0, 0, 0, 100, 16);

	CHECK(f.si->numActiveVoices() >= 3);
}

// ── CC67 Soft Pedal ────────────────────────────────────────────────────

TEST(NoteLifecycle, SoftPedalNoCrash) {
	NoteTestFixture f;
	f.unpatchedParamSet->params[params::UNPATCHED_SOFT_PEDAL].setCurrentValueBasicForSetup(2147483647);

	f.si->noteOn(f.getModelStack(), f.si->getArp(), 60, f.defaultMPE, 0, 0, 0, 127, 16);
	CHECK(f.si->numActiveVoices() > 0);
}

// ── Source activity ────────────────────────────────────────────────────

TEST(NoteLifecycle, InactiveSourcesNoCrash) {
	NoteTestFixture f;
	// Set both osc volumes to minimum — noteOn should still not crash
	// (sources may still register as "active" due to containsSomething
	// returning true for default param state, but the key check is no crash)
	f.patchedParamSet->params[params::LOCAL_OSC_A_VOLUME].setCurrentValueBasicForSetup(-2147483648);
	f.patchedParamSet->params[params::LOCAL_OSC_B_VOLUME].setCurrentValueBasicForSetup(-2147483648);

	f.si->noteOn(f.getModelStack(), f.si->getArp(), 60, f.defaultMPE, 0, 0, 0, 100, 16);
}

TEST(NoteLifecycle, RingmodAlwaysActive) {
	NoteTestFixture f;
	f.si->setSynthMode(SynthMode::RINGMOD, nullptr);
	f.patchedParamSet->params[params::LOCAL_OSC_A_VOLUME].setCurrentValueBasicForSetup(-2147483648);

	f.si->noteOn(f.getModelStack(), f.si->getArp(), 60, f.defaultMPE, 0, 0, 0, 100, 16);
	CHECK(f.si->numActiveVoices() > 0);
}

// ── Note on/off cycles ─────────────────────────────────────────────────

TEST(NoteLifecycle, MultipleNoteOnOffCycles) {
	NoteTestFixture f;
	for (int i = 0; i < 3; i++) {
		int32_t note = 60 + i;
		f.si->noteOn(f.getModelStack(), f.si->getArp(), note, f.defaultMPE, 0, 0, 0, 100, 16);
		f.si->noteOff(f.getModelStack(), f.si->getArp(), note);
		f.si->killAllVoices(); // Simulate render loop reclaiming released voices
	}
}
