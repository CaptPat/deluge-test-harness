// Additional ResizeableArray tests — focused on cloneFrom, swapStateWith,
// circular buffer wrap-around stress, and multi-delete edge cases.
// Complements the existing tests/model/resizeable_array_test.cpp.

#include "CppUTest/TestHarness.h"
#include "util/container/array/resizeable_array.h"
#include <cstring>

TEST_GROUP(ResizeableArrayDeepTest) {
	ResizeableArray arr{static_cast<int32_t>(sizeof(int32_t)), 16, 15};

	void teardown() override {
		arr.empty();
	}

	void setElement(int32_t index, int32_t value) {
		*(int32_t*)arr.getElementAddress(index) = value;
	}

	int32_t getElement(int32_t index) {
		return *(int32_t*)arr.getElementAddress(index);
	}

	void insertSequence(int32_t count) {
		for (int32_t i = 0; i < count; i++) {
			Error err = arr.insertAtIndex(i);
			CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
			setElement(i, i);
		}
	}
};

// ── cloneFrom ───────────────────────────────────────────────────────────

TEST(ResizeableArrayDeepTest, cloneFromCopiesAllData) {
	insertSequence(5);

	ResizeableArray clone{static_cast<int32_t>(sizeof(int32_t))};
	bool ok = clone.cloneFrom(&arr);
	CHECK(ok);

	LONGS_EQUAL(5, clone.getNumElements());
	for (int32_t i = 0; i < 5; i++) {
		LONGS_EQUAL(i, *(int32_t*)clone.getElementAddress(i));
	}

	// Modify clone, verify original unchanged
	*(int32_t*)clone.getElementAddress(0) = 99;
	LONGS_EQUAL(0, getElement(0));

	clone.empty();
}

TEST(ResizeableArrayDeepTest, cloneFromEmpty) {
	ResizeableArray clone{static_cast<int32_t>(sizeof(int32_t))};
	bool ok = clone.cloneFrom(&arr);
	CHECK(ok);
	LONGS_EQUAL(0, clone.getNumElements());
	clone.empty();
}

TEST(ResizeableArrayDeepTest, cloneFromLargeArray) {
	insertSequence(50);

	ResizeableArray clone{static_cast<int32_t>(sizeof(int32_t))};
	bool ok = clone.cloneFrom(&arr);
	CHECK(ok);

	LONGS_EQUAL(50, clone.getNumElements());
	for (int32_t i = 0; i < 50; i++) {
		LONGS_EQUAL(i, *(int32_t*)clone.getElementAddress(i));
	}

	clone.empty();
}

// ── swapStateWith ───────────────────────────────────────────────────────

TEST(ResizeableArrayDeepTest, swapStateExchangesContents) {
	insertSequence(3); // arr = [0, 1, 2]

	ResizeableArray other{static_cast<int32_t>(sizeof(int32_t))};
	Error err = other.insertAtIndex(0);
	CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
	*(int32_t*)other.getElementAddress(0) = 99;
	err = other.insertAtIndex(1);
	CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
	*(int32_t*)other.getElementAddress(1) = 100;

	arr.swapStateWith(&other);

	LONGS_EQUAL(2, arr.getNumElements());
	LONGS_EQUAL(99, getElement(0));
	LONGS_EQUAL(100, getElement(1));

	LONGS_EQUAL(3, other.getNumElements());
	LONGS_EQUAL(0, *(int32_t*)other.getElementAddress(0));
	LONGS_EQUAL(1, *(int32_t*)other.getElementAddress(1));
	LONGS_EQUAL(2, *(int32_t*)other.getElementAddress(2));

	other.empty();
}

TEST(ResizeableArrayDeepTest, swapWithEmpty) {
	insertSequence(5);

	ResizeableArray empty_arr{static_cast<int32_t>(sizeof(int32_t))};
	arr.swapStateWith(&empty_arr);

	LONGS_EQUAL(0, arr.getNumElements());
	LONGS_EQUAL(5, empty_arr.getNumElements());
	for (int32_t i = 0; i < 5; i++) {
		LONGS_EQUAL(i, *(int32_t*)empty_arr.getElementAddress(i));
	}

	empty_arr.empty();
}

