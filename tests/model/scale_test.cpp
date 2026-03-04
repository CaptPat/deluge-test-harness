// Scale system regression tests — ported from firmware/tests/unit/scale_tests.cpp
// Tests NoteSet, MusicalKey, ScaleMapper against real firmware implementations.

#include "CppUTest/TestHarness.h"
#include "model/scale/musical_key.h"
#include "model/scale/note_set.h"
#include "model/scale/preset_scales.h"
#include "model/scale/scale_change.h"
#include "model/scale/scale_mapper.h"
#include "model/scale/utils.h"

#include <algorithm>
#include <random>
#include <vector>

// ── NoteSet ─────────────────────────────────────────────────────────────

TEST_GROUP(NoteSetTest){};

TEST(NoteSetTest, init) {
	NoteSet notes;
	CHECK(notes.size == 12);
	for (int i = 0; i < notes.size; i++) {
		CHECK(!notes.has(i));
	}
}

TEST(NoteSetTest, listConstructor) {
	NoteSet notes = NoteSet({0, 1, 4, 11});
	CHECK_EQUAL(4, notes.count());
	CHECK_EQUAL(0, notes[0]);
	CHECK_EQUAL(1, notes[1]);
	CHECK_EQUAL(4, notes[2]);
	CHECK_EQUAL(11, notes[3]);
}

TEST(NoteSetTest, add) {
	NoteSet notes;
	notes.add(7);
	for (int i = 0; i < notes.size; i++) {
		CHECK(notes.has(i) == (i == 7));
	}
}

TEST(NoteSetTest, fill) {
	NoteSet notes;
	notes.fill();
	for (int i = 0; i < notes.size; i++) {
		CHECK(notes.has(i));
	}
}

TEST(NoteSetTest, count) {
	NoteSet notes;
	CHECK(notes.count() == 0);
	notes.add(3);
	CHECK(notes.count() == 1);
	notes.fill();
	CHECK(notes.count() == 12);
}

TEST(NoteSetTest, union) {
	NoteSet a;
	NoteSet b;
	NoteSet c;
	CHECK_EQUAL(c, a | b);
	CHECK_EQUAL(c, b | a);
	a.fill();
	c.fill();
	CHECK_EQUAL(c, a | b);
	CHECK_EQUAL(c, b | a);
	a.clear();
	c.clear();
	a.add(0);
	b.add(7);
	c.add(0);
	c.add(7);
	CHECK_EQUAL(c, a | b);
	CHECK_EQUAL(c, b | a);
	CHECK_EQUAL(1, a.count());
	CHECK_EQUAL(1, b.count());
}

TEST(NoteSetTest, scaleSize) {
	NoteSet notes;
	CHECK_EQUAL(1, notes.scaleSize());
	notes.add(0);
	CHECK_EQUAL(1, notes.scaleSize());
	notes.add(3);
	CHECK_EQUAL(2, notes.scaleSize());
}

TEST(NoteSetTest, clear) {
	NoteSet notes;
	notes.add(1);
	notes.add(2);
	CHECK_EQUAL(2, notes.count());
	notes.clear();
	CHECK_EQUAL(0, notes.count());
}

TEST(NoteSetTest, highest) {
	NoteSet notes;
	notes.add(0);
	CHECK_EQUAL(0, notes.highest());
	notes.add(1);
	CHECK_EQUAL(1, notes.highest());
	notes.add(7);
	CHECK_EQUAL(7, notes.highest());
	notes.add(11);
	CHECK_EQUAL(11, notes.highest());
}

TEST(NoteSetTest, addUntrusted) {
	NoteSet a;
	a.addUntrusted(0);
	a.addUntrusted(0);
	a.addUntrusted(12);
	CHECK_EQUAL(0, a[0]);
	CHECK_EQUAL(1, a[1]);
	CHECK_EQUAL(11, a[2]);
	CHECK_EQUAL(3, a.count());
}

TEST(NoteSetTest, degreeOfBasic) {
	NoteSet a;
	a.add(0);
	a.add(2);
	a.add(4);
	CHECK_EQUAL(0, a.degreeOf(0));
	CHECK_EQUAL(1, a.degreeOf(2));
	CHECK_EQUAL(2, a.degreeOf(4));
}

