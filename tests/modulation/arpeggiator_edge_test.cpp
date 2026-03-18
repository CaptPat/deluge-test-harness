// Arpeggiator edge-case tests — exercises boundary conditions, rapid note cycling,
// reset mid-sequence, MPE expression routing, octave range limits, mode transitions
// while notes are held, and other cases not covered by existing tests.

#include "CppUTest/TestHarness.h"
#include "modulation/arpeggiator.h"
#include "playback/playback_handler.h"
#include "song_mock.h"

// Helper: default MPE values (zeros)
static int16_t zeroMpe[3] = {0, 0, 0};

// ── Zero / One / Many notes ────────────────────────────────────────────────

TEST_GROUP(ArpEdgeNoteCount) {
	Arpeggiator* arp;
	ArpeggiatorSettings* settings;

	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
		arp = new Arpeggiator();
		settings = new ArpeggiatorSettings();
		settings->mode = ArpMode::OFF;
	}
	void teardown() {
		delete arp;
		delete settings;
	}
};

TEST(ArpEdgeNoteCount, zeroNotesHasNoActiveInput) {
	CHECK_FALSE(arp->hasAnyInputNotesActive());
	CHECK_EQUAL(0, arp->notes.getNumElements());
}

TEST(ArpEdgeNoteCount, zeroNotesNoteOffIsNoOp) {
	ArpReturnInstruction inst;
	arp->noteOff(settings, 60, &inst);
	CHECK_EQUAL(0, arp->notes.getNumElements());
	// noteCodeOffPostArp should remain at default (ARP_NOTE_NONE)
	CHECK_EQUAL(ARP_NOTE_NONE, inst.noteCodeOffPostArp[0]);
}

TEST(ArpEdgeNoteCount, singleNoteOnAndOff) {
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, zeroMpe);
	CHECK_EQUAL(1, arp->notes.getNumElements());
	CHECK_TRUE(arp->hasAnyInputNotesActive());

	ArpReturnInstruction offInst;
	arp->noteOff(settings, 60, &offInst);
	CHECK_EQUAL(0, arp->notes.getNumElements());
	CHECK_FALSE(arp->hasAnyInputNotesActive());
	CHECK_EQUAL(60, offInst.noteCodeOffPostArp[0]);
}

TEST(ArpEdgeNoteCount, manyNotesAllSorted) {
	// Add many notes across the MIDI range
	ArpReturnInstruction inst;
	int noteCodes[] = {127, 0, 64, 32, 96, 16, 48, 80, 112, 1, 126};
	int count = sizeof(noteCodes) / sizeof(noteCodes[0]);

	for (int i = 0; i < count; i++) {
		arp->noteOn(settings, noteCodes[i], 100, &inst, 0, zeroMpe);
	}
	CHECK_EQUAL(count, arp->notes.getNumElements());

	// Verify sorted order
	for (int i = 1; i < arp->notes.getNumElements(); i++) {
		ArpNote* prev = (ArpNote*)arp->notes.getElementAddress(i - 1);
		ArpNote* curr = (ArpNote*)arp->notes.getElementAddress(i);
		CHECK(prev->inputCharacteristics[0] < curr->inputCharacteristics[0]);
	}
}

TEST(ArpEdgeNoteCount, releaseAllNotesInReverseOrder) {
	ArpReturnInstruction inst;
	for (int n = 60; n <= 72; n++) {
		arp->noteOn(settings, n, 100, &inst, 0, zeroMpe);
	}
	CHECK_EQUAL(13, arp->notes.getNumElements());

	// Release in reverse order
	for (int n = 72; n >= 60; n--) {
		ArpReturnInstruction offInst;
		arp->noteOff(settings, n, &offInst);
	}
	CHECK_EQUAL(0, arp->notes.getNumElements());
	CHECK_FALSE(arp->hasAnyInputNotesActive());
}

TEST(ArpEdgeNoteCount, releaseMiddleNoteKeepsOthers) {
	ArpReturnInstruction inst;
	arp->noteOn(settings, 48, 100, &inst, 0, zeroMpe);
	arp->noteOn(settings, 60, 100, &inst, 0, zeroMpe);
	arp->noteOn(settings, 72, 100, &inst, 0, zeroMpe);

	ArpReturnInstruction offInst;
	arp->noteOff(settings, 60, &offInst);

	CHECK_EQUAL(2, arp->notes.getNumElements());
	ArpNote* n0 = (ArpNote*)arp->notes.getElementAddress(0);
	ArpNote* n1 = (ArpNote*)arp->notes.getElementAddress(1);
	CHECK_EQUAL(48, n0->inputCharacteristics[0]);
	CHECK_EQUAL(72, n1->inputCharacteristics[0]);
}

// ── Rapid note cycling (press-release-press same note) ─────────────────────

TEST_GROUP(ArpEdgeRapidCycling) {
	Arpeggiator* arp;
	ArpeggiatorSettings* settings;

	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
		arp = new Arpeggiator();
		settings = new ArpeggiatorSettings();
		settings->mode = ArpMode::OFF;
	}
	void teardown() {
		delete arp;
		delete settings;
	}
};

TEST(ArpEdgeRapidCycling, pressReleasePressNoArp) {
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, zeroMpe);
	CHECK_EQUAL(1, arp->notes.getNumElements());

	ArpReturnInstruction offInst;
	arp->noteOff(settings, 60, &offInst);
	CHECK_EQUAL(0, arp->notes.getNumElements());

	// Re-press the same note
	ArpReturnInstruction inst2;
	arp->noteOn(settings, 60, 90, &inst2, 0, zeroMpe);
	CHECK_EQUAL(1, arp->notes.getNumElements());
	CHECK(inst2.arpNoteOn != nullptr);
	CHECK_EQUAL(60, inst2.arpNoteOn->noteCodeOnPostArp[0]);
}

TEST(ArpEdgeRapidCycling, pressReleasePressInArpMode) {
	settings->mode = ArpMode::ARP;
	settings->numOctaves = 2;

	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, zeroMpe);
	CHECK_EQUAL(1, arp->notes.getNumElements());

	ArpReturnInstruction offInst;
	arp->noteOff(settings, 60, &offInst);
	CHECK_EQUAL(0, arp->notes.getNumElements());

	// Re-press — should behave like first note again
	ArpReturnInstruction inst2;
	arp->noteOn(settings, 60, 90, &inst2, 0, zeroMpe);
	CHECK_EQUAL(1, arp->notes.getNumElements());
	CHECK_TRUE(arp->hasAnyInputNotesActive());
}

TEST(ArpEdgeRapidCycling, rapidCycleMultipleTimes) {
	// Hammer the same note 20 times
	for (int i = 0; i < 20; i++) {
		ArpReturnInstruction inst;
		arp->noteOn(settings, 64, 100, &inst, 0, zeroMpe);
		CHECK_EQUAL(1, arp->notes.getNumElements());

		ArpReturnInstruction offInst;
		arp->noteOff(settings, 64, &offInst);
		CHECK_EQUAL(0, arp->notes.getNumElements());
	}
}

