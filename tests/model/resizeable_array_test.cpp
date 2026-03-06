// ResizeableArray regression tests — exercises the firmware's real container
// compiled on x86 with the mock memory allocator.

#include "CppUTest/TestHarness.h"
#include "util/container/array/resizeable_array.h"
#include "util/container/array/resizeable_pointer_array.h"
#include "util/container/array/ordered_resizeable_array.h"
#include "util/container/array/ordered_resizeable_array_with_multi_word_key.h"

// ── ResizeableArray basics ──────────────────────────────────────────────

TEST_GROUP(ResizeableArrayTest) {};

TEST(ResizeableArrayTest, emptyAfterConstruction) {
	ResizeableArray arr(sizeof(int32_t));
	CHECK_EQUAL(0, arr.getNumElements());
}

TEST(ResizeableArrayTest, insertAndRetrieve) {
	ResizeableArray arr(sizeof(int32_t));

	for (int32_t i = 0; i < 10; i++) {
		Error err = arr.insertAtIndex(i);
		CHECK(err == Error::NONE);
		*(int32_t*)arr.getElementAddress(i) = i * 100;
	}

	CHECK_EQUAL(10, arr.getNumElements());

	for (int32_t i = 0; i < 10; i++) {
		int32_t val = *(int32_t*)arr.getElementAddress(i);
		CHECK_EQUAL(i * 100, val);
	}
}

TEST(ResizeableArrayTest, insertAtFront) {
	ResizeableArray arr(sizeof(int32_t));

	for (int32_t i = 0; i < 5; i++) {
		arr.insertAtIndex(0);
		*(int32_t*)arr.getElementAddress(0) = i;
	}

	CHECK_EQUAL(5, arr.getNumElements());
	// Elements were inserted at front, so order is reversed: 4,3,2,1,0
	CHECK_EQUAL(4, *(int32_t*)arr.getElementAddress(0));
	CHECK_EQUAL(0, *(int32_t*)arr.getElementAddress(4));
}

TEST(ResizeableArrayTest, deleteFromMiddle) {
	ResizeableArray arr(sizeof(int32_t));

	for (int32_t i = 0; i < 5; i++) {
		arr.insertAtIndex(i);
		*(int32_t*)arr.getElementAddress(i) = i;
	}

	// Delete element at index 2 (value=2)
	arr.deleteAtIndex(2);
	CHECK_EQUAL(4, arr.getNumElements());

	CHECK_EQUAL(0, *(int32_t*)arr.getElementAddress(0));
	CHECK_EQUAL(1, *(int32_t*)arr.getElementAddress(1));
	CHECK_EQUAL(3, *(int32_t*)arr.getElementAddress(2));
	CHECK_EQUAL(4, *(int32_t*)arr.getElementAddress(3));
}

TEST(ResizeableArrayTest, deleteAll) {
	ResizeableArray arr(sizeof(int32_t));

	for (int32_t i = 0; i < 5; i++) {
		arr.insertAtIndex(i);
	}

	arr.deleteAtIndex(0, 5);
	CHECK_EQUAL(0, arr.getNumElements());
}

TEST(ResizeableArrayTest, emptyMethod) {
	ResizeableArray arr(sizeof(int32_t));

	for (int32_t i = 0; i < 10; i++) {
		arr.insertAtIndex(i);
	}
	CHECK_EQUAL(10, arr.getNumElements());

	arr.empty();
	CHECK_EQUAL(0, arr.getNumElements());
}

TEST(ResizeableArrayTest, insertManyElements) {
	ResizeableArray arr(sizeof(int32_t));

	// Insert 100 elements — triggers multiple memory allocations
	for (int32_t i = 0; i < 100; i++) {
		Error err = arr.insertAtIndex(i);
		CHECK(err == Error::NONE);
		*(int32_t*)arr.getElementAddress(i) = i;
	}

	CHECK_EQUAL(100, arr.getNumElements());

	for (int32_t i = 0; i < 100; i++) {
		CHECK_EQUAL(i, *(int32_t*)arr.getElementAddress(i));
	}
}

TEST(ResizeableArrayTest, swapElements) {
	ResizeableArray arr(sizeof(int32_t));

	for (int32_t i = 0; i < 3; i++) {
		arr.insertAtIndex(i);
		*(int32_t*)arr.getElementAddress(i) = i * 10;
	}

	arr.swapElements(0, 2);
	CHECK_EQUAL(20, *(int32_t*)arr.getElementAddress(0));
	CHECK_EQUAL(10, *(int32_t*)arr.getElementAddress(1));
	CHECK_EQUAL(0, *(int32_t*)arr.getElementAddress(2));
}

TEST(ResizeableArrayTest, repositionElement) {
	ResizeableArray arr(sizeof(int32_t));

	for (int32_t i = 0; i < 5; i++) {
		arr.insertAtIndex(i);
		*(int32_t*)arr.getElementAddress(i) = i;
	}

	// Move element from index 0 to index 3
	arr.repositionElement(0, 3);
	CHECK_EQUAL(1, *(int32_t*)arr.getElementAddress(0));
	CHECK_EQUAL(2, *(int32_t*)arr.getElementAddress(1));
	CHECK_EQUAL(3, *(int32_t*)arr.getElementAddress(2));
	CHECK_EQUAL(0, *(int32_t*)arr.getElementAddress(3));
	CHECK_EQUAL(4, *(int32_t*)arr.getElementAddress(4));
}

