#include "CppUTest/TestHarness.h"
#include "util/string.h"
#include <string>

TEST_GROUP(StringUtilTest){};

TEST(StringUtilTest, fromIntBasic) {
	std::string result = deluge::string::fromInt(42);
	STRCMP_EQUAL("42", result.c_str());
}

TEST(StringUtilTest, fromIntNegative) {
	std::string result = deluge::string::fromInt(-7);
	STRCMP_EQUAL("-7", result.c_str());
}

TEST(StringUtilTest, fromIntZero) {
	std::string result = deluge::string::fromInt(0);
	STRCMP_EQUAL("0", result.c_str());
}

TEST(StringUtilTest, fromIntLeadingZeros) {
	std::string result = deluge::string::fromInt(5, 3);
	STRCMP_EQUAL("005", result.c_str());
}

TEST(StringUtilTest, fromIntNegativeLeadingZeros) {
	std::string result = deluge::string::fromInt(-5, 3);
	STRCMP_EQUAL("-005", result.c_str());
}

TEST(StringUtilTest, fromFloatBasic) {
	std::string result = deluge::string::fromFloat(3.14f, 2);
	STRCMP_EQUAL("3.14", result.c_str());
}

TEST(StringUtilTest, fromFloatZero) {
	std::string result = deluge::string::fromFloat(0.0f, 1);
	STRCMP_EQUAL("0.0", result.c_str());
}

TEST(StringUtilTest, fromSlotNoSub) {
	std::string result = deluge::string::fromSlot(12, -1, 3);
	STRCMP_EQUAL("012", result.c_str());
}

TEST(StringUtilTest, fromSlotWithSub) {
	// subSlot 2 → 'C' (0='A', 1='B', 2='C')
	std::string result = deluge::string::fromSlot(5, 2, 3);
	STRCMP_EQUAL("005C", result.c_str());
}

TEST(StringUtilTest, fromNoteCodeMiddleC) {
	// Note 60 = C3 (in Deluge convention: octave = noteCode/12 - 2)
	std::string result = deluge::string::fromNoteCode(60);
	// Should start with 'C'
	CHECK(result.find('C') != std::string::npos);
}

TEST(StringUtilTest, fromNoteCodeSharp) {
	// Note 61 = C#3
	std::string result = deluge::string::fromNoteCode(61);
	// Should contain sharp marker (# or .)
	CHECK(result.find('#') != std::string::npos || result.find('.') != std::string::npos);
}
