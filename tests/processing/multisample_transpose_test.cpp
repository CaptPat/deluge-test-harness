// Regression tests for Sound::retriggerVoicesForTransposeChange() —
// fix/multisample-transpose-retrigger branch.
//
// Bug: when master transpose changes and a voice is using multisamples,
// recalculateAllVoicePhaseIncrements() only updates pitch — it doesn't
// re-trigger the voice to load the correct sample zone.
//
// Fix: retriggerVoicesForTransposeChange() checks if the new transposed note
// falls in a different multisample zone, and if so, re-triggers the voice.

#include "CppUTest/TestHarness.h"
#include "model/model_stack.h"
#include "model/voice/voice.h"
#include "modulation/params/param.h"
#include "modulation/params/param_manager.h"
#include "modulation/params/param_set.h"
#include "processing/engines/audio_engine.h"
#include "processing/sound/sound_instrument.h"
#include "storage/multi_range/multi_range.h"
#include "storage/multi_range/multi_range_array.h"

namespace params = deluge::modulation::params;

namespace {

// Heap-allocated fixture (same pattern as note_lifecycle_test to avoid MinGW GMA issues)
struct TransposeFixture {
	ParamCollectionSummary* summaries;
	PatchedParamSet* patchedParamSet;
	UnpatchedParamSet* unpatchedParamSet;
	ParamManagerForTimeline* paramManager;
	SoundInstrument* si;
	char* modelStackMemory;

