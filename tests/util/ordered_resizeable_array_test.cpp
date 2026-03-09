#include "CppUTest/TestHarness.h"
#include "util/container/array/ordered_resizeable_array.h"
#include <cstring>

// OrderedResizeableArrayWith32bitKey shadows the pointer-cast methods
// with safe *(int32_t*)address, so these tests are safe on x86-64.

TEST_GROUP(OrderedArrayTest) {
	// Element size = sizeof(int32_t) (the key itself is the element)
	OrderedResizeableArrayWith32bitKey arr{static_cast<int32_t>(sizeof(int32_t))};

	void teardown() override {
		arr.empty();
	}

	// Helper: insert N sequential keys starting at 'start'
	void insertKeys(int32_t start, int32_t count) {
		for (int32_t i = 0; i < count; i++) {
			int32_t idx = arr.insertAtKey(start + i);
			CHECK(idx >= 0);
		}
	}

	// Helper: verify the array contains exactly the expected sorted keys
	void verifyKeys(const int32_t* expected, int32_t count) {
		LONGS_EQUAL(count, arr.getNumElements());
		for (int32_t i = 0; i < count; i++) {
			LONGS_EQUAL(expected[i], arr.getKeyAtIndex(i));
		}
	}
};

// ── Construction ────────────────────────────────────────────────────────

TEST(OrderedArrayTest, constructEmpty) {
	LONGS_EQUAL(0, arr.getNumElements());
}

// ── search() ────────────────────────────────────────────────────────────

TEST(OrderedArrayTest, searchEmptyArrayReturnsZero) {
	LONGS_EQUAL(0, arr.search(42, GREATER_OR_EQUAL));
	LONGS_EQUAL(-1, arr.search(42, LESS));
}

TEST(OrderedArrayTest, searchGreaterOrEqualFindsExact) {
	insertKeys(10, 5); // 10,11,12,13,14
	LONGS_EQUAL(0, arr.search(10, GREATER_OR_EQUAL));
	LONGS_EQUAL(2, arr.search(12, GREATER_OR_EQUAL));
	LONGS_EQUAL(4, arr.search(14, GREATER_OR_EQUAL));
}

TEST(OrderedArrayTest, searchGreaterOrEqualFindsBetween) {
	insertKeys(10, 3); // 10,12 — actually let's insert specific keys
	arr.empty();
	int32_t idx;
	idx = arr.insertAtKey(10); CHECK(idx >= 0);
	idx = arr.insertAtKey(20); CHECK(idx >= 0);
	idx = arr.insertAtKey(30); CHECK(idx >= 0);

	// 15 should return index of 20 (index 1)
	LONGS_EQUAL(1, arr.search(15, GREATER_OR_EQUAL));
	// 25 should return index of 30 (index 2)
	LONGS_EQUAL(2, arr.search(25, GREATER_OR_EQUAL));
}

TEST(OrderedArrayTest, searchGreaterOrEqualBeyondEnd) {
	insertKeys(10, 3); // 10,11,12
	// Searching for key > all elements returns numElements
	LONGS_EQUAL(3, arr.search(100, GREATER_OR_EQUAL));
}

TEST(OrderedArrayTest, searchLessReturnsOneBelow) {
	insertKeys(10, 5); // 10,11,12,13,14
	// LESS = GREATER_OR_EQUAL - 1
	LONGS_EQUAL(-1, arr.search(10, LESS)); // nothing less than first
	LONGS_EQUAL(1, arr.search(12, LESS));  // index 1 (value 11) is less
	LONGS_EQUAL(4, arr.search(15, LESS));  // all are less, returns last index
}

TEST(OrderedArrayTest, searchBeforeFirst) {
	insertKeys(10, 3);
	LONGS_EQUAL(0, arr.search(5, GREATER_OR_EQUAL));
	LONGS_EQUAL(-1, arr.search(5, LESS));
}

TEST(OrderedArrayTest, searchWithRange) {
	insertKeys(0, 10); // 0..9
	// Search only in range [3, 7)
	LONGS_EQUAL(5, arr.search(5, GREATER_OR_EQUAL, 3, 7));
	LONGS_EQUAL(7, arr.search(100, GREATER_OR_EQUAL, 3, 7));
	LONGS_EQUAL(3, arr.search(0, GREATER_OR_EQUAL, 3, 7));
}

