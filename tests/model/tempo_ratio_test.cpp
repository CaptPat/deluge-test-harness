// Tempo ratio scaling tests for PR #4367 (per-clip tempo ratio).
// Tests scaleGlobalToClipTicks (accumulator-based) and clipTicksToGlobalTicks (ceiling division).

#include "CppUTest/TestHarness.h"
#include "clip_mocks.h"

TEST_GROUP(TempoRatioTest) {
	Clip clip;
	void setup() override {
		clip.tempoRatioNumerator = 1;
		clip.tempoRatioDenominator = 1;
		clip.tempoRatioAccumulator = 0;
	}
};

// ── Identity ratio (1:1) ─────────────────────────────────────────────────────

TEST(TempoRatioTest, identityRatioPassesThrough) {
	CHECK_EQUAL(10, clip.scaleGlobalToClipTicks(10));
}

TEST(TempoRatioTest, identityRatioNegative) {
	CHECK_EQUAL(-5, clip.scaleGlobalToClipTicks(-5));
}

TEST(TempoRatioTest, identityClipToGlobal) {
	CHECK_EQUAL(10, clip.clipTicksToGlobalTicks(10));
}

TEST(TempoRatioTest, hasTempoRatioFalseForIdentity) {
	CHECK_FALSE(clip.hasTempoRatio());
}

TEST(TempoRatioTest, hasTempoRatioTrueForNonIdentity) {
	clip.tempoRatioNumerator = 3;
	clip.tempoRatioDenominator = 2;
	CHECK_TRUE(clip.hasTempoRatio());
}

// ── 3:2 ratio (150% — 3 clip ticks per 2 global ticks) ──────────────────────

TEST(TempoRatioTest, threeToTwo_singleTickSequence) {
	clip.tempoRatioNumerator = 3;
	clip.tempoRatioDenominator = 2;
	// Feed 1 global tick at a time. 3/2 = 1.5x.
	// tick 1: total = 1*3 + 0 = 3, result = 3/2 = 1, accum = 1
	CHECK_EQUAL(1, clip.scaleGlobalToClipTicks(1));
	// tick 2: total = 1*3 + 1 = 4, result = 4/2 = 2, accum = 0
	CHECK_EQUAL(2, clip.scaleGlobalToClipTicks(1));
	// tick 3: total = 1*3 + 0 = 3, result = 1, accum = 1
	CHECK_EQUAL(1, clip.scaleGlobalToClipTicks(1));
	// tick 4: total = 1*3 + 1 = 4, result = 2, accum = 0
	CHECK_EQUAL(2, clip.scaleGlobalToClipTicks(1));
}

TEST(TempoRatioTest, threeToTwo_totalOverFourTicks) {
	clip.tempoRatioNumerator = 3;
	clip.tempoRatioDenominator = 2;
	int32_t total = 0;
	for (int i = 0; i < 4; i++) {
		total += clip.scaleGlobalToClipTicks(1);
	}
	// 4 global ticks * 3/2 = 6 clip ticks
	CHECK_EQUAL(6, total);
}

TEST(TempoRatioTest, threeToTwo_bulkConversion) {
	clip.tempoRatioNumerator = 3;
	clip.tempoRatioDenominator = 2;
	// 10 global ticks at once: 10 * 3 / 2 = 15
	CHECK_EQUAL(15, clip.scaleGlobalToClipTicks(10));
}

// ── 1:2 ratio (50% — half speed) ────────────────────────────────────────────

TEST(TempoRatioTest, oneToTwo_halfSpeed) {
	clip.tempoRatioNumerator = 1;
	clip.tempoRatioDenominator = 2;
	// tick 1: total = 1*1 + 0 = 1, result = 1/2 = 0, accum = 1
	CHECK_EQUAL(0, clip.scaleGlobalToClipTicks(1));
	// tick 2: total = 1*1 + 1 = 2, result = 2/2 = 1, accum = 0
	CHECK_EQUAL(1, clip.scaleGlobalToClipTicks(1));
}

