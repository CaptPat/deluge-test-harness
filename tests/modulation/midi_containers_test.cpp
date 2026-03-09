// MIDIParamVector and MidiKnobArray regression tests.

#include "CppUTest/TestHarness.h"
#include "modulation/midi/midi_param.h"
#include "modulation/midi/midi_param_vector.h"
#include "modulation/midi/midi_knob_array.h"
#include "modulation/knob.h"

// ── MIDIParamVector ─────────────────────────────────────────────────────

TEST_GROUP(MIDIParamVectorTest) {};

TEST(MIDIParamVectorTest, emptyAfterConstruction) {
	MIDIParamVector vec;
	CHECK_EQUAL(0, vec.getNumElements());
}

TEST(MIDIParamVectorTest, getOrCreateCreates) {
	MIDIParamVector vec;
	MIDIParam* p = vec.getOrCreateParamFromCC(64);
	CHECK(p != nullptr);
	CHECK_EQUAL(64, p->cc);
	CHECK_EQUAL(1, vec.getNumElements());
}

TEST(MIDIParamVectorTest, getOrCreateReturnsExisting) {
	MIDIParamVector vec;
	MIDIParam* p1 = vec.getOrCreateParamFromCC(64);
	MIDIParam* p2 = vec.getOrCreateParamFromCC(64);
	CHECK(p1 == p2);
	CHECK_EQUAL(1, vec.getNumElements());
}

TEST(MIDIParamVectorTest, getParamFromCCMissing) {
	MIDIParamVector vec;
	MIDIParam* p = vec.getParamFromCC(64);
	CHECK(p == nullptr);
}

TEST(MIDIParamVectorTest, getParamFromCCFound) {
	MIDIParamVector vec;
	vec.getOrCreateParamFromCC(64);
	MIDIParam* p = vec.getParamFromCC(64);
	CHECK(p != nullptr);
	CHECK_EQUAL(64, p->cc);
}

TEST(MIDIParamVectorTest, getOrCreateNoCreation) {
	MIDIParamVector vec;
	MIDIParam* p = vec.getOrCreateParamFromCC(64, 0, false);
	CHECK(p == nullptr);
	CHECK_EQUAL(0, vec.getNumElements());
}

TEST(MIDIParamVectorTest, multipleParamsOrdered) {
	MIDIParamVector vec;
	vec.getOrCreateParamFromCC(70);
	vec.getOrCreateParamFromCC(30);
	vec.getOrCreateParamFromCC(50);

	CHECK_EQUAL(3, vec.getNumElements());

	// OrderedResizeableArray keeps them sorted by key (cc)
	MIDIParam* p0 = vec.getElement(0);
	MIDIParam* p1 = vec.getElement(1);
	MIDIParam* p2 = vec.getElement(2);
	CHECK_EQUAL(30, p0->cc);
	CHECK_EQUAL(50, p1->cc);
	CHECK_EQUAL(70, p2->cc);
}

TEST(MIDIParamVectorTest, getOrCreateWithDefaultValue) {
	MIDIParamVector vec;
	MIDIParam* p = vec.getOrCreateParamFromCC(64, 12345);
	CHECK(p != nullptr);
	CHECK_EQUAL(12345, p->param.getCurrentValue());
}

// ── MidiKnobArray ───────────────────────────────────────────────────────

TEST_GROUP(MidiKnobArrayTest) {};

TEST(MidiKnobArrayTest, emptyAfterConstruction) {
	MidiKnobArray arr;
	CHECK_EQUAL(0, arr.getNumElements());
}

TEST(MidiKnobArrayTest, insertKnobAtEnd) {
	MidiKnobArray arr;
	MIDIKnob* k = arr.insertKnobAtEnd();
	CHECK(k != nullptr);
	CHECK_EQUAL(1, arr.getNumElements());
}

TEST(MidiKnobArrayTest, insertMultiple) {
	MidiKnobArray arr;
	arr.insertKnobAtEnd();
	arr.insertKnobAtEnd();
	arr.insertKnobAtEnd();
	CHECK_EQUAL(3, arr.getNumElements());
}

TEST(MidiKnobArrayTest, getElement) {
	MidiKnobArray arr;
	MIDIKnob* k0 = arr.insertKnobAtEnd();
	MIDIKnob* k1 = arr.insertKnobAtEnd();

	// getElement should return valid distinct pointers
	MIDIKnob* r0 = arr.getElement(0);
	MIDIKnob* r1 = arr.getElement(1);
	CHECK(r0 != nullptr);
	CHECK(r1 != nullptr);
	CHECK(r0 != r1);
}

TEST(MidiKnobArrayTest, insertKnobAtIndex) {
	MidiKnobArray arr;
	arr.insertKnobAtEnd();
	arr.insertKnobAtEnd();

	// Insert at index 0 (beginning)
	MIDIKnob* k = arr.insertKnob(0);
	CHECK(k != nullptr);
	CHECK_EQUAL(3, arr.getNumElements());
}

// ── MIDIKnob virtual method coverage (knob.h lines 37-39) ──────────────

TEST_GROUP(MIDIKnobTest) {};

TEST(MIDIKnobTest, isRelativeTrue) {
	MIDIKnob k;
	k.relative = true;
	CHECK(k.isRelative());
}

TEST(MIDIKnobTest, isRelativeFalse) {
	MIDIKnob k;
	k.relative = false;
	CHECK(!k.isRelative());
}

TEST(MIDIKnobTest, is14BitTrue) {
	MIDIKnob k;
	k.midiInput.noteOrCC = 128;
	CHECK(k.is14Bit());
}

TEST(MIDIKnobTest, is14BitFalse) {
	MIDIKnob k;
	k.midiInput.noteOrCC = 64;
	CHECK(!k.is14Bit());
}

TEST(MIDIKnobTest, topValueIs127NonRelativeBelow128) {
	MIDIKnob k;
	k.midiInput.noteOrCC = 64;
	k.relative = false;
	CHECK(k.topValueIs127());
}

TEST(MIDIKnobTest, topValueIs127RelativeBelow128) {
	MIDIKnob k;
	k.midiInput.noteOrCC = 64;
	k.relative = true;
	CHECK(!k.topValueIs127());
}

TEST(MIDIKnobTest, topValueIs127At128) {
	MIDIKnob k;
	k.midiInput.noteOrCC = 128;
	k.relative = false;
	CHECK(!k.topValueIs127());
}

// ── ModKnob virtual method coverage ────────────────────────────────────

TEST_GROUP(ModKnobTest) {};

TEST(ModKnobTest, isRelativeReturnsTrue) {
	ModKnob k;
	CHECK(k.isRelative());
}

TEST(ModKnobTest, is14BitReturnsFalse) {
	ModKnob k;
	CHECK(!k.is14Bit());
}

TEST(ModKnobTest, topValueIs127ReturnsFalse) {
	ModKnob k;
	CHECK(!k.topValueIs127());
}

// ── MIDIParam ───────────────────────────────────────────────────────────

TEST_GROUP(MIDIParamTest) {};

TEST(MIDIParamTest, constructorDefaults) {
	MIDIParam p;
	// AutoParam embedded — should be default constructed
	CHECK_EQUAL(0, p.param.getCurrentValue());
	CHECK_FALSE(p.param.isAutomated());
}