// ── searchExact() ───────────────────────────────────────────────────────

TEST(OrderedArrayTest, searchExactFindsKey) {
	insertKeys(10, 5);
	LONGS_EQUAL(0, arr.searchExact(10));
	LONGS_EQUAL(2, arr.searchExact(12));
	LONGS_EQUAL(4, arr.searchExact(14));
}

TEST(OrderedArrayTest, searchExactMissReturnsNegOne) {
	insertKeys(10, 5);
	LONGS_EQUAL(-1, arr.searchExact(9));
	LONGS_EQUAL(-1, arr.searchExact(15));
	LONGS_EQUAL(-1, arr.searchExact(11000));
}

TEST(OrderedArrayTest, searchExactEmpty) {
	LONGS_EQUAL(-1, arr.searchExact(42));
}

// ── insertAtKey() ───────────────────────────────────────────────────────

TEST(OrderedArrayTest, insertMaintainsOrder) {
	arr.insertAtKey(30);
	arr.insertAtKey(10);
	arr.insertAtKey(20);
	int32_t expected[] = {10, 20, 30};
	verifyKeys(expected, 3);
}

TEST(OrderedArrayTest, insertAtKeyDefinitelyLast) {
	arr.insertAtKey(10);
	arr.insertAtKey(20);
	// isDefinitelyLast=true skips binary search
	int32_t idx = arr.insertAtKey(30, true);
	LONGS_EQUAL(2, idx);
	LONGS_EQUAL(30, arr.getKeyAtIndex(2));
}

TEST(OrderedArrayTest, insertDuplicateKeys) {
	arr.insertAtKey(10);
	arr.insertAtKey(10);
	arr.insertAtKey(10);
	LONGS_EQUAL(3, arr.getNumElements());
	// All should be 10
	for (int32_t i = 0; i < 3; i++) {
		LONGS_EQUAL(10, arr.getKeyAtIndex(i));
	}
}

TEST(OrderedArrayTest, insertNegativeKeys) {
	arr.insertAtKey(-5);
	arr.insertAtKey(5);
	arr.insertAtKey(-10);
	arr.insertAtKey(0);
	int32_t expected[] = {-10, -5, 0, 5};
	verifyKeys(expected, 4);
}

TEST(OrderedArrayTest, insertManyMaintainsOrder) {
	// Insert 100 keys in reverse order
	for (int32_t i = 99; i >= 0; i--) {
		arr.insertAtKey(i * 7); // spacing to avoid adjacency
	}
	LONGS_EQUAL(100, arr.getNumElements());
	for (int32_t i = 0; i < 100; i++) {
		LONGS_EQUAL(i * 7, arr.getKeyAtIndex(i));
	}
}

// ── deleteAtKey() ───────────────────────────────────────────────────────

TEST(OrderedArrayTest, deleteAtKeyRemovesElement) {
	insertKeys(10, 5); // 10,11,12,13,14
	arr.deleteAtKey(12);
	LONGS_EQUAL(4, arr.getNumElements());
	int32_t expected[] = {10, 11, 13, 14};
	verifyKeys(expected, 4);
}

TEST(OrderedArrayTest, deleteAtKeyNonExistentIsNoop) {
	insertKeys(10, 3);
	arr.deleteAtKey(99);
	LONGS_EQUAL(3, arr.getNumElements());
}

TEST(OrderedArrayTest, deleteFirstAndLast) {
	insertKeys(10, 5); // 10,11,12,13,14
	arr.deleteAtKey(10);
	arr.deleteAtKey(14);
	int32_t expected[] = {11, 12, 13};
	verifyKeys(expected, 3);
}

TEST(OrderedArrayTest, deleteAllElements) {
	insertKeys(10, 3);
	arr.deleteAtKey(10);
	arr.deleteAtKey(11);
	arr.deleteAtKey(12);
	LONGS_EQUAL(0, arr.getNumElements());
}

// ── searchDual() ────────────────────────────────────────────────────────

TEST(OrderedArrayTest, searchDualBothFound) {
	insertKeys(0, 10); // 0..9
	int32_t terms[2] = {3, 7};
	int32_t results[2];
	arr.searchDual(terms, results);
	LONGS_EQUAL(3, results[0]);
	LONGS_EQUAL(7, results[1]);
}

