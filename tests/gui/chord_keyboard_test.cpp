// Regression tests for chord keyboard scale support (PR #88).
//
// The chord keyboard crashed when using melodic minor, harmonic minor,
// or Hungarian minor scales because:
//   1. The qualities[] array was uninitialized (garbage values used as indices)
//   2. These scales were not in acceptedScales, so qualities[] was never populated
//
// The fix: zero-initialize qualities{} and add the three minor scale variants
// to acceptedScales. These tests verify getChordQuality() produces valid
// results for all 7-note scale modes, ensuring no out-of-bounds access.

#include "CppUTest/TestHarness.h"
#include "gui/ui/keyboard/chords.h"
#include "model/scale/note_set.h"
#include "model/scale/preset_scales.h"

using deluge::gui::ui::keyboard::ChordQuality;
using deluge::gui::ui::keyboard::getChordQuality;

TEST_GROUP(ChordKeyboardScales) {};

// For each degree of a scale, modulate by the inverse of that degree's
// semitone offset, then check that getChordQuality returns a valid enum.
// This mirrors the loop in chord_keyboard.cpp that populates qualities[].
static void verifyAllModesProduceValidQuality(NoteSet scaleNotes) {
	for (int i = 0; i < static_cast<int>(scaleNotes.count()); i++) {
		int semitone = scaleNotes[i];
		NoteSet mode = scaleNotes.modulateByOffset(kOctaveSize - semitone);
		ChordQuality q = getChordQuality(mode);
		int qi = static_cast<int>(q);
		CHECK(qi >= 0);
		CHECK(qi < static_cast<int>(ChordQuality::CHORD_QUALITY_MAX));
	}
}

TEST(ChordKeyboardScales, majorScaleAllModesValid) {
	verifyAllModesProduceValidQuality(presetScaleNotes[MAJOR_SCALE]);
}

TEST(ChordKeyboardScales, minorScaleAllModesValid) {
	verifyAllModesProduceValidQuality(presetScaleNotes[MINOR_SCALE]);
}

TEST(ChordKeyboardScales, dorianScaleAllModesValid) {
	verifyAllModesProduceValidQuality(presetScaleNotes[DORIAN_SCALE]);
}

TEST(ChordKeyboardScales, melodicMinorAllModesValid) {
	verifyAllModesProduceValidQuality(presetScaleNotes[MELODIC_MINOR_SCALE]);
}

TEST(ChordKeyboardScales, harmonicMinorAllModesValid) {
	verifyAllModesProduceValidQuality(presetScaleNotes[HARMONIC_MINOR_SCALE]);
}

TEST(ChordKeyboardScales, hungarianMinorAllModesValid) {
	verifyAllModesProduceValidQuality(presetScaleNotes[HUNGARIAN_MINOR_SCALE]);
}

// Verify known chord qualities for major scale modes
TEST(ChordKeyboardScales, majorScaleExpectedQualities) {
	NoteSet ns = presetScaleNotes[MAJOR_SCALE];

	// I (Ionian) = Major
	NoteSet mode0 = ns.modulateByOffset(kOctaveSize - ns[0]);
	CHECK_EQUAL(static_cast<int>(ChordQuality::MAJOR), static_cast<int>(getChordQuality(mode0)));

	// ii (Dorian) = Minor
	NoteSet mode1 = ns.modulateByOffset(kOctaveSize - ns[1]);
	CHECK_EQUAL(static_cast<int>(ChordQuality::MINOR), static_cast<int>(getChordQuality(mode1)));

	// vii (Locrian) = Diminished
	NoteSet mode6 = ns.modulateByOffset(kOctaveSize - ns[6]);
	CHECK_EQUAL(static_cast<int>(ChordQuality::DIMINISHED), static_cast<int>(getChordQuality(mode6)));
}

// Verify all preset 7-note scales produce valid qualities
TEST(ChordKeyboardScales, allSevenNotePresetsValid) {
	for (int s = 0; s < NUM_PRESET_SCALES; s++) {
		NoteSet ns = presetScaleNotes[s];
		// Only test 7-note scales (chord keyboard requirement)
		if (ns.count() == 7) {
			verifyAllModesProduceValidQuality(ns);
		}
	}
}
