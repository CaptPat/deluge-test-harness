// Arpeggiator engine tests — exercises constructor, note on/off, reset,
// settings, render, and probability helpers.

#include "CppUTest/TestHarness.h"
#include "modulation/arpeggiator.h"
#include "playback/playback_handler.h"
#include "song_mock.h"

// ── ArpeggiatorSettings ─────────────────────────────────────────────────

TEST_GROUP(ArpSettingsTest) {
	void setup() {
		// Ensure currentSong is available for constructor
		currentSong = &testSong;
		preLoadedSong = nullptr;
	}
	void teardown() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
	}
};

TEST(ArpSettingsTest, defaultConstructorSetsDefaults) {
	ArpeggiatorSettings s;
	CHECK_EQUAL((int)ArpPreset::OFF, (int)s.preset);
	CHECK_EQUAL((int)ArpMode::OFF, (int)s.mode);
	CHECK_EQUAL(2, s.numOctaves);
	CHECK_EQUAL(1, s.numStepRepeats);
	CHECK_EQUAL((int)SYNC_TYPE_EVEN, (int)s.syncType);
	CHECK(!s.flagForceArpRestart);
	CHECK(!s.randomizerLock);
}

TEST(ArpSettingsTest, defaultConstructorUsesPreLoadedSong) {
	Song altSong;
	altSong.insideWorldTickMagnitude = 3;
	altSong.insideWorldTickMagnitudeOffsetFromBPM = 1;
	preLoadedSong = &altSong;

	ArpeggiatorSettings s;
	// syncLevel = 8 - (3 + 1) = 4
	CHECK_EQUAL(4, (int)s.syncLevel);
}

TEST(ArpSettingsTest, defaultConstructorFallsBackToCurrentSong) {
	preLoadedSong = nullptr;
	testSong.insideWorldTickMagnitude = 2;
	testSong.insideWorldTickMagnitudeOffsetFromBPM = 0;

	ArpeggiatorSettings s;
	// syncLevel = 8 - (2 + 0) = 6
	CHECK_EQUAL(6, (int)s.syncLevel);
}

TEST(ArpSettingsTest, generateNewNotePatternFillsArray) {
	ArpeggiatorSettings s;
	// Pattern should be filled with values in [0, PATTERN_MAX_BUFFER_SIZE)
	bool allValid = true;
	for (size_t i = 0; i < PATTERN_MAX_BUFFER_SIZE; i++) {
		if (s.notePattern[i] < 0 || s.notePattern[i] >= (int8_t)PATTERN_MAX_BUFFER_SIZE) {
			allValid = false;
			break;
		}
	}
	CHECK(allValid);
}

TEST(ArpSettingsTest, cloneFromCopiesFields) {
	ArpeggiatorSettings src;
	src.mode = ArpMode::ARP;
	src.numOctaves = 4;
	src.rate = 12345;
	src.gate = 6789;

	ArpeggiatorSettings dst;
	dst.cloneFrom(&src);

	CHECK_EQUAL((int)ArpMode::ARP, (int)dst.mode);
	CHECK_EQUAL(4, dst.numOctaves);
	CHECK_EQUAL(12345, dst.rate);
	CHECK_EQUAL(6789, dst.gate);
}

TEST(ArpSettingsTest, getPhaseIncrementPositiveRate) {
	ArpeggiatorSettings s;
	// With rate > 0, phaseIncrement should be positive and > 0
	uint32_t inc = s.getPhaseIncrement(1000000);
	CHECK(inc > 0);
}

TEST(ArpSettingsTest, getPhaseIncrementZeroRate) {
	ArpeggiatorSettings s;
	uint32_t inc = s.getPhaseIncrement(0);
	CHECK(inc > 0); // Default rate still produces movement
}

// ── ArpNote ─────────────────────────────────────────────────────────────

TEST_GROUP(ArpNoteTest) {};

TEST(ArpNoteTest, defaultConstruction) {
	ArpNote n;
	CHECK_EQUAL(0, n.velocity);
	CHECK_EQUAL(0, n.baseVelocity);
	for (int i = 0; i < (int)ARP_MAX_INSTRUCTION_NOTES; i++) {
		CHECK_EQUAL((int)ArpNoteStatus::OFF, (int)n.noteStatus[i]);
		CHECK_EQUAL(ARP_NOTE_NONE, n.noteCodeOnPostArp[i]);
		CHECK_EQUAL(MIDI_CHANNEL_NONE, n.outputMemberChannel[i]);
	}
}

TEST(ArpNoteTest, isPendingFalseByDefault) {
	ArpNote n;
	CHECK(!n.isPending());
}

TEST(ArpNoteTest, isPendingTrueWhenSet) {
	ArpNote n;
	n.noteStatus[2] = ArpNoteStatus::PENDING;
	CHECK(n.isPending());
}

