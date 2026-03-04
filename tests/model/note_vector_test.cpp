// NoteVector and NoteRowVector regression tests — exercises the firmware's real
// Note/NoteRow container types compiled on x86.

#include "CppUTest/TestHarness.h"
#include "model/note/note.h"
#include "model/note/note_row.h"
#include "model/note/note_row_vector.h"
#include "model/note/note_vector.h"

// ── NoteVector ───────────────────────────────────────────────────────────

TEST_GROUP(NoteVectorTest) {};

TEST(NoteVectorTest, emptyAfterConstruction) {
	NoteVector vec;
	CHECK_EQUAL(0, vec.getNumElements());
	CHECK(vec.getLast() == nullptr);
}

TEST(NoteVectorTest, insertAndRetrieve) {
	NoteVector vec;

	for (int32_t pos = 0; pos < 5; pos++) {
		int32_t idx = vec.insertAtKey(pos * 48);
		CHECK(idx >= 0);
		Note* note = (Note*)vec.getElementAddress(idx);
		note->setLength(24);
		note->setVelocity(100);
	}

	CHECK_EQUAL(5, vec.getNumElements());

	for (int32_t i = 0; i < 5; i++) {
		Note* note = vec.getElement(i);
		CHECK(note != nullptr);
		CHECK_EQUAL(i * 48, note->pos);
		CHECK_EQUAL(24, note->getLength());
		CHECK_EQUAL(100, note->getVelocity());
	}
}

TEST(NoteVectorTest, insertMaintainsPositionOrder) {
	NoteVector vec;

	int32_t positions[] = {192, 0, 96, 288, 48};
	for (int i = 0; i < 5; i++) {
		vec.insertAtKey(positions[i]);
	}

	CHECK_EQUAL(5, vec.getNumElements());

	for (int32_t i = 0; i < vec.getNumElements() - 1; i++) {
		CHECK(vec.getElement(i)->pos <= vec.getElement(i + 1)->pos);
	}
}

TEST(NoteVectorTest, searchByPosition) {
	NoteVector vec;

	for (int32_t pos = 0; pos < 8; pos++) {
		int32_t idx = vec.insertAtKey(pos * 24);
		Note* note = (Note*)vec.getElementAddress(idx);
		note->setVelocity(pos + 60);
	}

	int32_t idx = vec.searchExact(120); // position 120 = 5th note
	CHECK(idx >= 0);
	Note* note = vec.getElement(idx);
	CHECK_EQUAL(120, note->pos);
	CHECK_EQUAL(65, note->getVelocity());

	// Non-existent position
	CHECK(vec.searchExact(13) < 0);
}

TEST(NoteVectorTest, deleteNote) {
	NoteVector vec;

	for (int32_t pos = 0; pos < 5; pos++) {
		vec.insertAtKey(pos * 10);
	}

	vec.deleteAtKey(20);
	CHECK_EQUAL(4, vec.getNumElements());
	CHECK(vec.searchExact(20) < 0);
	CHECK(vec.searchExact(10) >= 0);
	CHECK(vec.searchExact(30) >= 0);
}

TEST(NoteVectorTest, getLast) {
	NoteVector vec;

	vec.insertAtKey(10);
	vec.insertAtKey(50);
	vec.insertAtKey(30);

	Note* last = vec.getLast();
	CHECK(last != nullptr);
	CHECK_EQUAL(50, last->pos);
}

TEST(NoteVectorTest, noteFieldsPreserved) {
	NoteVector vec;

	int32_t idx = vec.insertAtKey(96);
	Note* note = (Note*)vec.getElementAddress(idx);
	note->setLength(48);
	note->setVelocity(127);
	note->setProbability(15);
	note->setLift(80);
	note->setFill(3);
	note->setIterance(Iterance{2, 0b00000101});

	// Retrieve and verify all fields
	Note* retrieved = vec.getElement(0);
	CHECK_EQUAL(96, retrieved->pos);
	CHECK_EQUAL(48, retrieved->getLength());
	CHECK_EQUAL(127, retrieved->getVelocity());
	CHECK_EQUAL(15, retrieved->getProbability());
	CHECK_EQUAL(80, retrieved->getLift());
	CHECK_EQUAL(3, retrieved->getFill());
	CHECK(retrieved->getIterance() == (Iterance{2, 0b00000101}));
}

