// Deep tests for DrumName and drum-related patterns already compiled in the harness.
// drum.cpp itself cannot compile yet (requires instrument_clip_view.h shadow).
// These tests exercise: DrumName linked lists, NoteRow drum fields, Voiced interface,
// DrumType enum, and LearnedMIDI as drum input/mute command.

#include "CppUTest/TestHarness.h"

#include "definitions_cxx.hpp"
#include "io/midi/learned_midi.h"
#include "model/drum/drum_name.h"
#include "model/note/note_row.h"
#include "model/voiced.h"
#include "util/d_string.h"

// ── DrumName Linked List ────────────────────────────────────────────────

TEST_GROUP(DrumNameLinkedList){};

TEST(DrumNameLinkedList, singleNodeNextIsNull) {
	String str;
	str.set("kick");
	DrumName dn(&str);
	CHECK(dn.next == nullptr);
}

TEST(DrumNameLinkedList, chainTwoNodes) {
	String s1, s2;
	s1.set("kick");
	s2.set("snare");
	DrumName dn1(&s1);
	DrumName dn2(&s2);
	dn1.next = &dn2;

	STRCMP_EQUAL("kick", dn1.name.get());
	CHECK(dn1.next != nullptr);
	STRCMP_EQUAL("snare", dn1.next->name.get());
	CHECK(dn1.next->next == nullptr);
}

TEST(DrumNameLinkedList, chainThreeNodes) {
	String s1, s2, s3;
	s1.set("kick");
	s2.set("snare");
	s3.set("hihat");
	DrumName dn1(&s1);
	DrumName dn2(&s2);
	DrumName dn3(&s3);
	dn1.next = &dn2;
	dn2.next = &dn3;

	// Traverse the list and count
	int count = 0;
	DrumName* cur = &dn1;
	while (cur) {
		count++;
		cur = cur->next;
	}
	CHECK_EQUAL(3, count);
}

TEST(DrumNameLinkedList, traverseCollectsNames) {
	String s1, s2, s3;
	s1.set("BD");
	s2.set("SD");
	s3.set("HH");
	DrumName dn1(&s1);
	DrumName dn2(&s2);
	DrumName dn3(&s3);
	dn1.next = &dn2;
	dn2.next = &dn3;

	const char* expected[] = {"BD", "SD", "HH"};
	int i = 0;
	for (DrumName* cur = &dn1; cur; cur = cur->next, i++) {
		STRCMP_EQUAL(expected[i], cur->name.get());
	}
	CHECK_EQUAL(3, i);
}

TEST(DrumNameLinkedList, insertInMiddle) {
	String s1, s2, s3;
	s1.set("A");
	s2.set("C");
	s3.set("B");
	DrumName dn1(&s1);
	DrumName dn2(&s2);
	DrumName dn3(&s3);

	// Chain A -> C
	dn1.next = &dn2;
	// Insert B between A and C
	dn3.next = dn1.next;
	dn1.next = &dn3;

	STRCMP_EQUAL("A", dn1.name.get());
	STRCMP_EQUAL("B", dn1.next->name.get());
	STRCMP_EQUAL("C", dn1.next->next->name.get());
	CHECK(dn1.next->next->next == nullptr);
}

TEST(DrumNameLinkedList, removeFromMiddle) {
	String s1, s2, s3;
	s1.set("X");
	s2.set("Y");
	s3.set("Z");
	DrumName dn1(&s1);
	DrumName dn2(&s2);
	DrumName dn3(&s3);
	dn1.next = &dn2;
	dn2.next = &dn3;

	// Remove Y
	dn1.next = dn2.next;
	dn2.next = nullptr;

	STRCMP_EQUAL("X", dn1.name.get());
	STRCMP_EQUAL("Z", dn1.next->name.get());
	CHECK(dn1.next->next == nullptr);
}

// ── DrumName String Operations ──────────────────────────────────────────

TEST_GROUP(DrumNameStrOps){};

TEST(DrumNameStrOps, longName) {
	String str;
	str.set("VeryLongDrumSampleName_001");
	DrumName dn(&str);
	STRCMP_EQUAL("VeryLongDrumSampleName_001", dn.name.get());
}

TEST(DrumNameStrOps, nameWithSpecialChars) {
	String str;
	str.set("kick-808_v2");
	DrumName dn(&str);
	STRCMP_EQUAL("kick-808_v2", dn.name.get());
}

TEST(DrumNameStrOps, nameIndependentOfSource) {
	// DrumName copies the String, so modifying the source should not affect it
	String str;
	str.set("original");
	DrumName dn(&str);
	str.set("modified");
	// The DrumName should have its own copy
	STRCMP_EQUAL("original", dn.name.get());
}

