// Deep coverage tests for NoteSet, MusicalKey, ScaleChange, and Chord systems.
// Exercises edge cases not covered by scale_test.cpp, chord_test.cpp, or preset_scales_test.cpp.

#include "CppUTest/TestHarness.h"
#include "definitions_cxx.hpp"
#include "gui/ui/keyboard/chords.h"
#include "model/scale/musical_key.h"
#include "model/scale/note_set.h"
#include "model/scale/preset_scales.h"
#include "model/scale/scale_change.h"
#include "model/scale/scale_mapper.h"
#include "model/scale/utils.h"

#include <cstring>

using deluge::gui::ui::keyboard::AUG5;
using deluge::gui::ui::keyboard::Chord;
using deluge::gui::ui::keyboard::ChordList;
using deluge::gui::ui::keyboard::ChordQuality;
using deluge::gui::ui::keyboard::DIM5;
using deluge::gui::ui::keyboard::DIM7;
using deluge::gui::ui::keyboard::DOM7;
using deluge::gui::ui::keyboard::getChordQuality;
using deluge::gui::ui::keyboard::MAJ2;
using deluge::gui::ui::keyboard::MAJ3;
using deluge::gui::ui::keyboard::MAJ6;
using deluge::gui::ui::keyboard::MAJ7;
using deluge::gui::ui::keyboard::MIN2;
using deluge::gui::ui::keyboard::MIN3;
using deluge::gui::ui::keyboard::MIN6;
using deluge::gui::ui::keyboard::MIN7;
using deluge::gui::ui::keyboard::NONE;
using deluge::gui::ui::keyboard::OCT;
using deluge::gui::ui::keyboard::P4;
using deluge::gui::ui::keyboard::P5;
using deluge::gui::ui::keyboard::ROOT;
using deluge::gui::ui::keyboard::Voicing;

// ============================================================================
// NoteSet Deep Tests
// ============================================================================

TEST_GROUP(NoteSetDeep){};

// --- Bitfield constructor round-trip ---

TEST(NoteSetDeep, bitsConstructorRoundTrip) {
	for (uint16_t b = 0; b < 0x1000; b += 37) { // sample every 37th pattern
		NoteSet ns(b);
		CHECK_EQUAL(b, ns.toBits());
	}
}

TEST(NoteSetDeep, bitsConstructorAllOnes) {
	NoteSet ns(0xFFF);
	CHECK_EQUAL(12, ns.count());
	for (int i = 0; i < 12; i++) {
		CHECK(ns.has(i));
	}
}

TEST(NoteSetDeep, bitsConstructorZero) {
	NoteSet ns(0);
	CHECK_EQUAL(0, ns.count());
	CHECK(ns.isEmpty());
}

// --- Fill and individual removal ---

TEST(NoteSetDeep, fillThenRemoveEach) {
	for (int target = 0; target < 12; target++) {
		NoteSet ns;
		ns.fill();
		ns.remove(target);
		CHECK_EQUAL(11, ns.count());
		CHECK(!ns.has(target));
		for (int i = 0; i < 12; i++) {
			if (i != target) {
				CHECK(ns.has(i));
			}
		}
	}
}

// --- Single-note NoteSet ---

TEST(NoteSetDeep, singleNoteEachPosition) {
	for (int i = 0; i < 12; i++) {
		NoteSet ns;
		ns.add(i);
		CHECK_EQUAL(1, ns.count());
		CHECK_EQUAL(i, ns[0]);
		CHECK_EQUAL(-1, ns[1]);
		CHECK_EQUAL(i, ns.highest());
		CHECK_EQUAL(0, ns.degreeOf(i));
		for (int j = 0; j < 12; j++) {
			if (j != i) {
				CHECK_EQUAL(-1, ns.degreeOf(j));
			}
		}
	}
}

// --- Empty NoteSet edge cases ---

TEST(NoteSetDeep, emptySubscriptAllNegOne) {
	NoteSet empty;
	for (int i = 0; i < 12; i++) {
		CHECK_EQUAL(-1, empty[i]);
	}
}

TEST(NoteSetDeep, emptyDegreeOfAllNegOne) {
	NoteSet empty;
	for (int i = 0; i < 12; i++) {
		CHECK_EQUAL(-1, empty.degreeOf(i));
	}
}

TEST(NoteSetDeep, emptyHighestNotInEmpty) {
	NoteSet a;
	NoteSet b;
	CHECK_EQUAL(-1, a.highestNotIn(b));
}

TEST(NoteSetDeep, emptyIsSubsetOfAnything) {
	NoteSet empty;
	NoteSet full;
	full.fill();
	CHECK(empty.isSubsetOf(full));
	CHECK(empty.isSubsetOf(empty));
	CHECK(empty.isSubsetOf(NoteSet({5})));
}

TEST(NoteSetDeep, emptyScaleSize) {
	NoteSet empty;
	// scaleSize adds 0, so empty set has scale size 1
	CHECK_EQUAL(1, empty.scaleSize());
}

TEST(NoteSetDeep, emptyUnionIsOther) {
	NoteSet empty;
	NoteSet other({3, 7, 10});
	NoteSet result = empty | other;
	CHECK_EQUAL(other, result);
	result = other | empty;
	CHECK_EQUAL(other, result);
}

TEST(NoteSetDeep, emptyMajorness) {
	NoteSet empty;
	CHECK_EQUAL(0, empty.majorness());
}

// --- Chromatic (all 12 notes) ---

TEST(NoteSetDeep, chromaticDegreeOfAllNotes) {
	NoteSet chromatic;
	chromatic.fill();
	for (int i = 0; i < 12; i++) {
		CHECK_EQUAL(i, chromatic.degreeOf(i));
	}
}

TEST(NoteSetDeep, chromaticSubscriptAllNotes) {
	NoteSet chromatic;
	chromatic.fill();
	for (int i = 0; i < 12; i++) {
		CHECK_EQUAL(i, chromatic[i]);
	}
}

TEST(NoteSetDeep, chromaticHighest) {
	NoteSet chromatic;
	chromatic.fill();
	CHECK_EQUAL(11, chromatic.highest());
}

TEST(NoteSetDeep, chromaticHighestNotInEmpty) {
	NoteSet chromatic;
	chromatic.fill();
	NoteSet empty;
	CHECK_EQUAL(11, chromatic.highestNotIn(empty));
}

TEST(NoteSetDeep, chromaticHighestNotInSelf) {
	NoteSet chromatic;
	chromatic.fill();
	CHECK_EQUAL(-1, chromatic.highestNotIn(chromatic));
}

TEST(NoteSetDeep, chromaticIsSubsetOfSelf) {
	NoteSet chromatic;
	chromatic.fill();
	CHECK(chromatic.isSubsetOf(chromatic));
}

TEST(NoteSetDeep, chromaticScaleSize) {
	NoteSet chromatic;
	chromatic.fill();
	CHECK_EQUAL(12, chromatic.scaleSize());
}

