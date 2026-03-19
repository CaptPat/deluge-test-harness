// Deep tests for Iterance (note probability/fill/step pattern system)
// Covers: all divisors, multi-step patterns, passesCheck wrap-around,
// toInt/fromInt exhaustive round-trips, preset table integrity,
// Note integration, fill modes, edge cases.

#include "CppUTest/TestHarness.h"
#include "definitions_cxx.hpp"
#include "model/iterance/iterance.h"
#include "model/note/note.h"
#include "util/lookuptables/lookuptables.h"

// ============================================================================
// passesCheck: exhaustive single-step patterns for every divisor
// ============================================================================

TEST_GROUP(IteranceDeepPassesCheck){};

// Divisor 1: single step, always passes
TEST(IteranceDeepPassesCheck, divisor1AlwaysPasses) {
	Iterance it{1, 0b1};
	for (int i = 0; i < 64; i++) {
		CHECK(it.passesCheck(i));
	}
}

// Divisor 2: each single step
TEST(IteranceDeepPassesCheck, divisor2Step0) {
	Iterance it{2, 0b01};
	for (int i = 0; i < 32; i++) {
		CHECK_EQUAL(i % 2 == 0, it.passesCheck(i));
	}
}

TEST(IteranceDeepPassesCheck, divisor2Step1) {
	Iterance it{2, 0b10};
	for (int i = 0; i < 32; i++) {
		CHECK_EQUAL(i % 2 == 1, it.passesCheck(i));
	}
}

// Divisor 3: each single step
TEST(IteranceDeepPassesCheck, divisor3Step0) {
	Iterance it{3, 0b001};
	for (int i = 0; i < 30; i++) {
		CHECK_EQUAL(i % 3 == 0, it.passesCheck(i));
	}
}

TEST(IteranceDeepPassesCheck, divisor3Step1) {
	Iterance it{3, 0b010};
	for (int i = 0; i < 30; i++) {
		CHECK_EQUAL(i % 3 == 1, it.passesCheck(i));
	}
}

TEST(IteranceDeepPassesCheck, divisor3Step2) {
	Iterance it{3, 0b100};
	for (int i = 0; i < 30; i++) {
		CHECK_EQUAL(i % 3 == 2, it.passesCheck(i));
	}
}

// Divisor 4: each single step
TEST(IteranceDeepPassesCheck, divisor4AllSingleSteps) {
	for (int step = 0; step < 4; step++) {
		Iterance it{4, (uint8_t)(1 << step)};
		for (int i = 0; i < 32; i++) {
			CHECK_EQUAL(i % 4 == step, it.passesCheck(i));
		}
	}
}

// Divisor 5: each single step
TEST(IteranceDeepPassesCheck, divisor5AllSingleSteps) {
	for (int step = 0; step < 5; step++) {
		Iterance it{5, (uint8_t)(1 << step)};
		for (int i = 0; i < 40; i++) {
			CHECK_EQUAL(i % 5 == step, it.passesCheck(i));
		}
	}
}

// Divisor 6: each single step
TEST(IteranceDeepPassesCheck, divisor6AllSingleSteps) {
	for (int step = 0; step < 6; step++) {
		Iterance it{6, (uint8_t)(1 << step)};
		for (int i = 0; i < 48; i++) {
			CHECK_EQUAL(i % 6 == step, it.passesCheck(i));
		}
	}
}

// Divisor 7: each single step
TEST(IteranceDeepPassesCheck, divisor7AllSingleSteps) {
	for (int step = 0; step < 7; step++) {
		Iterance it{7, (uint8_t)(1 << step)};
		for (int i = 0; i < 56; i++) {
			CHECK_EQUAL(i % 7 == step, it.passesCheck(i));
		}
	}
}

// Divisor 8: each single step
TEST(IteranceDeepPassesCheck, divisor8AllSingleSteps) {
	for (int step = 0; step < 8; step++) {
		Iterance it{8, (uint8_t)(1 << step)};
		for (int i = 0; i < 64; i++) {
			CHECK_EQUAL(i % 8 == step, it.passesCheck(i));
		}
	}
}