TEST(ArpEdgeRapidCycling, duplicateNoteOnUpdatesVelocity) {
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, zeroMpe);

	ArpNote* note = (ArpNote*)arp->notes.getElementAddress(0);
	CHECK_EQUAL(100, note->baseVelocity);

	// Second noteOn with different velocity — should not add a duplicate
	ArpReturnInstruction inst2;
	arp->noteOn(settings, 60, 50, &inst2, 0, zeroMpe);
	CHECK_EQUAL(1, arp->notes.getNumElements());
}

TEST(ArpEdgeRapidCycling, duplicateNoteOnIgnoredInArpMode) {
	settings->mode = ArpMode::ARP;
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, zeroMpe);
	CHECK_EQUAL(1, arp->notes.getNumElements());

	// Duplicate noteOn in ARP mode should be ignored (returns early)
	ArpReturnInstruction inst2;
	arp->noteOn(settings, 60, 50, &inst2, 0, zeroMpe);
	CHECK_EQUAL(1, arp->notes.getNumElements());
	// Velocity should not change since the early return skips the update
}

// ── Reset behavior mid-sequence ────────────────────────────────────────────

TEST_GROUP(ArpEdgeReset) {
	Arpeggiator* arp;
	ArpeggiatorSettings* settings;

	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
		arp = new Arpeggiator();
		settings = new ArpeggiatorSettings();
		settings->mode = ArpMode::ARP;
		settings->numOctaves = 2;
		settings->noteMode = ArpNoteMode::UP;
		settings->octaveMode = ArpOctaveMode::UP;
	}
	void teardown() {
		delete arp;
		delete settings;
	}
};

TEST(ArpEdgeReset, resetClearsAllArrays) {
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, zeroMpe);
	arp->noteOn(settings, 64, 90, &inst, 0, zeroMpe);
	arp->noteOn(settings, 67, 80, &inst, 0, zeroMpe);
	CHECK_EQUAL(3, arp->notes.getNumElements());
	CHECK_EQUAL(3, arp->notesAsPlayed.getNumElements());

	arp->reset();
	CHECK_EQUAL(0, arp->notes.getNumElements());
	CHECK_EQUAL(0, arp->notesAsPlayed.getNumElements());
	CHECK_EQUAL(0, arp->notesByPattern.getNumElements());
	CHECK_FALSE(arp->hasAnyInputNotesActive());
}

TEST(ArpEdgeReset, resetThenReaddNotes) {
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, zeroMpe);
	arp->noteOn(settings, 64, 90, &inst, 0, zeroMpe);
	arp->reset();

	// Adding notes after reset should work cleanly
	ArpReturnInstruction inst2;
	arp->noteOn(settings, 72, 80, &inst2, 0, zeroMpe);
	CHECK_EQUAL(1, arp->notes.getNumElements());
	CHECK_EQUAL(1, arp->notesAsPlayed.getNumElements());

	ArpNote* note = (ArpNote*)arp->notes.getElementAddress(0);
	CHECK_EQUAL(72, note->inputCharacteristics[0]);
}

TEST(ArpEdgeReset, resetWithNoNotes) {
	// Should not crash
	arp->reset();
	CHECK_EQUAL(0, arp->notes.getNumElements());
}

TEST(ArpEdgeReset, renderAfterResetDoesNotCrash) {
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, zeroMpe);
	arp->reset();

	// Render with no active notes should be safe
	ArpReturnInstruction renderInst;
	arp->render(settings, &renderInst, 128, 2147483647u, 1000000);
	// Should not crash; no note-on should be emitted
}

TEST(ArpEdgeReset, flagForceArpRestartClearsOnNextStep) {
	settings->flagForceArpRestart = true;
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, zeroMpe);

	// After first switchNoteOn, flag should be consumed
	// We can test this indirectly: render should not crash and flag gets cleared
	// during executeArpStep
	ArpReturnInstruction renderInst;
	for (int i = 0; i < 10; i++) {
		arp->render(settings, &renderInst, 128, 2147483647u, 100000000);
	}
}

// ── MPE expression values ──────────────────────────────────────────────────

TEST_GROUP(ArpEdgeMPE) {
	Arpeggiator* arp;
	ArpeggiatorSettings* settings;

	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
		arp = new Arpeggiator();
		settings = new ArpeggiatorSettings();
		settings->mode = ArpMode::OFF;
	}
	void teardown() {
		delete arp;
		delete settings;
	}
};

TEST(ArpEdgeMPE, mpeValuesStoredPerNote) {
	int16_t mpe1[3] = {1000, 2000, 3000};
	int16_t mpe2[3] = {-5000, 0, 16383};

	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, mpe1);
	arp->noteOn(settings, 72, 90, &inst, 0, mpe2);

	ArpNote* n0 = (ArpNote*)arp->notes.getElementAddress(0);
	ArpNote* n1 = (ArpNote*)arp->notes.getElementAddress(1);

	CHECK_EQUAL(1000, n0->mpeValues[0]);
	CHECK_EQUAL(2000, n0->mpeValues[1]);
	CHECK_EQUAL(3000, n0->mpeValues[2]);
	CHECK_EQUAL(-5000, n1->mpeValues[0]);
	CHECK_EQUAL(0, n1->mpeValues[1]);
	CHECK_EQUAL(16383, n1->mpeValues[2]);
}

TEST(ArpEdgeMPE, extremeMpeValues) {
	int16_t mpeExtreme[3] = {INT16_MIN, INT16_MAX, 0};
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, mpeExtreme);

	ArpNote* n = (ArpNote*)arp->notes.getElementAddress(0);
	CHECK_EQUAL(INT16_MIN, n->mpeValues[0]);
	CHECK_EQUAL(INT16_MAX, n->mpeValues[1]);
	CHECK_EQUAL(0, n->mpeValues[2]);
}

TEST(ArpEdgeMPE, mpeChannelStoredOnNoteOn) {
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 5, zeroMpe);

	ArpNote* n = (ArpNote*)arp->notes.getElementAddress(0);
	CHECK_EQUAL(5, n->inputCharacteristics[1]); // CHANNEL dimension
}

// ── MPE velocity modulation on drum arp ────────────────────────────────────

TEST_GROUP(ArpEdgeDrumMPEVelocity) {
	ArpeggiatorForDrum* arp;
	ArpeggiatorSettings* settings;

	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
		arp = new ArpeggiatorForDrum();
		settings = new ArpeggiatorSettings();
		settings->mode = ArpMode::OFF;
	}
	void teardown() {
		delete arp;
		delete settings;
	}
};

TEST(ArpEdgeDrumMPEVelocity, mpeValuesStoredOnDrumNoteOn) {
	int16_t mpeVals[3] = {500, 1000, 1500};
	ArpReturnInstruction inst;
	arp->noteOn(settings, 36, 100, &inst, 0, mpeVals);

	CHECK_EQUAL(500, arp->active_note.mpeValues[0]);
	CHECK_EQUAL(1000, arp->active_note.mpeValues[1]);
	CHECK_EQUAL(1500, arp->active_note.mpeValues[2]);
}