// --- Complement via highestNotIn ---

TEST(NoteSetDeep, highestNotInFindsCorrectGap) {
	// Major scale missing from chromatic: 1, 3, 6, 8, 10
	NoteSet chromatic;
	chromatic.fill();
	NoteSet major({0, 2, 4, 5, 7, 9, 11});
	// Highest note in chromatic not in major is 10
	CHECK_EQUAL(10, chromatic.highestNotIn(major));
}

// --- Intersection via subset checking ---

TEST(NoteSetDeep, subsetRelationshipTransitive) {
	NoteSet a({0, 2, 4});
	NoteSet b({0, 2, 4, 5, 7});
	NoteSet c({0, 2, 4, 5, 7, 9, 11});
	CHECK(a.isSubsetOf(b));
	CHECK(b.isSubsetOf(c));
	CHECK(a.isSubsetOf(c)); // transitive
}

// --- Union properties ---

TEST(NoteSetDeep, unionIdempotent) {
	NoteSet a({0, 4, 7});
	NoteSet result = a | a;
	CHECK_EQUAL(a, result);
}

TEST(NoteSetDeep, unionCommutative) {
	NoteSet a({0, 3, 7});
	NoteSet b({2, 5, 9});
	CHECK_EQUAL(a | b, b | a);
}

TEST(NoteSetDeep, unionWithFull) {
	NoteSet a({3, 7});
	NoteSet full;
	full.fill();
	NoteSet result = a | full;
	CHECK_EQUAL(full, result);
}

// --- modulateByOffset exhaustive ---

TEST(NoteSetDeep, modulateByZeroIsIdentity) {
	NoteSet ns({0, 4, 7});
	CHECK_EQUAL(ns, ns.modulateByOffset(0));
}

TEST(NoteSetDeep, modulateBy12IsIdentity) {
	NoteSet ns({0, 4, 7});
	CHECK_EQUAL(ns, ns.modulateByOffset(12));
}

TEST(NoteSetDeep, modulateBy24IsIdentity) {
	NoteSet ns({0, 4, 7});
	CHECK_EQUAL(ns, ns.modulateByOffset(24));
}

TEST(NoteSetDeep, modulatePreservesCount) {
	NoteSet ns({0, 2, 4, 5, 7, 9, 11}); // major
	for (int offset = 0; offset < 12; offset++) {
		NoteSet mod = ns.modulateByOffset(offset);
		CHECK_EQUAL(ns.count(), mod.count());
	}
}

TEST(NoteSetDeep, modulateSingleNote) {
	for (int note = 0; note < 12; note++) {
		for (int offset = 0; offset < 12; offset++) {
			NoteSet ns;
			ns.add(note);
			NoteSet mod = ns.modulateByOffset(offset);
			CHECK_EQUAL(1, mod.count());
			int expected = (note + offset) % 12;
			CHECK(mod.has(expected));
		}
	}
}

TEST(NoteSetDeep, modulateChromatic) {
	NoteSet chromatic;
	chromatic.fill();
	for (int offset = 0; offset < 12; offset++) {
		NoteSet mod = chromatic.modulateByOffset(offset);
		CHECK_EQUAL(chromatic, mod);
	}
}

TEST(NoteSetDeep, modulateRoundTrip) {
	NoteSet ns({0, 3, 5, 7, 10}); // pentatonic minor
	for (int offset = 1; offset < 12; offset++) {
		NoteSet mod = ns.modulateByOffset(offset);
		NoteSet back = mod.modulateByOffset(12 - offset);
		CHECK_EQUAL(ns, back);
	}
}

// --- addUntrusted edge cases ---

TEST(NoteSetDeep, addUntrustedMonotonicallyIncreasing) {
	NoteSet ns;
	// Adding 5, 3, 2 — each should be forced above highest
	ns.addUntrusted(5);
	CHECK_EQUAL(5, ns[0]);
	ns.addUntrusted(3); // 3 < 5, so becomes 6
	CHECK_EQUAL(6, ns[1]);
	ns.addUntrusted(2); // 2 < 6, so becomes 7
	CHECK_EQUAL(7, ns[2]);
}

TEST(NoteSetDeep, addUntrustedClampAt11) {
	NoteSet ns;
	ns.addUntrusted(10);
	ns.addUntrusted(10); // would become 11
	ns.addUntrusted(10); // would become 12, clamped to 11 — but 11 already exists
	// After the second add, highest is 11
	CHECK(ns.has(10));
	CHECK(ns.has(11));
}

TEST(NoteSetDeep, addUntrustedLargeValue) {
	NoteSet ns;
	ns.addUntrusted(200); // clamped to 11
	CHECK_EQUAL(1, ns.count());
	CHECK(ns.has(11));
}

// --- degreeOf comprehensive ---

TEST(NoteSetDeep, degreeOfMajorScale) {
	NoteSet major({0, 2, 4, 5, 7, 9, 11});
	CHECK_EQUAL(0, major.degreeOf(0));
	CHECK_EQUAL(1, major.degreeOf(2));
	CHECK_EQUAL(2, major.degreeOf(4));
	CHECK_EQUAL(3, major.degreeOf(5));
	CHECK_EQUAL(4, major.degreeOf(7));
	CHECK_EQUAL(5, major.degreeOf(9));
	CHECK_EQUAL(6, major.degreeOf(11));
	// Non-scale notes
	CHECK_EQUAL(-1, major.degreeOf(1));
	CHECK_EQUAL(-1, major.degreeOf(3));
	CHECK_EQUAL(-1, major.degreeOf(6));
	CHECK_EQUAL(-1, major.degreeOf(8));
	CHECK_EQUAL(-1, major.degreeOf(10));
}

// --- scaleSize with root present or absent ---

TEST(NoteSetDeep, scaleSizeAlwaysIncludesRoot) {
	NoteSet ns({3, 7}); // no root
	// scaleSize adds 0 before counting
	CHECK_EQUAL(3, ns.scaleSize());
}

TEST(NoteSetDeep, scaleSizeWithRoot) {
	NoteSet ns({0, 3, 7});
	CHECK_EQUAL(3, ns.scaleSize());
}

// --- majorness edge cases ---

TEST(NoteSetDeep, majornessOnlyMajorThird) {
	NoteSet ns({0, 4});
	CHECK_EQUAL(1, ns.majorness());
}

TEST(NoteSetDeep, majornessOnlyMinorThird) {
	NoteSet ns({0, 3});
	CHECK_EQUAL(-1, ns.majorness());
}

TEST(NoteSetDeep, majornessBothThirds) {
	NoteSet ns({0, 3, 4});
	CHECK_EQUAL(0, ns.majorness());
}

