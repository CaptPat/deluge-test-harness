// Phase 14: Tests for value_scaling.h
// Exercises all UI <-> internal value conversion functions.

#include "CppUTest/TestHarness.h"
#include "gui/menu_item/value_scaling.h"
#include <cstdint>
#include <climits>

// ── Standard menu item (full int32 range ↔ 0-50) ────────────────────────

TEST_GROUP(ValueScalingStandard){};

TEST(ValueScalingStandard, minInternalMapsToZero) {
	CHECK_EQUAL(0, computeCurrentValueForStandardMenuItem(INT32_MIN));
}

TEST(ValueScalingStandard, maxInternalMapsTo50) {
	CHECK_EQUAL(50, computeCurrentValueForStandardMenuItem(INT32_MAX));
}

TEST(ValueScalingStandard, zeroInternalMapsTomid) {
	int32_t mid = computeCurrentValueForStandardMenuItem(0);
	CHECK(mid >= 24 && mid <= 26); // should be ~25
}

TEST(ValueScalingStandard, finalOf50IsMax) {
	CHECK_EQUAL(INT32_MAX, computeFinalValueForStandardMenuItem(50));
}

TEST(ValueScalingStandard, finalOf0IsMin) {
	CHECK_EQUAL(INT32_MIN, computeFinalValueForStandardMenuItem(0));
}

TEST(ValueScalingStandard, roundTripMidRange) {
	// Going current→final→current should stay close
	for (int ui = 0; ui <= 50; ui++) {
		int32_t internal = computeFinalValueForStandardMenuItem(ui);
		int32_t back = computeCurrentValueForStandardMenuItem(internal);
		CHECK(back >= ui - 1 && back <= ui + 1);
	}
}

// ── Half-precision menu item (0-INT32_MAX ↔ 0-50) ──────────────────────

TEST_GROUP(ValueScalingHalfPrecision){};

TEST(ValueScalingHalfPrecision, zeroMapsToZero) {
	CHECK_EQUAL(0, computeCurrentValueForHalfPrecisionMenuItem(0));
}

TEST(ValueScalingHalfPrecision, maxMapsTo50) {
	CHECK_EQUAL(50, computeCurrentValueForHalfPrecisionMenuItem(INT32_MAX));
}

TEST(ValueScalingHalfPrecision, finalOf50IsMax) {
	CHECK_EQUAL(INT32_MAX, computeFinalValueForHalfPrecisionMenuItem(50));
}

TEST(ValueScalingHalfPrecision, finalOf0IsZero) {
	CHECK_EQUAL(0, computeFinalValueForHalfPrecisionMenuItem(0));
}

TEST(ValueScalingHalfPrecision, roundTripHalfRange) {
	for (int ui = 0; ui <= 50; ui++) {
		int32_t internal = computeFinalValueForHalfPrecisionMenuItem(ui);
		int32_t back = computeCurrentValueForHalfPrecisionMenuItem(internal);
		CHECK(back >= ui - 1 && back <= ui + 1);
	}
}

// ── Pan (INT32 range ↔ -25 to 25) ──────────────────────────────────────

TEST_GROUP(ValueScalingPan){};

TEST(ValueScalingPan, minInternalMapsToNeg25) {
	int32_t val = computeCurrentValueForPan(INT32_MIN);
	CHECK(val >= -26 && val <= -24);
}

TEST(ValueScalingPan, maxInternalMapsTo25) {
	int32_t val = computeCurrentValueForPan(INT32_MAX);
	CHECK(val >= 24 && val <= 26);
}

TEST(ValueScalingPan, zeroInternalMapsToZero) {
	CHECK_EQUAL(0, computeCurrentValueForPan(0));
}

TEST(ValueScalingPan, finalOf25IsMax) {
	CHECK_EQUAL(INT32_MAX, computeFinalValueForPan(25));
}

TEST(ValueScalingPan, finalOfNeg25IsMin) {
	CHECK_EQUAL(INT32_MIN, computeFinalValueForPan(-25));
}

TEST(ValueScalingPan, finalOfZeroIsZero) {
	CHECK_EQUAL(0, computeFinalValueForPan(0));
}

TEST(ValueScalingPan, roundTripPan) {
	for (int ui = -25; ui <= 25; ui++) {
		int32_t internal = computeFinalValueForPan(ui);
		int32_t back = computeCurrentValueForPan(internal);
		CHECK(back >= ui - 1 && back <= ui + 1);
	}
}

// ── Unsigned menu item (UINT32 ↔ 0-50) ─────────────────────────────────

TEST_GROUP(ValueScalingUnsigned){};

TEST(ValueScalingUnsigned, zeroMapsToZero) {
	CHECK_EQUAL(0, computeCurrentValueForUnsignedMenuItem(0u));
}

TEST(ValueScalingUnsigned, maxUintMapsTo50) {
	CHECK_EQUAL(50, computeCurrentValueForUnsignedMenuItem(UINT32_MAX));
}

TEST(ValueScalingUnsigned, finalOf0IsZero) {
	CHECK_EQUAL(0u, computeFinalValueForUnsignedMenuItem(0));
}

TEST(ValueScalingUnsigned, finalIsMonotonic) {
	uint32_t prev = 0;
	for (int ui = 1; ui <= 50; ui++) {
		uint32_t val = computeFinalValueForUnsignedMenuItem(ui);
		CHECK(val > prev);
		prev = val;
	}
}

TEST(ValueScalingUnsigned, roundTripUnsigned) {
	for (int ui = 0; ui <= 50; ui++) {
		uint32_t internal = computeFinalValueForUnsignedMenuItem(ui);
		int32_t back = computeCurrentValueForUnsignedMenuItem(internal);
		CHECK(back >= ui - 1 && back <= ui + 1);
	}
}

// ── Transpose (semitones + cents ↔ combined value) ──────────────────────

TEST_GROUP(ValueScalingTranspose){};

TEST(ValueScalingTranspose, currentValueSimple) {
	CHECK_EQUAL(500, computeCurrentValueForTranspose(5, 0));
	CHECK_EQUAL(530, computeCurrentValueForTranspose(5, 30));
	CHECK_EQUAL(-1200, computeCurrentValueForTranspose(-12, 0));
}

TEST(ValueScalingTranspose, finalValuesRoundTrip) {
	int32_t transpose, cents;
	computeFinalValuesForTranspose(500, &transpose, &cents);
	CHECK_EQUAL(5, transpose);
	CHECK_EQUAL(0, cents);
}

TEST(ValueScalingTranspose, finalValuesWithCents) {
	int32_t transpose, cents;
	computeFinalValuesForTranspose(530, &transpose, &cents);
	CHECK_EQUAL(5, transpose);
	CHECK_EQUAL(30, cents);
}

TEST(ValueScalingTranspose, finalValuesNegative) {
	int32_t transpose, cents;
	computeFinalValuesForTranspose(-1200, &transpose, &cents);
	CHECK_EQUAL(-12, transpose);
	CHECK_EQUAL(0, cents);
}

TEST(ValueScalingTranspose, roundTripSeveralOctaves) {
	for (int semi = -24; semi <= 24; semi++) {
		int32_t current = computeCurrentValueForTranspose(semi, 0);
		int32_t transpose, cents;
		computeFinalValuesForTranspose(current, &transpose, &cents);
		CHECK_EQUAL(semi, transpose);
		CHECK(cents >= -1 && cents <= 1); // allow rounding
	}
}
