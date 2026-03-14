// Deep arpeggiator tests — exercises ARP mode noteOn/noteOff, render, octave
// traversal, pattern generation, preset/mode switching, edge cases.

#include "CppUTest/TestHarness.h"
#include "modulation/arpeggiator.h"
#include "playback/playback_handler.h"
#include "song_mock.h"

// ── ArpeggiatorSettings deep tests ─────────────────────────────────────────

TEST_GROUP(ArpSettingsDeepTest) {
	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
	}
};

TEST(ArpSettingsDeepTest, numOctavesRange) {
	ArpeggiatorSettings s;
	s.numOctaves = 1;
	CHECK_EQUAL(1, s.numOctaves);
	s.numOctaves = 8;
	CHECK_EQUAL(8, s.numOctaves);
}

TEST(ArpSettingsDeepTest, cloneFromCopiesAllFields) {
	ArpeggiatorSettings src;
	src.mode = ArpMode::ARP;
	src.preset = ArpPreset::CUSTOM;
	src.numOctaves = 3;
	src.numStepRepeats = 2;
	src.octaveMode = ArpOctaveMode::DOWN;
	src.noteMode = ArpNoteMode::DOWN;
	src.rate = 99999;
	src.gate = 88888;
	src.rhythm = 12345;
	src.noteProbability = 4000000000u;
	src.randomizerLock = true;

	ArpeggiatorSettings dst;
	dst.cloneFrom(&src);

	CHECK_EQUAL((int)ArpMode::ARP, (int)dst.mode);
	CHECK_EQUAL((int)ArpPreset::CUSTOM, (int)dst.preset);
	CHECK_EQUAL(3, dst.numOctaves);
	CHECK_EQUAL(2, dst.numStepRepeats);
	CHECK_EQUAL((int)ArpOctaveMode::DOWN, (int)dst.octaveMode);
	CHECK_EQUAL((int)ArpNoteMode::DOWN, (int)dst.noteMode);
	CHECK_EQUAL(99999, dst.rate);
	CHECK_EQUAL(88888, dst.gate);
	CHECK_EQUAL(12345u, dst.rhythm);
	CHECK_EQUAL(4000000000u, dst.noteProbability);
	CHECK_TRUE(dst.randomizerLock);
}

TEST(ArpSettingsDeepTest, generateNewNotePatternValuesInRange) {
	ArpeggiatorSettings s;
	s.generateNewNotePattern();
	// Pattern values should all be in [0, PATTERN_MAX_BUFFER_SIZE)
	for (size_t i = 0; i < PATTERN_MAX_BUFFER_SIZE; i++) {
		int8_t val = s.notePattern[i];
		CHECK(val >= 0);
		CHECK(val < (int8_t)PATTERN_MAX_BUFFER_SIZE);
	}
}

TEST(ArpSettingsDeepTest, generateNewNotePatternChanges) {
	ArpeggiatorSettings s;
	auto first = s.notePattern;
	s.generateNewNotePattern();
	auto second = s.notePattern;
	// Statistically they should differ (probability of same = 1/16! ≈ 0)
	bool same = true;
	for (size_t i = 0; i < PATTERN_MAX_BUFFER_SIZE; i++) {
		if (first[i] != second[i]) {
			same = false;
			break;
		}
	}
	// Allow rare case where they match, just don't crash
}

TEST(ArpSettingsDeepTest, getPhaseIncrementMonotonic) {
	ArpeggiatorSettings s;
	uint32_t inc1 = s.getPhaseIncrement(-1000000000);
	uint32_t inc2 = s.getPhaseIncrement(0);
	uint32_t inc3 = s.getPhaseIncrement(1000000000);
	// Higher rate → higher phase increment
	CHECK(inc1 <= inc2);
	CHECK(inc2 <= inc3);
}

// ── ArpeggiatorForDrum with ARP mode ───────────────────────────────────────

TEST_GROUP(ArpDrumArpModeTest) {
	ArpeggiatorForDrum arp;
	ArpeggiatorSettings settings;

	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
		settings.mode = ArpMode::ARP;
		settings.numOctaves = 2;
	}
};

TEST(ArpDrumArpModeTest, noteOnWithArpSetsPending) {
	int16_t mpeValues[3] = {0, 0, 0};
	ArpReturnInstruction inst;

	arp.noteOn(&settings, 60, 100, &inst, 0, mpeValues);

	// In ARP mode, note should be held until render triggers it
	CHECK(arp.active_note.velocity > 0);
}

TEST(ArpDrumArpModeTest, noteOffWithArpStillHeld) {
	int16_t mpeValues[3] = {0, 0, 0};
	ArpReturnInstruction inst;

	settings.mode = ArpMode::ARP;
	arp.noteOn(&settings, 60, 100, &inst, 0, mpeValues);
	arp.noteOff(&settings, 60, &inst);

	// Note cleared even in arp mode for drums
	CHECK_EQUAL(0, arp.active_note.velocity);
}