// ── Multi-element insert ────────────────────────────────────────────────

TEST(ResizeableArrayDeepTest, insertMultipleAtMiddle) {
	insertSequence(4); // [0, 1, 2, 3]
	Error err = arr.insertAtIndex(2, 3); // Insert 3 slots at index 2
	CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
	LONGS_EQUAL(7, arr.getNumElements());

	// Original elements shifted
	LONGS_EQUAL(0, getElement(0));
	LONGS_EQUAL(1, getElement(1));
	// Elements 2,3,4 are uninitialized
	LONGS_EQUAL(2, getElement(5));
	LONGS_EQUAL(3, getElement(6));
}

TEST(ResizeableArrayDeepTest, insertMultipleAtBeginning) {
	insertSequence(3); // [0, 1, 2]
	Error err = arr.insertAtIndex(0, 2);
	CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
	LONGS_EQUAL(5, arr.getNumElements());
	// Original data shifted right by 2
	LONGS_EQUAL(0, getElement(2));
	LONGS_EQUAL(1, getElement(3));
	LONGS_EQUAL(2, getElement(4));
}

// ── Multi-element delete ────────────────────────────────────────────────

TEST(ResizeableArrayDeepTest, deleteMultipleFromFront) {
	insertSequence(8); // [0,1,2,3,4,5,6,7]
	arr.deleteAtIndex(0, 3);
	LONGS_EQUAL(5, arr.getNumElements());
	LONGS_EQUAL(3, getElement(0));
	LONGS_EQUAL(7, getElement(4));
}

TEST(ResizeableArrayDeepTest, deleteMultipleFromEnd) {
	insertSequence(8);
	arr.deleteAtIndex(5, 3);
	LONGS_EQUAL(5, arr.getNumElements());
	LONGS_EQUAL(0, getElement(0));
	LONGS_EQUAL(4, getElement(4));
}

TEST(ResizeableArrayDeepTest, deleteMultipleFromMiddle) {
	insertSequence(10); // [0..9]
	arr.deleteAtIndex(3, 4); // Delete indices 3,4,5,6
	LONGS_EQUAL(6, arr.getNumElements());
	LONGS_EQUAL(0, getElement(0));
	LONGS_EQUAL(1, getElement(1));
	LONGS_EQUAL(2, getElement(2));
	LONGS_EQUAL(7, getElement(3));
	LONGS_EQUAL(8, getElement(4));
	LONGS_EQUAL(9, getElement(5));
}

// ── Wrap-around stress ──────────────────────────────────────────────────
// Delete from front pushes memoryStart forward, eventually wrapping.
// This exercises the circular buffer logic in deleteAtIndex/insertAtIndex.

TEST(ResizeableArrayDeepTest, wrapAroundViaFrontDeletion) {
	// Insert 100, then delete-from-front + insert-at-end to cycle memory
	for (int32_t i = 0; i < 20; i++) {
		Error err = arr.insertAtIndex(arr.getNumElements());
		CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
		setElement(arr.getNumElements() - 1, i);
	}

	// Now cycle: delete 1 from front, add 1 at end, 50 times
	for (int32_t cycle = 0; cycle < 50; cycle++) {
		arr.deleteAtIndex(0);
		Error err = arr.insertAtIndex(arr.getNumElements());
		CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
		setElement(arr.getNumElements() - 1, 20 + cycle);
	}

	// Should still have 20 elements, values 50..69
	LONGS_EQUAL(20, arr.getNumElements());
	LONGS_EQUAL(50, getElement(0));
	LONGS_EQUAL(69, getElement(19));
}