TEST(ArpNoteTest, resetPostArpArrays) {
	ArpNote n;
	n.noteStatus[0] = ArpNoteStatus::PLAYING;
	n.noteCodeOnPostArp[0] = 60;
	n.outputMemberChannel[0] = 1;
	n.resetPostArpArrays();
	CHECK_EQUAL((int)ArpNoteStatus::OFF, (int)n.noteStatus[0]);
	CHECK_EQUAL(ARP_NOTE_NONE, n.noteCodeOnPostArp[0]);
	CHECK_EQUAL(MIDI_CHANNEL_NONE, n.outputMemberChannel[0]);
}

// ── ArpReturnInstruction ────────────────────────────────────────────────

TEST_GROUP(ArpReturnInstructionTest) {};

TEST(ArpReturnInstructionTest, defaultConstruction) {
	ArpReturnInstruction inst;
	CHECK_EQUAL(0u, inst.sampleSyncLengthOn);
	CHECK(!inst.invertReversed);
	CHECK(inst.arpNoteOn == nullptr);
	for (int i = 0; i < (int)ARP_MAX_INSTRUCTION_NOTES; i++) {
		CHECK_EQUAL(MIDI_CHANNEL_NONE, inst.outputMIDIChannelOff[i]);
		CHECK_EQUAL(ARP_NOTE_NONE, inst.noteCodeOffPostArp[i]);
	}
}

// ── ArpeggiatorForDrum ──────────────────────────────────────────────────

TEST_GROUP(ArpForDrumTest) {
	ArpeggiatorForDrum arp;
	ArpeggiatorSettings settings;

	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
	}
};

TEST(ArpForDrumTest, constructorSetsVelocityZero) {
	CHECK_EQUAL(0, arp.active_note.velocity);
}

TEST(ArpForDrumTest, noteOnNoArpSetsInstruction) {
	settings.mode = ArpMode::OFF;
	int16_t mpeValues[3] = {0, 0, 0};
	ArpReturnInstruction inst;

	arp.noteOn(&settings, 60, 100, &inst, 0, mpeValues);

	// With arp off, note should pass through directly
	CHECK(inst.arpNoteOn != nullptr);
	CHECK_EQUAL(60, inst.arpNoteOn->noteCodeOnPostArp[0]);
	CHECK_EQUAL((int)ArpNoteStatus::PENDING, (int)inst.arpNoteOn->noteStatus[0]);
}

TEST(ArpForDrumTest, noteOnSetsVelocity) {
	settings.mode = ArpMode::OFF;
	int16_t mpeValues[3] = {0, 0, 0};
	ArpReturnInstruction inst;

	arp.noteOn(&settings, 60, 100, &inst, 0, mpeValues);
	CHECK(arp.active_note.velocity > 0);
	CHECK_EQUAL(100, arp.active_note.baseVelocity);
}

TEST(ArpForDrumTest, noteOffNoArpClearsNote) {
	settings.mode = ArpMode::OFF;
	int16_t mpeValues[3] = {0, 0, 0};
	ArpReturnInstruction inst;

	arp.noteOn(&settings, 60, 100, &inst, 0, mpeValues);
	arp.noteOff(&settings, 60, &inst);

	CHECK_EQUAL(0, arp.active_note.velocity); // note is off
	CHECK_EQUAL(60, inst.noteCodeOffPostArp[0]);
}

TEST(ArpForDrumTest, resetClearsVelocity) {
	int16_t mpeValues[3] = {0, 0, 0};
	ArpReturnInstruction inst;
	settings.mode = ArpMode::OFF;

	arp.noteOn(&settings, 60, 100, &inst, 0, mpeValues);
	arp.reset();
	CHECK_EQUAL(0, arp.active_note.velocity);
}

TEST(ArpForDrumTest, activeNoteVelocityTracksState) {
	// velocity == 0 means no active note
	CHECK_EQUAL(0, arp.active_note.velocity);

	int16_t mpeValues[3] = {0, 0, 0};
	ArpReturnInstruction inst;
	settings.mode = ArpMode::OFF;
	arp.noteOn(&settings, 60, 100, &inst, 0, mpeValues);
	CHECK(arp.active_note.velocity > 0);

	arp.noteOff(&settings, 60, &inst);
	CHECK_EQUAL(0, arp.active_note.velocity);
}

TEST(ArpForDrumTest, getArpType) {
	CHECK_EQUAL((int)ArpType::DRUM, (int)arp.getArpType());
}

// ── Arpeggiator (synth) ─────────────────────────────────────────────────

TEST_GROUP(ArpSynthTest) {
	Arpeggiator arp;
	ArpeggiatorSettings settings;

	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
	}
};

TEST(ArpSynthTest, constructorEmpty) {
	CHECK_EQUAL(0, arp.notes.getNumElements());
}

TEST(ArpSynthTest, getArpType) {
	CHECK_EQUAL((int)ArpType::SYNTH, (int)arp.getArpType());
}

TEST(ArpSynthTest, noteOnNoArpAddsNote) {
	settings.mode = ArpMode::OFF;
	int16_t mpeValues[3] = {0, 0, 0};
	ArpReturnInstruction inst;

	arp.noteOn(&settings, 60, 100, &inst, 0, mpeValues);

	CHECK_EQUAL(1, arp.notes.getNumElements());
	CHECK(inst.arpNoteOn != nullptr);
}

