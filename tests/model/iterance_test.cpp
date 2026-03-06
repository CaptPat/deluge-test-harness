// Phase 14: Tests for Iterance (step pattern / probability system)
// Exercises toInt/fromInt round-trips, passesCheck, and preset index mapping.

#include "CppUTest/TestHarness.h"
#include "definitions_cxx.hpp"
#include "model/iterance/iterance.h"
#include "util/lookuptables/lookuptables.h"

TEST_GROUP(IteranceTest){};

TEST(IteranceTest, defaultValueIsOff) {
	Iterance it = kDefaultIteranceValue;
	CHECK_EQUAL(0, it.divisor);
	CHECK_EQUAL(0, (int)it.iteranceStep.to_ulong());
}

TEST(IteranceTest, passesCheckAlwaysTrueWhenDivisor1) {
	Iterance it{1, 1}; // 1of1 — step bit 0 is set
	for (int i = 0; i < 16; i++) {
		CHECK(it.passesCheck(i));
	}
}

TEST(IteranceTest, passesCheckEveryOther) {
	Iterance it{2, 0b01}; // 1of2 — passes on even repeats only
	CHECK(it.passesCheck(0));
	CHECK(!it.passesCheck(1));
	CHECK(it.passesCheck(2));
	CHECK(!it.passesCheck(3));
}

TEST(IteranceTest, passesCheck2of2) {
	Iterance it{2, 0b10}; // 2of2 — passes on odd repeats
	CHECK(!it.passesCheck(0));
	CHECK(it.passesCheck(1));
	CHECK(!it.passesCheck(2));
	CHECK(it.passesCheck(3));
}

TEST(IteranceTest, passesCheckEveryThird) {
	Iterance it{3, 0b001}; // 1of3
	CHECK(it.passesCheck(0));
	CHECK(!it.passesCheck(1));
	CHECK(!it.passesCheck(2));
	CHECK(it.passesCheck(3));
}

TEST(IteranceTest, passesCheck2And3Of4) {
	Iterance it{4, 0b0110}; // steps 1 and 2 of 4
	CHECK(!it.passesCheck(0));
	CHECK(it.passesCheck(1));
	CHECK(it.passesCheck(2));
	CHECK(!it.passesCheck(3));
	CHECK(!it.passesCheck(4));
	CHECK(it.passesCheck(5));
}

// ── toInt / fromInt round-trip ──────────────────────────────────────────

TEST(IteranceTest, toIntFromIntRoundTrip) {
	Iterance original{3, 0b101}; // steps 0 and 2 of 3
	uint16_t packed = original.toInt();
	Iterance restored = Iterance::fromInt(packed);
	CHECK(original == restored);
}

TEST(IteranceTest, toIntEncoding) {
	Iterance it{4, 0b1010};
	uint16_t packed = it.toInt();
	CHECK_EQUAL(4, packed >> 8);
	CHECK_EQUAL(0b1010, packed & 0xFF);
}

TEST(IteranceTest, fromIntClampsBadDivisor) {
	// divisor 0 → default
	Iterance it = Iterance::fromInt(0x0001); // divisor=0, step=1
	CHECK(it == kDefaultIteranceValue);

	// divisor 9 → default
	it = Iterance::fromInt(0x0901); // divisor=9, step=1
	CHECK(it == kDefaultIteranceValue);
}

TEST(IteranceTest, fromIntAcceptsValidDivisors) {
	for (int d = 1; d <= 8; d++) {
		uint16_t packed = (d << 8) | 0xFF;
		Iterance it = Iterance::fromInt(packed);
		CHECK_EQUAL(d, it.divisor);
	}
}

// ── Preset index mapping ────────────────────────────────────────────────

TEST(IteranceTest, defaultToPresetIndexIsZero) {
	CHECK_EQUAL(0, kDefaultIteranceValue.toPresetIndex());
}

TEST(IteranceTest, allPresetsRoundTrip) {
	for (int32_t i = 0; i < kNumIterancePresets; i++) {
		Iterance preset = iterancePresets[i];
		int32_t presetIdx = preset.toPresetIndex();
		CHECK_EQUAL(i + 1, presetIdx);

		Iterance restored = Iterance::fromPresetIndex(presetIdx);
		CHECK(preset == restored);
	}
}

TEST(IteranceTest, customPresetIndex) {
	// An iterance that doesn't match any preset → returns kCustomIterancePreset
	Iterance custom{5, 0b10101};
	int32_t idx = custom.toPresetIndex();
	CHECK_EQUAL(kCustomIterancePreset, idx);
}

TEST(IteranceTest, fromPresetIndexCustomGives1of1) {
	Iterance it = Iterance::fromPresetIndex(kCustomIterancePreset);
	CHECK(it == kCustomIteranceValue);
}

TEST(IteranceTest, fromPresetIndexZeroGivesDefault) {
	Iterance it = Iterance::fromPresetIndex(0);
	CHECK(it == kDefaultIteranceValue);
}

TEST(IteranceTest, fromPresetIndexOutOfRange) {
	Iterance it = Iterance::fromPresetIndex(-1);
	CHECK(it == kDefaultIteranceValue);
	it = Iterance::fromPresetIndex(kNumIterancePresets + 5);
	CHECK(it == kDefaultIteranceValue);
}

TEST(IteranceTest, equality) {
	Iterance a{2, 0b01};
	Iterance b{2, 0b01};
	Iterance c{2, 0b10};
	CHECK(a == b);
	CHECK(!(a == c));
}

// ── First few preset values sanity check ────────────────────────────────

TEST(IteranceTest, firstPresetIs1of2) {
	Iterance first = iterancePresets[0];
	CHECK_EQUAL(2, first.divisor);
	CHECK(first.passesCheck(0));
	CHECK(!first.passesCheck(1));
}

TEST(IteranceTest, secondPresetIs2of2) {
	Iterance second = iterancePresets[1];
	CHECK_EQUAL(2, second.divisor);
	CHECK(!second.passesCheck(0));
	CHECK(second.passesCheck(1));
}