TEST(DrumNameStrOps, emptyNameLength) {
	String str;
	DrumName dn(&str);
	CHECK_EQUAL(0, dn.name.getLength());
}

TEST(DrumNameStrOps, nonEmptyNameLength) {
	String str;
	str.set("hihat");
	DrumName dn(&str);
	CHECK_EQUAL(5, dn.name.getLength());
}

TEST(DrumNameStrOps, nameIsEmpty) {
	String str;
	DrumName dn(&str);
	CHECK(dn.name.isEmpty());
}

TEST(DrumNameStrOps, nameIsNotEmpty) {
	String str;
	str.set("tom");
	DrumName dn(&str);
	CHECK_FALSE(dn.name.isEmpty());
}

// ── DrumName Heap Lifecycle ─────────────────────────────────────────────

TEST_GROUP(DrumNameHeap){};

TEST(DrumNameHeap, heapAllocatedChain) {
	String s1, s2;
	s1.set("kick");
	s2.set("snare");

	DrumName* head = new DrumName(&s1);
	head->next = new DrumName(&s2);

	STRCMP_EQUAL("kick", head->name.get());
	STRCMP_EQUAL("snare", head->next->name.get());

	// Clean up
	delete head->next;
	delete head;
}

TEST(DrumNameHeap, deleteChainInOrder) {
	String s1, s2, s3;
	s1.set("A");
	s2.set("B");
	s3.set("C");

	DrumName* head = new DrumName(&s1);
	head->next = new DrumName(&s2);
	head->next->next = new DrumName(&s3);

	// Walk and delete (mimics firmware's deleteOldDrumNames pattern)
	DrumName* cur = head;
	while (cur) {
		DrumName* toDelete = cur;
		cur = cur->next;
		delete toDelete;
	}
}

// ── NoteRow Drum Fields ─────────────────────────────────────────────────

TEST_GROUP(NoteRowDrumFields){};

TEST(NoteRowDrumFields, drumPointerInitNull) {
	NoteRow nr(60);
	CHECK(nr.drum == nullptr);
}

TEST(NoteRowDrumFields, firstOldDrumNameInitNull) {
	NoteRow nr(60);
	CHECK(nr.firstOldDrumName == nullptr);
}

TEST(NoteRowDrumFields, canAssignDrumName) {
	NoteRow nr(60);
	String str;
	str.set("kick");
	DrumName dn(&str);
	nr.firstOldDrumName = &dn;

	CHECK(nr.firstOldDrumName != nullptr);
	STRCMP_EQUAL("kick", nr.firstOldDrumName->name.get());
}

TEST(NoteRowDrumFields, drumNameChainOnNoteRow) {
	NoteRow nr(60);
	String s1, s2;
	s1.set("kick_v1");
	s2.set("kick_v2");
	DrumName dn1(&s1);
	DrumName dn2(&s2);
	dn1.next = &dn2;
	nr.firstOldDrumName = &dn1;

	// Traverse old drum names on the NoteRow
	int count = 0;
	for (DrumName* dn = nr.firstOldDrumName; dn; dn = dn->next) {
		count++;
	}
	CHECK_EQUAL(2, count);

	// First is v1, second is v2
	STRCMP_EQUAL("kick_v1", nr.firstOldDrumName->name.get());
	STRCMP_EQUAL("kick_v2", nr.firstOldDrumName->next->name.get());
}

TEST(NoteRowDrumFields, yValuePreserved) {
	// NoteRow's y field is the MIDI note in kit context
	NoteRow nr(36); // C1, typical kick drum MIDI note
	CHECK_EQUAL(36, nr.y);

	NoteRow nr2(38); // D1, typical snare
	CHECK_EQUAL(38, nr2.y);
}

// ── Voiced Interface ────────────────────────────────────────────────────

// Concrete test implementation of Voiced to verify the interface contract
class TestVoiced : public Voiced {
public:
	bool noteIsOn = false;
	bool voicesActive = false;
	bool killed = false;
	bool hibernated = false;
	bool allowTails = true;

	bool anyNoteIsOn() override { return noteIsOn; }
	[[nodiscard]] bool hasActiveVoices() const override { return voicesActive; }
	void killAllVoices() override {
		killed = true;
		voicesActive = false;
		noteIsOn = false;
	}
	bool allowNoteTails(ModelStackWithSoundFlags* /*modelStack*/, bool /*disregardSampleLoop*/ = false) override {
		return allowTails;
	}
	void prepareForHibernation() override { hibernated = true; }
};

TEST_GROUP(VoicedInterface){};

