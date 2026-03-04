#include "CppUTest/TestHarness.h"
#include "hid/encoder.h"

using deluge::hid::encoders::Encoder;

TEST_GROUP(EncoderTest) {
	Encoder enc;
};

TEST(EncoderTest, defaultState) {
	CHECK_EQUAL(0, enc.encPos);
	CHECK_EQUAL(0, enc.detentPos);
}

TEST(EncoderTest, getLimitedDetentPosAndResetDefaultsPositive) {
	// When detentPos >= 0, returns 1; when < 0, returns -1
	int32_t result = enc.getLimitedDetentPosAndReset();
	CHECK_EQUAL(1, result);
	CHECK_EQUAL(0, enc.detentPos); // resets to 0
}

TEST(EncoderTest, getLimitedDetentPosAndResetPositive) {
	enc.detentPos = 3;
	int32_t result = enc.getLimitedDetentPosAndReset();
	CHECK(result > 0);
	// After reset, detentPos should be adjusted
}

TEST(EncoderTest, getLimitedDetentPosAndResetNegative) {
	enc.detentPos = -2;
	int32_t result = enc.getLimitedDetentPosAndReset();
	CHECK(result < 0);
}

TEST(EncoderTest, setNonDetentMode) {
	// Should not crash
	enc.setNonDetentMode();
	enc.detentPos = 5;
	int32_t result = enc.getLimitedDetentPosAndReset();
	// In non-detent mode behavior may differ
	CHECK(result != 0);
}

TEST(EncoderTest, setPins) {
	// Should not crash — pins are stored for read()
	enc.setPins(1, 2, 3, 4);
	CHECK_EQUAL(0, enc.encPos); // Unchanged
}
