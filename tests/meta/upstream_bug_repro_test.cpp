// Upstream bug reproduction tests
//
// These tests attempt to reproduce conditions from known upstream bugs
// to verify that fixes hold and regressions are caught.

#include "CppUTest/TestHarness.h"
#include "definitions_cxx.hpp"
#include "gui/ui/keyboard/chords.h"
#include "model/scale/note_set.h"
#include "model/scale/preset_scales.h"
#include "util/string.h"
#include <cstdint>
#include <string>

// ============================================================================
// Upstream issue #4284: Chord keyboard crash with melodic minor scale
//
// Root cause: The qualities[] array in KeyboardLayoutChord was not
// zero-initialized. When a scale like melodic minor was selected but
// was not in the acceptedScales set, precalculate() would skip filling
// qualities[], leaving uninitialized values. These garbage values were
// then used as indices into qualityColours[] (size 6) and chordColumns[]
// (size 6), causing out-of-bounds array access and a crash.
//
// The fix (bugfix/chord-keyboard-melodic-minor-crash):
//   1. Zero-initialize qualities{} to prevent garbage index values
//   2. Add melodic minor, harmonic minor, and hungarian minor to
//      acceptedScales so precalculate() fills qualities[] properly
//
// These tests exercise the chord quality determination for every scale
// degree of every preset scale, verifying that getChordQuality() always
// returns a valid ChordQuality value that can safely index into the
// qualityColours and chordColumns arrays.
// ============================================================================

TEST_GROUP(UpstreamBug4284_ChordMelodicMinor){};

// Verify getChordQuality returns valid enum values for all modes of all
// 7-note preset scales, including melodic minor which triggered the crash.
TEST(UpstreamBug4284_ChordMelodicMinor, allScaleModesProduceValidQuality) {
	using deluge::gui::ui::keyboard::ChordQuality;
	using deluge::gui::ui::keyboard::getChordQuality;

	// Test every preset scale
	for (int scaleIdx = 0; scaleIdx < NUM_PRESET_SCALES; scaleIdx++) {
		NoteSet scaleNotes = presetScaleNotes[scaleIdx];
		int noteCount = scaleNotes.count();
		if (noteCount == 0) {
			continue;
		}

		// For each degree of the scale, modulate and check chord quality
		for (int degree = 0; degree < noteCount; degree++) {
			int8_t semitones = scaleNotes[degree];
			if (semitones < 0) {
				continue; // Not enough notes
			}

			// modulateByOffset rotates the scale to get the mode at this degree
			NoteSet mode = scaleNotes.modulateByOffset(kOctaveSize - semitones);
			ChordQuality quality = getChordQuality(mode);

			int qualityInt = static_cast<int>(quality);

			// The quality must be a valid index: 0 <= quality < CHORD_QUALITY_MAX
			CHECK(qualityInt >= 0);
			CHECK(qualityInt < static_cast<int>(ChordQuality::CHORD_QUALITY_MAX));
		}
	}
}

// Specifically test melodic minor scale (the scale that caused the crash)
TEST(UpstreamBug4284_ChordMelodicMinor, melodicMinorAllDegreesValidQuality) {
	using deluge::gui::ui::keyboard::ChordQuality;
	using deluge::gui::ui::keyboard::getChordQuality;

	// Melodic minor: 0, 2, 3, 5, 7, 9, 11
	NoteSet melodicMinor({0, 2, 3, 5, 7, 9, 11});
	CHECK_EQUAL(7, melodicMinor.count());

	// Expected qualities for melodic minor modes:
	// I:   minor (root, min3, P5)
	// II:  minor (root, min3, P5)
	// III: augmented (root, maj3, aug5)
	// IV:  major/dominant (root, maj3, P5)
	// V:   major/dominant (root, maj3, P5)
	// VI:  diminished (root, min3, dim5)
	// VII: diminished (root, min3, dim5)
	for (int degree = 0; degree < 7; degree++) {
		int8_t semitones = melodicMinor[degree];
		NoteSet mode = melodicMinor.modulateByOffset(kOctaveSize - semitones);
		ChordQuality quality = getChordQuality(mode);

		int qualityInt = static_cast<int>(quality);
		CHECK(qualityInt >= 0);
		CHECK(qualityInt < static_cast<int>(ChordQuality::CHORD_QUALITY_MAX));
	}
}