// ============================================================================
// passesCheck: multi-step (combined) patterns
// ============================================================================

TEST_GROUP(IteranceDeepMultiStep){};

// All steps active = always passes
TEST(IteranceDeepMultiStep, allStepsActiveAlwaysPasses) {
	for (int div = 1; div <= 8; div++) {
		uint8_t allBits = (1 << div) - 1;
		Iterance it{(uint8_t)div, allBits};
		for (int i = 0; i < 32; i++) {
			CHECK(it.passesCheck(i));
		}
	}
}

// No steps active = never passes
TEST(IteranceDeepMultiStep, noStepsActiveNeverPasses) {
	for (int div = 1; div <= 8; div++) {
		Iterance it{(uint8_t)div, 0};
		for (int i = 0; i < 32; i++) {
			CHECK(!it.passesCheck(i));
		}
	}
}

// Alternating steps for divisor 4: 0b0101 = steps 0 and 2
TEST(IteranceDeepMultiStep, divisor4Alternating0And2) {
	Iterance it{4, 0b0101};
	CHECK(it.passesCheck(0));
	CHECK(!it.passesCheck(1));
	CHECK(it.passesCheck(2));
	CHECK(!it.passesCheck(3));
	// Wrap
	CHECK(it.passesCheck(4));
	CHECK(!it.passesCheck(5));
	CHECK(it.passesCheck(6));
	CHECK(!it.passesCheck(7));
}

// Alternating steps for divisor 4: 0b1010 = steps 1 and 3
TEST(IteranceDeepMultiStep, divisor4Alternating1And3) {
	Iterance it{4, 0b1010};
	CHECK(!it.passesCheck(0));
	CHECK(it.passesCheck(1));
	CHECK(!it.passesCheck(2));
	CHECK(it.passesCheck(3));
}

// 3-of-4 active
TEST(IteranceDeepMultiStep, divisor4ThreeOf4) {
	Iterance it{4, 0b0111}; // steps 0,1,2
	CHECK(it.passesCheck(0));
	CHECK(it.passesCheck(1));
	CHECK(it.passesCheck(2));
	CHECK(!it.passesCheck(3));
}

// Divisor 8 with scattered pattern
TEST(IteranceDeepMultiStep, divisor8ScatteredPattern) {
	Iterance it{8, 0b10010100}; // steps 2, 4, 7
	CHECK(!it.passesCheck(0));
	CHECK(!it.passesCheck(1));
	CHECK(it.passesCheck(2));
	CHECK(!it.passesCheck(3));
	CHECK(it.passesCheck(4));
	CHECK(!it.passesCheck(5));
	CHECK(!it.passesCheck(6));
	CHECK(it.passesCheck(7));
	// Wrap around
	CHECK(!it.passesCheck(8));
	CHECK(it.passesCheck(10));
}

// ============================================================================
// passesCheck: large repeat counts (wrap-around stress)
// ============================================================================

TEST_GROUP(IteranceDeepWrapAround){};

TEST(IteranceDeepWrapAround, largeRepeatCountsDivisor3) {
	Iterance it{3, 0b001}; // step 0 only
	// Verify at large multiples of 3
	CHECK(it.passesCheck(300));
	CHECK(!it.passesCheck(301));
	CHECK(!it.passesCheck(302));
	CHECK(it.passesCheck(303));
}

TEST(IteranceDeepWrapAround, largeRepeatCountsDivisor8) {
	Iterance it{8, 0b00000001}; // step 0 only
	CHECK(it.passesCheck(0));
	CHECK(it.passesCheck(800));
	CHECK(!it.passesCheck(801));
	CHECK(it.passesCheck(808));
}

TEST(IteranceDeepWrapAround, maxIntRepeatCountDoesNotCrash) {
	// Ensure no division by zero or crash with edge values
	Iterance it{8, 0b11111111};
	// INT32_MAX % 8 should still work
	CHECK(it.passesCheck(2147483647));
}