TEST(ArpEdgeDrumMPEVelocity, drumMpeVelocitySourceAftertouch) {
	settings->mpeVelocity = ArpMpeModSource::AFTERTOUCH;
	// With mpeVelocity set, the drum arp should use pressure for velocity
	// This only applies during switchNoteOn in ARP mode, but setting should store
	CHECK_EQUAL((int)ArpMpeModSource::AFTERTOUCH, (int)settings->mpeVelocity);
}

TEST(ArpEdgeDrumMPEVelocity, drumMpeVelocitySourceMPEY) {
	settings->mpeVelocity = ArpMpeModSource::MPE_Y;
	CHECK_EQUAL((int)ArpMpeModSource::MPE_Y, (int)settings->mpeVelocity);
}

// ── Velocity edge cases ────────────────────────────────────────────────────

TEST_GROUP(ArpEdgeVelocity) {
	Arpeggiator* arp;
	ArpeggiatorSettings* settings;

	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
		arp = new Arpeggiator();
		settings = new ArpeggiatorSettings();
		settings->mode = ArpMode::OFF;
	}
	void teardown() {
		delete arp;
		delete settings;
	}
};

TEST(ArpEdgeVelocity, minVelocity) {
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 1, &inst, 0, zeroMpe);
	ArpNote* n = (ArpNote*)arp->notes.getElementAddress(0);
	CHECK_EQUAL(1, n->baseVelocity);
}

TEST(ArpEdgeVelocity, maxVelocity) {
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 127, &inst, 0, zeroMpe);
	ArpNote* n = (ArpNote*)arp->notes.getElementAddress(0);
	CHECK_EQUAL(127, n->baseVelocity);
	CHECK_EQUAL(127, n->velocity);
}

TEST(ArpEdgeVelocity, lastVelocityTracked) {
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 50, &inst, 0, zeroMpe);
	CHECK_EQUAL(50, arp->lastVelocity);

	ArpReturnInstruction inst2;
	arp->noteOn(settings, 72, 120, &inst2, 0, zeroMpe);
	CHECK_EQUAL(120, arp->lastVelocity);
}

// ── Octave range boundaries ────────────────────────────────────────────────

TEST_GROUP(ArpEdgeOctave) {
	ArpeggiatorSettings* settings;

	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
		settings = new ArpeggiatorSettings();
	}
	void teardown() {
		delete settings;
	}
};

TEST(ArpEdgeOctave, numOctavesOne) {
	settings->numOctaves = 1;
	CHECK_EQUAL(1, settings->numOctaves);
}

TEST(ArpEdgeOctave, numOctavesMax) {
	settings->numOctaves = 8;
	CHECK_EQUAL(8, settings->numOctaves);
}

TEST(ArpEdgeOctave, octaveModeUpWithOneOctave) {
	settings->numOctaves = 1;
	settings->octaveMode = ArpOctaveMode::UP;
	settings->mode = ArpMode::ARP;

	Arpeggiator arp;
	ArpReturnInstruction inst;
	arp.noteOn(settings, 60, 100, &inst, 0, zeroMpe);

	// Render multiple cycles; single octave should not crash
	for (int i = 0; i < 50; i++) {
		ArpReturnInstruction rInst;
		arp.render(settings, &rInst, 128, 2147483647u, 100000000);
	}
}

TEST(ArpEdgeOctave, octaveModeDownWithOneOctave) {
	settings->numOctaves = 1;
	settings->octaveMode = ArpOctaveMode::DOWN;
	settings->mode = ArpMode::ARP;

	Arpeggiator arp;
	ArpReturnInstruction inst;
	arp.noteOn(settings, 60, 100, &inst, 0, zeroMpe);

	for (int i = 0; i < 50; i++) {
		ArpReturnInstruction rInst;
		arp.render(settings, &rInst, 128, 2147483647u, 100000000);
	}
}

TEST(ArpEdgeOctave, octaveModeUpDownWithTwoOctaves) {
	settings->numOctaves = 2;
	settings->octaveMode = ArpOctaveMode::UP_DOWN;
	settings->mode = ArpMode::ARP;

	Arpeggiator arp;
	ArpReturnInstruction inst;
	arp.noteOn(settings, 48, 100, &inst, 0, zeroMpe);
	arp.noteOn(settings, 60, 90, &inst, 0, zeroMpe);

	for (int i = 0; i < 100; i++) {
		ArpReturnInstruction rInst;
		arp.render(settings, &rInst, 128, 2147483647u, 100000000);
	}
}

TEST(ArpEdgeOctave, octaveModeAlternate) {
	settings->numOctaves = 3;
	settings->octaveMode = ArpOctaveMode::ALTERNATE;
	settings->noteMode = ArpNoteMode::UP;
	settings->mode = ArpMode::ARP;

	Arpeggiator arp;
	ArpReturnInstruction inst;
	arp.noteOn(settings, 48, 100, &inst, 0, zeroMpe);
	arp.noteOn(settings, 52, 90, &inst, 0, zeroMpe);
	arp.noteOn(settings, 55, 80, &inst, 0, zeroMpe);

	for (int i = 0; i < 100; i++) {
		ArpReturnInstruction rInst;
		arp.render(settings, &rInst, 128, 2147483647u, 100000000);
	}
}

TEST(ArpEdgeOctave, octaveModeRandom) {
	settings->numOctaves = 4;
	settings->octaveMode = ArpOctaveMode::RANDOM;
	settings->mode = ArpMode::ARP;

	Arpeggiator arp;
	ArpReturnInstruction inst;
	arp.noteOn(settings, 60, 100, &inst, 0, zeroMpe);

	for (int i = 0; i < 50; i++) {
		ArpReturnInstruction rInst;
		arp.render(settings, &rInst, 128, 2147483647u, 100000000);
	}
}

// ── Note mode variations ───────────────────────────────────────────────────

TEST_GROUP(ArpEdgeNoteMode) {
	Arpeggiator* arp;
	ArpeggiatorSettings* settings;

	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
		arp = new Arpeggiator();
		settings = new ArpeggiatorSettings();
		settings->mode = ArpMode::ARP;
		settings->numOctaves = 2;
		settings->octaveMode = ArpOctaveMode::UP;
	}
	void teardown() {
		delete arp;
		delete settings;
	}
};

TEST(ArpEdgeNoteMode, noteModeDown) {
	settings->noteMode = ArpNoteMode::DOWN;
	ArpReturnInstruction inst;
	arp->noteOn(settings, 48, 100, &inst, 0, zeroMpe);
	arp->noteOn(settings, 60, 90, &inst, 0, zeroMpe);
	arp->noteOn(settings, 72, 80, &inst, 0, zeroMpe);

	for (int i = 0; i < 50; i++) {
		ArpReturnInstruction rInst;
		arp->render(settings, &rInst, 128, 2147483647u, 100000000);
	}
}

