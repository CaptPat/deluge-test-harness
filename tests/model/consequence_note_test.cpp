// Tests for Phase 15 note-level consequence subclasses:
// ConsequenceNoteArrayChange, ConsequenceNoteExistence,
// ConsequenceNoteRowHorizontalShift, ConsequenceNoteRowMute

#include "CppUTest/TestHarness.h"
#include "model/consequence/consequence_note_array_change.h"
#include "model/consequence/consequence_note_existence.h"
#include "model/consequence/consequence_note_row_horizontal_shift.h"
#include "model/consequence/consequence_note_row_mute.h"
#include "model/note/note.h"
#include "model/note/note_row.h"
#include "model/model_stack.h"
#include "clip_mocks.h"
#include "song_mock.h"

// Helper to build a minimal ModelStack
static uint8_t modelStackMemory[MODEL_STACK_MAX_SIZE] __attribute__((aligned(4)));

static ModelStack* makeModelStack() {
	auto* ms = (ModelStack*)modelStackMemory;
	ms->song = currentSong;
	return ms;
}

// ── ConsequenceNoteArrayChange ──────────────────────────────────────────

TEST_GROUP(ConsequenceNoteArrayChange) {
	InstrumentClip clip;
	NoteRow noteRow{60};

	void setup() {
		clip.noteRowMap[0] = &noteRow;
	}
};

TEST(ConsequenceNoteArrayChange, revertSwapsNoteArray) {
	// Set up original notes: one note at pos=100
	noteRow.notes.insertAtKey(100);
	Note* n = noteRow.notes.getElement(0);
	n->setLength(50);
	n->setVelocity(100);
	CHECK_EQUAL(1, noteRow.notes.getNumElements());

	// Create consequence by stealing the note data
	NoteVector originalNotes;
	originalNotes.cloneFrom(&noteRow.notes);
	ConsequenceNoteArrayChange consequence(&clip, 0, &noteRow.notes, true);

	// After stealing, noteRow should be empty
	CHECK_EQUAL(0, noteRow.notes.getNumElements());

	// Revert (undo) should swap back
	ModelStack* ms = makeModelStack();
	Error err = consequence.revert(BEFORE, ms);
	CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
	CHECK_EQUAL(1, noteRow.notes.getNumElements());
	CHECK_EQUAL(100, noteRow.notes.getElement(0)->pos);
}

TEST(ConsequenceNoteArrayChange, cloneModePreservesOriginal) {
	noteRow.notes.insertAtKey(200);
	Note* n = noteRow.notes.getElement(0);
	n->setLength(30);

	// Clone mode — original notes stay in noteRow
	ConsequenceNoteArrayChange consequence(&clip, 0, &noteRow.notes, false);
	CHECK_EQUAL(1, noteRow.notes.getNumElements());
}

TEST(ConsequenceNoteArrayChange, revertWithInvalidNoteRowReturnsError) {
	NoteVector vec;
	ConsequenceNoteArrayChange consequence(&clip, 999, &vec, false);

	ModelStack* ms = makeModelStack();
	Error err = consequence.revert(BEFORE, ms);
	CHECK_EQUAL(static_cast<int>(Error::BUG), static_cast<int>(err));
}

TEST(ConsequenceNoteArrayChange, doubleRevertRestoresOriginal) {
	noteRow.notes.insertAtKey(50);
	noteRow.notes.getElement(0)->setLength(25);
	noteRow.notes.getElement(0)->setVelocity(80);

	ConsequenceNoteArrayChange consequence(&clip, 0, &noteRow.notes, true);
	CHECK_EQUAL(0, noteRow.notes.getNumElements());

	ModelStack* ms = makeModelStack();
	consequence.revert(BEFORE, ms); // undo — restores notes
	CHECK_EQUAL(1, noteRow.notes.getNumElements());

	consequence.revert(AFTER, ms); // redo — steals again
	CHECK_EQUAL(0, noteRow.notes.getNumElements());
}

// ── ConsequenceNoteExistence ────────────────────────────────────────────

TEST_GROUP(ConsequenceNoteExistence) {
	InstrumentClip clip;
	NoteRow noteRow{60};

	void setup() {
		clip.noteRowMap[0] = &noteRow;
	}
};

TEST(ConsequenceNoteExistence, undoCreationDeletesNote) {
	// Create a note and record its creation
	noteRow.notes.insertAtKey(100);
	Note* n = noteRow.notes.getElement(0);
	n->setLength(50);
	n->setVelocity(100);
	n->setProbability(5);
	n->setLift(64);

	ConsequenceNoteExistence consequence(&clip, 0, n, ExistenceChangeType::CREATE);

	ModelStack* ms = makeModelStack();
	// Undo (time matches type → delete)
	Error err = consequence.revert(static_cast<TimeType>(ExistenceChangeType::CREATE), ms);
	CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
	CHECK_EQUAL(0, noteRow.notes.getNumElements());
}

TEST(ConsequenceNoteExistence, redoCreationRestoresNote) {
	// Create a note, record creation, then delete it
	noteRow.notes.insertAtKey(100);
	Note* n = noteRow.notes.getElement(0);
	n->setLength(50);
	n->setVelocity(100);
	n->setProbability(5);
	n->setLift(64);

	ConsequenceNoteExistence consequence(&clip, 0, n, ExistenceChangeType::CREATE);

	// Remove it manually
	noteRow.notes.deleteAtIndex(0);
	CHECK_EQUAL(0, noteRow.notes.getNumElements());

	// Redo creation: type=CREATE(0), time must differ → use AFTER(1) to trigger create path
	ModelStack* ms = makeModelStack();
	Error err = consequence.revert(AFTER, ms);
	CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
	CHECK_EQUAL(1, noteRow.notes.getNumElements());

	Note* restored = noteRow.notes.getElement(0);
	CHECK_EQUAL(100, restored->pos);
	CHECK_EQUAL(50, restored->getLength());
	CHECK_EQUAL(100, restored->getVelocity());
}