TEST(ResizeableArrayDeepTest, wrapAroundSwapElements) {
	// Build a wrapped array by cycling front deletions
	for (int32_t i = 0; i < 10; i++) {
		arr.insertAtIndex(arr.getNumElements());
		setElement(arr.getNumElements() - 1, i);
	}
	for (int32_t i = 0; i < 5; i++) {
		arr.deleteAtIndex(0);
		arr.insertAtIndex(arr.getNumElements());
		setElement(arr.getNumElements() - 1, 10 + i);
	}
	// arr = [5,6,7,8,9,10,11,12,13,14], possibly wrapped internally

	arr.swapElements(0, 9);
	LONGS_EQUAL(14, getElement(0));
	LONGS_EQUAL(5, getElement(9));

	// Middle elements unchanged
	LONGS_EQUAL(6, getElement(1));
	LONGS_EQUAL(13, getElement(8));
}

TEST(ResizeableArrayDeepTest, wrapAroundRepositionElement) {
	for (int32_t i = 0; i < 10; i++) {
		arr.insertAtIndex(arr.getNumElements());
		setElement(arr.getNumElements() - 1, i);
	}
	// Cycle to induce wrap
	for (int32_t i = 0; i < 7; i++) {
		arr.deleteAtIndex(0);
		arr.insertAtIndex(arr.getNumElements());
		setElement(arr.getNumElements() - 1, 10 + i);
	}
	// arr = [7,8,9,10,11,12,13,14,15,16]

	arr.repositionElement(0, 5);
	// [8,9,10,11,12,7,13,14,15,16]
	LONGS_EQUAL(8, getElement(0));
	LONGS_EQUAL(7, getElement(5));
	LONGS_EQUAL(16, getElement(9));
}

// ── ensureEnoughSpaceAllocated ──────────────────────────────────────────

TEST(ResizeableArrayDeepTest, ensureSpaceFromEmpty) {
	bool ok = arr.ensureEnoughSpaceAllocated(10);
	CHECK(ok);
	LONGS_EQUAL(0, arr.getNumElements());
}

TEST(ResizeableArrayDeepTest, ensureSpaceThenBulkInsert) {
	arr.ensureEnoughSpaceAllocated(50);
	for (int32_t i = 0; i < 50; i++) {
		Error err = arr.insertAtIndex(arr.getNumElements());
		CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
		setElement(arr.getNumElements() - 1, i);
	}
	LONGS_EQUAL(50, arr.getNumElements());
	for (int32_t i = 0; i < 50; i++) {
		LONGS_EQUAL(i, getElement(i));
	}
}

// ── Empty then reuse ────────────────────────────────────────────────────

TEST(ResizeableArrayDeepTest, emptyAndReuse) {
	insertSequence(20);
	arr.empty();
	LONGS_EQUAL(0, arr.getNumElements());

	// Reuse after emptying
	insertSequence(10);
	LONGS_EQUAL(10, arr.getNumElements());
	for (int32_t i = 0; i < 10; i++) {
		LONGS_EQUAL(i, getElement(i));
	}
}

// ── Larger element sizes ────────────────────────────────────────────────

TEST(ResizeableArrayDeepTest, structElements) {
	struct Record { int32_t id; int32_t value; int32_t flags; };
	ResizeableArray structArr{static_cast<int32_t>(sizeof(Record))};

	for (int32_t i = 0; i < 15; i++) {
		Error err = structArr.insertAtIndex(i);
		CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
		Record* r = (Record*)structArr.getElementAddress(i);
		r->id = i;
		r->value = i * 100;
		r->flags = (i % 2 == 0) ? 1 : 0;
	}

	// Delete some, verify remaining
	structArr.deleteAtIndex(5, 5); // Remove indices 5-9
	LONGS_EQUAL(10, structArr.getNumElements());

	Record* r0 = (Record*)structArr.getElementAddress(0);
	LONGS_EQUAL(0, r0->id);

	Record* r5 = (Record*)structArr.getElementAddress(5);
	LONGS_EQUAL(10, r5->id);
	LONGS_EQUAL(1000, r5->value);

	structArr.empty();
}

// ── Wrap-around insert/delete stress ────────────────────────────────
// These tests force the circular buffer into wrapped states and exercise
// the complex branching in deleteAtIndex (lines 278-441) and
// insertAtIndex (lines 968-1079) of resizeable_array.cpp.