TEST(TempoRatioTest, oneToTwo_bulkConversion) {
	clip.tempoRatioNumerator = 1;
	clip.tempoRatioDenominator = 2;
	CHECK_EQUAL(5, clip.scaleGlobalToClipTicks(10));
}

// ── 2:1 ratio (200% — double speed) ─────────────────────────────────────────

TEST(TempoRatioTest, twoToOne_doubleSpeed) {
	clip.tempoRatioNumerator = 2;
	clip.tempoRatioDenominator = 1;
	CHECK_EQUAL(2, clip.scaleGlobalToClipTicks(1));
	CHECK_EQUAL(20, clip.scaleGlobalToClipTicks(10));
}

// ── Negative ticks ───────────────────────────────────────────────────────────

TEST(TempoRatioTest, threeToTwo_negativeTick) {
	clip.tempoRatioNumerator = 3;
	clip.tempoRatioDenominator = 2;
	int32_t result = clip.scaleGlobalToClipTicks(-4);
	CHECK_EQUAL(-6, result);
}

// ── clipTicksToGlobalTicks (ceiling division) ────────────────────────────────

TEST(TempoRatioTest, clipToGlobal_threeToTwo) {
	clip.tempoRatioNumerator = 3;
	clip.tempoRatioDenominator = 2;
	// 3 clip ticks → ceil(3 * 2 / 3) = ceil(2) = 2 global ticks
	CHECK_EQUAL(2, clip.clipTicksToGlobalTicks(3));
}

TEST(TempoRatioTest, clipToGlobal_threeToTwo_nonExact) {
	clip.tempoRatioNumerator = 3;
	clip.tempoRatioDenominator = 2;
	// 1 clip tick → ceil(1 * 2 / 3) = ceil(0.67) = 1
	CHECK_EQUAL(1, clip.clipTicksToGlobalTicks(1));
}

TEST(TempoRatioTest, clipToGlobal_oneToTwo) {
	clip.tempoRatioNumerator = 1;
	clip.tempoRatioDenominator = 2;
	// 5 clip ticks → ceil(5 * 2 / 1) = 10
	CHECK_EQUAL(10, clip.clipTicksToGlobalTicks(5));
}

TEST(TempoRatioTest, clipToGlobal_twoToOne) {
	clip.tempoRatioNumerator = 2;
	clip.tempoRatioDenominator = 1;
	// 10 clip ticks → ceil(10 * 1 / 2) = 5
	CHECK_EQUAL(5, clip.clipTicksToGlobalTicks(10));
}

TEST(TempoRatioTest, clipToGlobal_ceilingRoundsUp) {
	clip.tempoRatioNumerator = 3;
	clip.tempoRatioDenominator = 2;
	// 2 clip ticks → ceil(2 * 2 / 3) = ceil(1.33) = 2
	CHECK_EQUAL(2, clip.clipTicksToGlobalTicks(2));
}

// ── Accumulator reset ────────────────────────────────────────────────────────

TEST(TempoRatioTest, accumulatorResetOnPositionReset) {
	clip.tempoRatioNumerator = 3;
	clip.tempoRatioDenominator = 2;
	clip.scaleGlobalToClipTicks(1); // accum = 1
	CHECK_EQUAL(1, clip.tempoRatioAccumulator);
	clip.tempoRatioAccumulator = 0; // simulates setPos() reset
	// Fresh start after reset
	CHECK_EQUAL(1, clip.scaleGlobalToClipTicks(1));
	CHECK_EQUAL(1, clip.tempoRatioAccumulator);
}

// ── Edge: zero ticks ─────────────────────────────────────────────────────────

TEST(TempoRatioTest, zeroTicksReturnsZero) {
	clip.tempoRatioNumerator = 3;
	clip.tempoRatioDenominator = 2;
	CHECK_EQUAL(0, clip.scaleGlobalToClipTicks(0));
	CHECK_EQUAL(0, clip.clipTicksToGlobalTicks(0));
}