TEST(ConsequenceNoteExistence, undoDeletionRestoresNote) {
	// Record a deletion
	noteRow.notes.insertAtKey(200);
	Note* n = noteRow.notes.getElement(0);
	n->setLength(30);
	n->setVelocity(80);
	n->setProbability(3);
	n->setLift(50);

	ConsequenceNoteExistence consequence(&clip, 0, n, ExistenceChangeType::DELETE);

	// Actually delete it
	noteRow.notes.deleteAtIndex(0);

	// Undo deletion: type=DELETE(1), time must differ → use BEFORE(0) to trigger create path
	ModelStack* ms = makeModelStack();
	Error err = consequence.revert(BEFORE, ms);
	CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
	CHECK_EQUAL(1, noteRow.notes.getNumElements());
	CHECK_EQUAL(200, noteRow.notes.getElement(0)->pos);
	CHECK_EQUAL(30, noteRow.notes.getElement(0)->getLength());
}

TEST(ConsequenceNoteExistence, invalidNoteRowReturnsError) {
	noteRow.notes.insertAtKey(10);
	Note* n = noteRow.notes.getElement(0);
	n->setLength(10);
	n->setVelocity(64);

	ConsequenceNoteExistence consequence(&clip, 999, n, ExistenceChangeType::CREATE);

	ModelStack* ms = makeModelStack();
	Error err = consequence.revert(BEFORE, ms);
	CHECK_EQUAL(static_cast<int>(Error::BUG), static_cast<int>(err));
}

// ── ConsequenceNoteRowHorizontalShift ───────────────────────────────────

TEST_GROUP(ConsequenceNoteRowHorizontalShift) {
	InstrumentClip clip;
	NoteRow noteRow{60};

	void setup() {
		clip.noteRowMap[0] = &noteRow;
		currentSong->currentClip = &clip;
	}
};

TEST(ConsequenceNoteRowHorizontalShift, constructorStoresParams) {
	ConsequenceNoteRowHorizontalShift consequence(0, 48, true, false);
	CHECK_EQUAL(48, consequence.amount);
	CHECK_EQUAL(0, consequence.noteRowId);
	CHECK(consequence.shiftAutomation);
	CHECK_FALSE(consequence.shiftSequenceAndMPE);
}

TEST(ConsequenceNoteRowHorizontalShift, revertUndoCallsShiftWithNegativeAmount) {
	ConsequenceNoteRowHorizontalShift consequence(0, 48, true, true);

	ModelStack* ms = makeModelStack();
	Error err = consequence.revert(BEFORE, ms);
	CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
	CHECK_EQUAL(-48, clip.lastShiftAmount);
	CHECK(clip.lastShiftAutomation);
	CHECK(clip.lastShiftSequenceAndMPE);
}

TEST(ConsequenceNoteRowHorizontalShift, revertRedoCallsShiftWithPositiveAmount) {
	ConsequenceNoteRowHorizontalShift consequence(0, 48, false, true);

	ModelStack* ms = makeModelStack();
	Error err = consequence.revert(AFTER, ms);
	CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
	CHECK_EQUAL(48, clip.lastShiftAmount);
}

// ── ConsequenceNoteRowMute ──────────────────────────────────────────────

TEST_GROUP(ConsequenceNoteRowMute) {
	InstrumentClip clip;
	NoteRow noteRow{60};

	void setup() {
		clip.noteRowMap[0] = &noteRow;
	}
};

TEST(ConsequenceNoteRowMute, constructorStoresClipAndId) {
	ConsequenceNoteRowMute consequence(&clip, 0);
	CHECK_EQUAL(0, consequence.noteRowId);
	POINTERS_EQUAL(&clip, consequence.clip);
}

TEST(ConsequenceNoteRowMute, revertTogglesFromUnmutedToMuted) {
	CHECK_FALSE(noteRow.muted);
	ConsequenceNoteRowMute consequence(&clip, 0);

	ModelStack* ms = makeModelStack();
	Error err = consequence.revert(BEFORE, ms);
	CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
	CHECK(noteRow.muted);
}

TEST(ConsequenceNoteRowMute, revertTogglesFromMutedToUnmuted) {
	noteRow.muted = true;
	ConsequenceNoteRowMute consequence(&clip, 0);

	ModelStack* ms = makeModelStack();
	Error err = consequence.revert(BEFORE, ms);
	CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
	CHECK_FALSE(noteRow.muted);
}

TEST(ConsequenceNoteRowMute, doubleRevertRestoresOriginalState) {
	CHECK_FALSE(noteRow.muted);
	ConsequenceNoteRowMute consequence(&clip, 0);

	ModelStack* ms = makeModelStack();
	consequence.revert(BEFORE, ms);
	CHECK(noteRow.muted);

	consequence.revert(AFTER, ms);
	CHECK_FALSE(noteRow.muted);
}

TEST(ConsequenceNoteRowMute, invalidNoteRowReturnsError) {
	ConsequenceNoteRowMute consequence(&clip, 999);

	ModelStack* ms = makeModelStack();
	Error err = consequence.revert(BEFORE, ms);
	CHECK_EQUAL(static_cast<int>(Error::BUG), static_cast<int>(err));
}