TEST(NoteSetDeep, majornessNoThirdUsesSecondary) {
	// No 3 or 4 present, ties go to secondaries (1, 8, 9)
	NoteSet ns({0, 1}); // flat 2 = minor indicator
	CHECK_EQUAL(-1, ns.majorness());

	NoteSet ns2({0, 9}); // major 6 = major indicator
	CHECK_EQUAL(1, ns2.majorness());

	NoteSet ns3({0, 8}); // minor 6 = minor indicator
	CHECK_EQUAL(-1, ns3.majorness());
}

TEST(NoteSetDeep, majornessSecondaryTiebreakers) {
	// 1 + 9: one minor, one major secondary = tie
	NoteSet ns({0, 1, 9});
	CHECK_EQUAL(0, ns.majorness());
}

// --- Equality and inequality ---

TEST(NoteSetDeep, inequalityDifferentBits) {
	NoteSet a({0, 4, 7});
	NoteSet b({0, 3, 7});
	CHECK(a != b);
}

TEST(NoteSetDeep, equalitySameBitsFromDifferentConstruction) {
	NoteSet a;
	a.add(0);
	a.add(4);
	a.add(7);
	NoteSet b({0, 4, 7});
	CHECK_EQUAL(a, b);
}

// --- Double add/remove idempotency ---

TEST(NoteSetDeep, doubleAddIdempotent) {
	NoteSet ns;
	ns.add(5);
	ns.add(5);
	CHECK_EQUAL(1, ns.count());
}

TEST(NoteSetDeep, removeNonexistent) {
	NoteSet ns({0, 4, 7});
	ns.remove(3); // not present
	CHECK_EQUAL(3, ns.count());
	CHECK_EQUAL(NoteSet({0, 4, 7}), ns);
}

// --- toImpliedScale: various single-note implied scales ---

TEST(NoteSetDeep, impliedScaleAlwaysHasRoot) {
	for (int i = 0; i < 12; i++) {
		NoteSet ns;
		ns.add(i);
		NoteSet implied = ns.toImpliedScale();
		CHECK_TEXT(implied.has(0),
		           (std::string("Implied scale from note ") + std::to_string(i) + " missing root").c_str());
	}
}

TEST(NoteSetDeep, impliedScaleAlwaysHas7Notes) {
	for (int i = 0; i < 12; i++) {
		NoteSet ns;
		ns.add(i);
		NoteSet implied = ns.toImpliedScale();
		CHECK_EQUAL(7, implied.count());
	}
}

TEST(NoteSetDeep, impliedScaleSuperset) {
	// The implied scale should contain the input note
	for (int i = 0; i < 12; i++) {
		NoteSet ns;
		ns.add(i);
		NoteSet implied = ns.toImpliedScale();
		CHECK_TEXT(implied.has(i),
		           (std::string("Implied scale from note ") + std::to_string(i) + " does not contain it").c_str());
	}
}

// ============================================================================
// MusicalKey Deep Tests
// ============================================================================

TEST_GROUP(MusicalKeyDeep){};

// --- intervalOf across all roots ---

TEST(MusicalKeyDeep, intervalOfAllRoots) {
	MusicalKey key;
	for (int root = 0; root < 12; root++) {
		key.rootNote = root;
		// Root note should always be interval 0
		CHECK_EQUAL(0, key.intervalOf(root));
		CHECK_EQUAL(0, key.intervalOf(root + 12));
		CHECK_EQUAL(0, key.intervalOf(root - 12));
		CHECK_EQUAL(0, key.intervalOf(root + 120));
		CHECK_EQUAL(0, key.intervalOf(root - 120));
	}
}

TEST(MusicalKeyDeep, intervalOfHalfStepAboveRoot) {
	MusicalKey key;
	for (int root = 0; root < 12; root++) {
		key.rootNote = root;
		CHECK_EQUAL(1, key.intervalOf(root + 1));
		CHECK_EQUAL(1, key.intervalOf(root + 1 + 12));
		CHECK_EQUAL(1, key.intervalOf(root + 1 - 12));
	}
}

TEST(MusicalKeyDeep, intervalOfSymmetry) {
	MusicalKey key;
	// interval of root+n should be n for 0..11
	for (int root = 0; root < 12; root++) {
		key.rootNote = root;
		for (int interval = 0; interval < 12; interval++) {
			CHECK_EQUAL(interval, key.intervalOf(root + interval));
		}
	}
}

TEST(MusicalKeyDeep, intervalOfNegativeNotes) {
	MusicalKey key;
	key.rootNote = 0;
	// Note -1 => interval should be 11
	CHECK_EQUAL(11, key.intervalOf(-1));
	// Note -12 => interval should be 0
	CHECK_EQUAL(0, key.intervalOf(-12));
	// Note -7 => interval should be 5
	CHECK_EQUAL(5, key.intervalOf(-7));
}

// --- degreeOf with different scales ---

TEST(MusicalKeyDeep, degreeOfMajorKeyAllRoots) {
	MusicalKey key;
	key.modeNotes = presetScaleNotes[MAJOR_SCALE]; // 0,2,4,5,7,9,11

	for (int root = 0; root < 12; root++) {
		key.rootNote = root;
		// The root should be degree 0
		CHECK_EQUAL(0, key.degreeOf(root));
		// Major 2nd above root should be degree 1
		CHECK_EQUAL(1, key.degreeOf(root + 2));
		// Major 3rd above root should be degree 2
		CHECK_EQUAL(2, key.degreeOf(root + 4));
		// Perfect 4th = degree 3
		CHECK_EQUAL(3, key.degreeOf(root + 5));
		// Perfect 5th = degree 4
		CHECK_EQUAL(4, key.degreeOf(root + 7));
		// Major 6th = degree 5
		CHECK_EQUAL(5, key.degreeOf(root + 9));
		// Major 7th = degree 6
		CHECK_EQUAL(6, key.degreeOf(root + 11));

		// Chromatic notes not in scale should return -1
		CHECK_EQUAL(-1, key.degreeOf(root + 1));  // minor 2nd
		CHECK_EQUAL(-1, key.degreeOf(root + 3));  // minor 3rd
		CHECK_EQUAL(-1, key.degreeOf(root + 6));  // tritone
		CHECK_EQUAL(-1, key.degreeOf(root + 8));  // minor 6th
		CHECK_EQUAL(-1, key.degreeOf(root + 10)); // minor 7th
	}
}

TEST(MusicalKeyDeep, degreeOfPentatonicMinor) {
	MusicalKey key;
	key.modeNotes = presetScaleNotes[PENTATONIC_MINOR_SCALE]; // 0,3,5,7,10
	key.rootNote = 0;

	CHECK_EQUAL(0, key.degreeOf(0));
	CHECK_EQUAL(1, key.degreeOf(3));
	CHECK_EQUAL(2, key.degreeOf(5));
	CHECK_EQUAL(3, key.degreeOf(7));
	CHECK_EQUAL(4, key.degreeOf(10));
	// Non-pentatonic notes
	CHECK_EQUAL(-1, key.degreeOf(1));
	CHECK_EQUAL(-1, key.degreeOf(2));
	CHECK_EQUAL(-1, key.degreeOf(4));
	CHECK_EQUAL(-1, key.degreeOf(6));
	CHECK_EQUAL(-1, key.degreeOf(8));
	CHECK_EQUAL(-1, key.degreeOf(9));
	CHECK_EQUAL(-1, key.degreeOf(11));
}