TEST(VoicedInterface, defaultState) {
	TestVoiced v;
	CHECK_FALSE(v.anyNoteIsOn());
	CHECK_FALSE(v.hasActiveVoices());
	CHECK_FALSE(v.killed);
}

TEST(VoicedInterface, noteOnState) {
	TestVoiced v;
	v.noteIsOn = true;
	v.voicesActive = true;
	CHECK(v.anyNoteIsOn());
	CHECK(v.hasActiveVoices());
}

TEST(VoicedInterface, killAllVoices) {
	TestVoiced v;
	v.noteIsOn = true;
	v.voicesActive = true;
	v.killAllVoices();

	CHECK(v.killed);
	CHECK_FALSE(v.anyNoteIsOn());
	CHECK_FALSE(v.hasActiveVoices());
}

TEST(VoicedInterface, allowNoteTailsDefault) {
	TestVoiced v;
	CHECK(v.allowNoteTails(nullptr));
}

TEST(VoicedInterface, disallowNoteTails) {
	TestVoiced v;
	v.allowTails = false;
	CHECK_FALSE(v.allowNoteTails(nullptr));
}

TEST(VoicedInterface, prepareForHibernation) {
	TestVoiced v;
	v.prepareForHibernation();
	CHECK(v.hibernated);
}

TEST(VoicedInterface, polymorphicThroughPointer) {
	TestVoiced v;
	v.noteIsOn = true;
	v.voicesActive = true;

	Voiced* ptr = &v;
	CHECK(ptr->anyNoteIsOn());
	CHECK(ptr->hasActiveVoices());
	ptr->killAllVoices();
	CHECK_FALSE(ptr->anyNoteIsOn());
}

TEST(VoicedInterface, destructorThroughBasePointer) {
	// Verify virtual destructor works through base pointer
	Voiced* ptr = new TestVoiced();
	delete ptr; // Should not leak or crash
}

// ── DrumType Enum ───────────────────────────────────────────────────────

TEST_GROUP(DrumTypeEnum){};

TEST(DrumTypeEnum, soundType) {
	DrumType t = DrumType::SOUND;
	CHECK(t == DrumType::SOUND);
	CHECK(t != DrumType::MIDI);
	CHECK(t != DrumType::GATE);
}

TEST(DrumTypeEnum, midiType) {
	DrumType t = DrumType::MIDI;
	CHECK(t == DrumType::MIDI);
	CHECK(t != DrumType::SOUND);
}

TEST(DrumTypeEnum, gateType) {
	DrumType t = DrumType::GATE;
	CHECK(t == DrumType::GATE);
	CHECK(t != DrumType::SOUND);
}

TEST(DrumTypeEnum, allTypesDistinct) {
	CHECK(DrumType::SOUND != DrumType::MIDI);
	CHECK(DrumType::SOUND != DrumType::GATE);
	CHECK(DrumType::MIDI != DrumType::GATE);
}

// ── LearnedMIDI as Drum Input/Mute ──────────────────────────────────────

TEST_GROUP(DrumMIDIInput){};

TEST(DrumMIDIInput, defaultLearnedMIDIIsEmpty) {
	LearnedMIDI midi;
	// Default-constructed LearnedMIDI should have no note/channel set
	CHECK_FALSE(midi.containsSomething());
}

TEST(DrumMIDIInput, twoIndependentInputs) {
	// Drum has both midiInput and muteMIDICommand as separate LearnedMIDI
	LearnedMIDI midiInput;
	LearnedMIDI muteMIDICommand;

	// They should be independently configurable
	CHECK_FALSE(midiInput.containsSomething());
	CHECK_FALSE(muteMIDICommand.containsSomething());
}

// ── Expression Dimensions ───────────────────────────────────────────────

TEST_GROUP(ExpressionDimensions){};

TEST(ExpressionDimensions, numDimensionsIs3) {
	CHECK_EQUAL(3, kNumExpressionDimensions);
}

TEST(ExpressionDimensions, expressionEnumValues) {
	CHECK_EQUAL(0, Expression::X_PITCH_BEND);
	CHECK_EQUAL(1, Expression::Y_SLIDE_TIMBRE);
	CHECK_EQUAL(2, Expression::Z_PRESSURE);
}

// ── DrumName Search Pattern ─────────────────────────────────────────────
// Tests the search-by-name pattern used in instrument_clip.cpp getNoteRowForDrumName

TEST_GROUP(DrumNameSearch){};

static DrumName* findDrumNameInChain(DrumName* head, const char* target) {
	for (DrumName* cur = head; cur; cur = cur->next) {
		if (strcmp(cur->name.get(), target) == 0) {
			return cur;
		}
	}
	return nullptr;
}