TEST(NoteSetTest, degreeOfNotAScale) {
	NoteSet a;
	a.add(1);
	a.add(2);
	a.add(4);
	CHECK_EQUAL(-1, a.degreeOf(0));
	CHECK_EQUAL(0, a.degreeOf(1));
	CHECK_EQUAL(1, a.degreeOf(2));
	CHECK_EQUAL(2, a.degreeOf(4));
}

TEST(NoteSetTest, isSubsetOf) {
	NoteSet a;
	NoteSet b;
	CHECK(a.isSubsetOf(b));
	CHECK(b.isSubsetOf(a));
	a.add(3);
	b.add(3);
	CHECK(a.isSubsetOf(b));
	CHECK(b.isSubsetOf(a));
	a.add(0);
	CHECK(!a.isSubsetOf(b));
	CHECK(b.isSubsetOf(a));
	b.add(0);
	b.add(11);
	CHECK(a.isSubsetOf(b));
	CHECK(!b.isSubsetOf(a));
	a.add(7);
	CHECK(!a.isSubsetOf(b));
	CHECK(!b.isSubsetOf(a));
}

TEST(NoteSetTest, equality) {
	NoteSet a;
	NoteSet b;
	CHECK(a == b);
	a.add(0);
	CHECK(a != b);
}

TEST(NoteSetTest, checkEqualAllowed) {
	CHECK_EQUAL(NoteSet(), NoteSet());
}

TEST(NoteSetTest, isEmpty) {
	NoteSet a;
	CHECK_EQUAL(true, a.isEmpty());
	a.add(0);
	CHECK_EQUAL(false, a.isEmpty());
}

TEST(NoteSetTest, subscript) {
	NoteSet a;
	for (int i = 0; i < NoteSet::size; i++) {
		CHECK_EQUAL(-1, a[i]);
	}
	a.add(0);
	a.add(2);
	a.add(4);
	a.add(5);
	a.add(7);
	a.add(9);
	a.add(11);
	CHECK_EQUAL(0, a[0]);
	CHECK_EQUAL(2, a[1]);
	CHECK_EQUAL(4, a[2]);
	CHECK_EQUAL(5, a[3]);
	CHECK_EQUAL(7, a[4]);
	CHECK_EQUAL(9, a[5]);
	CHECK_EQUAL(11, a[6]);
}

TEST(NoteSetTest, remove) {
	NoteSet a;
	for (int i = 0; i < 12; i++) {
		a.remove(i);
		CHECK_EQUAL(0, a.count());
	}
	a.fill();
	for (int i = 0; i < 12; i++) {
		CHECK_EQUAL(true, a.has(i));
		a.remove(i);
		CHECK_EQUAL(false, a.has(i));
	}
	a.fill();
	for (int i = 11; i >= 0; i--) {
		CHECK_EQUAL(true, a.has(i));
		a.remove(i);
		CHECK_EQUAL(false, a.has(i));
	}
}

TEST(NoteSetTest, getScale) {
	CHECK_EQUAL(MAJOR_SCALE, getScale(presetScaleNotes[MAJOR_SCALE]));
	CHECK_EQUAL(MINOR_SCALE, getScale(presetScaleNotes[MINOR_SCALE]));
	CHECK_EQUAL(USER_SCALE, getScale(NoteSet()));
}

TEST(NoteSetTest, majorness) {
	CHECK_EQUAL(0, NoteSet({0}).majorness());
	CHECK_EQUAL(-1, NoteSet({0, 3}).majorness());
	CHECK_EQUAL(1, NoteSet({0, 4}).majorness());
	CHECK_EQUAL(0, NoteSet({0, 3, 4}).majorness());
	CHECK_EQUAL(-1, NoteSet({0, 1}).majorness());
	CHECK_EQUAL(-1, NoteSet({0, 1, 3, 4}).majorness());
	CHECK_EQUAL(-1, NoteSet({0, 8}).majorness());
	CHECK_EQUAL(-1, NoteSet({0, 8, 3, 4}).majorness());
	CHECK_EQUAL(1, NoteSet({0, 9}).majorness());
	CHECK_EQUAL(1, NoteSet({0, 9, 3, 4}).majorness());
}

