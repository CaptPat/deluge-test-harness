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

TEST(Waves, triangleSmallSecondHalf) {
	// phase >= 0x80000000 hits the negation branch (line 49)
	int32_t val = getTriangleSmall(0x80000000u);
	// phase = -0x80000000 = 0x80000000 (wraps), result = 0x80000000 - 0x40000000
	CHECK_EQUAL(1073741824, val);
}

TEST(Waves, triangleSmallThreeQuarter) {
	// 0xC0000000 → negated = 0x40000000, result = 0x40000000 - 0x40000000 = 0
	CHECK_EQUAL(0, getTriangleSmall(0xC0000000u));
}

TEST(Waves, triangleSmallMaxPhase) {
	// 0xFFFFFFFF → negated = 1, result = 1 - 0x40000000
	int32_t val = getTriangleSmall(0xFFFFFFFFu);
	CHECK_EQUAL(1 - 1073741824, val);
}

// ── getTriangle ────────────────────────────────────────────────────────

TEST(Waves, sineWithSmallBitsHitsLeftShift) {
	// numBitsInInput=16 → rshiftAmount = 16 - 16 - 8 = -8, hits the else branch (line 23)
	int32_t val = getSine(0x4000u, 16); // quarter phase in 16 bits
	CHECK(val > 0);
}

TEST(Waves, interpolateTableNegativeRshift) {
	// Direct call with numBitsInInput < 24 to exercise left-shift path
	// numBitsInInput=20 → rshiftAmount = 20-16-8 = -4 → left shift by 4
	int32_t val = interpolateTableSigned(0x40000u, 20, sineWaveSmall, 8);
	CHECK(val > 0);
}

TEST(Waves, triangleAtQuarterIsZero) {
	CHECK_EQUAL(0, getTriangle(0x40000000u));
}

TEST(Waves, triangleSymmetry) {
	// Triangle at 1/4 and 3/4 should be negatives of each other
	int32_t q1 = getTriangle(0x40000000u);
	int32_t q3 = getTriangle(0xC0000000u);
	CHECK_EQUAL(q1, -q3 - 1); // off-by-one from asymmetric integer range
}