TEST(DrumNameSearch, findExistingName) {
	String s1, s2, s3;
	s1.set("kick");
	s2.set("snare");
	s3.set("hihat");
	DrumName dn1(&s1);
	DrumName dn2(&s2);
	DrumName dn3(&s3);
	dn1.next = &dn2;
	dn2.next = &dn3;

	DrumName* found = findDrumNameInChain(&dn1, "snare");
	CHECK(found != nullptr);
	STRCMP_EQUAL("snare", found->name.get());
}

TEST(DrumNameSearch, findFirstName) {
	String s1, s2;
	s1.set("kick");
	s2.set("snare");
	DrumName dn1(&s1);
	DrumName dn2(&s2);
	dn1.next = &dn2;

	DrumName* found = findDrumNameInChain(&dn1, "kick");
	CHECK(found == &dn1);
}

TEST(DrumNameSearch, findLastName) {
	String s1, s2;
	s1.set("kick");
	s2.set("snare");
	DrumName dn1(&s1);
	DrumName dn2(&s2);
	dn1.next = &dn2;

	DrumName* found = findDrumNameInChain(&dn1, "snare");
	CHECK(found == &dn2);
}

TEST(DrumNameSearch, notFound) {
	String s1;
	s1.set("kick");
	DrumName dn1(&s1);

	DrumName* found = findDrumNameInChain(&dn1, "clap");
	CHECK(found == nullptr);
}

TEST(DrumNameSearch, emptyChain) {
	DrumName* found = findDrumNameInChain(nullptr, "kick");
	CHECK(found == nullptr);
}

TEST(DrumNameSearch, duplicateNamesFindsFirst) {
	String s1, s2;
	s1.set("kick");
	s2.set("kick");
	DrumName dn1(&s1);
	DrumName dn2(&s2);
	dn1.next = &dn2;

	DrumName* found = findDrumNameInChain(&dn1, "kick");
	CHECK(found == &dn1); // Should find the first one
}

// ── Multiple NoteRows with Drum Names ───────────────────────────────────
// Models a Kit scenario where each NoteRow has an old drum name for reassignment

TEST_GROUP(KitDrumNamePattern){};

TEST(KitDrumNamePattern, multipleNoteRowsWithDrumNames) {
	NoteRow rows[3] = {NoteRow(36), NoteRow(38), NoteRow(42)};
	String s1, s2, s3;
	s1.set("kick");
	s2.set("snare");
	s3.set("hihat");
	DrumName dn1(&s1);
	DrumName dn2(&s2);
	DrumName dn3(&s3);

	rows[0].firstOldDrumName = &dn1;
	rows[1].firstOldDrumName = &dn2;
	rows[2].firstOldDrumName = &dn3;

	// Search across note rows for a drum name (like detachFromOutput/reattach pattern)
	NoteRow* foundRow = nullptr;
	for (int i = 0; i < 3; i++) {
		DrumName* found = findDrumNameInChain(rows[i].firstOldDrumName, "snare");
		if (found) {
			foundRow = &rows[i];
			break;
		}
	}

	CHECK(foundRow != nullptr);
	CHECK_EQUAL(38, foundRow->y);
	STRCMP_EQUAL("snare", foundRow->firstOldDrumName->name.get());
}

TEST(KitDrumNamePattern, noteRowWithMultipleOldNames) {
	// A NoteRow that was reassigned multiple times keeps a chain of old names
	NoteRow nr(36);
	String s1, s2, s3;
	s1.set("kick_v3");
	s2.set("kick_v2");
	s3.set("kick_v1");
	DrumName dn1(&s1);
	DrumName dn2(&s2);
	DrumName dn3(&s3);
	dn1.next = &dn2;
	dn2.next = &dn3;
	nr.firstOldDrumName = &dn1;

	// The chain preserves history: most recent first
	STRCMP_EQUAL("kick_v3", nr.firstOldDrumName->name.get());
	STRCMP_EQUAL("kick_v2", nr.firstOldDrumName->next->name.get());
	STRCMP_EQUAL("kick_v1", nr.firstOldDrumName->next->next->name.get());

	// Can find any version
	CHECK(findDrumNameInChain(nr.firstOldDrumName, "kick_v1") != nullptr);
	CHECK(findDrumNameInChain(nr.firstOldDrumName, "kick_v2") != nullptr);
	CHECK(findDrumNameInChain(nr.firstOldDrumName, "kick_v3") != nullptr);
	CHECK(findDrumNameInChain(nr.firstOldDrumName, "kick_v4") == nullptr);
}