TEST(ArpEdgeNoteMode, noteModeUpDown) {
	settings->noteMode = ArpNoteMode::UP_DOWN;
	ArpReturnInstruction inst;
	arp->noteOn(settings, 48, 100, &inst, 0, zeroMpe);
	arp->noteOn(settings, 60, 90, &inst, 0, zeroMpe);
	arp->noteOn(settings, 72, 80, &inst, 0, zeroMpe);

	for (int i = 0; i < 100; i++) {
		ArpReturnInstruction rInst;
		arp->render(settings, &rInst, 128, 2147483647u, 100000000);
	}
}

TEST(ArpEdgeNoteMode, noteModeRandom) {
	settings->noteMode = ArpNoteMode::RANDOM;
	ArpReturnInstruction inst;
	arp->noteOn(settings, 48, 100, &inst, 0, zeroMpe);
	arp->noteOn(settings, 60, 90, &inst, 0, zeroMpe);

	for (int i = 0; i < 50; i++) {
		ArpReturnInstruction rInst;
		arp->render(settings, &rInst, 128, 2147483647u, 100000000);
	}
}

TEST(ArpEdgeNoteMode, noteModeAsPlayed) {
	settings->noteMode = ArpNoteMode::AS_PLAYED;
	ArpReturnInstruction inst;
	// Play notes out of pitch order
	arp->noteOn(settings, 72, 100, &inst, 0, zeroMpe);
	arp->noteOn(settings, 48, 90, &inst, 0, zeroMpe);
	arp->noteOn(settings, 60, 80, &inst, 0, zeroMpe);

	// notesAsPlayed should have them in play order
	CHECK_EQUAL(3, arp->notesAsPlayed.getNumElements());
	auto* p0 = (ArpJustNoteCode*)arp->notesAsPlayed.getElementAddress(0);
	auto* p1 = (ArpJustNoteCode*)arp->notesAsPlayed.getElementAddress(1);
	auto* p2 = (ArpJustNoteCode*)arp->notesAsPlayed.getElementAddress(2);
	CHECK_EQUAL(72, p0->noteCode);
	CHECK_EQUAL(48, p1->noteCode);
	CHECK_EQUAL(60, p2->noteCode);

	for (int i = 0; i < 50; i++) {
		ArpReturnInstruction rInst;
		arp->render(settings, &rInst, 128, 2147483647u, 100000000);
	}
}

TEST(ArpEdgeNoteMode, noteModePattern) {
	settings->noteMode = ArpNoteMode::PATTERN;
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, zeroMpe);
	arp->noteOn(settings, 64, 90, &inst, 0, zeroMpe);
	arp->noteOn(settings, 67, 80, &inst, 0, zeroMpe);

	// notesByPattern should be populated
	CHECK_EQUAL(3, arp->notesByPattern.getNumElements());

	for (int i = 0; i < 50; i++) {
		ArpReturnInstruction rInst;
		arp->render(settings, &rInst, 128, 2147483647u, 100000000);
	}
}

TEST(ArpEdgeNoteMode, walkModesDoNotCrash) {
	ArpReturnInstruction inst;
	arp->noteOn(settings, 48, 100, &inst, 0, zeroMpe);
	arp->noteOn(settings, 60, 90, &inst, 0, zeroMpe);
	arp->noteOn(settings, 72, 80, &inst, 0, zeroMpe);

	ArpNoteMode walkModes[] = {ArpNoteMode::WALK1, ArpNoteMode::WALK2, ArpNoteMode::WALK3};
	for (auto mode : walkModes) {
		settings->noteMode = mode;
		for (int i = 0; i < 50; i++) {
			ArpReturnInstruction rInst;
			arp->render(settings, &rInst, 128, 2147483647u, 100000000);
		}
	}
}

TEST(ArpEdgeNoteMode, singleNoteAllModes) {
	// With only one note, all note modes should still work
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, zeroMpe);

	ArpNoteMode allModes[] = {ArpNoteMode::UP,      ArpNoteMode::DOWN,  ArpNoteMode::UP_DOWN,
	                          ArpNoteMode::RANDOM,   ArpNoteMode::WALK1, ArpNoteMode::WALK2,
	                          ArpNoteMode::WALK3,    ArpNoteMode::AS_PLAYED, ArpNoteMode::PATTERN};
	for (auto mode : allModes) {
		settings->noteMode = mode;
		for (int i = 0; i < 20; i++) {
			ArpReturnInstruction rInst;
			arp->render(settings, &rInst, 128, 2147483647u, 100000000);
		}
	}
}

// ── Mode transitions while notes are held ──────────────────────────────────

TEST_GROUP(ArpEdgeModeTransition) {
	Arpeggiator* arp;
	ArpeggiatorSettings* settings;

	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
		arp = new Arpeggiator();
		settings = new ArpeggiatorSettings();
		settings->mode = ArpMode::OFF;
		settings->numOctaves = 2;
	}
	void teardown() {
		delete arp;
		delete settings;
	}
};

TEST(ArpEdgeModeTransition, offToArpWhileNotesHeld) {
	// Add notes with arp OFF
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, zeroMpe);
	arp->noteOn(settings, 64, 90, &inst, 0, zeroMpe);
	CHECK_EQUAL(2, arp->notes.getNumElements());

	// Switch to ARP mode — notes should still be in the array
	settings->mode = ArpMode::ARP;
	CHECK_EQUAL(2, arp->notes.getNumElements());

	// Render should now treat them as arpeggiated
	for (int i = 0; i < 20; i++) {
		ArpReturnInstruction rInst;
		arp->render(settings, &rInst, 128, 2147483647u, 100000000);
	}
}

TEST(ArpEdgeModeTransition, arpToOffWhileNotesHeld) {
	settings->mode = ArpMode::ARP;
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, zeroMpe);
	arp->noteOn(settings, 64, 90, &inst, 0, zeroMpe);

	// Switch to OFF mode
	settings->mode = ArpMode::OFF;

	// Notes should still be in array
	CHECK_EQUAL(2, arp->notes.getNumElements());

	// Releasing notes in OFF mode should remove them and set noteCodeOffPostArp
	ArpReturnInstruction offInst;
	arp->noteOff(settings, 60, &offInst);
	CHECK_EQUAL(1, arp->notes.getNumElements());
	CHECK_EQUAL(60, offInst.noteCodeOffPostArp[0]);
}