TEST(IteranceDeepWrapAround, zeroRepeatCountAlwaysStep0) {
	for (int div = 1; div <= 8; div++) {
		Iterance it{(uint8_t)div, 0b1}; // step 0 set
		CHECK(it.passesCheck(0));
	}
}

// ============================================================================
// toInt / fromInt: exhaustive round-trip for all valid combinations
// ============================================================================

TEST_GROUP(IteranceDeepRoundTrip){};

// Every single-step preset round-trips
TEST(IteranceDeepRoundTrip, allSingleStepRoundTrips) {
	for (int div = 1; div <= 8; div++) {
		for (int step = 0; step < div; step++) {
			uint8_t bits = 1 << step;
			Iterance orig{(uint8_t)div, bits};
			uint16_t packed = orig.toInt();
			Iterance restored = Iterance::fromInt(packed);
			CHECK(orig == restored);
		}
	}
}

// Every multi-step combination for divisor 2-4 (small enough to enumerate)
TEST(IteranceDeepRoundTrip, exhaustiveDivisor2) {
	for (int bits = 0; bits < 4; bits++) {
		Iterance orig{2, (uint8_t)bits};
		Iterance restored = Iterance::fromInt(orig.toInt());
		CHECK(orig == restored);
	}
}

TEST(IteranceDeepRoundTrip, exhaustiveDivisor3) {
	for (int bits = 0; bits < 8; bits++) {
		Iterance orig{3, (uint8_t)bits};
		Iterance restored = Iterance::fromInt(orig.toInt());
		CHECK(orig == restored);
	}
}

TEST(IteranceDeepRoundTrip, exhaustiveDivisor4) {
	for (int bits = 0; bits < 16; bits++) {
		Iterance orig{4, (uint8_t)bits};
		Iterance restored = Iterance::fromInt(orig.toInt());
		CHECK(orig == restored);
	}
}

// Spot-check divisor 8 with all bits
TEST(IteranceDeepRoundTrip, divisor8AllBits) {
	for (int bits = 0; bits < 256; bits++) {
		Iterance orig{8, (uint8_t)bits};
		Iterance restored = Iterance::fromInt(orig.toInt());
		CHECK(orig == restored);
	}
}

// Verify toInt encoding structure
TEST(IteranceDeepRoundTrip, toIntEncodingStructure) {
	for (int div = 1; div <= 8; div++) {
		uint8_t bits = (1 << div) - 1; // all bits for this divisor
		Iterance it{(uint8_t)div, bits};
		uint16_t packed = it.toInt();
		CHECK_EQUAL(div, packed >> 8);
		CHECK_EQUAL(bits, packed & 0xFF);
	}
}

// ============================================================================
// fromInt: invalid/boundary values
// ============================================================================

TEST_GROUP(IteranceDeepFromIntEdge){};

TEST(IteranceDeepFromIntEdge, divisor0FallsBackToDefault) {
	// Any step bits with divisor 0 should give default
	for (int bits = 0; bits < 256; bits++) {
		Iterance it = Iterance::fromInt((0 << 8) | bits);
		CHECK(it == kDefaultIteranceValue);
	}
}

TEST(IteranceDeepFromIntEdge, divisor9Through255FallBackToDefault) {
	// Divisors 9+ are out of range
	for (int div = 9; div <= 15; div++) {
		Iterance it = Iterance::fromInt((div << 8) | 0x01);
		CHECK(it == kDefaultIteranceValue);
	}
}

TEST(IteranceDeepFromIntEdge, divisor8IsMaxValid) {
	Iterance it = Iterance::fromInt((8 << 8) | 0xFF);
	CHECK_EQUAL(8, it.divisor);
	CHECK_EQUAL(0xFF, (int)it.iteranceStep.to_ulong());
}

TEST(IteranceDeepFromIntEdge, negativeValueTreatedAsBadDivisor) {
	// Negative int32_t will have high bits set, divisor extraction will be > 8
	Iterance it = Iterance::fromInt(-1);
	CHECK(it == kDefaultIteranceValue);
}