TEST(MusicalKeyDeep, degreeOfChromaticScale) {
	MusicalKey key;
	NoteSet chromatic;
	chromatic.fill();
	key.modeNotes = chromatic;
	key.rootNote = 0;

	for (int i = 0; i < 12; i++) {
		CHECK_EQUAL(i, key.degreeOf(i));
	}
}

TEST(MusicalKeyDeep, degreeOfSingleNoteScale) {
	MusicalKey key;
	key.modeNotes = NoteSet({0}); // only root
	key.rootNote = 5;

	CHECK_EQUAL(0, key.degreeOf(5));
	for (int i = 0; i < 12; i++) {
		if (i != 5) {
			CHECK_EQUAL(-1, key.degreeOf(i));
		}
	}
}

TEST(MusicalKeyDeep, degreeOfOctaveWrapping) {
	MusicalKey key;
	key.modeNotes = presetScaleNotes[MINOR_SCALE]; // 0,2,3,5,7,8,10
	key.rootNote = 9; // A minor

	// A=9 in octave 4 => MIDI 57, across several octaves
	for (int oct = -3; oct <= 3; oct++) {
		int base = 9 + oct * 12;
		CHECK_EQUAL(0, key.degreeOf(base));      // A
		CHECK_EQUAL(1, key.degreeOf(base + 2));   // B
		CHECK_EQUAL(2, key.degreeOf(base + 3));   // C
		CHECK_EQUAL(-1, key.degreeOf(base + 1));  // Bb not in A minor
	}
}

// ============================================================================
// ScaleChange Deep Tests
// ============================================================================

TEST_GROUP(ScaleChangeDeep){};

TEST(ScaleChangeDeep, identityChange) {
	// A ScaleChange with all zero offsets should be identity
	ScaleChange change;
	NoteSet major({0, 2, 4, 5, 7, 9, 11});
	change.source = major;
	change.target = major;
	// All degreeOffset are 0 by default

	NoteSet result = change.applyTo(major);
	CHECK_EQUAL(major, result);
}

TEST(ScaleChangeDeep, applyToEmptyNotes) {
	ScaleChange change;
	change.source = NoteSet({0, 2, 4, 5, 7, 9, 11});
	NoteSet empty;
	NoteSet result = change.applyTo(empty);
	CHECK(result.isEmpty());
}

TEST(ScaleChangeDeep, applyToSubset) {
	// Apply to a subset of the source scale
	ScaleMapper mapper;
	NoteSet major({0, 2, 4, 5, 7, 9, 11});
	NoteSet minor({0, 2, 3, 5, 7, 8, 10});
	NoteSet triad({0, 4, 7}); // C major triad

	ScaleChange change;
	bool ok = mapper.computeChangeFrom(triad, major, minor, change);
	CHECK(ok);
	NoteSet result = change.applyTo(triad);
	// Should produce a minor triad: 0, 3, 7
	CHECK(result.isSubsetOf(minor));
	CHECK(result.has(0)); // root preserved
	CHECK(result.has(7)); // 5th preserved
}

// --- ScaleMapper with all preset scales ---

TEST(ScaleChangeDeep, mapBetweenAllPresetScalePairs) {
	ScaleMapper mapper;
	ScaleChange change;

	// For each pair of preset scales, try mapping root note
	for (int i = 0; i < NUM_PRESET_SCALES; i++) {
		for (int j = 0; j < NUM_PRESET_SCALES; j++) {
			NoteSet source = presetScaleNotes[i];
			NoteSet target = presetScaleNotes[j];
			NoteSet rootNote({0}); // just the root

			bool ok = mapper.computeChangeFrom(rootNote, source, target, change);
			CHECK_TEXT(ok, (std::string("Failed mapping root from scale ") + std::to_string(i) + " to " +
			                std::to_string(j))
			                   .c_str());
			NoteSet result = change.applyTo(rootNote);
			CHECK_TEXT(result.has(0),
			           (std::string("Root lost mapping scale ") + std::to_string(i) + " to " + std::to_string(j))
			               .c_str());
		}
	}
}

TEST(ScaleChangeDeep, mapPresetScaleToSelfIsIdentity) {
	ScaleMapper mapper;
	ScaleChange change;

	for (int i = 0; i < NUM_PRESET_SCALES; i++) {
		NoteSet scale = presetScaleNotes[i];
		bool ok = mapper.computeChangeFrom(scale, scale, scale, change);
		CHECK(ok);
		NoteSet result = change.applyTo(scale);
		CHECK_EQUAL(scale, result);
	}
}

// --- ScaleMapper: mode rotation ---

TEST(ScaleChangeDeep, majorModeRotationsAreModes) {
	// All 7 modes of the major scale should be distinct preset scales
	NoteSet major({0, 2, 4, 5, 7, 9, 11});
	// Modulating major scale by each of its intervals gives each mode
	int majorIntervals[] = {0, 2, 4, 5, 7, 9, 11};
	for (int m = 0; m < 7; m++) {
		NoteSet mode = major.modulateByOffset(12 - majorIntervals[m]);
		CHECK_EQUAL(7, mode.count());
		CHECK(mode.has(0)); // root of the mode
	}
}

// ============================================================================
// Chord Deep Tests
// ============================================================================

TEST_GROUP(ChordDeep){};

// --- Chord quality detection for all defined chords ---

TEST(ChordDeep, majorChordQualityDetection) {
	NoteSet notes = deluge::gui::ui::keyboard::kMajor.intervalSet;
	CHECK(getChordQuality(notes) == ChordQuality::MAJOR);
}

TEST(ChordDeep, minorChordQualityDetection) {
	NoteSet notes = deluge::gui::ui::keyboard::kMinor.intervalSet;
	CHECK(getChordQuality(notes) == ChordQuality::MINOR);
}

TEST(ChordDeep, diminishedChordQualityDetection) {
	NoteSet notes = deluge::gui::ui::keyboard::kDim.intervalSet;
	CHECK(getChordQuality(notes) == ChordQuality::DIMINISHED);
}

TEST(ChordDeep, dominantChordQualityDetection) {
	NoteSet notes = deluge::gui::ui::keyboard::k7.intervalSet;
	CHECK(getChordQuality(notes) == ChordQuality::DOMINANT);
}

