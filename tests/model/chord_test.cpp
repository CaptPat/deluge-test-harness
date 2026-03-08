// Chord system regression tests — ported from firmware/tests/unit/chord_tests.cpp

#include "CppUTest/TestHarness.h"
#include "definitions_cxx.hpp"
#include "gui/ui/keyboard/chords.h"

using deluge::gui::ui::keyboard::ChordList;
using deluge::gui::ui::keyboard::NONE;
using deluge::gui::ui::keyboard::Voicing;
using deluge::gui::ui::keyboard::ROOT;
using deluge::gui::ui::keyboard::MAJ3;
using deluge::gui::ui::keyboard::MIN3;
using deluge::gui::ui::keyboard::P4;
using deluge::gui::ui::keyboard::P5;
using deluge::gui::ui::keyboard::DIM5;
using deluge::gui::ui::keyboard::AUG5;
using deluge::gui::ui::keyboard::MIN7;

TEST_GROUP(ChordTests) {
	ChordList chordList;
};

TEST(ChordTests, getChordBoundsCheck) {
	for (int chordNo = 0; chordNo < kUniqueChords; chordNo++) {
		for (int voicingOffset = -5; voicingOffset < 2 * kUniqueVoicings; voicingOffset++) {
			chordList.voicingOffset[chordNo] = voicingOffset;
			Voicing voicing = chordList.getChordVoicing(chordNo);
			bool valid = false;
			for (int i = 0; i < kMaxChordKeyboardSize; i++) {
				int32_t offset = voicing.offsets[i];
				if (offset != NONE) {
					valid = true;
				}
			}
			CHECK(valid);
		}
	}
}

TEST(ChordTests, adjustChordRowOffsetBoundsCheck) {
	chordList.chordRowOffset = 0;
	chordList.adjustChordRowOffset(-1);
	CHECK_EQUAL(0, chordList.chordRowOffset);

	chordList.chordRowOffset = kOffScreenChords;
	chordList.adjustChordRowOffset(1);
	CHECK_EQUAL(kOffScreenChords, chordList.chordRowOffset);

	chordList.chordRowOffset = kUniqueChords / 2;
	chordList.adjustChordRowOffset(0);
	CHECK_EQUAL(kUniqueChords / 2, chordList.chordRowOffset);

	chordList.chordRowOffset = 0;
	chordList.adjustChordRowOffset(1);
	CHECK_EQUAL(1, chordList.chordRowOffset);

	chordList.chordRowOffset = kOffScreenChords;
	chordList.adjustChordRowOffset(-1);
	CHECK_EQUAL(kOffScreenChords - 1, chordList.chordRowOffset);
}

// --- getChordQuality branch coverage ---

TEST(ChordTests, majorChordQuality) {
	NoteSet notes({ROOT, MAJ3, P5});
	CHECK(deluge::gui::ui::keyboard::getChordQuality(notes) == deluge::gui::ui::keyboard::ChordQuality::MAJOR);
}

TEST(ChordTests, minorChordQuality) {
	NoteSet notes({ROOT, MIN3, P5});
	CHECK(deluge::gui::ui::keyboard::getChordQuality(notes) == deluge::gui::ui::keyboard::ChordQuality::MINOR);
}

TEST(ChordTests, dominantChordQuality) {
	NoteSet notes({ROOT, MAJ3, P5, MIN7});
	CHECK(deluge::gui::ui::keyboard::getChordQuality(notes) == deluge::gui::ui::keyboard::ChordQuality::DOMINANT);
}

TEST(ChordTests, diminishedChordQuality) {
	NoteSet notes({ROOT, MIN3, DIM5});
	CHECK(deluge::gui::ui::keyboard::getChordQuality(notes) == deluge::gui::ui::keyboard::ChordQuality::DIMINISHED);
}

TEST(ChordTests, augmentedChordQuality) {
	NoteSet notes({ROOT, MAJ3, AUG5});
	CHECK(deluge::gui::ui::keyboard::getChordQuality(notes) == deluge::gui::ui::keyboard::ChordQuality::AUGMENTED);
}

TEST(ChordTests, otherChordQuality) {
	// Three notes but none matching standard quality patterns (e.g. root + P4 + MIN7)
	NoteSet notes({ROOT, P4, MIN7});
	CHECK(deluge::gui::ui::keyboard::getChordQuality(notes) == deluge::gui::ui::keyboard::ChordQuality::OTHER);
}

TEST(ChordTests, tooFewNotesIsOther) {
	// Less than 3 notes → OTHER
	NoteSet notes({ROOT, MAJ3});
	CHECK(deluge::gui::ui::keyboard::getChordQuality(notes) == deluge::gui::ui::keyboard::ChordQuality::OTHER);
}

// --- validateChordNo boundary tests ---

TEST(ChordTests, validateChordNoNegative) {
	// Negative chord number should be clamped — getChordVoicing calls validateChordNo internally
	Voicing v = chordList.getChordVoicing(-5);
	// Should return chord 0's voicing without crashing
	bool valid = false;
	for (int i = 0; i < kMaxChordKeyboardSize; i++) {
		if (v.offsets[i] != NONE)
			valid = true;
	}
	CHECK(valid);
}

TEST(ChordTests, validateChordNoTooHigh) {
	Voicing v = chordList.getChordVoicing(kUniqueChords + 10);
	bool valid = false;
	for (int i = 0; i < kMaxChordKeyboardSize; i++) {
		if (v.offsets[i] != NONE)
			valid = true;
	}
	CHECK(valid);
}

TEST(ChordTests, adjustVoicingOffsetBoundsCheck) {
	for (int chordNo = 0; chordNo < kUniqueChords; chordNo++) {
		chordList.voicingOffset[chordNo] = 0;
		chordList.adjustVoicingOffset(chordNo, -1);
		CHECK_EQUAL(0, chordList.voicingOffset[chordNo]);

		chordList.voicingOffset[chordNo] = kUniqueVoicings - 1;
		chordList.adjustVoicingOffset(chordNo, 1);
		CHECK_EQUAL(kUniqueVoicings - 1, chordList.voicingOffset[chordNo]);

		chordList.voicingOffset[chordNo] = kUniqueVoicings / 2;
		chordList.adjustVoicingOffset(chordNo, 0);
		CHECK_EQUAL(kUniqueVoicings / 2, chordList.voicingOffset[chordNo]);

		chordList.voicingOffset[chordNo] = 0;
		chordList.adjustVoicingOffset(chordNo, 1);
		CHECK_EQUAL(1, chordList.voicingOffset[chordNo]);

		chordList.voicingOffset[chordNo] = kUniqueVoicings - 1;
		chordList.adjustVoicingOffset(chordNo, -1);
		CHECK_EQUAL(kUniqueVoicings - 2, chordList.voicingOffset[chordNo]);
	}
}