TEST(OrderedArrayTest, searchDualBetweenKeys) {
	// Insert even numbers: 0,2,4,6,8
	for (int32_t i = 0; i < 5; i++) {
		arr.insertAtKey(i * 2);
	}
	int32_t terms[2] = {3, 7};
	int32_t results[2];
	arr.searchDual(terms, results);
	// 3 -> index of 4 (index 2), 7 -> index of 8 (index 4)
	LONGS_EQUAL(2, results[0]);
	LONGS_EQUAL(4, results[1]);
}

TEST(OrderedArrayTest, searchDualSameKey) {
	insertKeys(0, 10);
	int32_t terms[2] = {5, 5};
	int32_t results[2];
	arr.searchDual(terms, results);
	LONGS_EQUAL(5, results[0]);
	LONGS_EQUAL(5, results[1]);
}

TEST(OrderedArrayTest, searchDualBeyondEnd) {
	insertKeys(0, 5);
	int32_t terms[2] = {3, 100};
	int32_t results[2];
	arr.searchDual(terms, results);
	LONGS_EQUAL(3, results[0]);
	LONGS_EQUAL(5, results[1]); // past end
}

// ── generateRepeats() ──────────────────────────────────────────────────

TEST(OrderedArrayTest, generateRepeatsDoublesPattern) {
	// Pattern: keys at 0, 10, 20 with wrapPoint=30, endPos=60 (2 repeats)
	arr.insertAtKey(0);
	arr.insertAtKey(10);
	arr.insertAtKey(20);

	bool ok = arr.generateRepeats(30, 60);
	CHECK(ok);

	LONGS_EQUAL(6, arr.getNumElements());
	int32_t expected[] = {0, 10, 20, 30, 40, 50};
	verifyKeys(expected, 6);
}

TEST(OrderedArrayTest, generateRepeatsPartial) {
	// Pattern: 0, 10, 20 with wrapPoint=30, endPos=45 (1 full + partial)
	arr.insertAtKey(0);
	arr.insertAtKey(10);
	arr.insertAtKey(20);

	bool ok = arr.generateRepeats(30, 45);
	CHECK(ok);

	// 1 full repeat (30,40,50) + partial up to 45 → 0,10,20,30,40
	// endPosWithinFirstRepeat = 45 - 1*30 = 15
	// iEndPosWithinFirstRepeat = search(15) = index of 20 = 2
	// newNum = 3*1 + 2 = 5
	LONGS_EQUAL(5, arr.getNumElements());
	int32_t expected[] = {0, 10, 20, 30, 40};
	verifyKeys(expected, 5);
}

TEST(OrderedArrayTest, generateRepeatsEmptyIsNoop) {
	// Empty array — memory is null, should return true
	bool ok = arr.generateRepeats(10, 30);
	CHECK(ok);
	LONGS_EQUAL(0, arr.getNumElements());
}

// ── shiftHorizontal() ──────────────────────────────────────────────────

TEST(OrderedArrayTest, shiftHorizontalRight) {
	// Keys: 0,10,20 effectiveLength=30
	arr.insertAtKey(0);
	arr.insertAtKey(10);
	arr.insertAtKey(20);

	arr.shiftHorizontal(5, 30);
	// Each key += 5, no wrap: 5,15,25
	LONGS_EQUAL(3, arr.getNumElements());
	LONGS_EQUAL(5, arr.getKeyAtIndex(0));
	LONGS_EQUAL(15, arr.getKeyAtIndex(1));
	LONGS_EQUAL(25, arr.getKeyAtIndex(2));
}

TEST(OrderedArrayTest, shiftHorizontalLeft) {
	arr.insertAtKey(5);
	arr.insertAtKey(15);
	arr.insertAtKey(25);

	arr.shiftHorizontal(-5, 30);
	// Keys shifted left by 5: 0,10,20
	LONGS_EQUAL(3, arr.getNumElements());
	LONGS_EQUAL(0, arr.getKeyAtIndex(0));
	LONGS_EQUAL(10, arr.getKeyAtIndex(1));
	LONGS_EQUAL(20, arr.getKeyAtIndex(2));
}