TEST(NoteVectorTest, manyNotes) {
	NoteVector vec;

	for (int32_t i = 0; i < 64; i++) {
		int32_t idx = vec.insertAtKey(i * 12);
		CHECK(idx >= 0);
		Note* note = (Note*)vec.getElementAddress(idx);
		note->setVelocity(i);
		note->setLength(12);
	}

	CHECK_EQUAL(64, vec.getNumElements());

	for (int32_t i = 0; i < 64; i++) {
		Note* note = vec.getElement(i);
		CHECK_EQUAL(i * 12, note->pos);
		CHECK_EQUAL(i, note->getVelocity());
	}
}

// ── NoteRowVector ────────────────────────────────────────────────────────

TEST_GROUP(NoteRowVectorTest) {};

TEST(NoteRowVectorTest, emptyAfterConstruction) {
	NoteRowVector vec;
	CHECK_EQUAL(0, vec.getNumElements());
}

TEST(NoteRowVectorTest, insertNoteRowAtIndex) {
	NoteRowVector vec;

	NoteRow* row = vec.insertNoteRowAtIndex(0);
	CHECK(row != nullptr);
	CHECK_EQUAL(1, vec.getNumElements());
}

TEST(NoteRowVectorTest, insertNoteRowAtY) {
	NoteRowVector vec;
	int32_t index;

	NoteRow* row = vec.insertNoteRowAtY(60, &index);
	CHECK(row != nullptr);
	CHECK_EQUAL(60, row->y);

	row = vec.insertNoteRowAtY(48, &index);
	CHECK(row != nullptr);
	CHECK_EQUAL(48, row->y);

	row = vec.insertNoteRowAtY(72, &index);
	CHECK(row != nullptr);
	CHECK_EQUAL(72, row->y);

	CHECK_EQUAL(3, vec.getNumElements());

	// Verify sorted by y value
	CHECK_EQUAL(48, vec.getElement(0)->y);
	CHECK_EQUAL(60, vec.getElement(1)->y);
	CHECK_EQUAL(72, vec.getElement(2)->y);
}

TEST(NoteRowVectorTest, deleteNoteRow) {
	NoteRowVector vec;

	vec.insertNoteRowAtY(60, nullptr);
	vec.insertNoteRowAtY(64, nullptr);
	vec.insertNoteRowAtY(67, nullptr);

	CHECK_EQUAL(3, vec.getNumElements());

	vec.deleteNoteRowAtIndex(1); // Delete y=64
	CHECK_EQUAL(2, vec.getNumElements());
	CHECK_EQUAL(60, vec.getElement(0)->y);
	CHECK_EQUAL(67, vec.getElement(1)->y);
}

TEST(NoteRowVectorTest, multipleRows) {
	NoteRowVector vec;

	// Insert 12 note rows (one octave of chromatic notes)
	for (int32_t y = 48; y < 60; y++) {
		NoteRow* row = vec.insertNoteRowAtY(y, nullptr);
		CHECK(row != nullptr);
	}

	CHECK_EQUAL(12, vec.getNumElements());

	// Verify sorted
	for (int32_t i = 0; i < 12; i++) {
		CHECK_EQUAL(48 + i, vec.getElement(i)->y);
	}
}

TEST(NoteRowVectorTest, getElement) {
	NoteRowVector vec;

	vec.insertNoteRowAtY(60, nullptr);
	vec.insertNoteRowAtY(72, nullptr);

	NoteRow* row0 = vec.getElement(0);
	NoteRow* row1 = vec.getElement(1);

	CHECK(row0 != nullptr);
	CHECK(row1 != nullptr);
	CHECK_EQUAL(60, row0->y);
	CHECK_EQUAL(72, row1->y);
}
