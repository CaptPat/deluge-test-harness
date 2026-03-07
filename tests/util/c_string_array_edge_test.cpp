// Extended tests for strcmpspecial natural sort and CStringArray search/sort edge cases.

#include "CppUTest/TestHarness.h"
#include "util/container/array/c_string_array.h"
#include "util/functions.h"
#include <cstring>

// ── strcmpspecial edge cases ────────────────────────────────────────────────

TEST_GROUP(StrcmpSpecialEdgeCases) {
	void setup() {
		shouldInterpretNoteNames = false;
		octaveStartsFromA = false;
	}
};

TEST(StrcmpSpecialEdgeCases, leadingZerosTiebreak) {
	// "01" and "1" have the same numeric value, but the leading-zero
	// tiebreaker stores '0'-'1' = -1, so "01" < "1"
	CHECK(strcmpspecial("01", "1") < 0);
}

TEST(StrcmpSpecialEdgeCases, leadingZerosTiebreaker) {
	// "file01" vs "file1" — same numeric value, but different leading zeros
	// The resultIfGetToEndOfBothStrings tiebreaker kicks in
	int result = strcmpspecial("file01", "file1");
	// Leading zero gives a tiebreaker; '0' < '1' in ASCII so file01 < file1
	CHECK(result < 0);
}

TEST(StrcmpSpecialEdgeCases, multiDigitNaturalSort) {
	CHECK(strcmpspecial("track1", "track2") < 0);
	CHECK(strcmpspecial("track9", "track10") < 0);
	CHECK(strcmpspecial("track99", "track100") < 0);
	CHECK(strcmpspecial("track1000", "track999") > 0);
}

TEST(StrcmpSpecialEdgeCases, underscoreBeforeLetters) {
	CHECK(strcmpspecial("_init", "ainit") < 0);
}

TEST(StrcmpSpecialEdgeCases, dotBeforeUnderscore) {
	CHECK(strcmpspecial(".hidden", "_hidden") < 0);
}

TEST(StrcmpSpecialEdgeCases, dotBeforeLetters) {
	CHECK(strcmpspecial(".file", "afile") < 0);
}

TEST(StrcmpSpecialEdgeCases, numbersBeforeLetters) {
	// '0' (48) < 'a' (97) in ASCII, so numbers come before letters
	CHECK(strcmpspecial("1abc", "abc") < 0);
}

TEST(StrcmpSpecialEdgeCases, caseInsensitiveTiebreaker) {
	// "ABC" and "abc" compare equal due to case folding
	CHECK_EQUAL(0, strcmpspecial("Hello", "hello"));
	CHECK_EQUAL(0, strcmpspecial("WORLD", "world"));
}

TEST(StrcmpSpecialEdgeCases, mixedCaseAndNumbers) {
	CHECK(strcmpspecial("File2", "file10") < 0);
	CHECK(strcmpspecial("FILE10", "file2") > 0);
}

TEST(StrcmpSpecialEdgeCases, prefixShorterComesFirst) {
	CHECK(strcmpspecial("abc", "abcd") < 0);
	CHECK(strcmpspecial("abcd", "abc") > 0);
}

TEST(StrcmpSpecialEdgeCases, identicalStrings) {
	CHECK_EQUAL(0, strcmpspecial("exact_match", "exact_match"));
}

TEST(StrcmpSpecialEdgeCases, singleCharComparison) {
	CHECK(strcmpspecial("a", "b") < 0);
	CHECK(strcmpspecial("z", "a") > 0);
}

// Note: shouldInterpretNoteNames is not implemented in the mock strcmpspecial,
// so note-name-ordering tests are omitted here. The mock correctly handles all
// non-note comparisons (numeric natural sort, case folding, dot/underscore priority).

// ── CStringArray search after sort ──────────────────────────────────────────

TEST_GROUP(CStringArraySearchSort) {
	CStringArray arr{static_cast<int32_t>(sizeof(char*))};

	void setup() {
		shouldInterpretNoteNames = false;
		octaveStartsFromA = false;
	}

	void insertString(const char* str) {
		int32_t idx = arr.getNumElements();
		Error err = arr.insertAtIndex(idx);
		CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
		char const** slot = (char const**)arr.getElementAddress(idx);
		*slot = str;
	}
};

TEST(CStringArraySearchSort, searchAfterSortFindsAll) {
	const char* names[] = {"zebra", "apple", "mango", "banana", "fig"};
	for (auto* n : names) {
		insertString(n);
	}
	arr.sortForStrings();

	for (auto* n : names) {
		bool found = false;
		arr.search(n, &found);
		CHECK(found);
	}
}

TEST(CStringArraySearchSort, searchReturnsInsertionPoint) {
	insertString("alpha");
	insertString("gamma");
	arr.sortForStrings();

	bool found = false;
	int32_t idx = arr.search("beta", &found);
	CHECK_FALSE(found);
	// "beta" would be inserted between "alpha" (0) and "gamma" (1)
	CHECK_EQUAL(1, idx);
}

TEST(CStringArraySearchSort, sortWithDuplicates) {
	insertString("dup");
	insertString("dup");
	insertString("dup");
	arr.sortForStrings();

	CHECK_EQUAL(3, arr.getNumElements());
	bool found = false;
	arr.search("dup", &found);
	CHECK(found);
}

TEST(CStringArraySearchSort, sortManyElements) {
	const char* items[] = {
	    "patch100", "patch2", "patch10", "patch1", "patch20", "patch3",
	    "patch30",  "patch4", "patch5",  "patch6", "patch7",  "patch8",
	};
	for (auto* s : items) {
		insertString(s);
	}
	arr.sortForStrings();

	// Verify natural numeric order
	STRCMP_EQUAL("patch1", *(char const**)arr.getElementAddress(0));
	STRCMP_EQUAL("patch2", *(char const**)arr.getElementAddress(1));
	STRCMP_EQUAL("patch3", *(char const**)arr.getElementAddress(2));
	STRCMP_EQUAL("patch100", *(char const**)arr.getElementAddress(arr.getNumElements() - 1));
}

TEST(CStringArraySearchSort, searchSingleElement) {
	insertString("only");
	arr.sortForStrings();

	bool found = false;
	int32_t idx = arr.search("only", &found);
	CHECK(found);
	CHECK_EQUAL(0, idx);
}

TEST(CStringArraySearchSort, searchBeforeFirstElement) {
	insertString("middle");
	arr.sortForStrings();

	bool found = false;
	int32_t idx = arr.search("aaa", &found);
	CHECK_FALSE(found);
	CHECK_EQUAL(0, idx); // insertion point = before "middle"
}

TEST(CStringArraySearchSort, searchAfterLastElement) {
	insertString("middle");
	arr.sortForStrings();

	bool found = false;
	int32_t idx = arr.search("zzz", &found);
	CHECK_FALSE(found);
	CHECK_EQUAL(1, idx); // insertion point = after "middle"
}