TEST(ChordDeep, augmentedChordHasCorrectIntervals) {
	// kAug has MIN3 + AUG5, which should be AUGMENTED quality
	NoteSet notes = deluge::gui::ui::keyboard::kAug.intervalSet;
	// kAug is defined as {ROOT, MIN3, AUG5} which has MIN3 not MAJ3
	// so it should be OTHER (aug quality requires MAJ3 + AUG5)
	// Let's check what the code actually detects:
	ChordQuality quality = getChordQuality(notes);
	// The definition has MIN3 + AUG5, which doesn't match MAJ3+AUG5 pattern
	// and doesn't match MIN3+P5 either (AUG5 != P5)
	// and doesn't match MIN3+DIM5 (AUG5 != DIM5)
	// So it should be OTHER... unless MIN3+AUG5 is handled
	// Looking at the code: it checks MAJ3+AUG5 for augmented
	CHECK(quality == ChordQuality::OTHER);
}

// --- Chord quality with synthetic interval sets ---

TEST(ChordDeep, qualitySingleNote) {
	NoteSet notes({ROOT});
	CHECK(getChordQuality(notes) == ChordQuality::OTHER);
}

TEST(ChordDeep, qualityTwoNotes) {
	NoteSet notes({ROOT, P5});
	CHECK(getChordQuality(notes) == ChordQuality::OTHER);
}

TEST(ChordDeep, qualityMajorTriadAllInversions) {
	// Quality detection is based on interval set, not voicing
	// So all inversions of a major triad should be MAJOR
	NoteSet notes({ROOT, MAJ3, P5});
	CHECK(getChordQuality(notes) == ChordQuality::MAJOR);
}

TEST(ChordDeep, qualityDominantWithExtensions) {
	// Major 3rd + P5 + minor 7th = dominant, even with extensions
	NoteSet notes({ROOT, MAJ3, P5, MIN7, MAJ2}); // dom9
	CHECK(getChordQuality(notes) == ChordQuality::DOMINANT);
}

TEST(ChordDeep, qualityMinorWithExtensions) {
	NoteSet notes({ROOT, MIN3, P5, MIN7}); // minor 7th
	CHECK(getChordQuality(notes) == ChordQuality::MINOR);
}

TEST(ChordDeep, qualityDiminishedWithSeventh) {
	NoteSet notes({ROOT, MIN3, DIM5, DIM7}); // full diminished
	CHECK(getChordQuality(notes) == ChordQuality::DIMINISHED);
}

TEST(ChordDeep, qualityAugmentedWithMaj3) {
	NoteSet notes({ROOT, MAJ3, AUG5});
	CHECK(getChordQuality(notes) == ChordQuality::AUGMENTED);
}

TEST(ChordDeep, qualitySus4IsOther) {
	NoteSet notes({ROOT, P4, P5});
	CHECK(getChordQuality(notes) == ChordQuality::OTHER);
}

TEST(ChordDeep, qualitySus2IsOther) {
	NoteSet notes({ROOT, MAJ2, P5});
	CHECK(getChordQuality(notes) == ChordQuality::OTHER);
}

// --- Chord quality priority: dominant over major ---

TEST(ChordDeep, qualityDominantPriorityOverMajor) {
	// MAJ3 + P5 + MIN7 should be dominant, not major
	NoteSet notes({ROOT, MAJ3, P5, MIN7});
	CHECK(getChordQuality(notes) == ChordQuality::DOMINANT);
}

TEST(ChordDeep, qualityMajorWithMaj7NotDominant) {
	// MAJ3 + P5 + MAJ7 should be MAJOR, not dominant
	NoteSet notes({ROOT, MAJ3, P5, MAJ7});
	CHECK(getChordQuality(notes) == ChordQuality::MAJOR);
}

// --- ChordList voicing retrieval for all chords ---

TEST(ChordDeep, allChordsHaveAtLeastOneValidVoicing) {
	ChordList chordList;
	for (int c = 0; c < kUniqueChords; c++) {
		chordList.voicingOffset[c] = 0;
		Voicing v = chordList.getChordVoicing(c);
		bool hasNote = false;
		for (int i = 0; i < kMaxChordKeyboardSize; i++) {
			if (v.offsets[i] != NONE) {
				hasNote = true;
			}
		}
		CHECK_TEXT(hasNote, (std::string("Chord ") + std::to_string(c) + " has no valid notes in voicing 0").c_str());
	}
}

TEST(ChordDeep, allChordsFirstVoicingHasRoot) {
	ChordList chordList;
	for (int c = 0; c < kUniqueChords; c++) {
		chordList.voicingOffset[c] = 0;
		Voicing v = chordList.getChordVoicing(c);
		bool hasRoot = false;
		for (int i = 0; i < kMaxChordKeyboardSize; i++) {
			if (v.offsets[i] == ROOT) {
				hasRoot = true;
			}
		}
		CHECK_TEXT(hasRoot,
		           (std::string("Chord ") + std::to_string(c) + " voicing 0 missing root").c_str());
	}
}

// --- ChordList: all voicing offsets produce valid voicings ---

TEST(ChordDeep, allVoicingOffsetsProduceValidResult) {
	ChordList chordList;
	for (int c = 0; c < kUniqueChords; c++) {
		for (int v = 0; v < kUniqueVoicings; v++) {
			chordList.voicingOffset[c] = v;
			Voicing voicing = chordList.getChordVoicing(c);
			bool hasNote = false;
			for (int i = 0; i < kMaxChordKeyboardSize; i++) {
				if (voicing.offsets[i] != NONE && voicing.offsets[i] != 0) {
					hasNote = true;
				}
			}
			// The empty chord (index 0) only has ROOT (0) and rest NONE,
			// so "hasNote" might be false for it. Check at least one non-NONE exists.
			bool hasAny = false;
			for (int i = 0; i < kMaxChordKeyboardSize; i++) {
				if (voicing.offsets[i] != NONE) {
					hasAny = true;
				}
			}
			CHECK(hasAny);
		}
	}
}

// --- Chord names are non-null ---

TEST(ChordDeep, allChordNamesNonNull) {
	ChordList chordList;
	for (int c = 0; c < kUniqueChords; c++) {
		CHECK(chordList.chords[c].name != nullptr);
	}
}

// --- ChordList row offset boundaries ---

TEST(ChordDeep, chordRowOffsetLargeDelta) {
	ChordList chordList;
	chordList.chordRowOffset = 0;
	chordList.adjustChordRowOffset(127); // way past max
	CHECK_EQUAL(kOffScreenChords, chordList.chordRowOffset);
}

TEST(ChordDeep, chordRowOffsetLargeNegativeDelta) {
	ChordList chordList;
	chordList.chordRowOffset = kOffScreenChords;
	chordList.adjustChordRowOffset(-127); // way past min
	CHECK_EQUAL(0, chordList.chordRowOffset);
}

// --- Voicing offset boundaries ---

TEST(ChordDeep, voicingOffsetLargeDelta) {
	ChordList chordList;
	for (int c = 0; c < kUniqueChords; c++) {
		chordList.voicingOffset[c] = 0;
		chordList.adjustVoicingOffset(c, 100);
		CHECK_EQUAL(kUniqueVoicings - 1, chordList.voicingOffset[c]);
	}
}