TEST(NoteSetTest, addMajorDependentModeNotes) {
	NoteSet a;
	a.addMajorDependentModeNotes(1, false, NoteSet{1});
	CHECK_EQUAL(NoteSet({1}), a);
	a.clear();
	a.addMajorDependentModeNotes(1, true, NoteSet{1});
	CHECK_EQUAL(NoteSet({1}), a);

	a.clear();
	a.addMajorDependentModeNotes(1, false, NoteSet{2});
	CHECK_EQUAL(NoteSet({2}), a);
	a.clear();
	a.addMajorDependentModeNotes(1, true, NoteSet{2});
	CHECK_EQUAL(NoteSet({2}), a);

	a.clear();
	a.addMajorDependentModeNotes(1, false, NoteSet{1, 2});
	CHECK_EQUAL(NoteSet({1, 2}), a);
	a.clear();
	a.addMajorDependentModeNotes(1, true, NoteSet{1, 2});
	CHECK_EQUAL(NoteSet({1, 2}), a);

	a.clear();
	a.addMajorDependentModeNotes(1, false, NoteSet{});
	CHECK_EQUAL(NoteSet({1}), a);
	a.clear();
	a.addMajorDependentModeNotes(1, true, NoteSet{});
	CHECK_EQUAL(NoteSet({2}), a);
}

TEST(NoteSetTest, toImpliedScale) {
	CHECK_EQUAL(presetScaleNotes[MAJOR_SCALE], NoteSet({}).toImpliedScale());
	CHECK_EQUAL(presetScaleNotes[PHRYGIAN_SCALE], NoteSet({1}).toImpliedScale());
	CHECK_EQUAL(presetScaleNotes[MAJOR_SCALE], NoteSet({2}).toImpliedScale());
	CHECK_EQUAL(presetScaleNotes[MINOR_SCALE], NoteSet({3}).toImpliedScale());
	CHECK_EQUAL(presetScaleNotes[MAJOR_SCALE], NoteSet({4}).toImpliedScale());
	CHECK_EQUAL(presetScaleNotes[MAJOR_SCALE], NoteSet({5}).toImpliedScale());
	CHECK_EQUAL(presetScaleNotes[LYDIAN_SCALE], NoteSet({6}).toImpliedScale());
	CHECK_EQUAL(presetScaleNotes[MAJOR_SCALE], NoteSet({7}).toImpliedScale());
	CHECK_EQUAL(presetScaleNotes[MINOR_SCALE], NoteSet({8}).toImpliedScale());
	CHECK_EQUAL(presetScaleNotes[MAJOR_SCALE], NoteSet({9}).toImpliedScale());
	CHECK_EQUAL(presetScaleNotes[MIXOLYDIAN_SCALE], NoteSet({10}).toImpliedScale());
	CHECK_EQUAL(presetScaleNotes[MAJOR_SCALE], NoteSet({11}).toImpliedScale());
}

TEST(NoteSetTest, highestNotIn) {
	NoteSet a;
	NoteSet b;
	a.clear();
	b.clear();
	CHECK_EQUAL(-1, a.highestNotIn(b));
	a.clear();
	b.fill();
	CHECK_EQUAL(-1, a.highestNotIn(b));
	a.fill();
	b.clear();
	CHECK_EQUAL(11, a.highestNotIn(b));
	a.fill();
	b.fill();
	CHECK_EQUAL(-1, a.highestNotIn(b));

	const NoteSet major{0, 2, 4, 5, 7, 9, 11};
	a = major;
	for (int i = 0; i < (int)sizeof(major); i++) {
		b = major;
		b.remove(major[i]);
		CHECK_EQUAL(major[i], a.highestNotIn(b));
	}
}

TEST(NoteSetTest, modulateByOffset) {
	NoteSet notes = NoteSet({0, 1, 4, 11});
	NoteSet modulated = notes.modulateByOffset(3);
	CHECK_EQUAL(2, modulated[0]);
	CHECK_EQUAL(3, modulated[1]);
	CHECK_EQUAL(4, modulated[2]);
	CHECK_EQUAL(7, modulated[3]);
}

// ── MusicalKey ──────────────────────────────────────────────────────────

TEST_GROUP(MusicalKeyTest){};

TEST(MusicalKeyTest, ctor) {
	MusicalKey k;
	CHECK_EQUAL(0, k.rootNote);
	CHECK_EQUAL(1, k.modeNotes.count());
	CHECK_EQUAL(true, k.modeNotes.has(0));
}