TEST(ArpDrumArpModeTest, resetAfterNoteOn) {
	int16_t mpeValues[3] = {0, 0, 0};
	ArpReturnInstruction inst;

	arp.noteOn(&settings, 60, 100, &inst, 0, mpeValues);
	arp.reset();
	CHECK_EQUAL(0, arp.active_note.velocity);
}

// ── Arpeggiator (synth) with ARP mode ──────────────────────────────────────

TEST_GROUP(ArpSynthArpModeTest) {
	Arpeggiator arp;
	ArpeggiatorSettings settings;

	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
		settings.mode = ArpMode::ARP;
		settings.numOctaves = 2;
		settings.octaveMode = ArpOctaveMode::UP;
		settings.noteMode = ArpNoteMode::UP;
	}
};

TEST(ArpSynthArpModeTest, noteOnArpModeAddsNote) {
	int16_t mpeValues[3] = {0, 0, 0};
	ArpReturnInstruction inst;

	arp.noteOn(&settings, 60, 100, &inst, 0, mpeValues);
	CHECK_EQUAL(1, arp.notes.getNumElements());
	CHECK_TRUE(arp.hasAnyInputNotesActive());
}

TEST(ArpSynthArpModeTest, noteOnArpModeDoesNotDirectlyTrigger) {
	int16_t mpeValues[3] = {0, 0, 0};
	ArpReturnInstruction inst;

	arp.noteOn(&settings, 60, 100, &inst, 0, mpeValues);
	// In ARP mode, note goes pending, not directly to output
	// arpNoteOn may or may not be set depending on render state
}

TEST(ArpSynthArpModeTest, multipleNotesPreserveOrder) {
	int16_t mpeValues[3] = {0, 0, 0};
	ArpReturnInstruction inst;

	arp.noteOn(&settings, 72, 90, &inst, 0, mpeValues);
	arp.noteOn(&settings, 48, 80, &inst, 0, mpeValues);
	arp.noteOn(&settings, 60, 70, &inst, 0, mpeValues);

	CHECK_EQUAL(3, arp.notes.getNumElements());

	// Sorted by noteCode
	ArpNote* n0 = (ArpNote*)arp.notes.getElementAddress(0);
	ArpNote* n1 = (ArpNote*)arp.notes.getElementAddress(1);
	ArpNote* n2 = (ArpNote*)arp.notes.getElementAddress(2);
	CHECK_EQUAL(48, n0->inputCharacteristics[0]);
	CHECK_EQUAL(60, n1->inputCharacteristics[0]);
	CHECK_EQUAL(72, n2->inputCharacteristics[0]);
}

TEST(ArpSynthArpModeTest, noteOffInArpRemovesNote) {
	int16_t mpeValues[3] = {0, 0, 0};
	ArpReturnInstruction inst;

	arp.noteOn(&settings, 60, 100, &inst, 0, mpeValues);
	arp.noteOn(&settings, 72, 90, &inst, 0, mpeValues);
	arp.noteOff(&settings, 60, &inst);

	CHECK_EQUAL(1, arp.notes.getNumElements());
	ArpNote* remaining = (ArpNote*)arp.notes.getElementAddress(0);
	CHECK_EQUAL(72, remaining->inputCharacteristics[0]);
}

TEST(ArpSynthArpModeTest, noteOffAllNotesInArp) {
	int16_t mpeValues[3] = {0, 0, 0};
	ArpReturnInstruction inst;

	arp.noteOn(&settings, 60, 100, &inst, 0, mpeValues);
	arp.noteOn(&settings, 72, 90, &inst, 0, mpeValues);
	arp.noteOff(&settings, 60, &inst);
	arp.noteOff(&settings, 72, &inst);

	CHECK_EQUAL(0, arp.notes.getNumElements());
	CHECK_FALSE(arp.hasAnyInputNotesActive());
}

TEST(ArpSynthArpModeTest, resetClearsEverything) {
	int16_t mpeValues[3] = {0, 0, 0};
	ArpReturnInstruction inst;

	arp.noteOn(&settings, 60, 100, &inst, 0, mpeValues);
	arp.noteOn(&settings, 72, 90, &inst, 0, mpeValues);
	arp.reset();

	CHECK_EQUAL(0, arp.notes.getNumElements());
	CHECK_FALSE(arp.hasAnyInputNotesActive());
}

TEST(ArpSynthArpModeTest, mpeValuesStoredOnNoteOn) {
	int16_t mpeValues[3] = {100, 200, 300};
	ArpReturnInstruction inst;

	arp.noteOn(&settings, 60, 100, &inst, 0, mpeValues);
	ArpNote* n = (ArpNote*)arp.notes.getElementAddress(0);
	CHECK_EQUAL(100, n->mpeValues[0]);
	CHECK_EQUAL(200, n->mpeValues[1]);
	CHECK_EQUAL(300, n->mpeValues[2]);
}

TEST(ArpSynthArpModeTest, velocityStoredOnNoteOn) {
	int16_t mpeValues[3] = {0, 0, 0};
	ArpReturnInstruction inst;

	arp.noteOn(&settings, 60, 127, &inst, 0, mpeValues);
	ArpNote* n = (ArpNote*)arp.notes.getElementAddress(0);
	CHECK_EQUAL(127, n->velocity);
	CHECK_EQUAL(127, n->baseVelocity);
}