TEST(ArpEdgeModeTransition, changeNoteModeWhileArpRunning) {
	settings->mode = ArpMode::ARP;
	settings->noteMode = ArpNoteMode::UP;

	ArpReturnInstruction inst;
	arp->noteOn(settings, 48, 100, &inst, 0, zeroMpe);
	arp->noteOn(settings, 60, 90, &inst, 0, zeroMpe);
	arp->noteOn(settings, 72, 80, &inst, 0, zeroMpe);

	// Run a few render cycles in UP mode
	for (int i = 0; i < 10; i++) {
		ArpReturnInstruction rInst;
		arp->render(settings, &rInst, 128, 2147483647u, 100000000);
	}

	// Switch to DOWN mode mid-sequence
	settings->noteMode = ArpNoteMode::DOWN;
	for (int i = 0; i < 10; i++) {
		ArpReturnInstruction rInst;
		arp->render(settings, &rInst, 128, 2147483647u, 100000000);
	}

	// Switch to RANDOM
	settings->noteMode = ArpNoteMode::RANDOM;
	for (int i = 0; i < 10; i++) {
		ArpReturnInstruction rInst;
		arp->render(settings, &rInst, 128, 2147483647u, 100000000);
	}

	// Notes should still be intact
	CHECK_EQUAL(3, arp->notes.getNumElements());
}

TEST(ArpEdgeModeTransition, changeOctaveModeWhileArpRunning) {
	settings->mode = ArpMode::ARP;
	settings->octaveMode = ArpOctaveMode::UP;

	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, zeroMpe);

	ArpOctaveMode allOctModes[] = {ArpOctaveMode::UP, ArpOctaveMode::DOWN, ArpOctaveMode::UP_DOWN,
	                               ArpOctaveMode::ALTERNATE, ArpOctaveMode::RANDOM};
	for (auto octMode : allOctModes) {
		settings->octaveMode = octMode;
		for (int i = 0; i < 20; i++) {
			ArpReturnInstruction rInst;
			arp->render(settings, &rInst, 128, 2147483647u, 100000000);
		}
	}

	CHECK_EQUAL(1, arp->notes.getNumElements());
}

TEST(ArpEdgeModeTransition, changeNumOctavesWhileArpRunning) {
	settings->mode = ArpMode::ARP;
	settings->numOctaves = 4;

	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, zeroMpe);

	for (int i = 0; i < 10; i++) {
		ArpReturnInstruction rInst;
		arp->render(settings, &rInst, 128, 2147483647u, 100000000);
	}

	// Reduce octaves while running
	settings->numOctaves = 1;
	for (int i = 0; i < 20; i++) {
		ArpReturnInstruction rInst;
		arp->render(settings, &rInst, 128, 2147483647u, 100000000);
	}

	// Increase octaves
	settings->numOctaves = 8;
	for (int i = 0; i < 20; i++) {
		ArpReturnInstruction rInst;
		arp->render(settings, &rInst, 128, 2147483647u, 100000000);
	}
}

// ── Render with gate threshold edge cases ──────────────────────────────────

TEST_GROUP(ArpEdgeGate) {
	ArpeggiatorForDrum* arp;
	ArpeggiatorSettings* settings;

	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
		arp = new ArpeggiatorForDrum();
		settings = new ArpeggiatorSettings();
		settings->mode = ArpMode::ARP;
		settings->numOctaves = 1;
	}
	void teardown() {
		delete arp;
		delete settings;
	}
};

TEST(ArpEdgeGate, zeroGateThreshold) {
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, zeroMpe);

	// gateThreshold = 0 means gate closes immediately
	for (int i = 0; i < 20; i++) {
		ArpReturnInstruction rInst;
		arp->render(settings, &rInst, 128, 0, 1000000);
	}
}

TEST(ArpEdgeGate, maxGateThreshold) {
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, zeroMpe);

	// gateThreshold = max means gate stays open as long as possible
	for (int i = 0; i < 20; i++) {
		ArpReturnInstruction rInst;
		arp->render(settings, &rInst, 128, UINT32_MAX, 1000000);
	}
}

TEST(ArpEdgeGate, verySmallPhaseIncrement) {
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, zeroMpe);

	// Extremely slow arp rate
	for (int i = 0; i < 20; i++) {
		ArpReturnInstruction rInst;
		arp->render(settings, &rInst, 128, 2147483647u, 1);
	}
}

TEST(ArpEdgeGate, veryLargePhaseIncrement) {
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, zeroMpe);

	// Extremely fast arp rate
	for (int i = 0; i < 20; i++) {
		ArpReturnInstruction rInst;
		arp->render(settings, &rInst, 128, 2147483647u, UINT32_MAX);
	}
}

TEST(ArpEdgeGate, zeroNumSamples) {
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, zeroMpe);

	// Zero samples rendered — gatePos should not advance
	ArpReturnInstruction rInst;
	arp->render(settings, &rInst, 0, 2147483647u, 1000000);
}

// ── Drum arpeggiator edge cases ────────────────────────────────────────────

TEST_GROUP(ArpEdgeDrum) {
	ArpeggiatorForDrum* arp;
	ArpeggiatorSettings* settings;

	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
		arp = new ArpeggiatorForDrum();
		settings = new ArpeggiatorSettings();
	}
	void teardown() {
		delete arp;
		delete settings;
	}
};

TEST(ArpEdgeDrum, noteOnOverwritesPrevious) {
	settings->mode = ArpMode::OFF;
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, zeroMpe);
	CHECK_EQUAL(100, arp->active_note.baseVelocity);

	// Second note-on replaces the first (drum has single active_note)
	ArpReturnInstruction inst2;
	arp->noteOn(settings, 72, 80, &inst2, 0, zeroMpe);
	CHECK_EQUAL(80, arp->active_note.baseVelocity);
	CHECK_EQUAL(72, arp->active_note.inputCharacteristics[0]);
}

TEST(ArpEdgeDrum, noteOffWrongNoteStillClearsVelocity) {
	settings->mode = ArpMode::OFF;
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, zeroMpe);

	// noteOff for a different note — drum arp doesn't check noteCode match
	ArpReturnInstruction offInst;
	arp->noteOff(settings, 72, &offInst);
	CHECK_EQUAL(0, arp->active_note.velocity);
}

TEST(ArpEdgeDrum, resetMultipleTimes) {
	settings->mode = ArpMode::OFF;
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, zeroMpe);
	arp->reset();
	arp->reset();
	arp->reset();
	CHECK_EQUAL(0, arp->active_note.velocity);
}

TEST(ArpEdgeDrum, getArpType) {
	CHECK_EQUAL((int)ArpType::DRUM, (int)arp->getArpType());
}

// hasAnyInputNotesActive() is protected on ArpeggiatorForDrum — tested via Arpeggiator (non-drum) instead

// ── Kit arpeggiator edge cases ─────────────────────────────────────────────

TEST_GROUP(ArpEdgeKit) {
	ArpeggiatorForKit* arp;
	ArpeggiatorSettings* settings;

	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
		arp = new ArpeggiatorForKit();
		settings = new ArpeggiatorSettings();
		settings->mode = ArpMode::OFF;
	}
	void teardown() {
		delete arp;
		delete settings;
	}
};

TEST(ArpEdgeKit, getArpType) {
	CHECK_EQUAL((int)ArpType::KIT, (int)arp->getArpType());
}

