#include "CppUTest/TestHarness.h"
#include "util/d_string.h"

extern const char nothing;

TEST_GROUP(StringTest) {};

// --- Default state ---

TEST(StringTest, defaultConstructorEmpty) {
	String s;
	CHECK(s.isEmpty());
	STRCMP_EQUAL("", s.get());
	CHECK_EQUAL(&nothing, s.get());
}

// --- set(const char*) ---

TEST(StringTest, setFromCString) {
	String s;
	Error err = s.set("hello");
	CHECK_EQUAL((int)Error::NONE, (int)err);
	STRCMP_EQUAL("hello", s.get());
	CHECK_EQUAL(5, (int)s.getLength());
	CHECK(!s.isEmpty());
}

TEST(StringTest, setFromCStringWithLength) {
	String s;
	Error err = s.set("hello world", 5);
	CHECK_EQUAL((int)Error::NONE, (int)err);
	STRCMP_EQUAL("hello", s.get());
	CHECK_EQUAL(5, (int)s.getLength());
}

TEST(StringTest, setEmptyStringClears) {
	String s;
	s.set("hi");
	Error err = s.set("", 0);
	CHECK_EQUAL((int)Error::NONE, (int)err);
	CHECK(s.isEmpty());
}

TEST(StringTest, setOverwritesPrevious) {
	String s;
	s.set("first");
	s.set("second");
	STRCMP_EQUAL("second", s.get());
}

// --- clear ---

TEST(StringTest, clearReleasesMemory) {
	String s;
	s.set("hello");
	s.clear();
	CHECK(s.isEmpty());
	STRCMP_EQUAL("", s.get());
}

// --- getLength ---

TEST(StringTest, getLengthEmpty) {
	String s;
	CHECK_EQUAL(0, (int)s.getLength());
}

TEST(StringTest, getLengthNonEmpty) {
	String s;
	s.set("abc");
	CHECK_EQUAL(3, (int)s.getLength());
}

// --- equals ---

TEST(StringTest, equalsTrue) {
	String s;
	s.set("abc");
	CHECK(s.equals("abc"));
}

TEST(StringTest, equalsFalse) {
	String s;
	s.set("abc");
	CHECK(!s.equals("xyz"));
}

TEST(StringTest, equalsCaseIrrespective) {
	String s;
	s.set("Hello");
	CHECK(s.equalsCaseIrrespective("hello"));
	CHECK(s.equalsCaseIrrespective("HELLO"));
}

// --- contains ---

TEST(StringTest, contains) {
	String s;
	s.set("hello world");
	CHECK(s.contains("world"));
	CHECK(s.contains("hello"));
	CHECK(!s.contains("xyz"));
}

// --- concatenate ---

TEST(StringTest, concatenateCString) {
	String s;
	s.set("hello");
	Error err = s.concatenate(" world");
	CHECK_EQUAL((int)Error::NONE, (int)err);
	STRCMP_EQUAL("hello world", s.get());
}

TEST(StringTest, concatenateToEmpty) {
	String s;
	String other;
	other.set("hello");
	Error err = s.concatenate(&other);
	CHECK_EQUAL((int)Error::NONE, (int)err);
	STRCMP_EQUAL("hello", s.get());
}

TEST(StringTest, concatenateAtPos) {
	String s;
	s.set("abcdef");
	Error err = s.concatenateAtPos("XY", 3);
	CHECK_EQUAL((int)Error::NONE, (int)err);
	STRCMP_EQUAL("abcXY", s.get());
}

// --- concatenateInt / setInt ---

TEST(StringTest, concatenateInt) {
	String s;
	s.set("val:");
	Error err = s.concatenateInt(42);
	CHECK_EQUAL((int)Error::NONE, (int)err);
	STRCMP_EQUAL("val:42", s.get());
}

TEST(StringTest, setInt) {
	String s;
	Error err = s.setInt(123);
	CHECK_EQUAL((int)Error::NONE, (int)err);
	STRCMP_EQUAL("123", s.get());
}

TEST(StringTest, setIntMinDigits) {
	String s;
	Error err = s.setInt(42, 5);
	CHECK_EQUAL((int)Error::NONE, (int)err);
	STRCMP_EQUAL("00042", s.get());
}

TEST(StringTest, setIntNegative) {
	String s;
	s.setInt(-7);
	STRCMP_EQUAL("-7", s.get());
}

// --- setChar ---

TEST(StringTest, setChar) {
	String s;
	s.set("abc");
	Error err = s.setChar('X', 1);
	CHECK_EQUAL((int)Error::NONE, (int)err);
	STRCMP_EQUAL("aXc", s.get());
}

// --- shorten ---

TEST(StringTest, shorten) {
	String s;
	s.set("hello");
	Error err = s.shorten(3);
	CHECK_EQUAL((int)Error::NONE, (int)err);
	STRCMP_EQUAL("hel", s.get());
	CHECK_EQUAL(3, (int)s.getLength());
}

TEST(StringTest, shortenToZero) {
	String s;
	s.set("hi");
	s.shorten(0);
	CHECK(s.isEmpty());
}

// --- Copy semantics (reference counting) ---

TEST(StringTest, copyConstructor) {
	String a;
	a.set("shared");
	String b(a);
	STRCMP_EQUAL("shared", b.get());
	STRCMP_EQUAL("shared", a.get());
	// They share the same memory (same pointer)
	CHECK_EQUAL(a.get(), b.get());
}

TEST(StringTest, assignmentOperator) {
	String a;
	a.set("data");
	String b;
	b = a;
	STRCMP_EQUAL("data", b.get());
	CHECK_EQUAL(a.get(), b.get());
}

TEST(StringTest, copyThenModifyOriginal) {
	String a;
	a.set("original");
	String b(a);
	// Modifying a should clone memory (copy-on-write via set)
	a.set("modified");
	STRCMP_EQUAL("modified", a.get());
	STRCMP_EQUAL("original", b.get());
}

TEST(StringTest, equalsStringObject) {
	String a;
	a.set("same");
	String b;
	b.set("same");
	CHECK(a.equals(&b));
}

TEST(StringTest, equalsStringObjectSameMemory) {
	String a;
	a.set("test");
	String b(a);
	// Same underlying memory
	CHECK(a.equals(&b));
}
