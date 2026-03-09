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