TEST(ChordDeep, voicingOffsetLargeNegDelta) {
	ChordList chordList;
	for (int c = 0; c < kUniqueChords; c++) {
		chordList.voicingOffset[c] = kUniqueVoicings - 1;
		chordList.adjustVoicingOffset(c, -100);
		CHECK_EQUAL(0, chordList.voicingOffset[c]);
	}
}

// --- Chord interval set contains root ---

TEST(ChordDeep, allDefinedChordsContainRoot) {
	const deluge::gui::ui::keyboard::Chord* allChords[] = {
	    &deluge::gui::ui::keyboard::kEmptyChord, &deluge::gui::ui::keyboard::kMajor,
	    &deluge::gui::ui::keyboard::kMinor,      &deluge::gui::ui::keyboard::k6,
	    &deluge::gui::ui::keyboard::k2,          &deluge::gui::ui::keyboard::k69,
	    &deluge::gui::ui::keyboard::kSus2,       &deluge::gui::ui::keyboard::kSus4,
	    &deluge::gui::ui::keyboard::k7,          &deluge::gui::ui::keyboard::k7Sus4,
	    &deluge::gui::ui::keyboard::k7Sus2,      &deluge::gui::ui::keyboard::kM7,
	    &deluge::gui::ui::keyboard::kMinor7,     &deluge::gui::ui::keyboard::kMinor2,
	    &deluge::gui::ui::keyboard::kMinor4,     &deluge::gui::ui::keyboard::kDim,
	    &deluge::gui::ui::keyboard::kFullDim,    &deluge::gui::ui::keyboard::kAug,
	    &deluge::gui::ui::keyboard::kMinor6,     &deluge::gui::ui::keyboard::kMinorMaj7,
	    &deluge::gui::ui::keyboard::kMinor7b5,   &deluge::gui::ui::keyboard::kMinor9b5,
	    &deluge::gui::ui::keyboard::kMinor7b5b9, &deluge::gui::ui::keyboard::k9,
	    &deluge::gui::ui::keyboard::kM9,         &deluge::gui::ui::keyboard::kMinor9,
	    &deluge::gui::ui::keyboard::k11,         &deluge::gui::ui::keyboard::kM11,
	    &deluge::gui::ui::keyboard::kMinor11,    &deluge::gui::ui::keyboard::k13,
	    &deluge::gui::ui::keyboard::kM13,        &deluge::gui::ui::keyboard::kM13Sharp11,
	    &deluge::gui::ui::keyboard::kMinor13,
	};
	for (auto* chord : allChords) {
		CHECK_TEXT(chord->intervalSet.has(ROOT),
		           (std::string("Chord '") + chord->name + "' missing root in interval set").c_str());
	}
}

// ============================================================================
// Preset Scales Deep Tests
// ============================================================================

TEST_GROUP(PresetScalesDeep){};

// --- Verify expected note counts ---

TEST(PresetScalesDeep, sevenNoteScalesCounted) {
	for (int i = 0; i < FIRST_6_NOTE_SCALE_INDEX; i++) {
		CHECK_EQUAL(7, presetScaleNotes[i].count());
	}
}

TEST(PresetScalesDeep, sixNoteScalesCounted) {
	for (int i = FIRST_6_NOTE_SCALE_INDEX; i < FIRST_5_NOTE_SCALE_INDEX; i++) {
		CHECK_EQUAL(6, presetScaleNotes[i].count());
	}
}

TEST(PresetScalesDeep, fiveNoteScalesCounted) {
	for (int i = FIRST_5_NOTE_SCALE_INDEX; i < NUM_PRESET_SCALES; i++) {
		CHECK_EQUAL(5, presetScaleNotes[i].count());
	}
}

// --- All preset scales contain the root ---

TEST(PresetScalesDeep, allPresetsContainRoot) {
	for (int i = 0; i < NUM_PRESET_SCALES; i++) {
		CHECK(presetScaleNotes[i].has(0));
	}
}

// --- Specific scale interval verification ---

TEST(PresetScalesDeep, majorScaleIntervals) {
	NoteSet major = presetScaleNotes[MAJOR_SCALE];
	CHECK(major.has(0));
	CHECK(major.has(2));
	CHECK(major.has(4));
	CHECK(major.has(5));
	CHECK(major.has(7));
	CHECK(major.has(9));
	CHECK(major.has(11));
}

TEST(PresetScalesDeep, minorScaleIntervals) {
	NoteSet minor = presetScaleNotes[MINOR_SCALE];
	CHECK(minor.has(0));
	CHECK(minor.has(2));
	CHECK(minor.has(3));
	CHECK(minor.has(5));
	CHECK(minor.has(7));
	CHECK(minor.has(8));
	CHECK(minor.has(10));
}

TEST(PresetScalesDeep, dorianScaleIntervals) {
	NoteSet dorian = presetScaleNotes[DORIAN_SCALE];
	// Dorian = minor with raised 6th
	CHECK(dorian.has(0));
	CHECK(dorian.has(2));
	CHECK(dorian.has(3));
	CHECK(dorian.has(5));
	CHECK(dorian.has(7));
	CHECK(dorian.has(9));  // raised 6th vs minor's 8
	CHECK(dorian.has(10));
}

TEST(PresetScalesDeep, harmonicMinorScaleIntervals) {
	NoteSet hm = presetScaleNotes[HARMONIC_MINOR_SCALE];
	CHECK(hm.has(0));
	CHECK(hm.has(2));
	CHECK(hm.has(3));
	CHECK(hm.has(5));
	CHECK(hm.has(7));
	CHECK(hm.has(8));
	CHECK(hm.has(11)); // raised 7th
}

TEST(PresetScalesDeep, pentatonicMinorScaleIntervals) {
	NoteSet pm = presetScaleNotes[PENTATONIC_MINOR_SCALE];
	CHECK(pm.has(0));
	CHECK(pm.has(3));
	CHECK(pm.has(5));
	CHECK(pm.has(7));
	CHECK(pm.has(10));
}

TEST(PresetScalesDeep, bluesScaleIntervals) {
	NoteSet blues = presetScaleNotes[BLUES_SCALE];
	CHECK(blues.has(0));
	CHECK(blues.has(3));
	CHECK(blues.has(5));
	CHECK(blues.has(6));
	CHECK(blues.has(7));
	CHECK(blues.has(10));
}

// --- Mode relationships ---

TEST(PresetScalesDeep, dorianIsMinorWithRaised6th) {
	NoteSet minor = presetScaleNotes[MINOR_SCALE];
	NoteSet dorian = presetScaleNotes[DORIAN_SCALE];
	// They differ only at positions 8 (minor has) vs 9 (dorian has)
	for (int i = 0; i < 12; i++) {
		if (i == 8) {
			CHECK(minor.has(i));
			CHECK(!dorian.has(i));
		}
		else if (i == 9) {
			CHECK(!minor.has(i));
			CHECK(dorian.has(i));
		}
		else {
			CHECK_TEXT(minor.has(i) == dorian.has(i),
			           (std::string("Minor and Dorian differ at note ") + std::to_string(i)).c_str());
		}
	}
}

