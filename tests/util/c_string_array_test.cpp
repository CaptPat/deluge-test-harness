#include "CppUTest/TestHarness.h"
#include "util/container/array/c_string_array.h"
#include "util/functions.h"
#include <cstring>

TEST_GROUP(CStringArrayTest) {
	// Element size = sizeof(char*) since CStringArray stores pointers to strings
	CStringArray arr{static_cast<int32_t>(sizeof(char*))};

	void setup() override {
		shouldInterpretNoteNames = false;
		octaveStartsFromA = false;
	}

	void teardown() override {
		// Let CStringArray destructor handle cleanup
	}

	// Helper: insert a string pointer at index
	void insertString(const char* str) {
		int32_t idx = arr.getNumElements();
		Error err = arr.insertAtIndex(idx);
		CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
		char const** slot = (char const**)arr.getElementAddress(idx);
		*slot = str;
	}
};

TEST(CStringArrayTest, constructEmpty) {
	CHECK_EQUAL(0, arr.getNumElements());
}

TEST(CStringArrayTest, searchEmptyReturnsZero) {
	bool found = false;
	int32_t idx = arr.search("anything", &found);
	CHECK_EQUAL(0, idx);
	CHECK_FALSE(found);
}

TEST(CStringArrayTest, insertAndSearch) {
	insertString("hello");
	bool found = false;
	int32_t idx = arr.search("hello", &found);
	CHECK_EQUAL(0, idx);
	CHECK(found);
}

TEST(CStringArrayTest, searchNotFound) {
	insertString("alpha");
	bool found = false;
	arr.search("beta", &found);
	CHECK_FALSE(found);
}

TEST(CStringArrayTest, sortAlphabetical) {
	insertString("cherry");
	insertString("apple");
	insertString("banana");
	arr.sortForStrings();
	STRCMP_EQUAL("apple", *(char const**)arr.getElementAddress(0));
	STRCMP_EQUAL("banana", *(char const**)arr.getElementAddress(1));
	STRCMP_EQUAL("cherry", *(char const**)arr.getElementAddress(2));
}

TEST(CStringArrayTest, sortNumeric) {
	insertString("file10");
	insertString("file2");
	insertString("file1");
	arr.sortForStrings();
	STRCMP_EQUAL("file1", *(char const**)arr.getElementAddress(0));
	STRCMP_EQUAL("file2", *(char const**)arr.getElementAddress(1));
	STRCMP_EQUAL("file10", *(char const**)arr.getElementAddress(2));
}

// ── strcmpspecial direct tests ───────────────────────────────────────────

TEST_GROUP(StrcmpSpecialTest){};

TEST(StrcmpSpecialTest, equalStrings) {
	CHECK_EQUAL(0, strcmpspecial("abc", "abc"));
}

TEST(StrcmpSpecialTest, lessThan) {
	CHECK(strcmpspecial("abc", "abd") < 0);
}

TEST(StrcmpSpecialTest, greaterThan) {
	CHECK(strcmpspecial("abd", "abc") > 0);
}

TEST(StrcmpSpecialTest, numericOrdering) {
	// Natural ordering: "file2" < "file10"
	CHECK(strcmpspecial("file2", "file10") < 0);
}

TEST(StrcmpSpecialTest, caseInsensitive) {
	CHECK_EQUAL(0, strcmpspecial("ABC", "abc"));
}

TEST(StrcmpSpecialTest, emptyStrings) {
	CHECK_EQUAL(0, strcmpspecial("", ""));
}

TEST(StrcmpSpecialTest, emptyVsNonEmpty) {
	CHECK(strcmpspecial("", "a") < 0);
	CHECK(strcmpspecial("a", "") > 0);
}

TEST(StrcmpSpecialTest, dotBeforeUnderscore) {
	CHECK(strcmpspecial(".file", "_file") < 0);
}