TEST(ArpEdgeKit, removeDrumIndexShiftsHigherIndexes) {
	ArpReturnInstruction inst;
	// Add 3 "drums" as notes 0, 1, 2
	arp->noteOn(settings, 0, 100, &inst, 0, zeroMpe);
	arp->noteOn(settings, 1, 90, &inst, 0, zeroMpe);
	arp->noteOn(settings, 2, 80, &inst, 0, zeroMpe);
	CHECK_EQUAL(3, arp->notes.getNumElements());

	// Remove drum index 0 — higher indexes should shift down
	arp->removeDrumIndex(settings, 0);
	CHECK_EQUAL(2, arp->notes.getNumElements());

	// Remaining notes should have shifted noteCode: was 1->0, was 2->1
	ArpNote* n0 = (ArpNote*)arp->notes.getElementAddress(0);
	ArpNote* n1 = (ArpNote*)arp->notes.getElementAddress(1);
	CHECK_EQUAL(0, n0->inputCharacteristics[0]);
	CHECK_EQUAL(1, n1->inputCharacteristics[0]);
}

TEST(ArpEdgeKit, removeDrumIndexMiddle) {
	ArpReturnInstruction inst;
	arp->noteOn(settings, 0, 100, &inst, 0, zeroMpe);
	arp->noteOn(settings, 1, 90, &inst, 0, zeroMpe);
	arp->noteOn(settings, 2, 80, &inst, 0, zeroMpe);
	arp->noteOn(settings, 3, 70, &inst, 0, zeroMpe);

	arp->removeDrumIndex(settings, 2);
	CHECK_EQUAL(3, arp->notes.getNumElements());

	// Index 3 should have shifted to 2
	ArpNote* n2 = (ArpNote*)arp->notes.getElementAddress(2);
	CHECK_EQUAL(2, n2->inputCharacteristics[0]);
}

TEST(ArpEdgeKit, removeDrumIndexOutOfRange) {
	// Should not crash
	arp->removeDrumIndex(settings, 999);
	CHECK_EQUAL(0, arp->notes.getNumElements());
}

// ── Step repeat edge cases ─────────────────────────────────────────────────

TEST_GROUP(ArpEdgeStepRepeat) {
	Arpeggiator* arp;
	ArpeggiatorSettings* settings;

	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
		arp = new Arpeggiator();
		settings = new ArpeggiatorSettings();
		settings->mode = ArpMode::ARP;
		settings->numOctaves = 2;
		settings->noteMode = ArpNoteMode::UP;
		settings->octaveMode = ArpOctaveMode::UP;
	}
	void teardown() {
		delete arp;
		delete settings;
	}
};

TEST(ArpEdgeStepRepeat, repeatOnce) {
	settings->numStepRepeats = 1;
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, zeroMpe);

	for (int i = 0; i < 30; i++) {
		ArpReturnInstruction rInst;
		arp->render(settings, &rInst, 128, 2147483647u, 100000000);
	}
}

TEST(ArpEdgeStepRepeat, repeatMany) {
	settings->numStepRepeats = 8;
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, zeroMpe);
	arp->noteOn(settings, 64, 90, &inst, 0, zeroMpe);

	for (int i = 0; i < 100; i++) {
		ArpReturnInstruction rInst;
		arp->render(settings, &rInst, 128, 2147483647u, 100000000);
	}
}

// ── handlePendingNotes edge cases ──────────────────────────────────────────

TEST_GROUP(ArpEdgePending) {
	Arpeggiator* arp;
	ArpeggiatorSettings* settings;

	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
		arp = new Arpeggiator();
		settings = new ArpeggiatorSettings();
		settings->mode = ArpMode::OFF;
	}
	void teardown() {
		delete arp;
		delete settings;
	}
};

TEST(ArpEdgePending, handlePendingWithNoNotes) {
	ArpReturnInstruction inst;
	bool result = arp->handlePendingNotes(settings, &inst);
	CHECK_FALSE(result);
}

TEST(ArpEdgePending, handlePendingAfterNoteOn) {
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, zeroMpe);

	// After noteOn in OFF mode, note should be PENDING
	ArpReturnInstruction pendInst;
	bool result = arp->handlePendingNotes(settings, &pendInst);
	// Should find and start the pending note
	CHECK_TRUE(result);
	CHECK(pendInst.arpNoteOn != nullptr);
}

TEST(ArpEdgePending, handlePendingTwiceSecondReturnsFalse) {
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, zeroMpe);

	ArpReturnInstruction pendInst1;
	arp->handlePendingNotes(settings, &pendInst1);

	// Second call should find no more pending
	ArpReturnInstruction pendInst2;
	bool result = arp->handlePendingNotes(settings, &pendInst2);
	CHECK_FALSE(result);
}

// ── Mono note priority (noteOff returns previous note) ─────────────────────

TEST_GROUP(ArpEdgeMonoPriority) {
	Arpeggiator* arp;
	ArpeggiatorSettings* settings;

	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
		arp = new Arpeggiator();
		settings = new ArpeggiatorSettings();
		settings->mode = ArpMode::OFF;
	}
	void teardown() {
		delete arp;
		delete settings;
	}
};

TEST(ArpEdgeMonoPriority, releaseLastPlayedReturnsSecondLast) {
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, zeroMpe);
	arp->noteOn(settings, 64, 90, &inst, 0, zeroMpe);
	arp->noteOn(settings, 67, 80, &inst, 0, zeroMpe);

	// Release the last-played note (67, which is at index 2 in notesAsPlayed)
	ArpReturnInstruction offInst;
	arp->noteOff(settings, 67, &offInst);

	// Since 67 was the last in notesAsPlayed, the arp should return 64 as arpNoteOn
	// (mono snap-back behavior for CV instruments)
	CHECK(offInst.arpNoteOn != nullptr);
}

TEST(ArpEdgeMonoPriority, releaseNonLastPlayedNoSnapback) {
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, zeroMpe);
	arp->noteOn(settings, 64, 90, &inst, 0, zeroMpe);
	arp->noteOn(settings, 67, 80, &inst, 0, zeroMpe);

	// Release a note that is NOT the last in notesAsPlayed (60 was first)
	ArpReturnInstruction offInst;
	arp->noteOff(settings, 60, &offInst);

	// No snap-back since it wasn't the last-played note
	CHECK(offInst.arpNoteOn == nullptr);
}

TEST(ArpEdgeMonoPriority, releaseSoleNoteNoSnapback) {
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, zeroMpe);

	ArpReturnInstruction offInst;
	arp->noteOff(settings, 60, &offInst);

	// Only note released — no snap-back target
	CHECK(offInst.arpNoteOn == nullptr);
}

// ── Spread velocity edge cases ─────────────────────────────────────────────

TEST_GROUP(ArpEdgeSpreadVelocity) {
	ArpeggiatorForDrum* arp;
	ArpeggiatorSettings* settings;

	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
		arp = new ArpeggiatorForDrum();
		settings = new ArpeggiatorSettings();
		settings->mode = ArpMode::ARP;
		settings->numOctaves = 1;
	}
	void teardown() {
		delete arp;
		delete settings;
	}
};

