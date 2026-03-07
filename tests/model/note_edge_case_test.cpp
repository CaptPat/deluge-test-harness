// Edge-case tests for Note fields and NoteVector boundary conditions.
// Covers velocity/probability/lift/fill boundaries, isDrone logic,
// and NoteVector corner cases (duplicate keys, zero pos, large sets).

#include "CppUTest/TestHarness.h"
#include "model/note/note.h"
#include "model/note/note_vector.h"

// ── Note field boundaries ───────────────────────────────────────────────────

TEST_GROUP(NoteFieldBoundaries) {
	Note note;

	void setup() {
		note = Note();
		note.pos = 0;
		note.length = 0;
	}
};

// Velocity tests
TEST(NoteFieldBoundaries, velocityZero) {
	note.setVelocity(0);
	CHECK_EQUAL(0, note.getVelocity());
}

TEST(NoteFieldBoundaries, velocityOne) {
	note.setVelocity(1);
	CHECK_EQUAL(1, note.getVelocity());
}

TEST(NoteFieldBoundaries, velocityMidiMax) {
	note.setVelocity(127);
	CHECK_EQUAL(127, note.getVelocity());
}

TEST(NoteFieldBoundaries, velocityAboveMidiMax) {
	// uint8_t field: 128 is valid storage, even if MIDI caps at 127
	note.setVelocity(128);
	CHECK_EQUAL(128, note.getVelocity());
}

TEST(NoteFieldBoundaries, velocityMaxUint8) {
	note.setVelocity(255);
	CHECK_EQUAL(255, note.getVelocity());
}

TEST(NoteFieldBoundaries, velocityTruncatesAbove255) {
	// setVelocity takes int32_t but field is uint8_t — value truncates
	note.setVelocity(256);
	CHECK_EQUAL(0, note.getVelocity());
}

TEST(NoteFieldBoundaries, velocityNegativeTruncates) {
	// -1 as int32_t → 0xFF when stored in uint8_t
	note.setVelocity(-1);
	CHECK_EQUAL(255, note.getVelocity());
}

// Lift tests (noteOff velocity)
TEST(NoteFieldBoundaries, liftZero) {
	note.setLift(0);
	CHECK_EQUAL(0, note.getLift());
}

TEST(NoteFieldBoundaries, liftMidiMax) {
	note.setLift(127);
	CHECK_EQUAL(127, note.getLift());
}

TEST(NoteFieldBoundaries, liftMaxUint8) {
	note.setLift(255);
	CHECK_EQUAL(255, note.getLift());
}

TEST(NoteFieldBoundaries, liftTruncatesAbove255) {
	note.setLift(256);
	CHECK_EQUAL(0, note.getLift());
}

// Probability tests
TEST(NoteFieldBoundaries, probabilityZero) {
	note.setProbability(0);
	CHECK_EQUAL(0, note.getProbability());
}

TEST(NoteFieldBoundaries, probabilityMaxValue) {
	// kNumProbabilityValues = 20; values up to 20 are "standalone" probabilities
	note.setProbability(20);
	CHECK_EQUAL(20, note.getProbability());
}

TEST(NoteFieldBoundaries, probabilityWithBasedOnPreviousBit) {
	// Bit 7 (0x80) means "based on previous note". Value in lower 7 bits.
	note.setProbability(0x80 | 5); // based-on-previous, probability 5
	CHECK_EQUAL(0x85, note.getProbability());
	CHECK_EQUAL(5, note.getProbability() & 0x7F);
	CHECK(note.getProbability() & 0x80);
}

TEST(NoteFieldBoundaries, probabilityMaxUint8) {
	note.setProbability(255);
	CHECK_EQUAL(255, note.getProbability());
	CHECK_EQUAL(127, note.getProbability() & 0x7F);
	CHECK(note.getProbability() & 0x80);
}

// Fill tests
TEST(NoteFieldBoundaries, fillZero) {
	note.setFill(0);
	CHECK_EQUAL(0, note.getFill());
}

TEST(NoteFieldBoundaries, fillMaxUint8) {
	note.setFill(255);
	CHECK_EQUAL(255, note.getFill());
}

// ── isDrone ──────────────────────────────────────────────────────────────────

TEST_GROUP(NoteDrone) {};

TEST(NoteDrone, isDroneWhenPosZeroAndLengthMatchesEffective) {
	Note note;
	note.pos = 0;
	note.length = 1000;
	CHECK(note.isDrone(1000));
}

TEST(NoteDrone, isNotDroneWhenPosNonZero) {
	Note note;
	note.pos = 1;
	note.length = 1000;
	CHECK_FALSE(note.isDrone(1000));
}