// Helper: create a wrapped array by cycling delete-front + insert-end.
// Returns an array of size 'count' with values [startVal .. startVal+count-1],
// where memoryStart has been advanced past the end of the allocated block.
static void makeWrappedArray(ResizeableArray& a, int32_t count, int32_t cycles, int32_t startVal = 0) {
	// Fill initially
	for (int32_t i = 0; i < count; i++) {
		Error err = a.insertAtIndex(a.getNumElements());
		CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
		*(int32_t*)a.getElementAddress(a.getNumElements() - 1) = startVal + i;
	}
	// Cycle to advance memoryStart past the buffer end → wrapping
	for (int32_t c = 0; c < cycles; c++) {
		a.deleteAtIndex(0);
		Error err = a.insertAtIndex(a.getNumElements());
		CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
		*(int32_t*)a.getElementAddress(a.getNumElements() - 1) = startVal + count + c;
	}
}

static void verifySequence(ResizeableArray& a, int32_t startVal) {
	for (int32_t i = 0; i < a.getNumElements(); i++) {
		LONGS_EQUAL(startVal + i, *(int32_t*)a.getElementAddress(i));
	}
}

TEST(ResizeableArrayDeepTest, wrappedDeleteAtFront) {
	// Create wrapped array, then delete from front
	makeWrappedArray(arr, 10, 8); // values [8..17]
	arr.deleteAtIndex(0, 3);
	LONGS_EQUAL(7, arr.getNumElements());
	verifySequence(arr, 11);
}

TEST(ResizeableArrayDeepTest, wrappedDeleteAtEnd) {
	makeWrappedArray(arr, 10, 8); // values [8..17]
	arr.deleteAtIndex(7, 3);
	LONGS_EQUAL(7, arr.getNumElements());
	verifySequence(arr, 8);
}

TEST(ResizeableArrayDeepTest, wrappedDeleteFromMiddle) {
	makeWrappedArray(arr, 10, 8); // values [8..17]
	arr.deleteAtIndex(4, 2);
	LONGS_EQUAL(8, arr.getNumElements());
	// [8,9,10,11,14,15,16,17]
	LONGS_EQUAL(8, getElement(0));
	LONGS_EQUAL(11, getElement(3));
	LONGS_EQUAL(14, getElement(4));
	LONGS_EQUAL(17, getElement(7));
}

TEST(ResizeableArrayDeepTest, wrappedDeleteSingle) {
	makeWrappedArray(arr, 10, 8);
	for (int32_t pass = 0; pass < 5; pass++) {
		arr.deleteAtIndex(0);
	}
	LONGS_EQUAL(5, arr.getNumElements());
	verifySequence(arr, 13);
}

TEST(ResizeableArrayDeepTest, wrappedInsertAtFront) {
	makeWrappedArray(arr, 10, 8); // values [8..17]
	Error err = arr.insertAtIndex(0);
	CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
	setElement(0, 7);
	LONGS_EQUAL(11, arr.getNumElements());
	LONGS_EQUAL(7, getElement(0));
	for (int32_t i = 1; i <= 10; i++) {
		LONGS_EQUAL(7 + i, getElement(i));
	}
}

TEST(ResizeableArrayDeepTest, wrappedInsertAtEnd) {
	makeWrappedArray(arr, 10, 8); // values [8..17]
	Error err = arr.insertAtIndex(arr.getNumElements());
	CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
	setElement(arr.getNumElements() - 1, 18);
	LONGS_EQUAL(11, arr.getNumElements());
	verifySequence(arr, 8);
}

TEST(ResizeableArrayDeepTest, wrappedInsertAtMiddle) {
	makeWrappedArray(arr, 10, 8); // values [8..17]
	Error err = arr.insertAtIndex(5, 2);
	CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
	setElement(5, 99);
	setElement(6, 100);
	LONGS_EQUAL(12, arr.getNumElements());
	LONGS_EQUAL(12, getElement(4));
	LONGS_EQUAL(99, getElement(5));
	LONGS_EQUAL(100, getElement(6));
	LONGS_EQUAL(13, getElement(7));
}