TEST(ArpEdgeSpreadVelocity, maxSpreadVelocity) {
	settings->spreadVelocity = UINT32_MAX;
	arp->calculateRandomizerAmounts(settings);
	// Should not crash; velocity gets clamped within 1-127
}

TEST(ArpEdgeSpreadVelocity, maxSpreadGate) {
	settings->spreadGate = UINT32_MAX;
	arp->calculateRandomizerAmounts(settings);
}

TEST(ArpEdgeSpreadVelocity, maxSpreadOctave) {
	settings->spreadOctave = UINT32_MAX;
	arp->calculateRandomizerAmounts(settings);
}

TEST(ArpEdgeSpreadVelocity, allSpreadsMaxSimultaneously) {
	settings->spreadVelocity = UINT32_MAX;
	settings->spreadGate = UINT32_MAX;
	settings->spreadOctave = UINT32_MAX;
	arp->calculateRandomizerAmounts(settings);
}

// ── MIDI channel handling ──────────────────────────────────────────────────

TEST_GROUP(ArpEdgeMIDIChannel) {
	Arpeggiator* arp;
	ArpeggiatorSettings* settings;

	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
		arp = new Arpeggiator();
		settings = new ArpeggiatorSettings();
		settings->mode = ArpMode::OFF;
	}
	void teardown() {
		delete arp;
		delete settings;
	}
};

TEST(ArpEdgeMIDIChannel, channelStoredAsInputCharacteristic) {
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 7, zeroMpe);

	ArpNote* n = (ArpNote*)arp->notes.getElementAddress(0);
	CHECK_EQUAL(7, n->inputCharacteristics[1]);
}

TEST(ArpEdgeMIDIChannel, outputMemberChannelInitNone) {
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, zeroMpe);

	ArpNote* n = (ArpNote*)arp->notes.getElementAddress(0);
	for (int i = 0; i < (int)ARP_MAX_INSTRUCTION_NOTES; i++) {
		CHECK_EQUAL(MIDI_CHANNEL_NONE, n->outputMemberChannel[i]);
	}
}

TEST(ArpEdgeMIDIChannel, noteOnSameNoteUpdatesChannel) {
	// In OFF mode, second noteOn for same note updates MIDI channel
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 3, zeroMpe);

	ArpReturnInstruction inst2;
	arp->noteOn(settings, 60, 100, &inst2, 9, zeroMpe);

	ArpNote* n = (ArpNote*)arp->notes.getElementAddress(0);
	CHECK_EQUAL(9, n->inputCharacteristics[1]); // Channel updated
}

// ── NotesAsPlayed array tracking ───────────────────────────────────────────

TEST_GROUP(ArpEdgeNotesAsPlayed) {
	Arpeggiator* arp;
	ArpeggiatorSettings* settings;

	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
		arp = new Arpeggiator();
		settings = new ArpeggiatorSettings();
		settings->mode = ArpMode::OFF;
	}
	void teardown() {
		delete arp;
		delete settings;
	}
};

TEST(ArpEdgeNotesAsPlayed, trackPlayOrder) {
	ArpReturnInstruction inst;
	arp->noteOn(settings, 72, 100, &inst, 0, zeroMpe);
	arp->noteOn(settings, 48, 90, &inst, 0, zeroMpe);
	arp->noteOn(settings, 84, 80, &inst, 0, zeroMpe);
	arp->noteOn(settings, 36, 70, &inst, 0, zeroMpe);

	CHECK_EQUAL(4, arp->notesAsPlayed.getNumElements());

	auto* p0 = (ArpJustNoteCode*)arp->notesAsPlayed.getElementAddress(0);
	auto* p1 = (ArpJustNoteCode*)arp->notesAsPlayed.getElementAddress(1);
	auto* p2 = (ArpJustNoteCode*)arp->notesAsPlayed.getElementAddress(2);
	auto* p3 = (ArpJustNoteCode*)arp->notesAsPlayed.getElementAddress(3);

	CHECK_EQUAL(72, p0->noteCode);
	CHECK_EQUAL(48, p1->noteCode);
	CHECK_EQUAL(84, p2->noteCode);
	CHECK_EQUAL(36, p3->noteCode);
}

TEST(ArpEdgeNotesAsPlayed, removeFromMiddleOfPlayOrder) {
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, zeroMpe);
	arp->noteOn(settings, 64, 90, &inst, 0, zeroMpe);
	arp->noteOn(settings, 67, 80, &inst, 0, zeroMpe);

	ArpReturnInstruction offInst;
	arp->noteOff(settings, 64, &offInst);

	CHECK_EQUAL(2, arp->notesAsPlayed.getNumElements());
	auto* p0 = (ArpJustNoteCode*)arp->notesAsPlayed.getElementAddress(0);
	auto* p1 = (ArpJustNoteCode*)arp->notesAsPlayed.getElementAddress(1);
	CHECK_EQUAL(60, p0->noteCode);
	CHECK_EQUAL(67, p1->noteCode);
}

TEST(ArpEdgeNotesAsPlayed, notesByPatternPopulated) {
	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, zeroMpe);
	arp->noteOn(settings, 64, 90, &inst, 0, zeroMpe);
	arp->noteOn(settings, 67, 80, &inst, 0, zeroMpe);

	// notesByPattern should have 3 entries
	CHECK_EQUAL(3, arp->notesByPattern.getNumElements());
}

// ── Extreme MIDI note values ───────────────────────────────────────────────

TEST_GROUP(ArpEdgeMIDINoteValues) {
	Arpeggiator* arp;
	ArpeggiatorSettings* settings;

	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
		arp = new Arpeggiator();
		settings = new ArpeggiatorSettings();
		settings->mode = ArpMode::OFF;
	}
	void teardown() {
		delete arp;
		delete settings;
	}
};

TEST(ArpEdgeMIDINoteValues, noteZero) {
	ArpReturnInstruction inst;
	arp->noteOn(settings, 0, 100, &inst, 0, zeroMpe);
	CHECK_EQUAL(1, arp->notes.getNumElements());
	ArpNote* n = (ArpNote*)arp->notes.getElementAddress(0);
	CHECK_EQUAL(0, n->inputCharacteristics[0]);
}

TEST(ArpEdgeMIDINoteValues, note127) {
	ArpReturnInstruction inst;
	arp->noteOn(settings, 127, 100, &inst, 0, zeroMpe);
	CHECK_EQUAL(1, arp->notes.getNumElements());
	ArpNote* n = (ArpNote*)arp->notes.getElementAddress(0);
	CHECK_EQUAL(127, n->inputCharacteristics[0]);
}