TEST(IteranceDeepFromIntEdge, highBitsInStepFieldPreserved) {
	// Divisor 3 only uses bits 0-2, but higher bits in step byte are stored
	Iterance it = Iterance::fromInt((3 << 8) | 0xFF);
	CHECK_EQUAL(3, it.divisor);
	CHECK_EQUAL(0xFF, (int)it.iteranceStep.to_ulong());
	// But passesCheck only looks at bits 0-2 via modulo
	CHECK(it.passesCheck(0)); // bit 0
	CHECK(it.passesCheck(1)); // bit 1
	CHECK(it.passesCheck(2)); // bit 2
}

// ============================================================================
// Preset table integrity
// ============================================================================

TEST_GROUP(IteranceDeepPresets){};

// All 35 presets are single-step patterns (one bit set)
TEST(IteranceDeepPresets, allPresetsAreSingleStep) {
	for (int i = 0; i < kNumIterancePresets; i++) {
		Iterance preset = iterancePresets[i];
		// Count set bits
		int setBits = 0;
		for (int b = 0; b < 8; b++) {
			if (preset.iteranceStep[b]) {
				setBits++;
			}
		}
		CHECK_EQUAL(1, setBits);
	}
}

// Presets cover all single-step patterns for divisors 2-8
TEST(IteranceDeepPresets, presetsAreDivisor2Through8SingleSteps) {
	int presetIndex = 0;
	for (int div = 2; div <= 8; div++) {
		for (int step = 0; step < div; step++) {
			LONGS_EQUAL(div, iterancePresets[presetIndex].divisor);
			LONGS_EQUAL(1 << step, (int)iterancePresets[presetIndex].iteranceStep.to_ulong());
			presetIndex++;
		}
	}
	// Total: 2+3+4+5+6+7+8 = 35
	CHECK_EQUAL(kNumIterancePresets, presetIndex);
}

// Preset count constant matches array size
TEST(IteranceDeepPresets, presetCountMatchesArraySize) {
	CHECK_EQUAL(35, kNumIterancePresets);
	CHECK_EQUAL(kNumIterancePresets, (int)iterancePresets.size());
}

// ============================================================================
// toPresetIndex / fromPresetIndex: exhaustive
// ============================================================================

TEST_GROUP(IteranceDeepPresetIndex){};

// Every preset round-trips through toPresetIndex/fromPresetIndex
TEST(IteranceDeepPresetIndex, allPresetsRoundTripExhaustive) {
	for (int i = 0; i < kNumIterancePresets; i++) {
		Iterance preset = iterancePresets[i];
		int32_t idx = preset.toPresetIndex();
		CHECK_EQUAL(i + 1, idx);
		Iterance restored = Iterance::fromPresetIndex(idx);
		CHECK(preset == restored);
	}
}

// Default value maps to index 0
TEST(IteranceDeepPresetIndex, defaultMapsToZero) {
	Iterance def = kDefaultIteranceValue;
	CHECK_EQUAL(0, def.toPresetIndex());
}

// fromPresetIndex(0) returns default
TEST(IteranceDeepPresetIndex, fromIndex0GivesDefault) {
	Iterance it = Iterance::fromPresetIndex(0);
	CHECK(it == kDefaultIteranceValue);
}

// fromPresetIndex with negative values returns default
TEST(IteranceDeepPresetIndex, negativeIndexGivesDefault) {
	for (int i = -10; i < 0; i++) {
		Iterance it = Iterance::fromPresetIndex(i);
		CHECK(it == kDefaultIteranceValue);
	}
}

// fromPresetIndex with values above kNumIterancePresets (but not kCustomIterancePreset) returns default
TEST(IteranceDeepPresetIndex, outOfRangeIndexGivesDefault) {
	for (int i = kNumIterancePresets + 1; i < kCustomIterancePreset; i++) {
		Iterance it = Iterance::fromPresetIndex(i);
		CHECK(it == kDefaultIteranceValue);
	}
}

// fromPresetIndex(kCustomIterancePreset) returns kCustomIteranceValue
TEST(IteranceDeepPresetIndex, customPresetReturnsCustomValue) {
	Iterance it = Iterance::fromPresetIndex(kCustomIterancePreset);
	CHECK(it == kCustomIteranceValue);
	CHECK_EQUAL(1, it.divisor);
	CHECK_EQUAL(1, (int)it.iteranceStep.to_ulong());
}