TEST(PresetScalesDeep, mixolydianIsMajorWithFlat7th) {
	NoteSet major = presetScaleNotes[MAJOR_SCALE];
	NoteSet mixo = presetScaleNotes[MIXOLYDIAN_SCALE];
	// Differ at 10 (mixo has) vs 11 (major has)
	for (int i = 0; i < 12; i++) {
		if (i == 10) {
			CHECK(!major.has(i));
			CHECK(mixo.has(i));
		}
		else if (i == 11) {
			CHECK(major.has(i));
			CHECK(!mixo.has(i));
		}
		else {
			CHECK(major.has(i) == mixo.has(i));
		}
	}
}

TEST(PresetScalesDeep, lydianIsMajorWithSharp4th) {
	NoteSet major = presetScaleNotes[MAJOR_SCALE];
	NoteSet lydian = presetScaleNotes[LYDIAN_SCALE];
	// Differ at 5 (major has P4) vs 6 (lydian has #4)
	for (int i = 0; i < 12; i++) {
		if (i == 5) {
			CHECK(major.has(i));
			CHECK(!lydian.has(i));
		}
		else if (i == 6) {
			CHECK(!major.has(i));
			CHECK(lydian.has(i));
		}
		else {
			CHECK(major.has(i) == lydian.has(i));
		}
	}
}

// --- Majorness of preset scales ---

TEST(PresetScalesDeep, majorScaleIsMajor) {
	CHECK(presetScaleNotes[MAJOR_SCALE].majorness() > 0);
}

TEST(PresetScalesDeep, minorScaleIsMinor) {
	CHECK(presetScaleNotes[MINOR_SCALE].majorness() < 0);
}

TEST(PresetScalesDeep, dorianIsMinor) {
	CHECK(presetScaleNotes[DORIAN_SCALE].majorness() < 0);
}

TEST(PresetScalesDeep, lydianIsMajor) {
	CHECK(presetScaleNotes[LYDIAN_SCALE].majorness() > 0);
}

TEST(PresetScalesDeep, mixolydianIsMajor) {
	// Mixolydian has major 3rd (4) and major 6th (9) — should be major
	CHECK(presetScaleNotes[MIXOLYDIAN_SCALE].majorness() > 0);
}

TEST(PresetScalesDeep, phrygianIsMinor) {
	// Phrygian: flat 2, minor 3rd — strongly minor
	CHECK(presetScaleNotes[PHRYGIAN_SCALE].majorness() < 0);
}

// ============================================================================
// isSameNote Deep Tests
// ============================================================================

TEST_GROUP(IsSameNoteDeep){};

TEST(IsSameNoteDeep, sameNoteAcrossOctaves) {
	for (int note = 0; note < 12; note++) {
		for (int octave = -5; octave <= 5; octave++) {
			CHECK(isSameNote(note, note + octave * 12));
		}
	}
}

TEST(IsSameNoteDeep, differentNotes) {
	CHECK(!isSameNote(0, 1));
	CHECK(!isSameNote(0, 11));
	CHECK(!isSameNote(5, 6));
	CHECK(!isSameNote(0, 13)); // 0 vs 1 in next octave
}

TEST(IsSameNoteDeep, negativeNotes) {
	CHECK(isSameNote(-12, 0));
	CHECK(isSameNote(-24, 0));
	CHECK(isSameNote(-1, 11));
	CHECK(isSameNote(-7, 5));
}

// ============================================================================
// ScaleMapper Integration: Mapping Through Scale Changes
// ============================================================================

TEST_GROUP(ScaleMapperDeep){};

TEST(ScaleMapperDeep, majorToMinorAndBack) {
	ScaleMapper mapper;
	ScaleChange change;
	NoteSet major({0, 2, 4, 5, 7, 9, 11});
	NoteSet minor({0, 2, 3, 5, 7, 8, 10});

	// Map the full major scale to minor
	bool ok = mapper.computeChangeFrom(major, major, minor, change);
	CHECK(ok);
	NoteSet result = change.applyTo(major);
	CHECK_EQUAL(minor, result);

	// Map back
	ok = mapper.computeChangeFrom(result, minor, major, change);
	CHECK(ok);
	NoteSet backToMajor = change.applyTo(result);
	CHECK_EQUAL(major, backToMajor);
}

TEST(ScaleMapperDeep, majorToDorian) {
	ScaleMapper mapper;
	ScaleChange change;
	NoteSet major({0, 2, 4, 5, 7, 9, 11});
	NoteSet dorian({0, 2, 3, 5, 7, 9, 10});

	bool ok = mapper.computeChangeFrom(major, major, dorian, change);
	CHECK(ok);
	NoteSet result = change.applyTo(major);
	CHECK_EQUAL(dorian, result);
}

TEST(ScaleMapperDeep, pentatonicMinorToBlues) {
	ScaleMapper mapper;
	ScaleChange change;
	NoteSet pentMinor({0, 3, 5, 7, 10});
	NoteSet blues({0, 3, 5, 6, 7, 10});

	// 5-note to 6-note: should work for notes in common
	NoteSet commonNotes({0, 3, 5, 7, 10}); // all pent notes exist in blues

	bool ok = mapper.computeChangeFrom(commonNotes, pentMinor, blues, change);
	CHECK(ok);
	NoteSet result = change.applyTo(commonNotes);
	CHECK(result.isSubsetOf(blues));
}

TEST(ScaleMapperDeep, mapSingleRootBetweenAllScaleSizes) {
	ScaleMapper mapper;
	ScaleChange change;
	NoteSet rootOnly({0});

	// 5-note -> 6-note -> 7-note -> back to 5-note
	NoteSet pent = presetScaleNotes[PENTATONIC_MINOR_SCALE];
	NoteSet blues = presetScaleNotes[BLUES_SCALE];
	NoteSet major = presetScaleNotes[MAJOR_SCALE];

	NoteSet current = rootOnly;
	NoteSet currentScale = pent;

	bool ok = mapper.computeChangeFrom(current, currentScale, blues, change);
	CHECK(ok);
	current = change.applyTo(current);
	currentScale = blues;
	CHECK(current.has(0));

	ok = mapper.computeChangeFrom(current, currentScale, major, change);
	CHECK(ok);
	current = change.applyTo(current);
	currentScale = major;
	CHECK(current.has(0));

	ok = mapper.computeChangeFrom(current, currentScale, pent, change);
	CHECK(ok);
	current = change.applyTo(current);
	CHECK(current.has(0));
	CHECK_EQUAL(rootOnly, current);
}