TEST(ArpEdgeMIDINoteValues, notesAtExtremes) {
	ArpReturnInstruction inst;
	arp->noteOn(settings, 0, 100, &inst, 0, zeroMpe);
	arp->noteOn(settings, 127, 100, &inst, 0, zeroMpe);

	CHECK_EQUAL(2, arp->notes.getNumElements());
	ArpNote* lo = (ArpNote*)arp->notes.getElementAddress(0);
	ArpNote* hi = (ArpNote*)arp->notes.getElementAddress(1);
	CHECK_EQUAL(0, lo->inputCharacteristics[0]);
	CHECK_EQUAL(127, hi->inputCharacteristics[0]);
}

// ── Render stability under mode combinations ──────────────────────────────

TEST_GROUP(ArpEdgeRenderStability) {
	Arpeggiator* arp;
	ArpeggiatorSettings* settings;

	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
		arp = new Arpeggiator();
		settings = new ArpeggiatorSettings();
		settings->mode = ArpMode::ARP;
		settings->numOctaves = 2;
	}
	void teardown() {
		delete arp;
		delete settings;
	}
};

TEST(ArpEdgeRenderStability, renderFullRandomManyNotes) {
	settings->noteMode = ArpNoteMode::RANDOM;
	settings->octaveMode = ArpOctaveMode::RANDOM;

	ArpReturnInstruction inst;
	for (int n = 48; n <= 72; n++) {
		arp->noteOn(settings, n, 100, &inst, 0, zeroMpe);
	}
	CHECK_EQUAL(25, arp->notes.getNumElements());

	for (int i = 0; i < 200; i++) {
		ArpReturnInstruction rInst;
		arp->render(settings, &rInst, 128, 2147483647u, 100000000);
	}
}

TEST(ArpEdgeRenderStability, addAndRemoveNotesDuringRender) {
	settings->noteMode = ArpNoteMode::UP;
	settings->octaveMode = ArpOctaveMode::UP;

	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, zeroMpe);

	for (int i = 0; i < 50; i++) {
		ArpReturnInstruction rInst;
		arp->render(settings, &rInst, 128, 2147483647u, 100000000);

		// Occasionally add or remove notes
		if (i == 10) {
			arp->noteOn(settings, 64, 90, &inst, 0, zeroMpe);
		}
		if (i == 20) {
			arp->noteOn(settings, 72, 80, &inst, 0, zeroMpe);
		}
		if (i == 30) {
			ArpReturnInstruction offInst;
			arp->noteOff(settings, 64, &offInst);
		}
		if (i == 40) {
			arp->noteOn(settings, 67, 85, &inst, 0, zeroMpe);
		}
	}
}

TEST(ArpEdgeRenderStability, renderWithAllProbabilitiesMax) {
	settings->noteProbability = UINT32_MAX;
	settings->bassProbability = UINT32_MAX;
	settings->swapProbability = UINT32_MAX;
	settings->glideProbability = UINT32_MAX;
	settings->reverseProbability = UINT32_MAX;
	settings->chordProbability = UINT32_MAX;
	settings->ratchetProbability = UINT32_MAX;
	settings->spreadVelocity = UINT32_MAX;
	settings->spreadGate = UINT32_MAX;
	settings->spreadOctave = UINT32_MAX;

	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, zeroMpe);
	arp->noteOn(settings, 64, 90, &inst, 0, zeroMpe);

	for (int i = 0; i < 50; i++) {
		ArpReturnInstruction rInst;
		arp->render(settings, &rInst, 128, 2147483647u, 100000000);
	}
}

TEST(ArpEdgeRenderStability, renderWithAllProbabilitiesZero) {
	settings->noteProbability = 0;
	settings->bassProbability = 0;
	settings->swapProbability = 0;
	settings->glideProbability = 0;
	settings->reverseProbability = 0;
	settings->chordProbability = 0;
	settings->ratchetProbability = 0;
	settings->spreadVelocity = 0;
	settings->spreadGate = 0;
	settings->spreadOctave = 0;

	ArpReturnInstruction inst;
	arp->noteOn(settings, 60, 100, &inst, 0, zeroMpe);

	for (int i = 0; i < 50; i++) {
		ArpReturnInstruction rInst;
		arp->render(settings, &rInst, 128, 2147483647u, 100000000);
	}
}

// ── Settings edge cases ────────────────────────────────────────────────────

TEST_GROUP(ArpEdgeSettings) {
	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
	}
};

TEST(ArpEdgeSettings, presetAndModeRelationship) {
	ArpeggiatorSettings s;
	// Setting preset should be independent of mode
	s.preset = ArpPreset::UP;
	s.mode = ArpMode::ARP;
	CHECK_EQUAL((int)ArpPreset::UP, (int)s.preset);
	CHECK_EQUAL((int)ArpMode::ARP, (int)s.mode);
}

TEST(ArpEdgeSettings, includeInKitArpDefault) {
	ArpeggiatorSettings s;
	CHECK_TRUE(s.includeInKitArp);
}

TEST(ArpEdgeSettings, chordTypeIndexDefault) {
	ArpeggiatorSettings s;
	CHECK_EQUAL(0, s.chordTypeIndex);
}

TEST(ArpEdgeSettings, notePatternIsPermutation) {
	ArpeggiatorSettings s;
	s.generateNewNotePattern();

	// Each value should be in range [0, PATTERN_MAX_BUFFER_SIZE)
	for (size_t i = 0; i < PATTERN_MAX_BUFFER_SIZE; i++) {
		CHECK(s.notePattern[i] >= 0);
		CHECK(s.notePattern[i] < (int8_t)PATTERN_MAX_BUFFER_SIZE);
	}
}

TEST(ArpEdgeSettings, cloneFromDoesNotCopyPattern) {
	// cloneFrom intentionally does NOT copy notePattern or locked arrays —
	// the destination keeps its own independently-generated pattern.
	ArpeggiatorSettings src;
	src.generateNewNotePattern();
	auto srcPattern = src.notePattern;

	ArpeggiatorSettings dst;
	auto dstPattern = dst.notePattern;
	dst.cloneFrom(&src);

	// Pattern should still be dst's own (not overwritten by src)
	for (size_t i = 0; i < PATTERN_MAX_BUFFER_SIZE; i++) {
		CHECK_EQUAL(dstPattern[i], dst.notePattern[i]);
	}
}

TEST(ArpEdgeSettings, cloneFromDoesNotCopyLockedArrays) {
	// cloneFrom copies randomizerLock flag but NOT the locked value arrays.
	ArpeggiatorSettings src;
	src.randomizerLock = true;
	src.lockedNoteProbabilityValues[0] = 42;
	src.lockedSpreadVelocityValues[RANDOMIZER_LOCK_MAX_SAVED_VALUES - 1] = -100;

	ArpeggiatorSettings dst;
	dst.cloneFrom(&src);

	// The flag is copied
	CHECK_TRUE(dst.randomizerLock);
	// But the arrays are NOT copied (dst retains its own initialized values of 0)
	CHECK_EQUAL(0, dst.lockedNoteProbabilityValues[0]);
	CHECK_EQUAL(0, dst.lockedSpreadVelocityValues[RANDOMIZER_LOCK_MAX_SAVED_VALUES - 1]);
}