TEST(ArpSynthTest, noteOnMultipleNotesSorted) {
	settings.mode = ArpMode::OFF;
	int16_t mpeValues[3] = {0, 0, 0};
	ArpReturnInstruction inst;

	arp.noteOn(&settings, 72, 90, &inst, 0, mpeValues);
	arp.noteOn(&settings, 60, 100, &inst, 0, mpeValues);
	arp.noteOn(&settings, 64, 80, &inst, 0, mpeValues);

	CHECK_EQUAL(3, arp.notes.getNumElements());

	// Notes should be sorted by noteCode
	ArpNote* n0 = (ArpNote*)arp.notes.getElementAddress(0);
	ArpNote* n1 = (ArpNote*)arp.notes.getElementAddress(1);
	ArpNote* n2 = (ArpNote*)arp.notes.getElementAddress(2);
	CHECK_EQUAL(60, n0->inputCharacteristics[0]);
	CHECK_EQUAL(64, n1->inputCharacteristics[0]);
	CHECK_EQUAL(72, n2->inputCharacteristics[0]);
}

TEST(ArpSynthTest, noteOffRemovesNote) {
	settings.mode = ArpMode::OFF;
	int16_t mpeValues[3] = {0, 0, 0};
	ArpReturnInstruction inst;

	arp.noteOn(&settings, 60, 100, &inst, 0, mpeValues);
	arp.noteOn(&settings, 64, 80, &inst, 0, mpeValues);
	CHECK_EQUAL(2, arp.notes.getNumElements());

	arp.noteOff(&settings, 60, &inst);
	CHECK_EQUAL(1, arp.notes.getNumElements());

	// Remaining note should be 64
	ArpNote* remaining = (ArpNote*)arp.notes.getElementAddress(0);
	CHECK_EQUAL(64, remaining->inputCharacteristics[0]);
}

TEST(ArpSynthTest, noteOffSetsInstruction) {
	settings.mode = ArpMode::OFF;
	int16_t mpeValues[3] = {0, 0, 0};
	ArpReturnInstruction inst;

	arp.noteOn(&settings, 60, 100, &inst, 0, mpeValues);
	arp.noteOff(&settings, 60, &inst);

	CHECK_EQUAL(60, inst.noteCodeOffPostArp[0]);
}

TEST(ArpSynthTest, duplicateNoteOnIgnoredNoArp) {
	settings.mode = ArpMode::OFF;
	int16_t mpeValues[3] = {0, 0, 0};
	ArpReturnInstruction inst;

	arp.noteOn(&settings, 60, 100, &inst, 0, mpeValues);
	arp.noteOn(&settings, 60, 90, &inst, 0, mpeValues);

	// Should still be 1 note (not duplicated) — though velocity updated
	CHECK_EQUAL(1, arp.notes.getNumElements());
}

TEST(ArpSynthTest, resetClearsAllNotes) {
	settings.mode = ArpMode::OFF;
	int16_t mpeValues[3] = {0, 0, 0};
	ArpReturnInstruction inst;

	arp.noteOn(&settings, 60, 100, &inst, 0, mpeValues);
	arp.noteOn(&settings, 64, 80, &inst, 0, mpeValues);
	arp.reset();

	CHECK_EQUAL(0, arp.notes.getNumElements());
}

TEST(ArpSynthTest, noteOffNonExistentNoteNoOp) {
	settings.mode = ArpMode::OFF;
	ArpReturnInstruction inst;

	// Should not crash
	arp.noteOff(&settings, 60, &inst);
	CHECK_EQUAL(0, arp.notes.getNumElements());
}

// ── ArpeggiatorForKit ───────────────────────────────────────────────────

TEST_GROUP(ArpForKitTest) {
	ArpeggiatorForKit arp;
	ArpeggiatorSettings settings;

	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
	}
};

TEST(ArpForKitTest, getArpType) {
	CHECK_EQUAL((int)ArpType::KIT, (int)arp.getArpType());
}

TEST(ArpForKitTest, removeDrumIndexFromEmpty) {
	// Should not crash on empty notes
	arp.removeDrumIndex(&settings, 0);
	CHECK_EQUAL(0, arp.notes.getNumElements());
}

// ── Probability helpers ─────────────────────────────────────────────────

TEST_GROUP(ArpProbabilityTest) {
	ArpeggiatorForDrum arp;
	ArpeggiatorSettings settings;

	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
	}
};

TEST(ArpProbabilityTest, calculateRandomizerAmountsDefault) {
	// Default probabilities: noteProbability=MAX, others=0
	// Should not crash; we verify indirectly via noteOn behavior
	arp.calculateRandomizerAmounts(&settings);
}

TEST(ArpProbabilityTest, calculateRandomizerAmountsWithLock) {
	settings.randomizerLock = true;
	arp.calculateRandomizerAmounts(&settings);
	// Should not crash — locked randomizer path stores values
}