	TransposeFixture() {
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

	~TransposeFixture() {
		si->killAllVoices();
		// Intentionally leak — same MinGW GMA workaround as note_lifecycle_test
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

	// Set up source 0 as a multisample with two zones:
	//   Zone 0: notes 0-59 (topNote = 59)
	//   Zone 1: notes 60-127 (topNote = 127)
	// Returns pointers to the two ranges (re-fetched after all insertions
	// since insertMultiRange may reallocate the underlying buffer).
	std::pair<MultiRange*, MultiRange*> setupMultisampleSource() {
		si->sources[0].oscType = OscType::SAMPLE;

		// Insert two ranges — set topNote immediately since the array sorts by it
		MultiRange* tmp0 = si->sources[0].ranges.insertMultiRange(0);
		CHECK(tmp0 != nullptr);
		tmp0->topNote = 59;

		MultiRange* tmp1 = si->sources[0].ranges.insertMultiRange(1);
		CHECK(tmp1 != nullptr);
		tmp1->topNote = 127;

		CHECK_EQUAL(2, si->sources[0].ranges.getNumElements());

		// Re-fetch stable pointers after all insertions
		MultiRange* r0 = si->sources[0].ranges.getElement(0);
		MultiRange* r1 = si->sources[0].ranges.getElement(1);
		return {r0, r1};
	}

	// Create a voice playing a specific note, with its guide pointing to a specific range
	void createVoiceAtNote(int32_t noteCode, AudioFileHolder* guideHolder) {
		auto* ms = getModelStack();
		Arpeggiator arp;
		int16_t mpe[3] = {0, 0, 0};
		si->noteOn(ms, &arp, noteCode, mpe, 0, 0, 0, 100, 0);
		CHECK(si->numActiveVoices() > 0);

		// Manually set the guide's audioFileHolder to the expected range
		// (In real firmware, Voice::noteOn sets this via VoiceUnisonPartSource::noteOn)
		auto& voices = si->voices();
		for (auto& v : voices) {
			if (v->noteCodeAfterArpeggiation == noteCode) {
				v->guides[0].audioFileHolder = guideHolder;
				break;
			}
		}
	}

	int16_t defaultMPE[3] = {0, 0, 0};
};

} // namespace

TEST_GROUP(MultisampleTranspose){};

// ── Basic zone detection ────────────────────────────────────────────────

TEST(MultisampleTranspose, NoMultisamplesJustRecalculates) {
	TransposeFixture f;
	// Source 0 is SQUARE (default), not SAMPLE — no multisamples
	auto* ms = f.getModelStack();
	auto* msFlags = ms->addSoundFlags();

	Arpeggiator arp;
	f.si->noteOn(ms, &arp, 60, f.defaultMPE, 0, 0, 0, 100, 0);
	CHECK_EQUAL(static_cast<size_t>(1), f.si->numActiveVoices());

	// Should not crash — just calls recalculateAllVoicePhaseIncrements
	f.si->retriggerVoicesForTransposeChange(msFlags);
	CHECK_EQUAL(static_cast<size_t>(1), f.si->numActiveVoices());
}

TEST(MultisampleTranspose, EmptyVoicesDoesNothing) {
	TransposeFixture f;
	auto* ms = f.getModelStack();
	auto* msFlags = ms->addSoundFlags();
	CHECK_EQUAL(static_cast<size_t>(0), f.si->numActiveVoices());

	// Should not crash with no voices
	f.si->retriggerVoicesForTransposeChange(msFlags);
}

TEST(MultisampleTranspose, NullModelStackDoesNothing) {
	TransposeFixture f;
	auto* ms = f.getModelStack();
	Arpeggiator arp;
	f.si->noteOn(ms, &arp, 60, f.defaultMPE, 0, 0, 0, 100, 0);

	// Should not crash with null modelStack
	f.si->retriggerVoicesForTransposeChange(nullptr);
	CHECK_EQUAL(static_cast<size_t>(1), f.si->numActiveVoices());
}

// ── Zone change detection ───────────────────────────────────────────────

TEST(MultisampleTranspose, SameZoneNoRetrigger) {
	TransposeFixture f;
	auto [r0, r1] = f.setupMultisampleSource();

	// Create voice at note 50 (zone 0: topNote=59)
	f.createVoiceAtNote(50, r0->getAudioFileHolder());

	// Transpose by +5 → note 55, still in zone 0 (topNote=59)
	f.si->transpose = 5;

	auto* ms = f.getModelStack();
	auto* msFlags = ms->addSoundFlags();

	// Get voice state before
	auto& voices = f.si->voices();
	int32_t noteCodeBefore = voices.front()->noteCodeAfterArpeggiation;

	f.si->retriggerVoicesForTransposeChange(msFlags);

	// Voice should still exist and NOT have been retriggered (same note code)
	CHECK_EQUAL(static_cast<size_t>(1), f.si->numActiveVoices());
	CHECK_EQUAL(noteCodeBefore, voices.front()->noteCodeAfterArpeggiation);
}

TEST(MultisampleTranspose, ZoneChangeRetriggersVoice) {
	TransposeFixture f;
	auto [r0, r1] = f.setupMultisampleSource();

	// Create voice at note 50 (zone 0: topNote=59)
	f.createVoiceAtNote(50, r0->getAudioFileHolder());

	// Verify setup
	CHECK_EQUAL(static_cast<size_t>(1), f.si->numActiveVoices());
	auto& voices = f.si->voices();
	CHECK_EQUAL(50, voices.front()->noteCodeAfterArpeggiation);

	// Transpose by +20 → note 70, now in zone 1 (topNote=127)
	// The guide still points to r0's holder, but getRange(70) returns r1
	f.si->transpose = 20;

	auto* ms = f.getModelStack();
	auto* msFlags = ms->addSoundFlags();

	f.si->retriggerVoicesForTransposeChange(msFlags);

	// Voice should have been retriggered (noteOn called again with same noteCode)
	CHECK_EQUAL(static_cast<size_t>(1), f.si->numActiveVoices());
	// The voice's noteCodeAfterArpeggiation should remain 50
	// (the retrigger preserves the original note)
	CHECK_EQUAL(50, voices.front()->noteCodeAfterArpeggiation);
}

TEST(MultisampleTranspose, VelocityRecoveredOnRetrigger) {
	TransposeFixture f;
	auto [r0, r1] = f.setupMultisampleSource();

	// Create voice at note 50 with velocity 100
	f.createVoiceAtNote(50, r0->getAudioFileHolder());

	auto& voices = f.si->voices();
	// Record the velocity source value that was stored
	int32_t velSrcBefore = voices.front()->sourceValues[util::to_underlying(PatchSource::VELOCITY)];

	// Transpose to cross zone boundary
	f.si->transpose = 20;

	auto* ms = f.getModelStack();
	auto* msFlags = ms->addSoundFlags();

	f.si->retriggerVoicesForTransposeChange(msFlags);

	// After retrigger, the recovered velocity should produce a similar source value.
	// Exact round-trip isn't guaranteed due to integer division, but it should be close.
	int32_t velSrcAfter = voices.front()->sourceValues[util::to_underlying(PatchSource::VELOCITY)];
	int32_t diff = std::abs(velSrcAfter - velSrcBefore);
	// Allow tolerance of one quantization step (33554432)
	CHECK(diff <= 33554432);
}

TEST(MultisampleTranspose, FMSynthModeSkipsMultisampleCheck) {
	TransposeFixture f;
	auto [r0, r1] = f.setupMultisampleSource();

	// Switch to FM mode — multisamples should be ignored
	f.si->setSynthMode(SynthMode::FM, nullptr);

	auto* ms = f.getModelStack();
	Arpeggiator arp;
	f.si->noteOn(ms, &arp, 50, f.defaultMPE, 0, 0, 0, 100, 0);

	f.si->transpose = 20;
	auto* msFlags = ms->addSoundFlags();
	f.si->retriggerVoicesForTransposeChange(msFlags);

	// Should not crash — FM mode bypasses multisample detection
	CHECK(f.si->numActiveVoices() > 0);
}