TEST(MusicalKeyTest, intervalOf) {
	MusicalKey key;
	for (int8_t octave = -10; octave <= 10; octave++) {
		for (uint8_t note = 0; note < 12; note++) {
			for (uint8_t root = 0; root < 12; root++) {
				key.rootNote = root;
				if (root <= note) {
					CHECK_EQUAL(note - root, key.intervalOf(note + octave * 12));
				}
				else {
					CHECK_EQUAL(12 - root + note, key.intervalOf(note + octave * 12));
				}
			}
		}
	}
}

TEST(MusicalKeyTest, degreeOf) {
	MusicalKey key;
	key.rootNote = 9; // A
	key.modeNotes = presetScaleNotes[MINOR_SCALE];

	for (int octave = -2; octave <= 2; octave++) {
		CHECK_EQUAL(0, key.degreeOf(9 + octave * 12));
		CHECK_EQUAL(1, key.degreeOf(11 + octave * 12));
		CHECK_EQUAL(2, key.degreeOf(0 + octave * 12));
		CHECK_EQUAL(3, key.degreeOf(2 + octave * 12));
		CHECK_EQUAL(4, key.degreeOf(4 + octave * 12));
		CHECK_EQUAL(5, key.degreeOf(5 + octave * 12));
		CHECK_EQUAL(6, key.degreeOf(7 + octave * 12));
		CHECK_EQUAL(-1, key.degreeOf(10 + octave * 12));
		CHECK_EQUAL(-1, key.degreeOf(1 + octave * 12));
		CHECK_EQUAL(-1, key.degreeOf(3 + octave * 12));
		CHECK_EQUAL(-1, key.degreeOf(6 + octave * 12));
		CHECK_EQUAL(-1, key.degreeOf(8 + octave * 12));
	}
}

// ── Utility ─────────────────────────────────────────────────────────────

TEST_GROUP(UtilTest){};

TEST(UtilTest, isSameNote) {
	for (int a = -200; a <= 200; a++) {
		for (int b = -200; b <= 200; b++) {
			int legacy1 = (int32_t)std::abs(a - b) % 12 == 0;
			bool same = isSameNote(a, b);
			CHECK_EQUAL(legacy1, same);
		}
	}
}

// ── ScaleMapper ─────────────────────────────────────────────────────────

TEST_GROUP(ScaleMapperTest){};

TEST(ScaleMapperTest, smallerTargetScaleExactlyUsed) {
	ScaleMapper scaleMapper;
	const NoteSet diatonicMajor = NoteSet{0, 2, 4, 5, 7, 9, 11};
	const NoteSet pentatonicMajor = NoteSet{0, 2, 4, 7, 9};
	NoteSet notes = pentatonicMajor;

	ScaleChange changes;
	bool ok = scaleMapper.computeChangeFrom(notes, diatonicMajor, pentatonicMajor, changes);
	CHECK(ok);
	NoteSet targetNotes = changes.applyTo(notes);
	CHECK_EQUAL(pentatonicMajor, targetNotes);

	ok = scaleMapper.computeChangeFrom(targetNotes, pentatonicMajor, diatonicMajor, changes);
	CHECK(ok);
	NoteSet reverseNotes = changes.applyTo(targetNotes);
	CHECK_EQUAL(notes, reverseNotes);
}

TEST(ScaleMapperTest, pentatonicMinorToDiatonicMajor) {
	ScaleMapper scaleMapper;
	const NoteSet pentatonicMinor = NoteSet{0, 2, 3, 7, 9};
	const NoteSet diatonicMajor = NoteSet{0, 2, 4, 5, 7, 9, 11};
	const NoteSet notes = NoteSet{0, 3, 7, 9};
	const NoteSet want = NoteSet{0, 4, 7, 9};

	ScaleChange changes;
	bool ok = scaleMapper.computeChangeFrom(notes, pentatonicMinor, diatonicMajor, changes);
	CHECK(ok);
	NoteSet targetNotes = changes.applyTo(notes);
	CHECK_EQUAL(want, targetNotes);

	ok = scaleMapper.computeChangeFrom(targetNotes, diatonicMajor, pentatonicMinor, changes);
	CHECK(ok);
	NoteSet reverseNotes = changes.applyTo(targetNotes);
	CHECK_EQUAL(notes, reverseNotes);
}