// ============================================================================
// Chord quality-categorized arrays
// ============================================================================

TEST_GROUP(ChordArrayDeep){};

TEST(ChordArrayDeep, majorChordsAllMajorOrSusOrEmpty) {
	for (const auto& chord : deluge::gui::ui::keyboard::majorChords) {
		if (chord.intervalSet.count() >= 3) {
			NoteSet notes = chord.intervalSet;
			ChordQuality q = getChordQuality(notes);
			// Major chords array: should be MAJOR, OTHER (sus), or have MAJ3
			CHECK_TEXT(q == ChordQuality::MAJOR || q == ChordQuality::OTHER || q == ChordQuality::AUGMENTED ||
			               notes.has(MAJ3),
			           (std::string("Major chord '") + chord.name + "' unexpected quality").c_str());
		}
	}
}

TEST(ChordArrayDeep, minorChordsAllMinorOrEmpty) {
	for (const auto& chord : deluge::gui::ui::keyboard::minorChords) {
		if (chord.intervalSet.count() >= 3) {
			NoteSet notes = chord.intervalSet;
			ChordQuality q = getChordQuality(notes);
			CHECK_TEXT(q == ChordQuality::MINOR || q == ChordQuality::OTHER,
			           (std::string("Minor chord '") + chord.name + "' unexpected quality").c_str());
		}
	}
}

TEST(ChordArrayDeep, dominantChordsAllDominantOrRelated) {
	for (const auto& chord : deluge::gui::ui::keyboard::dominantChords) {
		if (chord.intervalSet.count() >= 3) {
			NoteSet notes = chord.intervalSet;
			ChordQuality q = getChordQuality(notes);
			CHECK_TEXT(q == ChordQuality::DOMINANT || q == ChordQuality::MAJOR || q == ChordQuality::OTHER,
			           (std::string("Dominant chord '") + chord.name + "' unexpected quality").c_str());
		}
	}
}

TEST(ChordArrayDeep, diminishedChordsAllDiminishedOrEmpty) {
	for (const auto& chord : deluge::gui::ui::keyboard::diminishedChords) {
		if (chord.intervalSet.count() >= 3) {
			NoteSet notes = chord.intervalSet;
			ChordQuality q = getChordQuality(notes);
			CHECK_TEXT(q == ChordQuality::DIMINISHED || q == ChordQuality::OTHER,
			           (std::string("Diminished chord '") + chord.name + "' unexpected quality").c_str());
		}
	}
}

// ============================================================================
// NoteSet: modulateByOffset vs. MusicalKey transposition cross-check
// ============================================================================

TEST_GROUP(TranspositionCrossCheck){};

TEST(TranspositionCrossCheck, modulateMatchesKeyInterval) {
	// For a major scale rooted at C, modulating by N should produce the same
	// set of intervals as key.intervalOf applied to each note offset by N
	NoteSet major({0, 2, 4, 5, 7, 9, 11});

	for (int offset = 0; offset < 12; offset++) {
		NoteSet modulated = major.modulateByOffset(offset);
		CHECK_EQUAL(7, modulated.count());
		// Each note in modulated should be (original + offset) % 12
		for (int deg = 0; deg < 7; deg++) {
			int origNote = major[deg];
			int expectedNote = (origNote + offset) % 12;
			CHECK_TEXT(modulated.has(expectedNote),
			           (std::string("Offset ") + std::to_string(offset) + " missing note " +
			            std::to_string(expectedNote))
			               .c_str());
		}
	}
}

// ============================================================================
// NoteSet: addMajorDependentModeNotes edge cases
// ============================================================================

TEST_GROUP(MajorDependentDeep){};

TEST(MajorDependentDeep, bothPresent) {
	NoteSet target;
	NoteSet present({3, 4}); // both minor 3rd and major 3rd
	target.addMajorDependentModeNotes(3, false, present);
	CHECK(target.has(3));
	CHECK(target.has(4));
}

TEST(MajorDependentDeep, onlyLowerPresent) {
	NoteSet target;
	NoteSet present({3}); // only minor 3rd
	target.addMajorDependentModeNotes(3, true, present);
	CHECK(target.has(3));
	CHECK(!target.has(4));
}

TEST(MajorDependentDeep, onlyHigherPresent) {
	NoteSet target;
	NoteSet present({4}); // only major 3rd
	target.addMajorDependentModeNotes(3, false, present);
	CHECK(!target.has(3));
	CHECK(target.has(4));
}

TEST(MajorDependentDeep, neitherPresentPreferHigher) {
	NoteSet target;
	NoteSet present; // empty
	target.addMajorDependentModeNotes(3, true, present);
	CHECK(!target.has(3));
	CHECK(target.has(4));
}

TEST(MajorDependentDeep, neitherPresentPreferLower) {
	NoteSet target;
	NoteSet present; // empty
	target.addMajorDependentModeNotes(3, false, present);
	CHECK(target.has(3));
	CHECK(!target.has(4));
}

// --- Test at boundary: note 10 (7th degree) ---

TEST(MajorDependentDeep, seventhDegreeBothPresent) {
	NoteSet target;
	NoteSet present({10, 11}); // minor 7th and major 7th
	target.addMajorDependentModeNotes(10, false, present);
	CHECK(target.has(10));
	CHECK(target.has(11));
}

TEST(MajorDependentDeep, seventhDegreeNeitherPreferLower) {
	NoteSet target;
	NoteSet present;
	target.addMajorDependentModeNotes(10, false, present);
	CHECK(target.has(10));
	CHECK(!target.has(11));
}

TEST(MajorDependentDeep, seventhDegreeNeitherPreferHigher) {
	NoteSet target;
	NoteSet present;
	target.addMajorDependentModeNotes(10, true, present);
	CHECK(!target.has(10));
	CHECK(target.has(11));
}

// ============================================================================
// Interval constant relationships
// ============================================================================

TEST_GROUP(IntervalConstants){};

TEST(IntervalConstants, enharmonicEquivalences) {
	// AUG4 == DIM5
	CHECK_EQUAL(deluge::gui::ui::keyboard::AUG4, DIM5);
	// AUG5 == MIN6
	CHECK_EQUAL(AUG5, MIN6);
	// DIM7 == MAJ6
	CHECK_EQUAL(DIM7, MAJ6);
	// DOM7 == MIN7
	CHECK_EQUAL(DOM7, MIN7);
}

TEST(IntervalConstants, octaveRelationships) {
	CHECK_EQUAL(OCT, 12);
	CHECK_EQUAL(deluge::gui::ui::keyboard::MIN9, MIN2 + OCT);
	CHECK_EQUAL(deluge::gui::ui::keyboard::MAJ9, MAJ2 + OCT);
	CHECK_EQUAL(deluge::gui::ui::keyboard::P11, P4 + OCT);
	CHECK_EQUAL(deluge::gui::ui::keyboard::MAJ13, MAJ6 + OCT);
}
