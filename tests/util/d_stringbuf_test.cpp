#include "CppUTest/TestHarness.h"
#include "util/d_stringbuf.h"

#include <cstring>

TEST_GROUP(StringBufTest) {
	char buffer[64];
	StringBuf* buf;
	void setup() override {
		memset(buffer, 0, sizeof(buffer));
		buf = new StringBuf(buffer, sizeof(buffer));
	}
	void teardown() override { delete buf; }
};

// --- Construction ---

TEST(StringBufTest, constructorZeroed) {
	CHECK(buf->empty());
	CHECK_EQUAL(0, (int)buf->size());
	CHECK_EQUAL(64, (int)buf->capacity());
}

// --- Append ---

TEST(StringBufTest, appendStringView) {
	buf->append("hello");
	STRCMP_EQUAL("hello", buf->c_str());
	CHECK_EQUAL(5, (int)buf->size());
}

TEST(StringBufTest, appendMultiple) {
	buf->append("ab");
	buf->append("cd");
	STRCMP_EQUAL("abcd", buf->c_str());
	CHECK_EQUAL(4, (int)buf->size());
}

TEST(StringBufTest, appendChar) {
	buf->append('X');
	STRCMP_EQUAL("X", buf->c_str());
	CHECK_EQUAL(1, (int)buf->size());
}

TEST(StringBufTest, appendOverflow) {
	// Capacity is 64, so max string length is 63 (null terminator)
	char longStr[80];
	memset(longStr, 'A', 79);
	longStr[79] = '\0';
	buf->append(longStr);
	// Should be truncated, not overflow
	CHECK(buf->size() <= 63);
}

// --- Clear / Truncate ---

TEST(StringBufTest, clear) {
	buf->append("hello");
	buf->clear();
	CHECK(buf->empty());
	CHECK_EQUAL(0, (int)buf->size());
}

TEST(StringBufTest, truncate) {
	buf->append("hello");
	buf->truncate(3);
	STRCMP_EQUAL("hel", buf->c_str());
	CHECK_EQUAL(3, (int)buf->size());
}

// --- Equality ---

TEST(StringBufTest, equalityCString) {
	buf->append("abc");
	CHECK(*buf == "abc");
}

TEST(StringBufTest, equalityStringBuf) {
	char buffer2[64] = {};
	StringBuf other(buffer2, sizeof(buffer2));
	buf->append("test");
	other.append("test");
	CHECK(*buf == other);
}

// --- Data accessors ---

TEST(StringBufTest, dataReturnsPointer) {
	buf->append("hi");
	STRCMP_EQUAL("hi", buf->data());
	CHECK_EQUAL(buffer, buf->data());
}

TEST(StringBufTest, lengthEqualsSize) {
	buf->append("abc");
	CHECK_EQUAL(buf->size(), buf->length());
}

// --- appendInt ---

TEST(StringBufTest, appendInt) {
	buf->appendInt(42);
	STRCMP_EQUAL("42", buf->c_str());
}

TEST(StringBufTest, appendIntMinChars) {
	buf->appendInt(7, 3);
	STRCMP_EQUAL("007", buf->c_str());
}

TEST(StringBufTest, appendIntAfterText) {
	buf->append("val:");
	buf->appendInt(99);
	STRCMP_EQUAL("val:99", buf->c_str());
}

// --- DEF_STACK_STRING_BUF macro ---

TEST(StringBufTest, stackMacro) {
	DEF_STACK_STRING_BUF(myBuf, 32);
	myBuf.append("macro");
	STRCMP_EQUAL("macro", myBuf.c_str());
	CHECK_EQUAL(32, (int)myBuf.capacity());
}

// --- Hex functions (free functions from d_stringbuf.cpp) ---

TEST(StringBufTest, halfByteToHexCharDigits) {
	CHECK_EQUAL('0', halfByteToHexChar(0));
	CHECK_EQUAL('5', halfByteToHexChar(5));
	CHECK_EQUAL('9', halfByteToHexChar(9));
}

TEST(StringBufTest, halfByteToHexCharLetters) {
	CHECK_EQUAL('A', halfByteToHexChar(10));
	CHECK_EQUAL('F', halfByteToHexChar(15));
}

TEST(StringBufTest, intToHexSmall) {
	char out[16] = {};
	intToHex(0xFF, out, 2);
	STRCMP_EQUAL("FF", out);
}

TEST(StringBufTest, intToHexLarge) {
	char out[16] = {};
	intToHex(0xDEADBEEF, out, 8);
	STRCMP_EQUAL("DEADBEEF", out);
}

TEST(StringBufTest, hexToIntSmall) {
	CHECK_EQUAL(255u, hexToInt("FF"));
}

TEST(StringBufTest, hexToIntLarge) {
	CHECK_EQUAL(0xDEADBEEFu, hexToInt("DEADBEEF"));
}

TEST(StringBufTest, hexToIntFixedLength) {
	CHECK_EQUAL(255u, hexToIntFixedLength("00FF", 4));
}

TEST(StringBufTest, hexRoundTrip) {
	char out[16] = {};
	intToHex(0x1234ABCD, out, 8);
	CHECK_EQUAL(0x1234ABCDu, hexToInt(out));
}