TEST(ScaleMapperTest, willRefuseIfDoesNotFitTarget) {
	ScaleMapper scaleMapper;
	const NoteSet diatonicMajor = NoteSet{0, 2, 4, 5, 7, 9, 11};
	const NoteSet pentatonicMajor = NoteSet{0, 2, 4, 7, 9};
	const NoteSet notes = NoteSet{2, 4, 7, 9, 11};

	ScaleChange changes;
	bool ok = scaleMapper.computeChangeFrom(notes, diatonicMajor, pentatonicMajor, changes);
	CHECK(!ok);
}

TEST(ScaleMapperTest, willRefuseIfSourceNotesNotInScale) {
	ScaleMapper scaleMapper;
	const NoteSet diatonicMajor = NoteSet{0, 2, 4, 5, 7, 9, 11};
	const NoteSet pentatonicMajor = NoteSet{0, 2, 4, 7, 9};
	const NoteSet notes = NoteSet{3};

	ScaleChange changes;
	bool ok = scaleMapper.computeChangeFrom(notes, diatonicMajor, pentatonicMajor, changes);
	CHECK(!ok);
}

TEST(ScaleMapperTest, randomTest) {
	std::random_device r;
	int s1 = r();
	int s2 = r();
	std::seed_seq seeds{s1, s2};
	std::mt19937 engine(seeds);

	std::uniform_int_distribution<int> randomNonRoot(1, 11);
	std::uniform_int_distribution<int> randomCoin(0, 1);

	auto randomScale = [&](uint8_t size) -> NoteSet {
		NoteSet scale;
		scale.add(0);
		if (size > 6) {
			scale.fill();
		}
		while (scale.count() != size) {
			uint8_t note = randomNonRoot(engine);
			if (scale.count() < size && !scale.has(note)) {
				scale.add(note);
			}
			else if (scale.count() > size && scale.has(note)) {
				scale.remove(note);
			}
		}
		return scale;
	};

	auto randomNotesIn = [&](NoteSet scale) -> NoteSet {
		NoteSet notes;
		while (notes.isEmpty()) {
			for (uint8_t n = 0; n < scale.count(); n++) {
				if (randomCoin(engine)) {
					notes.add(scale[n]);
				}
			}
		}
		return notes;
	};

	std::vector<NoteSet> scales;
	std::vector<int> scaleRanges;
	scaleRanges.push_back(0);
	scaleRanges.push_back(0);

	const int nScalesPerSize = 2;
	for (int s = scaleRanges.size(); s < kMaxScaleSize; s++) {
		int base = scales.size();
		scaleRanges.push_back(base);
		while (static_cast<int>(scales.size()) - base < nScalesPerSize) {
			NoteSet newScale = randomScale(s);
			if (std::find(scales.begin() + base, scales.end(), newScale) == std::end(scales)) {
				scales.push_back(newScale);
			}
		}
	}

	ScaleMapper scaleMapper;
	for (auto& sourceScale : scales) {
		ScaleChange changes;
		NoteSet sourceNotes = randomNotesIn(sourceScale);
		int size = sourceNotes.scaleSize();
		int start = scaleRanges[size];
		CHECK(start >= 0);
		int end = scales.size();
		NoteSet testScale = sourceScale;
		NoteSet testNotes = sourceNotes;

		std::vector<int> worklist(end - start);
		for (int i = start; i < end; i++) {
			worklist[i - start] = i;
		}
		std::shuffle(worklist.begin(), worklist.end(), engine);

		for (int n : worklist) {
			NoteSet targetScale = scales[n];
			bool ok = scaleMapper.computeChangeFrom(testNotes, testScale, targetScale, changes);
			CHECK(ok);
			testNotes = changes.applyTo(testNotes);
			testScale = targetScale;
			CHECK(testNotes.isSubsetOf(testScale));
			ok = scaleMapper.computeChangeFrom(testNotes, testScale, sourceScale, changes);
			CHECK(ok);
			NoteSet reverse = changes.applyTo(testNotes);
			CHECK_EQUAL(sourceNotes, reverse);
		}
	}
}