TEST(OrderedArrayTest, shiftHorizontalWrapsRight) {
	// Keys: 0, 10, 25. effectiveLength=30. Shift right by 10.
	// key 0 → 10, key 10 → 20, key 25 → 35-30 = 5
	// So sorted result should be: 5, 10, 20
	arr.insertAtKey(0);
	arr.insertAtKey(10);
	arr.insertAtKey(25);

	arr.shiftHorizontal(10, 30);
	LONGS_EQUAL(3, arr.getNumElements());
	LONGS_EQUAL(5, arr.getKeyAtIndex(0));
	LONGS_EQUAL(10, arr.getKeyAtIndex(1));
	LONGS_EQUAL(20, arr.getKeyAtIndex(2));
}

TEST(OrderedArrayTest, shiftHorizontalZeroIsNoop) {
	arr.insertAtKey(10);
	arr.insertAtKey(20);
	arr.shiftHorizontal(0, 30);
	LONGS_EQUAL(10, arr.getKeyAtIndex(0));
	LONGS_EQUAL(20, arr.getKeyAtIndex(1));
}

TEST(OrderedArrayTest, shiftHorizontalEmptyIsNoop) {
	arr.shiftHorizontal(10, 30); // should not crash
	LONGS_EQUAL(0, arr.getNumElements());
}

TEST(OrderedArrayTest, shiftHorizontalWrapsAmount) {
	arr.insertAtKey(0);
	arr.insertAtKey(10);
	// Shift by exactly effectiveLength — should be noop
	arr.shiftHorizontal(30, 30);
	LONGS_EQUAL(0, arr.getKeyAtIndex(0));
	LONGS_EQUAL(10, arr.getKeyAtIndex(1));
}

TEST(OrderedArrayTest, shiftHorizontalLargeWrap) {
	arr.insertAtKey(0);
	arr.insertAtKey(10);
	// Shift by 65 with effectiveLength=30 → 65 % 30 = 5
	arr.shiftHorizontal(65, 30);
	LONGS_EQUAL(5, arr.getKeyAtIndex(0));
	LONGS_EQUAL(15, arr.getKeyAtIndex(1));
}

// ── searchMultiple() ────────────────────────────────────────────────────

TEST(OrderedArrayTest, searchMultipleBasic) {
	insertKeys(0, 20); // 0..19
	int32_t terms[3] = {5, 10, 15};
	arr.searchMultiple(terms, 3);
	// Results written back into terms as GREATER_OR_EQUAL indexes
	LONGS_EQUAL(5, terms[0]);
	LONGS_EQUAL(10, terms[1]);
	LONGS_EQUAL(15, terms[2]);
}

TEST(OrderedArrayTest, searchMultipleSparse) {
	// Insert even keys: 0,2,4,...,18
	for (int32_t i = 0; i < 10; i++) {
		arr.insertAtKey(i * 2);
	}
	int32_t terms[3] = {3, 7, 15};
	arr.searchMultiple(terms, 3);
	// 3 → index of 4 (idx 2), 7 → index of 8 (idx 4), 15 → index of 16 (idx 8)
	LONGS_EQUAL(2, terms[0]);
	LONGS_EQUAL(4, terms[1]);
	LONGS_EQUAL(8, terms[2]);
}

TEST(OrderedArrayTest, searchMultipleSingleTerm) {
	insertKeys(0, 10);
	int32_t term = 5;
	arr.searchMultiple(&term, 1);
	LONGS_EQUAL(5, term);
}

// ── Stress: insert + delete + verify ────────────────────────────────────

TEST(OrderedArrayTest, insertDeleteStress) {
	// Insert 50 items in random-ish order, delete half, verify order
	for (int32_t i = 0; i < 50; i++) {
		arr.insertAtKey((i * 37) % 97); // pseudo-random spread over 0..96
	}
	LONGS_EQUAL(50, arr.getNumElements());

	// Verify ascending order
	for (int32_t i = 1; i < arr.getNumElements(); i++) {
		CHECK(arr.getKeyAtIndex(i) >= arr.getKeyAtIndex(i - 1));
	}

	// Delete every other element
	for (int32_t i = arr.getNumElements() - 1; i >= 0; i -= 2) {
		arr.deleteAtIndex(i);
	}

	// Verify still sorted
	for (int32_t i = 1; i < arr.getNumElements(); i++) {
		CHECK(arr.getKeyAtIndex(i) >= arr.getKeyAtIndex(i - 1));
	}
}
