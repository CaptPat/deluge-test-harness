// TimeSignature struct tests: bar/beat tick calculations, defaults, equality.

#include "CppUTest/TestHarness.h"

#if __has_include("model/time_signature.h")
#include "model/time_signature.h"
#define HAS_TIME_SIGNATURE 1
#endif

#ifdef HAS_TIME_SIGNATURE
TEST_GROUP(TimeSignatureTest) {};

// ── barLengthInBaseTicks ────────────────────────────────────────────────────

TEST(TimeSignatureTest, fourFourBarLength) {
	TimeSignature ts{4, 4};
	CHECK_EQUAL(96, ts.barLengthInBaseTicks());
}

TEST(TimeSignatureTest, threeFourBarLength) {
	TimeSignature ts{3, 4};
	CHECK_EQUAL(72, ts.barLengthInBaseTicks());
}

TEST(TimeSignatureTest, sevenEighthBarLength) {
	// 7 * (96/8) = 7 * 12 = 84
	TimeSignature ts{7, 8};
	CHECK_EQUAL(84, ts.barLengthInBaseTicks());
}

TEST(TimeSignatureTest, sixEighthBarLength) {
	TimeSignature ts{6, 8};
	CHECK_EQUAL(72, ts.barLengthInBaseTicks());
}

TEST(TimeSignatureTest, fiveFourBarLength) {
	TimeSignature ts{5, 4};
	CHECK_EQUAL(120, ts.barLengthInBaseTicks());
}

TEST(TimeSignatureTest, twoFourBarLength) {
	TimeSignature ts{2, 4};
	CHECK_EQUAL(48, ts.barLengthInBaseTicks());
}

TEST(TimeSignatureTest, threeSixteenthBarLength) {
	// 3 * (96/16) = 3 * 6 = 18
	TimeSignature ts{3, 16};
	CHECK_EQUAL(18, ts.barLengthInBaseTicks());
}

TEST(TimeSignatureTest, twoTwoBarLength) {
	// 2 * (96/2) = 2 * 48 = 96
	TimeSignature ts{2, 2};
	CHECK_EQUAL(96, ts.barLengthInBaseTicks());
}

// ── beatLengthInBaseTicks ───────────────────────────────────────────────────

TEST(TimeSignatureTest, quarterNoteBeatLength) {
	TimeSignature ts{4, 4};
	CHECK_EQUAL(24, ts.beatLengthInBaseTicks());
}

TEST(TimeSignatureTest, eighthNoteBeatLength) {
	TimeSignature ts{6, 8};
	CHECK_EQUAL(12, ts.beatLengthInBaseTicks());
}

TEST(TimeSignatureTest, halfNoteBeatLength) {
	TimeSignature ts{2, 2};
	CHECK_EQUAL(48, ts.beatLengthInBaseTicks());
}

TEST(TimeSignatureTest, sixteenthNoteBeatLength) {
	TimeSignature ts{4, 16};
	CHECK_EQUAL(6, ts.beatLengthInBaseTicks());
}

// ── isDefault ───────────────────────────────────────────────────────────────

TEST(TimeSignatureTest, fourFourIsDefault) {
	TimeSignature ts{4, 4};
	CHECK_TRUE(ts.isDefault());
}

TEST(TimeSignatureTest, defaultConstructorIsDefault) {
	TimeSignature ts{};
	CHECK_TRUE(ts.isDefault());
}

TEST(TimeSignatureTest, threeFourIsNotDefault) {
	TimeSignature ts{3, 4};
	CHECK_FALSE(ts.isDefault());
}

TEST(TimeSignatureTest, fourEightIsNotDefault) {
	TimeSignature ts{4, 8};
	CHECK_FALSE(ts.isDefault());
}

// ── equality ────────────────────────────────────────────────────────────────

TEST(TimeSignatureTest, sameValuesAreEqual) {
	TimeSignature a{3, 4};
	TimeSignature b{3, 4};
	CHECK_TRUE(a == b);
}

TEST(TimeSignatureTest, differentNumeratorsNotEqual) {
	TimeSignature a{3, 4};
	TimeSignature b{4, 4};
	CHECK_FALSE(a == b);
}

TEST(TimeSignatureTest, differentDenominatorsNotEqual) {
	TimeSignature a{4, 4};
	TimeSignature b{4, 8};
	CHECK_FALSE(a == b);
}

TEST(TimeSignatureTest, defaultsAreEqual) {
	TimeSignature a{};
	TimeSignature b{};
	CHECK_TRUE(a == b);
}

#endif // HAS_TIME_SIGNATURE