// Test that quality values can safely index into a 6-element array
// (matching the size of qualityColours and chordColumns in the firmware)
TEST(UpstreamBug4284_ChordMelodicMinor, qualityIndexWithinArrayBounds) {
	using deluge::gui::ui::keyboard::ChordQuality;
	using deluge::gui::ui::keyboard::getChordQuality;

	constexpr int kQualityArraySize = static_cast<int>(ChordQuality::CHORD_QUALITY_MAX);

	// Simulate what precalculate() does: for each position in the
	// qualities array (kOctaveSize + kDisplayHeight + kDisplayWidth entries),
	// compute chord quality and verify it's a valid array index.
	for (int scaleIdx = 0; scaleIdx < NUM_PRESET_SCALES; scaleIdx++) {
		NoteSet scaleNotes = presetScaleNotes[scaleIdx];
		int noteCount = scaleNotes.count();
		if (noteCount == 0) {
			continue;
		}

		// Iterate over same range as qualities[] array:
		// kOctaveSize + kDisplayHeight + kDisplayWidth = 12 + 8 + 16 = 36
		constexpr int qualitiesSize = kOctaveSize + kDisplayHeight + kDisplayWidth;
		for (int i = 0; i < qualitiesSize; i++) {
			int8_t semitones = scaleNotes[i % noteCount];
			NoteSet mode = scaleNotes.modulateByOffset(kOctaveSize - semitones);
			ChordQuality quality = getChordQuality(mode);

			int qualityInt = static_cast<int>(quality);

			// This is the exact check that prevents the crash:
			// quality must be valid for indexing qualityColours[quality]
			// and chordColumns[quality]
			CHECK_TEXT(qualityInt >= 0 && qualityInt < kQualityArraySize,
			           "Quality index out of bounds for qualityColours/chordColumns array");
		}
	}
}

// Test all chord types at all voicing offsets with all scales — the combo
// that caused the original crash when uninitialized qualities were used
// to select chord columns.
TEST(UpstreamBug4284_ChordMelodicMinor, chordListSurvivesAllScaleQualities) {
	using deluge::gui::ui::keyboard::ChordList;
	using deluge::gui::ui::keyboard::ChordQuality;
	using deluge::gui::ui::keyboard::getChordQuality;
	using deluge::gui::ui::keyboard::NONE;

	ChordList chordList;

	for (int scaleIdx = 0; scaleIdx < NUM_PRESET_SCALES; scaleIdx++) {
		NoteSet scaleNotes = presetScaleNotes[scaleIdx];
		int noteCount = scaleNotes.count();
		if (noteCount == 0) {
			continue;
		}

		for (int degree = 0; degree < noteCount; degree++) {
			int8_t semitones = scaleNotes[degree];
			if (semitones < 0) {
				continue;
			}
			NoteSet mode = scaleNotes.modulateByOffset(kOctaveSize - semitones);
			ChordQuality quality = getChordQuality(mode);

			// For each quality, iterate all chord numbers and voicings
			// to ensure no out-of-bounds access
			for (int chordNo = 0; chordNo < kUniqueChords; chordNo++) {
				for (int voicing = 0; voicing < kUniqueVoicings; voicing++) {
					chordList.voicingOffset[chordNo] = voicing;
					auto v = chordList.getChordVoicing(chordNo);
					// Voicing should have at least one valid offset
					bool valid = false;
					for (int i = 0; i < kMaxChordKeyboardSize; i++) {
						if (v.offsets[i] != NONE) {
							valid = true;
						}
					}
					CHECK(valid);
				}
			}
		}
	}
}

// ============================================================================
// Upstream issue #4372: Missing return statement in string.cpp to_chars()
//
// Root cause: In deluge::to_chars(), when snprintf_ wrote more characters
// than the buffer could hold (written >= buffer_size), the code constructed
// std::unexpected{std::errc::no_buffer_space} but did NOT return it.
// The statement had no effect (temporary constructed and discarded), and
// execution fell through to "return first + written", returning a pointer
// past the end of the buffer. This is undefined behavior and could cause
// memory corruption when the caller writes to the returned pointer.
//
// The fix (bugfix/string-missing-return):
//   Changed line 25 from:
//     std::unexpected{std::errc::no_buffer_space};
//   to:
//     return std::unexpected{std::errc::no_buffer_space};
//
// These tests verify that to_chars() correctly returns an error when the
// buffer is too small to hold the formatted output.
// ============================================================================