TEST(ResizeableArrayDeepTest, wrappedInsertNearWrapBoundary) {
	// Cycle enough to guarantee wrapping, then insert at various points
	makeWrappedArray(arr, 8, 12); // values [12..19]
	// Insert near the middle (likely near wrap boundary)
	for (int32_t pos = 0; pos <= 8; pos++) {
		ResizeableArray test{static_cast<int32_t>(sizeof(int32_t)), 16, 15};
		makeWrappedArray(test, 8, 12);
		Error err = test.insertAtIndex(pos);
		CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
		*(int32_t*)test.getElementAddress(pos) = -1;
		LONGS_EQUAL(9, test.getNumElements());
		// Verify elements before insertion
		for (int32_t i = 0; i < pos; i++) {
			LONGS_EQUAL(12 + i, *(int32_t*)test.getElementAddress(i));
		}
		LONGS_EQUAL(-1, *(int32_t*)test.getElementAddress(pos));
		// Verify elements after insertion
		for (int32_t i = pos + 1; i < 9; i++) {
			LONGS_EQUAL(12 + i - 1, *(int32_t*)test.getElementAddress(i));
		}
		test.empty();
	}
}

TEST(ResizeableArrayDeepTest, wrappedDeleteNearWrapBoundary) {
	// Try deleting at every position in a wrapped array
	for (int32_t pos = 0; pos < 8; pos++) {
		ResizeableArray test{static_cast<int32_t>(sizeof(int32_t)), 16, 15};
		makeWrappedArray(test, 8, 12); // values [12..19]
		test.deleteAtIndex(pos);
		LONGS_EQUAL(7, test.getNumElements());
		// Verify remaining elements
		int32_t expected = 12;
		for (int32_t i = 0; i < 7; i++) {
			if (i == pos) {
				expected++; // skip deleted
			}
			LONGS_EQUAL(expected, *(int32_t*)test.getElementAddress(i));
			expected++;
		}
		test.empty();
	}
}

TEST(ResizeableArrayDeepTest, wrappedMultiDeleteSpanningWrap) {
	// Delete a range that spans the internal wrap point
	makeWrappedArray(arr, 12, 10); // values [10..21]
	// Delete 6 from middle — likely spans wrap
	arr.deleteAtIndex(3, 6);
	LONGS_EQUAL(6, arr.getNumElements());
	// Remaining: [10,11,12,19,20,21]
	LONGS_EQUAL(10, getElement(0));
	LONGS_EQUAL(11, getElement(1));
	LONGS_EQUAL(12, getElement(2));
	LONGS_EQUAL(19, getElement(3));
	LONGS_EQUAL(20, getElement(4));
	LONGS_EQUAL(21, getElement(5));
}

TEST(ResizeableArrayDeepTest, wrappedCloneFrom) {
	makeWrappedArray(arr, 10, 8); // values [8..17], internally wrapped
	ResizeableArray clone{static_cast<int32_t>(sizeof(int32_t))};
	bool ok = clone.cloneFrom(&arr);
	CHECK(ok);
	LONGS_EQUAL(10, clone.getNumElements());
	verifySequence(clone, 8);
	clone.empty();
}

TEST(ResizeableArrayDeepTest, wrappedBeenCloned) {
	makeWrappedArray(arr, 10, 8); // values [8..17]
	// beenCloned copies to new flat memory
	Error err = arr.beenCloned();
	CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
	LONGS_EQUAL(10, arr.getNumElements());
	verifySequence(arr, 8);
}

TEST(ResizeableArrayDeepTest, wrappedSwapElements) {
	makeWrappedArray(arr, 10, 8); // values [8..17]
	arr.swapElements(0, 9); // Swap first and last (likely across wrap)
	LONGS_EQUAL(17, getElement(0));
	LONGS_EQUAL(8, getElement(9));
	// Middle unchanged
	for (int32_t i = 1; i < 9; i++) {
		LONGS_EQUAL(8 + i, getElement(i));
	}
}

