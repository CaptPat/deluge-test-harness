// Tests for deluge::to_chars() — float-to-string conversion (PR #82).
//
// The bug: the snprintf overflow path was missing `return`, so the
// function would fall through and return a pointer past the buffer
// instead of returning an error. This could cause buffer overflows
// when formatting floats that need more precision than the buffer allows.

#include "CppUTest/TestHarness.h"
#include "util/string.h"

#include <system_error>

TEST_GROUP(ToCharsTest) {};

TEST(ToCharsTest, normalConversion) {
	char buf[32];
	auto result = deluge::to_chars(buf, buf + sizeof(buf), 3.14f, 2);
	CHECK(result.has_value());
	STRCMP_EQUAL("3.14", buf);
}

TEST(ToCharsTest, zeroValue) {
	char buf[32];
	auto result = deluge::to_chars(buf, buf + sizeof(buf), 0.0f, 1);
	CHECK(result.has_value());
	STRCMP_EQUAL("0.0", buf);
}

TEST(ToCharsTest, negativeValue) {
	char buf[32];
	auto result = deluge::to_chars(buf, buf + sizeof(buf), -1.5f, 1);
	CHECK(result.has_value());
	STRCMP_EQUAL("-1.5", buf);
}

TEST(ToCharsTest, highPrecision) {
	char buf[32];
	auto result = deluge::to_chars(buf, buf + sizeof(buf), 1.23456789f, 6);
	CHECK(result.has_value());
	// Result pointer should be past the written characters
	CHECK(*result > buf);
}

TEST(ToCharsTest, emptyBufferReturnsError) {
	char buf[1];
	auto result = deluge::to_chars(buf, buf, 1.0f, 1);
	CHECK(!result.has_value());
	CHECK_EQUAL(static_cast<int>(std::errc::no_buffer_space), static_cast<int>(result.error()));
}

TEST(ToCharsTest, tinyBufferOverflowReturnsError) {
	// Buffer too small for "3.14" (needs 5 chars including null)
	char buf[3];
	auto result = deluge::to_chars(buf, buf + sizeof(buf), 3.14f, 2);
	CHECK(!result.has_value());
	CHECK_EQUAL(static_cast<int>(std::errc::no_buffer_space), static_cast<int>(result.error()));
}

TEST(ToCharsTest, exactFitSucceeds) {
	// "1.0" needs 4 bytes (3 chars + null)
	char buf[4];
	auto result = deluge::to_chars(buf, buf + sizeof(buf), 1.0f, 1);
	CHECK(result.has_value());
	STRCMP_EQUAL("1.0", buf);
}

TEST(ToCharsTest, returnedPointerIsCorrect) {
	char buf[32];
	auto result = deluge::to_chars(buf, buf + sizeof(buf), 42.5f, 1);
	CHECK(result.has_value());
	// "42.5" is 4 characters, pointer should be at buf+4
	CHECK_EQUAL(buf + 4, *result);
}
