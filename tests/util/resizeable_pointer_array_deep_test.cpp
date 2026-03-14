// Deep tests for ResizeablePointerArray — insert, get, set, delete, growth.

#include "CppUTest/TestHarness.h"
#include "util/container/array/resizeable_pointer_array.h"

TEST_GROUP(PointerArrayDeep){};

TEST(PointerArrayDeep, constructionDefaults) {
	ResizeablePointerArray arr;
	CHECK_EQUAL(0, arr.getNumElements());
}

TEST(PointerArrayDeep, insertAtIndex0) {
	ResizeablePointerArray arr;
	int dummy = 42;
	Error err = arr.insertPointerAtIndex(&dummy, 0);
	CHECK_TRUE(err == Error::NONE);
	CHECK_EQUAL(1, arr.getNumElements());
	POINTERS_EQUAL(&dummy, arr.getPointerAtIndex(0));
}

TEST(PointerArrayDeep, insertMultiplePointersInOrder) {
	ResizeablePointerArray arr;
	int a = 1, b = 2, c = 3;
	arr.insertPointerAtIndex(&a, 0);
	arr.insertPointerAtIndex(&b, 1);
	arr.insertPointerAtIndex(&c, 2);
	CHECK_EQUAL(3, arr.getNumElements());
	POINTERS_EQUAL(&a, arr.getPointerAtIndex(0));
	POINTERS_EQUAL(&b, arr.getPointerAtIndex(1));
	POINTERS_EQUAL(&c, arr.getPointerAtIndex(2));
}

TEST(PointerArrayDeep, insertAtFrontShiftsOthers) {
	ResizeablePointerArray arr;
	int a = 1, b = 2, c = 3;
	arr.insertPointerAtIndex(&a, 0);
	arr.insertPointerAtIndex(&b, 1);
	// Insert at front
	arr.insertPointerAtIndex(&c, 0);
	CHECK_EQUAL(3, arr.getNumElements());
	POINTERS_EQUAL(&c, arr.getPointerAtIndex(0));
	POINTERS_EQUAL(&a, arr.getPointerAtIndex(1));
	POINTERS_EQUAL(&b, arr.getPointerAtIndex(2));
}

TEST(PointerArrayDeep, insertInMiddle) {
	ResizeablePointerArray arr;
	int a = 1, b = 2, c = 3;
	arr.insertPointerAtIndex(&a, 0);
	arr.insertPointerAtIndex(&b, 1);
	arr.insertPointerAtIndex(&c, 1); // insert between a and b
	CHECK_EQUAL(3, arr.getNumElements());
	POINTERS_EQUAL(&a, arr.getPointerAtIndex(0));
	POINTERS_EQUAL(&c, arr.getPointerAtIndex(1));
	POINTERS_EQUAL(&b, arr.getPointerAtIndex(2));
}

TEST(PointerArrayDeep, setPointerOverwrites) {
	ResizeablePointerArray arr;
	int a = 1, b = 2;
	arr.insertPointerAtIndex(&a, 0);
	POINTERS_EQUAL(&a, arr.getPointerAtIndex(0));
	arr.setPointerAtIndex(&b, 0);
	POINTERS_EQUAL(&b, arr.getPointerAtIndex(0));
}

TEST(PointerArrayDeep, deleteAtIndex) {
	ResizeablePointerArray arr;
	int a = 1, b = 2, c = 3;
	arr.insertPointerAtIndex(&a, 0);
	arr.insertPointerAtIndex(&b, 1);
	arr.insertPointerAtIndex(&c, 2);

	arr.deleteAtIndex(1); // remove b
	CHECK_EQUAL(2, arr.getNumElements());
	POINTERS_EQUAL(&a, arr.getPointerAtIndex(0));
	POINTERS_EQUAL(&c, arr.getPointerAtIndex(1));
}

TEST(PointerArrayDeep, deleteFirst) {
	ResizeablePointerArray arr;
	int a = 1, b = 2;
	arr.insertPointerAtIndex(&a, 0);
	arr.insertPointerAtIndex(&b, 1);

	arr.deleteAtIndex(0);
	CHECK_EQUAL(1, arr.getNumElements());
	POINTERS_EQUAL(&b, arr.getPointerAtIndex(0));
}

TEST(PointerArrayDeep, deleteLast) {
	ResizeablePointerArray arr;
	int a = 1, b = 2;
	arr.insertPointerAtIndex(&a, 0);
	arr.insertPointerAtIndex(&b, 1);

	arr.deleteAtIndex(1);
	CHECK_EQUAL(1, arr.getNumElements());
	POINTERS_EQUAL(&a, arr.getPointerAtIndex(0));
}

TEST(PointerArrayDeep, deleteAll) {
	ResizeablePointerArray arr;
	int a = 1, b = 2, c = 3;
	arr.insertPointerAtIndex(&a, 0);
	arr.insertPointerAtIndex(&b, 1);
	arr.insertPointerAtIndex(&c, 2);

	arr.deleteAtIndex(2);
	arr.deleteAtIndex(1);
	arr.deleteAtIndex(0);
	CHECK_EQUAL(0, arr.getNumElements());
}

TEST(PointerArrayDeep, growthBeyondInitialCapacity) {
	ResizeablePointerArray arr;
	int dummies[50];
	for (int i = 0; i < 50; i++) {
		dummies[i] = i;
		Error err = arr.insertPointerAtIndex(&dummies[i], i);
		CHECK_TRUE(err == Error::NONE);
	}
	CHECK_EQUAL(50, arr.getNumElements());

	// Verify all pointers correct after growth
	for (int i = 0; i < 50; i++) {
		POINTERS_EQUAL(&dummies[i], arr.getPointerAtIndex(i));
	}
}

TEST(PointerArrayDeep, nullPointerStored) {
	ResizeablePointerArray arr;
	arr.insertPointerAtIndex(nullptr, 0);
	CHECK_EQUAL(1, arr.getNumElements());
	POINTERS_EQUAL(nullptr, arr.getPointerAtIndex(0));
}

TEST(PointerArrayDeep, interleavedInsertDelete) {
	ResizeablePointerArray arr;
	int a = 1, b = 2, c = 3, d = 4;

	arr.insertPointerAtIndex(&a, 0);
	arr.insertPointerAtIndex(&b, 1);
	arr.deleteAtIndex(0); // remove a
	arr.insertPointerAtIndex(&c, 0);
	arr.insertPointerAtIndex(&d, 2);

	CHECK_EQUAL(3, arr.getNumElements());
	POINTERS_EQUAL(&c, arr.getPointerAtIndex(0));
	POINTERS_EQUAL(&b, arr.getPointerAtIndex(1));
	POINTERS_EQUAL(&d, arr.getPointerAtIndex(2));
}