// Multi-step patterns (custom) always map to kCustomIterancePreset
TEST(IteranceDeepPresetIndex, multiStepAlwaysCustom) {
	// 2-step patterns
	CHECK_EQUAL(kCustomIterancePreset, Iterance(4, 0b0101).toPresetIndex());
	CHECK_EQUAL(kCustomIterancePreset, Iterance(4, 0b1010).toPresetIndex());
	CHECK_EQUAL(kCustomIterancePreset, Iterance(4, 0b0110).toPresetIndex());
	CHECK_EQUAL(kCustomIterancePreset, Iterance(4, 0b1001).toPresetIndex());
	// 3-step patterns
	CHECK_EQUAL(kCustomIterancePreset, Iterance(4, 0b0111).toPresetIndex());
	CHECK_EQUAL(kCustomIterancePreset, Iterance(4, 0b1110).toPresetIndex());
	// All steps active
	CHECK_EQUAL(kCustomIterancePreset, Iterance(4, 0b1111).toPresetIndex());
}

// Divisor-1 patterns are all custom (not in preset table)
TEST(IteranceDeepPresetIndex, divisor1IsAlwaysCustom) {
	Iterance it{1, 0b1};
	CHECK_EQUAL(kCustomIterancePreset, it.toPresetIndex());
}

// Values above kCustomIterancePreset also return default
TEST(IteranceDeepPresetIndex, farOutOfRangeGivesDefault) {
	Iterance it = Iterance::fromPresetIndex(100);
	CHECK(it == kDefaultIteranceValue);
	it = Iterance::fromPresetIndex(1000);
	CHECK(it == kDefaultIteranceValue);
}

// ============================================================================
// Equality operator
// ============================================================================

TEST_GROUP(IteranceDeepEquality){};

TEST(IteranceDeepEquality, sameDivisorDifferentSteps) {
	for (int div = 1; div <= 8; div++) {
		Iterance a{(uint8_t)div, 0b01};
		Iterance b{(uint8_t)div, 0b10};
		CHECK(!(a == b));
	}
}

TEST(IteranceDeepEquality, differentDivisorSameSteps) {
	Iterance a{2, 0b01};
	Iterance b{3, 0b01};
	CHECK(!(a == b));
}

TEST(IteranceDeepEquality, selfEquality) {
	for (int div = 1; div <= 8; div++) {
		Iterance it{(uint8_t)div, (uint8_t)(1 << (div - 1))};
		CHECK(it == it);
	}
}

TEST(IteranceDeepEquality, defaultEqualsDefault) {
	Iterance a = kDefaultIteranceValue;
	Iterance b = kDefaultIteranceValue;
	CHECK(a == b);
}

TEST(IteranceDeepEquality, customEqualsCustom) {
	Iterance a = kCustomIteranceValue;
	Iterance b = kCustomIteranceValue;
	CHECK(a == b);
}

TEST(IteranceDeepEquality, defaultNotEqualCustom) {
	CHECK(!(kDefaultIteranceValue == kCustomIteranceValue));
}

// ============================================================================
// Note integration: iterance and fill on Note objects
// ============================================================================

TEST_GROUP(IteranceDeepNoteIntegration){};

TEST(IteranceDeepNoteIntegration, noteSetGetIteranceBasic) {
	Note note;
	// Note() does not initialize iterance, so set it explicitly first
	note.setIterance(kDefaultIteranceValue);
	Iterance it = note.getIterance();
	CHECK(it == kDefaultIteranceValue);
}

TEST(IteranceDeepNoteIntegration, noteSetGetIteranceRoundTrip) {
	Note note;
	for (int i = 0; i < kNumIterancePresets; i++) {
		note.setIterance(iterancePresets[i]);
		Iterance got = note.getIterance();
		CHECK(iterancePresets[i] == got);
	}
}