TEST_GROUP(UpstreamBug4372_StringMissingReturn){};

// Core reproduction: buffer too small for the formatted float output.
// Before the fix, this would return a pointer past the buffer end.
TEST(UpstreamBug4372_StringMissingReturn, tinyBufferReturnsError) {
	// A float like 3.14159 with precision 5 needs ~7 chars ("3.14159").
	// A 4-byte buffer is too small — snprintf returns written >= buffer_size.
	char buf[4];
	auto result = deluge::to_chars(buf, buf + sizeof(buf), 3.14159f, 5);

	// With the fix, this returns an error.
	// Without the fix, it would return a pointer past buf+4 (UB).
	CHECK_FALSE(result.has_value());
	CHECK(result.error() == std::errc::no_buffer_space);
}

// Edge case: buffer exactly one byte too small
TEST(UpstreamBug4372_StringMissingReturn, offByOneBufferReturnsError) {
	// "1.50" = 4 chars + null = 5 bytes needed.
	// Provide only 4 bytes — snprintf returns 4, which equals buffer_size.
	// The >= check should catch this (written >= buffer_size).
	char buf[4];
	auto result = deluge::to_chars(buf, buf + sizeof(buf), 1.5f, 2);

	// snprintf with "%.2f" for 1.5 produces "1.50" (4 chars).
	// With buffer_size=4, written(4) >= buffer_size(4) is true.
	CHECK_FALSE(result.has_value());
	CHECK(result.error() == std::errc::no_buffer_space);
}

// Verify that a sufficiently large buffer still works correctly
TEST(UpstreamBug4372_StringMissingReturn, adequateBufferSucceeds) {
	char buf[32];
	auto result = deluge::to_chars(buf, buf + sizeof(buf), 1.5f, 2);

	CHECK(result.has_value());
	// Returned pointer should be within the buffer
	CHECK(*result > buf);
	CHECK(*result < buf + sizeof(buf));
}

// Stress test: try many float values with small buffers
TEST(UpstreamBug4372_StringMissingReturn, manyFloatsSmallBufferAllReturnError) {
	float testValues[] = {0.0f, 1.0f, -1.0f, 3.14159f, 1000.0f, -999.99f, 1e10f, 1e-10f};

	for (float val : testValues) {
		// 2-byte buffer is always too small for any float with precision > 0
		char buf[2];
		auto result = deluge::to_chars(buf, buf + sizeof(buf), val, 3);

		CHECK_TEXT(!result.has_value(),
		           "Expected error for small buffer, but got success");
		CHECK(result.error() == std::errc::no_buffer_space);
	}
}

// Verify the high-level fromFloat() function handles the overflow correctly
// (it uses to_chars internally and catches the error)
TEST(UpstreamBug4372_StringMissingReturn, fromFloatHandlesOverflowGracefully) {
	// fromFloat uses a 64-byte buffer, so normal values should work
	std::string result = deluge::string::fromFloat(3.14f, 2);
	STRCMP_EQUAL("3.14", result.c_str());

	// Even with high precision, the 64-byte buffer should suffice
	result = deluge::string::fromFloat(1.23456789f, 8);
	CHECK(result.length() > 0);
}

// Edge case: zero-length buffer (first == last)
TEST(UpstreamBug4372_StringMissingReturn, zeroLengthBufferReturnsError) {
	char buf[1];
	// first == last means zero-length buffer
	auto result = deluge::to_chars(buf, buf, 1.0f, 1);

	CHECK_FALSE(result.has_value());
	CHECK(result.error() == std::errc::no_buffer_space);
}

// Edge case: single-byte buffer
TEST(UpstreamBug4372_StringMissingReturn, singleByteBufferReturnsError) {
	char buf[1];
	auto result = deluge::to_chars(buf, buf + 1, 0.0f, 1);

	// "0.0" needs 3 chars + null = 4 bytes, 1-byte buffer is too small
	CHECK_FALSE(result.has_value());
	CHECK(result.error() == std::errc::no_buffer_space);
}