TEST(NoteDrone, isNotDroneWhenLengthShorter) {
	Note note;
	note.pos = 0;
	note.length = 999;
	CHECK_FALSE(note.isDrone(1000));
}

TEST(NoteDrone, isNotDroneWhenLengthLonger) {
	Note note;
	note.pos = 0;
	note.length = 1001;
	CHECK_FALSE(note.isDrone(1000));
}

TEST(NoteDrone, isDroneWithZeroLength) {
	Note note;
	note.pos = 0;
	note.length = 0;
	CHECK(note.isDrone(0));
}

// ── NoteVector edge cases ───────────────────────────────────────────────────

TEST_GROUP(NoteVectorEdgeCases) {};

TEST(NoteVectorEdgeCases, insertAtPositionZero) {
	NoteVector vec;
	int32_t idx = vec.insertAtKey(0);
	CHECK(idx >= 0);
	Note* note = (Note*)vec.getElementAddress(idx);
	note->setVelocity(64);
	CHECK_EQUAL(0, vec.getElement(0)->pos);
}

TEST(NoteVectorEdgeCases, insertDuplicateKeyCreatesTwoEntries) {
	NoteVector vec;
	int32_t idx1 = vec.insertAtKey(100);
	Note* n1 = (Note*)vec.getElementAddress(idx1);
	n1->setVelocity(80);

	// NoteVector allows duplicate keys (multiple notes at same position)
	int32_t idx2 = vec.insertAtKey(100);
	CHECK_EQUAL(2, vec.getNumElements());
}

TEST(NoteVectorEdgeCases, deleteFromSingleElementVector) {
	NoteVector vec;
	vec.insertAtKey(42);
	CHECK_EQUAL(1, vec.getNumElements());

	vec.deleteAtKey(42);
	CHECK_EQUAL(0, vec.getNumElements());
	CHECK(vec.getLast() == nullptr);
}

TEST(NoteVectorEdgeCases, deleteNonExistentKeyIsHarmless) {
	NoteVector vec;
	vec.insertAtKey(10);
	vec.deleteAtKey(999);
	CHECK_EQUAL(1, vec.getNumElements());
}

TEST(NoteVectorEdgeCases, searchExactOnEmpty) {
	NoteVector vec;
	CHECK(vec.searchExact(0) < 0);
}

TEST(NoteVectorEdgeCases, largePositionValues) {
	NoteVector vec;
	int32_t bigPos = 2147483000; // near INT32_MAX
	int32_t idx = vec.insertAtKey(bigPos);
	CHECK(idx >= 0);
	CHECK_EQUAL(bigPos, vec.getElement(0)->pos);
}

TEST(NoteVectorEdgeCases, insertInReverseOrder) {
	NoteVector vec;
	for (int32_t i = 99; i >= 0; i--) {
		vec.insertAtKey(i * 48);
	}
	CHECK_EQUAL(100, vec.getNumElements());

	// Should still be sorted ascending
	for (int32_t i = 0; i < 100; i++) {
		CHECK_EQUAL(i * 48, vec.getElement(i)->pos);
	}
}

TEST(NoteVectorEdgeCases, getLastAfterDeletingLast) {
	NoteVector vec;
	vec.insertAtKey(10);
	vec.insertAtKey(20);
	vec.insertAtKey(30);

	vec.deleteAtKey(30);
	Note* last = vec.getLast();
	CHECK(last != nullptr);
	CHECK_EQUAL(20, last->pos);
}

TEST(NoteVectorEdgeCases, allFieldsSurviveVectorRealloc) {
	NoteVector vec;

	// Insert enough notes to force internal reallocation
	for (int32_t i = 0; i < 50; i++) {
		int32_t idx = vec.insertAtKey(i * 24);
		Note* n = (Note*)vec.getElementAddress(idx);
		n->setLength(12);
		n->setVelocity(i & 0x7F);
		n->setLift((i * 3) & 0x7F);
		n->setProbability(i % 21);
		n->setFill(i & 0x0F);
	}

	// Verify all fields survived potential reallocs
	for (int32_t i = 0; i < 50; i++) {
		Note* n = vec.getElement(i);
		CHECK_EQUAL(i * 24, n->pos);
		CHECK_EQUAL(12, n->getLength());
		CHECK_EQUAL(i & 0x7F, n->getVelocity());
		CHECK_EQUAL((i * 3) & 0x7F, n->getLift());
		CHECK_EQUAL(i % 21, n->getProbability());
		CHECK_EQUAL(i & 0x0F, n->getFill());
	}
}