TEST(IteranceDeepNoteIntegration, noteSetGetCustomIterance) {
	Note note;
	Iterance custom{5, 0b10101}; // steps 0, 2, 4
	note.setIterance(custom);
	Iterance got = note.getIterance();
	CHECK(custom == got);
}

TEST(IteranceDeepNoteIntegration, noteFillModes) {
	Note note;
	note.setFill(FillMode::OFF);
	CHECK_EQUAL(FillMode::OFF, note.getFill());

	note.setFill(FillMode::FILL);
	CHECK_EQUAL(FillMode::FILL, note.getFill());

	note.setFill(FillMode::NOT_FILL);
	CHECK_EQUAL(FillMode::NOT_FILL, note.getFill());
}

TEST(IteranceDeepNoteIntegration, noteIteranceAndFillIndependent) {
	Note note;
	// Set iterance
	Iterance it{4, 0b0110};
	note.setIterance(it);
	// Set fill
	note.setFill(FillMode::FILL);
	// Both should retain independently
	CHECK(it == note.getIterance());
	CHECK_EQUAL(FillMode::FILL, note.getFill());
	// Change fill, iterance should not change
	note.setFill(FillMode::NOT_FILL);
	CHECK(it == note.getIterance());
	CHECK_EQUAL(FillMode::NOT_FILL, note.getFill());
	// Change iterance, fill should not change
	Iterance it2{3, 0b101};
	note.setIterance(it2);
	CHECK(it2 == note.getIterance());
	CHECK_EQUAL(FillMode::NOT_FILL, note.getFill());
}

TEST(IteranceDeepNoteIntegration, noteProbabilityRange) {
	Note note;
	// Probability ranges from 0 to kNumProbabilityValues (20)
	note.setProbability(0);
	CHECK_EQUAL(0, note.getProbability());
	note.setProbability(kNumProbabilityValues);
	CHECK_EQUAL(kNumProbabilityValues, note.getProbability());
}

TEST(IteranceDeepNoteIntegration, noteAllFieldsCoexist) {
	Note note;
	note.setVelocity(100);
	note.setLift(80);
	note.setProbability(15);
	note.setIterance(Iterance{6, 0b100000});
	note.setFill(FillMode::FILL);
	note.setLength(1234);

	CHECK_EQUAL(100, note.getVelocity());
	CHECK_EQUAL(80, note.getLift());
	CHECK_EQUAL(15, note.getProbability());
	CHECK_EQUAL(6, note.getIterance().divisor);
	CHECK_EQUAL(0b100000, (int)note.getIterance().iteranceStep.to_ulong());
	CHECK_EQUAL(FillMode::FILL, note.getFill());
	CHECK_EQUAL(1234, note.getLength());
}

// ============================================================================
// Fill mode values
// ============================================================================

TEST_GROUP(IteranceDeepFillMode){};

TEST(IteranceDeepFillMode, fillModeEnumValues) {
	CHECK_EQUAL(0, FillMode::OFF);
	CHECK_EQUAL(1, FillMode::NOT_FILL);
	CHECK_EQUAL(2, FillMode::FILL);
}

TEST(IteranceDeepFillMode, kNumFillValuesMatchesFillEnum) {
	CHECK_EQUAL(FillMode::FILL, kNumFillValues);
}

// ============================================================================
// Bitset edge cases: bits beyond divisor range
// ============================================================================

TEST_GROUP(IteranceDeepBitsetEdge){};

// Setting bits beyond the divisor range does not affect passesCheck
// because passesCheck uses repeatCount % divisor to index the bitset
TEST(IteranceDeepBitsetEdge, bitsAboveDivisorIgnoredByPassesCheck) {
	// Divisor 2 with bit 7 also set: 0b10000001
	Iterance it{2, 0b10000001};
	// passesCheck only looks at bits 0 and 1 (via %2)
	CHECK(it.passesCheck(0));  // bit 0 set
	CHECK(!it.passesCheck(1)); // bit 1 not set
}

