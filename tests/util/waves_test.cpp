// Phase 11: Waves / waveform generation tests
// Tests the PRNG global and inline waveform generators from util/waves.h

#include "CppUTest/TestHarness.h"
#include "util/waves.h"

TEST_GROUP(Waves){};

// ── PRNG global ────────────────────────────────────────────────────────

TEST(Waves, jcongInitializedNonZero) {
	CHECK(jcong != 0);
}

TEST(Waves, congMacroDeterministic) {
	uint32_t saved = jcong;
	uint32_t first = CONG;
	jcong = saved;
	uint32_t second = CONG;
	CHECK_EQUAL(first, second);
	jcong = saved; // restore
}

// ── getSine ────────────────────────────────────────────────────────────

TEST(Waves, sineAtZeroIsZero) {
	int32_t val = getSine(0);
	CHECK_EQUAL(0, val);
}

TEST(Waves, sineAtQuarterPhaseIsPositive) {
	int32_t val = getSine(0x40000000u);
	CHECK(val > 0);
}

TEST(Waves, sineAtThreeQuarterIsNegative) {
	int32_t val = getSine(0xC0000000u);
	CHECK(val < 0);
}

// ── getSquare ──────────────────────────────────────────────────────────

TEST(Waves, squareBeforeThreshold) {
	CHECK_EQUAL(2147483647, getSquare(0));
}

TEST(Waves, squareAfterThreshold) {
	CHECK_EQUAL(-2147483648, getSquare(0x80000000u));
}

// ── getSquareSmall ─────────────────────────────────────────────────────

TEST(Waves, squareSmallBeforeThreshold) {
	CHECK_EQUAL(1073741823, getSquareSmall(0));
}

TEST(Waves, squareSmallAfterThreshold) {
	CHECK_EQUAL(-1073741824, getSquareSmall(0x80000000u));
}

// ── getTriangleSmall ───────────────────────────────────────────────────

TEST(Waves, triangleSmallAtZero) {
	CHECK_EQUAL(-1073741824, getTriangleSmall(0));
}

TEST(Waves, triangleSmallAtQuarter) {
	CHECK_EQUAL(0, getTriangleSmall(0x40000000u));
}

// ── getTriangle ────────────────────────────────────────────────────────

TEST(Waves, triangleAtQuarterIsZero) {
	CHECK_EQUAL(0, getTriangle(0x40000000u));
}

TEST(Waves, triangleSymmetry) {
	// Triangle at 1/4 and 3/4 should be negatives of each other
	int32_t q1 = getTriangle(0x40000000u);
	int32_t q3 = getTriangle(0xC0000000u);
	CHECK_EQUAL(q1, -q3 - 1); // off-by-one from asymmetric integer range
}