TEST(ResizeableArrayTest, largeElements) {
	// Test with 64-byte elements (like a small struct)
	ResizeableArray arr(64);

	for (int32_t i = 0; i < 20; i++) {
		Error err = arr.insertAtIndex(i);
		CHECK(err == Error::NONE);
		memset(arr.getElementAddress(i), (uint8_t)i, 64);
	}

	CHECK_EQUAL(20, arr.getNumElements());

	for (int32_t i = 0; i < 20; i++) {
		uint8_t* data = (uint8_t*)arr.getElementAddress(i);
		CHECK_EQUAL((uint8_t)i, data[0]);
		CHECK_EQUAL((uint8_t)i, data[63]);
	}
}

// ── ResizeablePointerArray ──────────────────────────────────────────────

TEST_GROUP(ResizeablePointerArrayTest) {};

TEST(ResizeablePointerArrayTest, insertAndRetrievePointers) {
	ResizeablePointerArray arr;

	int32_t values[5] = {10, 20, 30, 40, 50};

	for (int32_t i = 0; i < 5; i++) {
		Error err = arr.insertPointerAtIndex(&values[i], i);
		CHECK(err == Error::NONE);
	}

	CHECK_EQUAL(5, arr.getNumElements());

	for (int32_t i = 0; i < 5; i++) {
		int32_t* ptr = (int32_t*)arr.getPointerAtIndex(i);
		CHECK_EQUAL(&values[i], ptr);
		CHECK_EQUAL(values[i], *ptr);
	}
}

TEST(ResizeablePointerArrayTest, setPointer) {
	ResizeablePointerArray arr;

	int32_t a = 1, b = 2, c = 3;
	arr.insertPointerAtIndex(&a, 0);
	arr.insertPointerAtIndex(&b, 1);

	arr.setPointerAtIndex(&c, 0);
	CHECK_EQUAL(&c, arr.getPointerAtIndex(0));
	CHECK_EQUAL(&b, arr.getPointerAtIndex(1));
}

// ── OrderedResizeableArray ──────────────────────────────────────────────

TEST_GROUP(OrderedResizeableArrayTest) {};

TEST(OrderedResizeableArrayTest, insertAtKeyMaintainsOrder) {
	// 8 bytes per element: 4-byte key + 4-byte payload
	OrderedResizeableArrayWith32bitKey arr(sizeof(int32_t) * 2);

	// Insert keys in random order
	int32_t keys[] = {50, 10, 30, 70, 20, 60, 40};
	for (int i = 0; i < 7; i++) {
		int32_t idx = arr.insertAtKey(keys[i]);
		CHECK(idx >= 0);
		// Set payload (second int32) to the key value
		int32_t* elem = (int32_t*)arr.getElementAddress(idx);
		elem[1] = keys[i] * 2; // payload = key * 2
	}

	CHECK_EQUAL(7, arr.getNumElements());

	// Verify sorted order
	for (int32_t i = 0; i < arr.getNumElements() - 1; i++) {
		int32_t key_i = arr.getKeyAtIndex(i);
		int32_t key_next = arr.getKeyAtIndex(i + 1);
		CHECK(key_i <= key_next);
	}
}

TEST(OrderedResizeableArrayTest, searchExact) {
	OrderedResizeableArrayWith32bitKey arr(sizeof(int32_t) * 2);

	for (int32_t k = 0; k < 10; k++) {
		int32_t idx = arr.insertAtKey(k * 10);
		int32_t* elem = (int32_t*)arr.getElementAddress(idx);
		elem[1] = k;
	}

	// Search for key 50 (which should be at index 5)
	int32_t idx = arr.searchExact(50);
	CHECK(idx >= 0);
	int32_t* elem = (int32_t*)arr.getElementAddress(idx);
	CHECK_EQUAL(50, elem[0]);
	CHECK_EQUAL(5, elem[1]);

	// Search for non-existent key
	int32_t missing = arr.searchExact(55);
	CHECK(missing < 0);
}

TEST(OrderedResizeableArrayTest, deleteAtKey) {
	OrderedResizeableArrayWith32bitKey arr(sizeof(int32_t) * 2);

	for (int32_t k = 0; k < 5; k++) {
		arr.insertAtKey(k * 10);
	}
	CHECK_EQUAL(5, arr.getNumElements());

	arr.deleteAtKey(20);
	CHECK_EQUAL(4, arr.getNumElements());

	// Verify 20 is gone
	CHECK(arr.searchExact(20) < 0);
	// Others still present
	CHECK(arr.searchExact(0) >= 0);
	CHECK(arr.searchExact(10) >= 0);
	CHECK(arr.searchExact(30) >= 0);
	CHECK(arr.searchExact(40) >= 0);
}