TEST(IteranceDeepBitsetEdge, divisor3WithHighBitsSet) {
	// Divisor 3: only bits 0,1,2 matter for passesCheck
	Iterance it{3, 0b11111111};
	CHECK(it.passesCheck(0));
	CHECK(it.passesCheck(1));
	CHECK(it.passesCheck(2));
	// Wrap
	CHECK(it.passesCheck(3));
	CHECK(it.passesCheck(4));
	CHECK(it.passesCheck(5));
}

// However, these extra bits DO survive round-trip through toInt/fromInt
TEST(IteranceDeepBitsetEdge, extraBitsSurviveRoundTrip) {
	Iterance orig{2, 0b11111111};
	Iterance restored = Iterance::fromInt(orig.toInt());
	CHECK(orig == restored);
	CHECK_EQUAL(0xFF, (int)restored.iteranceStep.to_ulong());
}

// ============================================================================
// Constants sanity
// ============================================================================

TEST_GROUP(IteranceDeepConstants){};

TEST(IteranceDeepConstants, defaultValueIsOff) {
	CHECK_EQUAL(0, kDefaultIteranceValue.divisor);
	CHECK_EQUAL(0, (int)kDefaultIteranceValue.iteranceStep.to_ulong());
}

TEST(IteranceDeepConstants, customValueIs1of1) {
	CHECK_EQUAL(1, kCustomIteranceValue.divisor);
	CHECK_EQUAL(1, (int)kCustomIteranceValue.iteranceStep.to_ulong());
}

TEST(IteranceDeepConstants, customPresetIndexIs36) {
	CHECK_EQUAL(36, kCustomIterancePreset);
}

TEST(IteranceDeepConstants, numPresetsIs35) {
	CHECK_EQUAL(35, kNumIterancePresets);
}

TEST(IteranceDeepConstants, numProbabilityValuesIs20) {
	CHECK_EQUAL(20, kNumProbabilityValues);
}

// ============================================================================
// Symmetry: passesCheck pattern repeats exactly every <divisor> steps
// ============================================================================

TEST_GROUP(IteranceDeepSymmetry){};

TEST(IteranceDeepSymmetry, patternRepeatsPerfectly) {
	// For every divisor and arbitrary step pattern, the pattern of passes
	// must repeat with exact period = divisor
	uint8_t testPatterns[] = {0b01, 0b10, 0b110, 0b101, 0b0110, 0b10101010, 0b11001100};
	uint8_t testDivisors[] = {2, 2, 3, 3, 4, 8, 8};

	for (int t = 0; t < 7; t++) {
		Iterance it{testDivisors[t], testPatterns[t]};
		int div = testDivisors[t];
		for (int i = 0; i < 64; i++) {
			CHECK_EQUAL(it.passesCheck(i % div), it.passesCheck(i));
		}
	}
}

// ============================================================================
// Stress: all 256 step bit patterns for divisor 8
// ============================================================================

TEST_GROUP(IteranceDeepStress){};

TEST(IteranceDeepStress, allDivisor8PatternsRoundTrip) {
	for (int bits = 0; bits < 256; bits++) {
		Iterance orig{8, (uint8_t)bits};
		uint16_t packed = orig.toInt();

		// Verify encoding
		CHECK_EQUAL(8, packed >> 8);
		CHECK_EQUAL(bits, packed & 0xFF);

		// Verify round-trip
		Iterance restored = Iterance::fromInt(packed);
		CHECK(orig == restored);

		// Verify passesCheck consistency
		for (int step = 0; step < 8; step++) {
			bool expected = (bits >> step) & 1;
			CHECK_EQUAL(expected, orig.passesCheck(step));
		}
	}
}

// All valid divisors, all step patterns: full enumeration of passesCheck correctness
TEST(IteranceDeepStress, allDivisorsPatternsPassesCheckCorrect) {
	for (int div = 1; div <= 8; div++) {
		int maxBits = 1 << div;
		for (int bits = 0; bits < maxBits; bits++) {
			Iterance it{(uint8_t)div, (uint8_t)bits};
			for (int rep = 0; rep < div * 3; rep++) {
				int idx = rep % div;
				bool expected = (bits >> idx) & 1;
				CHECK_EQUAL(expected, it.passesCheck(rep));
			}
		}
	}
}