TEST(ResizeableArrayDeepTest, wrappedRepositionForward) {
	makeWrappedArray(arr, 8, 10); // values [10..17]
	arr.repositionElement(1, 6);
	// [10, 12, 13, 14, 15, 16, 11, 17]
	LONGS_EQUAL(10, getElement(0));
	LONGS_EQUAL(12, getElement(1));
	LONGS_EQUAL(11, getElement(6));
	LONGS_EQUAL(17, getElement(7));
}

TEST(ResizeableArrayDeepTest, wrappedRepositionBackward) {
	makeWrappedArray(arr, 8, 10); // values [10..17]
	arr.repositionElement(6, 1);
	// [10, 16, 11, 12, 13, 14, 15, 17]
	LONGS_EQUAL(10, getElement(0));
	LONGS_EQUAL(16, getElement(1));
	LONGS_EQUAL(11, getElement(2));
	LONGS_EQUAL(17, getElement(7));
}

TEST(ResizeableArrayDeepTest, heavyCyclingStress) {
	// Insert 20 elements, then cycle 200 times with random-ish insert/delete
	for (int32_t i = 0; i < 20; i++) {
		arr.insertAtIndex(arr.getNumElements());
		setElement(arr.getNumElements() - 1, i);
	}

	int32_t nextVal = 20;
	for (int32_t cycle = 0; cycle < 200; cycle++) {
		// Delete from front
		arr.deleteAtIndex(0);
		// Insert at end
		arr.insertAtIndex(arr.getNumElements());
		setElement(arr.getNumElements() - 1, nextVal++);

		// Every 10th cycle, also insert in middle
		if (cycle % 10 == 0) {
			int32_t mid = arr.getNumElements() / 2;
			arr.insertAtIndex(mid);
			setElement(mid, nextVal++);
		}

		// Every 15th cycle, delete from middle
		if (cycle % 15 == 0 && arr.getNumElements() > 5) {
			arr.deleteAtIndex(arr.getNumElements() / 3);
		}
	}

	// Just verify the array is consistent (no crashes, correct count)
	CHECK(arr.getNumElements() > 0);
	// All elements should be readable without crash
	for (int32_t i = 0; i < arr.getNumElements(); i++) {
		volatile int32_t v = getElement(i);
		(void)v;
	}
}

TEST(ResizeableArrayDeepTest, wrappedSwapStateWith) {
	makeWrappedArray(arr, 8, 10); // values [10..17], wrapped

	ResizeableArray other{static_cast<int32_t>(sizeof(int32_t)), 16, 15};
	makeWrappedArray(other, 5, 6); // values [6..10], wrapped

	arr.swapStateWith(&other);

	LONGS_EQUAL(5, arr.getNumElements());
	verifySequence(arr, 6);
	LONGS_EQUAL(8, other.getNumElements());
	for (int32_t i = 0; i < 8; i++) {
		LONGS_EQUAL(10 + i, *(int32_t*)other.getElementAddress(i));
	}

	other.empty();
}

TEST(ResizeableArrayDeepTest, wrappedEnsureSpaceThenInsert) {
	makeWrappedArray(arr, 10, 8); // wrapped
	bool ok = arr.ensureEnoughSpaceAllocated(20);
	CHECK(ok);
	// Original data should still be intact
	LONGS_EQUAL(10, arr.getNumElements());
	verifySequence(arr, 8);
	// Now insert 20 more
	for (int32_t i = 0; i < 20; i++) {
		Error err = arr.insertAtIndex(arr.getNumElements());
		CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
		setElement(arr.getNumElements() - 1, 18 + i);
	}
	LONGS_EQUAL(30, arr.getNumElements());
	verifySequence(arr, 8);
}

TEST(ResizeableArrayDeepTest, deleteAllFromWrapped) {
	makeWrappedArray(arr, 10, 8); // wrapped
	arr.deleteAtIndex(0, 10);
	LONGS_EQUAL(0, arr.getNumElements());
}

TEST(ResizeableArrayDeepTest, deleteToOneElement) {
	makeWrappedArray(arr, 10, 8); // values [8..17]
	arr.deleteAtIndex(0, 9);
	LONGS_EQUAL(1, arr.getNumElements());
	LONGS_EQUAL(17, getElement(0));
}