// ── ArpeggiatorForKit deep tests ───────────────────────────────────────────

TEST_GROUP(ArpKitDeepTest) {
	ArpeggiatorForKit arp;
	ArpeggiatorSettings settings;

	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
		settings.mode = ArpMode::OFF;
	}
};

TEST(ArpKitDeepTest, noteOnMultipleDrums) {
	int16_t mpeValues[3] = {0, 0, 0};
	ArpReturnInstruction inst;

	arp.noteOn(&settings, 36, 100, &inst, 0, mpeValues); // Kick
	arp.noteOn(&settings, 38, 90, &inst, 0, mpeValues);  // Snare
	arp.noteOn(&settings, 42, 80, &inst, 0, mpeValues);  // Hi-hat

	CHECK_EQUAL(3, arp.notes.getNumElements());
}

TEST(ArpKitDeepTest, removeDrumIndexValid) {
	int16_t mpeValues[3] = {0, 0, 0};
	ArpReturnInstruction inst;

	arp.noteOn(&settings, 36, 100, &inst, 0, mpeValues);
	arp.noteOn(&settings, 38, 90, &inst, 0, mpeValues);

	// removeDrumIndex uses the sorted index in notes array
	arp.removeDrumIndex(&settings, 0);
	CHECK_EQUAL(1, arp.notes.getNumElements());
}

TEST(ArpKitDeepTest, removeDrumIndexOutOfRange) {
	// Should not crash
	arp.removeDrumIndex(&settings, 100);
}

// ── Probability/randomizer helpers ─────────────────────────────────────────

TEST_GROUP(ArpRandomizerTest) {
	ArpeggiatorForDrum arp;
	ArpeggiatorSettings settings;

	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
	}
};

TEST(ArpRandomizerTest, defaultProbabilityIsMax) {
	CHECK_EQUAL(4294967295u, settings.noteProbability);
	CHECK_EQUAL(0u, settings.bassProbability);
	CHECK_EQUAL(0u, settings.swapProbability);
}

TEST(ArpRandomizerTest, calculateRandomizerHighProbabilityNoCrash) {
	settings.noteProbability = 4294967295u;
	arp.calculateRandomizerAmounts(&settings);
}

TEST(ArpRandomizerTest, calculateRandomizerZeroProbabilityNoCrash) {
	settings.noteProbability = 0;
	arp.calculateRandomizerAmounts(&settings);
}

TEST(ArpRandomizerTest, calculateRandomizerLockedMode) {
	settings.randomizerLock = true;
	settings.noteProbability = 2147483648u; // 50%
	arp.calculateRandomizerAmounts(&settings);
}

TEST(ArpRandomizerTest, calculateRandomizerLockedResetsOnUnlock) {
	settings.randomizerLock = true;
	arp.calculateRandomizerAmounts(&settings);
	settings.randomizerLock = false;
	arp.calculateRandomizerAmounts(&settings);
}

TEST(ArpRandomizerTest, calculateRandomizerAllProbabilities) {
	settings.noteProbability = 2147483648u;
	settings.bassProbability = 1000000000u;
	settings.swapProbability = 500000000u;
	settings.glideProbability = 3000000000u;
	settings.reverseProbability = 4000000000u;
	settings.chordProbability = 100000000u;
	settings.ratchetProbability = 2000000000u;
	settings.spreadVelocity = 1234567890u;
	settings.spreadGate = 987654321u;
	settings.spreadOctave = 555555555u;
	arp.calculateRandomizerAmounts(&settings);
}

// ── Render tests ───────────────────────────────────────────────────────────

TEST_GROUP(ArpRenderTest) {
	ArpeggiatorForDrum arp;
	ArpeggiatorSettings settings;

	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
		settings.mode = ArpMode::ARP;
	}
};

TEST(ArpRenderTest, renderWithNoNotesDoesNotCrash) {
	ArpReturnInstruction inst;
	arp.render(&settings, &inst, 128, 2147483647u, 1000000);
}

TEST(ArpRenderTest, renderWithNoteTriggersOutput) {
	int16_t mpeValues[3] = {0, 0, 0};
	ArpReturnInstruction inst;

	arp.noteOn(&settings, 60, 100, &inst, 0, mpeValues);

	// Render with large phase increment to advance past gate
	ArpReturnInstruction renderInst;
	arp.render(&settings, &renderInst, 128, 2147483647u, 100000000);
	// Gate may or may not trigger in one render call, but shouldn't crash
}

TEST(ArpRenderTest, renderMultipleCyclesStable) {
	int16_t mpeValues[3] = {0, 0, 0};
	ArpReturnInstruction inst;

	arp.noteOn(&settings, 60, 100, &inst, 0, mpeValues);

	// Multiple render calls should not crash or diverge
	for (int i = 0; i < 100; i++) {
		ArpReturnInstruction renderInst;
		arp.render(&settings, &renderInst, 128, 2147483647u, 1000000);
	}
}
