// Debug test: isolate where noteOn crashes
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

TEST_GROUP(NoteDebug){};

TEST(NoteDebug, VoiceNoteOnDirect) {
	SoundInstrument si;
	auto& pool = AudioEngine::VoicePool::get();
	auto voice = pool.acquire(si);
	CHECK(voice != nullptr);

	// Try Voice::noteOn directly
	char modelStackMemory[MODEL_STACK_MAX_SIZE]{};
	auto* ms = (ModelStackWithSoundFlags*)modelStackMemory;
	ms->song = nullptr;
	ms->setTimelineCounter(nullptr);
	ms->setNoteRow(nullptr);
	ms->noteRowId = 0;
	ms->modControllable = &si;
	ms->paramManager = nullptr;
	for (int i = 0; i < NUM_SOUND_FLAGS; i++) {
		ms->soundFlags[i] = FLAG_TBD;
	}

	int16_t mpe[3] = {0, 0, 0};
	bool success = voice->noteOn(ms, 60, 60, 100, 0, 0, 0, true, 16, mpe);
	// Voice::noteOn is a stub that returns true
	CHECK(success);
}

TEST(NoteDebug, AcquireVoiceAndNoteOn) {
	// Test the minimum path: acquireVoice + voice->noteOn
	SoundInstrument si;
	ParamCollectionSummary summaries[PARAM_COLLECTIONS_STORAGE_NUM]{};
	PatchedParamSet patchedParamSet{&summaries[1]};
	UnpatchedParamSet unpatchedParamSet{&summaries[0]};
	ParamManagerForTimeline paramManager;
	paramManager.summaries[0].paramCollection = &unpatchedParamSet;
	paramManager.summaries[1].paramCollection = &patchedParamSet;

	char modelStackMemory[MODEL_STACK_MAX_SIZE]{};
	auto* ms = (ModelStackWithSoundFlags*)modelStackMemory;
	ms->song = nullptr;
	ms->setTimelineCounter(nullptr);
	ms->setNoteRow(nullptr);
	ms->noteRowId = 0;
	ms->modControllable = &si;
	ms->paramManager = nullptr; // Test with nullptr first
	for (int i = 0; i < NUM_SOUND_FLAGS; i++) {
		ms->soundFlags[i] = FLAG_TBD;
	}

	// Acquire a voice via pool
	auto voice = AudioEngine::VoicePool::get().acquire(si);
	CHECK(voice != nullptr);

	// noteOn on the acquired voice — with nullptr paramManager like VoiceNoteOnDirect
	int16_t mpe[3] = {0, 0, 0};
	bool success = voice->noteOn(ms, 60, 60, 100, 0, 0, 0, true, 16, mpe);
	CHECK(success);
}

TEST(NoteDebug, SoundNoteOnMinimal) {
	// Replicate the NoteTestFixture setup
	ParamCollectionSummary summaries[PARAM_COLLECTIONS_STORAGE_NUM]{};
	PatchedParamSet patchedParamSet{&summaries[1]};
	UnpatchedParamSet unpatchedParamSet{&summaries[0]};
	ParamManagerForTimeline paramManager;
	SoundInstrument si;

	paramManager.summaries[0].paramCollection = &unpatchedParamSet;
	paramManager.summaries[1].paramCollection = &patchedParamSet;
	unpatchedParamSet.kind = params::Kind::UNPATCHED_SOUND;
	patchedParamSet.params[params::LOCAL_OSC_A_VOLUME].setCurrentValueBasicForSetup(2147483647);
	unpatchedParamSet.params[params::UNPATCHED_SUSTAIN_PEDAL].setCurrentValueBasicForSetup(-2147483648);
	unpatchedParamSet.params[params::UNPATCHED_SOSTENUTO_PEDAL].setCurrentValueBasicForSetup(-2147483648);
	unpatchedParamSet.params[params::UNPATCHED_SOFT_PEDAL].setCurrentValueBasicForSetup(-2147483648);
	unpatchedParamSet.params[params::UNPATCHED_PORTAMENTO].setCurrentValueBasicForSetup(-2147483648);

	char modelStackMemory[MODEL_STACK_MAX_SIZE]{};
	auto* ms = (ModelStackWithThreeMainThings*)modelStackMemory;
	ms->song = nullptr;
	ms->setTimelineCounter(nullptr);
	ms->setNoteRow(nullptr);
	ms->noteRowId = 0;
	ms->modControllable = &si;
	ms->paramManager = &paramManager;

	int16_t mpe[3] = {0, 0, 0};

	// This is the call that crashes in NoteLifecycle tests
	si.noteOn(ms, si.getArp(), 60, mpe, 0, 0, 0, 100, 16);
	CHECK(si.numActiveVoices() > 0);
}

TEST(NoteDebug, ArpNoteOnDirect) {
	SoundInstrument si;
	ArpeggiatorBase* arp = si.getArp();
	CHECK(arp != nullptr);

	ArpeggiatorSettings* arpSettings = si.getArpSettings();
	CHECK(arpSettings != nullptr);
	CHECK(arpSettings->mode == ArpMode::OFF);

	ArpReturnInstruction instruction;
	instruction.sampleSyncLengthOn = 0;
	int16_t mpe[3] = {0, 0, 0};

	// Call arp->noteOn directly (bypassing Sound::noteOn)
	arp->noteOn(arpSettings, 60, 100, &instruction, 16, mpe);

	// With arp OFF, noteOn should pass the note through directly
	CHECK(instruction.arpNoteOn != nullptr);
	if (instruction.arpNoteOn) {
		CHECK_EQUAL(60, instruction.arpNoteOn->noteCodeOnPostArp[0]);
	}
}