TEST(OrderedResizeableArrayTest, insertDuplicateKeyCreatesNewEntry) {
	OrderedResizeableArrayWith32bitKey arr(sizeof(int32_t) * 2);

	arr.insertAtKey(100);
	arr.insertAtKey(100);
	CHECK_EQUAL(2, arr.getNumElements());
	// Both elements should have key 100
	CHECK_EQUAL(100, arr.getKeyAtIndex(0));
	CHECK_EQUAL(100, arr.getKeyAtIndex(1));
}

TEST(OrderedResizeableArrayTest, searchMultipleKeysAllFound) {
	OrderedResizeableArrayWith32bitKey arr(sizeof(int32_t) * 2);

	for (int32_t k = 0; k < 50; k++) {
		arr.insertAtKey(k * 3); // 0, 3, 6, 9, ..., 147
	}
	CHECK_EQUAL(50, arr.getNumElements());

	// All keys should be findable
	for (int32_t k = 0; k < 50; k++) {
		CHECK(arr.searchExact(k * 3) >= 0);
	}

	// Keys between should not be found
	CHECK(arr.searchExact(1) < 0);
	CHECK(arr.searchExact(2) < 0);
	CHECK(arr.searchExact(4) < 0);
}

TEST(OrderedResizeableArrayTest, deleteNonExistentKeyNoOp) {
	OrderedResizeableArrayWith32bitKey arr(sizeof(int32_t) * 2);

	arr.insertAtKey(10);
	arr.insertAtKey(20);
	arr.deleteAtKey(15); // doesn't exist
	CHECK_EQUAL(2, arr.getNumElements());
}

TEST(OrderedResizeableArrayTest, insertAtKeyReturnsCorrectIndex) {
	OrderedResizeableArrayWith32bitKey arr(sizeof(int32_t) * 2);

	// Insert in reverse order; each should go to start
	int32_t idx3 = arr.insertAtKey(30);
	int32_t idx1 = arr.insertAtKey(10);
	int32_t idx2 = arr.insertAtKey(20);

	// After all inserts, order should be 10, 20, 30
	CHECK_EQUAL(10, arr.getKeyAtIndex(0));
	CHECK_EQUAL(20, arr.getKeyAtIndex(1));
	CHECK_EQUAL(30, arr.getKeyAtIndex(2));
}

TEST(OrderedResizeableArrayTest, getKeyAtIndexBoundary) {
	OrderedResizeableArrayWith32bitKey arr(sizeof(int32_t) * 2);

	arr.insertAtKey(100);
	arr.insertAtKey(200);
	arr.insertAtKey(300);

	CHECK_EQUAL(100, arr.getKeyAtIndex(0));
	CHECK_EQUAL(300, arr.getKeyAtIndex(2));
}

TEST(OrderedResizeableArrayTest, deleteAllThenReinsert) {
	OrderedResizeableArrayWith32bitKey arr(sizeof(int32_t) * 2);

	for (int32_t k = 0; k < 5; k++) {
		arr.insertAtKey(k * 10);
	}
	// Delete all
	for (int32_t k = 4; k >= 0; k--) {
		arr.deleteAtKey(k * 10);
	}
	CHECK_EQUAL(0, arr.getNumElements());

	// Reinsert
	arr.insertAtKey(42);
	CHECK_EQUAL(1, arr.getNumElements());
	CHECK_EQUAL(42, arr.getKeyAtIndex(0));
}

// ── OrderedResizeableArrayWithMultiWordKey ──────────────────────────────

TEST_GROUP(MultiWordKeyArrayTest) {};

TEST(MultiWordKeyArrayTest, insertAndSearchMultiWord) {
	// Default: 2 words per key, element = 2 * uint32
	OrderedResizeableArrayWithMultiWordKey arr;

	uint32_t keys[][2] = {{5, 100}, {3, 200}, {5, 50}, {1, 300}};

	for (int i = 0; i < 4; i++) {
		int32_t idx = arr.insertAtKeyMultiWord(keys[i]);
		CHECK(idx >= 0);
	}

	CHECK_EQUAL(4, arr.getNumElements());

	// Search for {5, 100}
	uint32_t search1[] = {5, 100};
	int32_t idx = arr.searchMultiWordExact(search1);
	CHECK(idx >= 0);

	// Search for non-existent
	uint32_t search2[] = {5, 75};
	idx = arr.searchMultiWordExact(search2);
	CHECK(idx < 0);
}

TEST(MultiWordKeyArrayTest, deleteMultiWord) {
	OrderedResizeableArrayWithMultiWordKey arr;

	uint32_t keys[][2] = {{1, 10}, {2, 20}, {3, 30}};
	for (int i = 0; i < 3; i++) {
		arr.insertAtKeyMultiWord(keys[i]);
	}

	uint32_t toDelete[] = {2, 20};
	bool deleted = arr.deleteAtKeyMultiWord(toDelete);
	CHECK(deleted);
	CHECK_EQUAL(2, arr.getNumElements());
}
