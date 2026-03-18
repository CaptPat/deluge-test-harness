// Container stress and edge-case tests — exercises ResizeableArray, OrderedResizeableArray,
// NoteVector, CStringArray, and BidirectionalLinkedList under heavy load and boundary conditions.

#include "CppUTest/TestHarness.h"
#include "model/note/note.h"
#include "model/note/note_vector.h"
#include "util/container/array/c_string_array.h"
#include "util/container/array/ordered_resizeable_array.h"
#include "util/container/array/resizeable_array.h"
#include "util/container/list/bidirectional_linked_list.h"
#include "util/functions.h"
#include <algorithm>
#include <cstring>

// ════════════════════════════════════════════════════════════════════════════════
// ResizeableArray stress tests
// ════════════════════════════════════════════════════════════════════════════════

TEST_GROUP(ResizeableArrayStress) {
	ResizeableArray arr{static_cast<int32_t>(sizeof(int32_t)), 16, 15};

	void teardown() override { arr.empty(); }

	void setElement(int32_t index, int32_t value) { *(int32_t*)arr.getElementAddress(index) = value; }

	int32_t getElement(int32_t index) { return *(int32_t*)arr.getElementAddress(index); }
};

TEST(ResizeableArrayStress, insert1000ElementsSequentially) {
	for (int32_t i = 0; i < 1000; i++) {
		Error err = arr.insertAtIndex(i);
		CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
		setElement(i, i);
	}
	LONGS_EQUAL(1000, arr.getNumElements());
	for (int32_t i = 0; i < 1000; i++) {
		LONGS_EQUAL(i, getElement(i));
	}
}

TEST(ResizeableArrayStress, insert1000ElementsAtFront) {
	// Inserting at front every time forces repeated rightward shifts
	for (int32_t i = 0; i < 1000; i++) {
		Error err = arr.insertAtIndex(0);
		CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
		setElement(0, 999 - i);
	}
	LONGS_EQUAL(1000, arr.getNumElements());
	for (int32_t i = 0; i < 1000; i++) {
		LONGS_EQUAL(i, getElement(i));
	}
}

TEST(ResizeableArrayStress, insertAtEveryPosition) {
	// Build a 100-element array, then insert at every position from 0..100
	for (int32_t i = 0; i < 100; i++) {
		Error err = arr.insertAtIndex(i);
		CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
		setElement(i, i * 2); // Even numbers: 0,2,4,...,198
	}

	// Insert at each position, verify data integrity after each
	for (int32_t pos = 0; pos <= 100; pos++) {
		ResizeableArray test{static_cast<int32_t>(sizeof(int32_t)), 16, 15};
		// Copy arr manually
		for (int32_t i = 0; i < 100; i++) {
			test.insertAtIndex(i);
			*(int32_t*)test.getElementAddress(i) = i * 2;
		}

		Error err = test.insertAtIndex(pos);
		CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
		*(int32_t*)test.getElementAddress(pos) = -1;
		LONGS_EQUAL(101, test.getNumElements());

		// Verify elements before insertion
		for (int32_t i = 0; i < pos; i++) {
			LONGS_EQUAL(i * 2, *(int32_t*)test.getElementAddress(i));
		}
		LONGS_EQUAL(-1, *(int32_t*)test.getElementAddress(pos));
		// Verify elements after insertion
		for (int32_t i = pos + 1; i < 101; i++) {
			LONGS_EQUAL((i - 1) * 2, *(int32_t*)test.getElementAddress(i));
		}
		test.empty();
	}
}

TEST(ResizeableArrayStress, deleteFromEveryPosition) {
	for (int32_t pos = 0; pos < 50; pos++) {
		ResizeableArray test{static_cast<int32_t>(sizeof(int32_t)), 16, 15};
		for (int32_t i = 0; i < 50; i++) {
			test.insertAtIndex(i);
			*(int32_t*)test.getElementAddress(i) = i;
		}

		test.deleteAtIndex(pos);
		LONGS_EQUAL(49, test.getNumElements());

		int32_t expected = 0;
		for (int32_t i = 0; i < 49; i++) {
			if (expected == pos) {
				expected++;
			}
			LONGS_EQUAL(expected, *(int32_t*)test.getElementAddress(i));
			expected++;
		}
		test.empty();
	}
}

TEST(ResizeableArrayStress, bulkInsertDelete1000Cycles) {
	// Simulate a FIFO queue pattern: insert at end, delete from front, 1000 cycles
	// This forces the circular buffer to wrap many times
	const int32_t windowSize = 32;
	for (int32_t i = 0; i < windowSize; i++) {
		arr.insertAtIndex(arr.getNumElements());
		setElement(arr.getNumElements() - 1, i);
	}

	for (int32_t cycle = 0; cycle < 1000; cycle++) {
		arr.deleteAtIndex(0);
		arr.insertAtIndex(arr.getNumElements());
		setElement(arr.getNumElements() - 1, windowSize + cycle);
	}

	LONGS_EQUAL(windowSize, arr.getNumElements());
	for (int32_t i = 0; i < windowSize; i++) {
		LONGS_EQUAL(1000 + i, getElement(i));
	}
}

TEST(ResizeableArrayStress, bulkInsertDeleteMiddle500Cycles) {
	// Insert 50 elements, then cycle: insert in middle, delete from middle
	for (int32_t i = 0; i < 50; i++) {
		arr.insertAtIndex(i);
		setElement(i, i);
	}

	for (int32_t cycle = 0; cycle < 500; cycle++) {
		int32_t mid = arr.getNumElements() / 2;
		arr.deleteAtIndex(mid);
		Error err = arr.insertAtIndex(mid);
		CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
		setElement(mid, 1000 + cycle);
	}

	LONGS_EQUAL(50, arr.getNumElements());
}

TEST(ResizeableArrayStress, deleteAllOneByOneFromFront) {
	for (int32_t i = 0; i < 200; i++) {
		arr.insertAtIndex(i);
		setElement(i, i);
	}
	for (int32_t i = 0; i < 200; i++) {
		LONGS_EQUAL(i, getElement(0));
		arr.deleteAtIndex(0);
	}
	LONGS_EQUAL(0, arr.getNumElements());
}

TEST(ResizeableArrayStress, deleteAllOneByOneFromEnd) {
	for (int32_t i = 0; i < 200; i++) {
		arr.insertAtIndex(i);
		setElement(i, i);
	}
	for (int32_t i = 199; i >= 0; i--) {
		LONGS_EQUAL(i, getElement(arr.getNumElements() - 1));
		arr.deleteAtIndex(arr.getNumElements() - 1);
	}
	LONGS_EQUAL(0, arr.getNumElements());
}

TEST(ResizeableArrayStress, moveElementsLeftStress) {
	// Insert 100 elements, then shift a range left
	for (int32_t i = 0; i < 100; i++) {
		arr.insertAtIndex(i);
		setElement(i, i);
	}
	// moveElementsLeft(oldStart, oldStop, distance) moves elements [oldStart..oldStop) left by distance
	arr.moveElementsLeft(10, 20, 5);
	// Elements at indices 10..19 should now be at 5..14
	for (int32_t i = 5; i < 15; i++) {
		LONGS_EQUAL(i + 5, getElement(i));
	}
}

TEST(ResizeableArrayStress, moveElementsRightStress) {
	for (int32_t i = 0; i < 100; i++) {
		arr.insertAtIndex(i);
		setElement(i, i);
	}
	arr.moveElementsRight(10, 20, 5);
	// Elements at indices 10..19 should now be at 15..24
	for (int32_t i = 15; i < 25; i++) {
		LONGS_EQUAL(i - 5, getElement(i));
	}
}

TEST(ResizeableArrayStress, repositionElementAcrossEntireArray) {
	for (int32_t i = 0; i < 50; i++) {
		arr.insertAtIndex(i);
		setElement(i, i);
	}
	// Move first element to last position
	arr.repositionElement(0, 49);
	LONGS_EQUAL(0, getElement(49));
	LONGS_EQUAL(1, getElement(0));

	// Move last element to first position
	arr.repositionElement(49, 0);
	LONGS_EQUAL(0, getElement(0));
	LONGS_EQUAL(1, getElement(1));
}

TEST(ResizeableArrayStress, swapElementsExhaustive) {
	for (int32_t i = 0; i < 20; i++) {
		arr.insertAtIndex(i);
		setElement(i, i);
	}
	// Swap every pair of adjacent elements
	for (int32_t i = 0; i < 19; i++) {
		int32_t a = getElement(i);
		int32_t b = getElement(i + 1);
		arr.swapElements(i, i + 1);
		LONGS_EQUAL(b, getElement(i));
		LONGS_EQUAL(a, getElement(i + 1));
		arr.swapElements(i, i + 1); // swap back
	}
	// Array should be unchanged
	for (int32_t i = 0; i < 20; i++) {
		LONGS_EQUAL(i, getElement(i));
	}
}

TEST(ResizeableArrayStress, emptyReuseMultipleTimes) {
	for (int32_t round = 0; round < 10; round++) {
		int32_t count = 50 + round * 10;
		for (int32_t i = 0; i < count; i++) {
			arr.insertAtIndex(i);
			setElement(i, round * 1000 + i);
		}
		LONGS_EQUAL(count, arr.getNumElements());
		for (int32_t i = 0; i < count; i++) {
			LONGS_EQUAL(round * 1000 + i, getElement(i));
		}
		arr.empty();
		LONGS_EQUAL(0, arr.getNumElements());
	}
}

TEST(ResizeableArrayStress, cloneFromLargeArray) {
	for (int32_t i = 0; i < 500; i++) {
		arr.insertAtIndex(i);
		setElement(i, i);
	}
	ResizeableArray clone{static_cast<int32_t>(sizeof(int32_t))};
	bool ok = clone.cloneFrom(&arr);
	CHECK(ok);
	LONGS_EQUAL(500, clone.getNumElements());
	for (int32_t i = 0; i < 500; i++) {
		LONGS_EQUAL(i, *(int32_t*)clone.getElementAddress(i));
	}
	clone.empty();
}

TEST(ResizeableArrayStress, insertMultipleElementsBulk) {
	// Insert 100 elements at once at index 0
	Error err = arr.insertAtIndex(0, 100);
	CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
	LONGS_EQUAL(100, arr.getNumElements());

	// Fill them
	for (int32_t i = 0; i < 100; i++) {
		setElement(i, i);
	}

	// Insert another 100 in the middle
	err = arr.insertAtIndex(50, 100);
	CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
	LONGS_EQUAL(200, arr.getNumElements());

	// Original first 50 should be intact
	for (int32_t i = 0; i < 50; i++) {
		LONGS_EQUAL(i, getElement(i));
	}
	// Original last 50 should be shifted
	for (int32_t i = 150; i < 200; i++) {
		LONGS_EQUAL(i - 100, getElement(i));
	}
}

TEST(ResizeableArrayStress, deleteMultipleElementsBulk) {
	for (int32_t i = 0; i < 200; i++) {
		arr.insertAtIndex(i);
		setElement(i, i);
	}

	// Delete 100 from the middle
	arr.deleteAtIndex(50, 100);
	LONGS_EQUAL(100, arr.getNumElements());

	for (int32_t i = 0; i < 50; i++) {
		LONGS_EQUAL(i, getElement(i));
	}
	for (int32_t i = 50; i < 100; i++) {
		LONGS_EQUAL(i + 100, getElement(i));
	}
}

TEST(ResizeableArrayStress, singleElementInsertDeleteCycle) {
	// Repeatedly insert and delete a single element 500 times
	for (int32_t i = 0; i < 500; i++) {
		Error err = arr.insertAtIndex(0);
		CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
		setElement(0, i);
		LONGS_EQUAL(1, arr.getNumElements());
		LONGS_EQUAL(i, getElement(0));
		arr.deleteAtIndex(0);
		LONGS_EQUAL(0, arr.getNumElements());
	}
}

TEST(ResizeableArrayStress, ensureSpaceThenMassInsert) {
	bool ok = arr.ensureEnoughSpaceAllocated(2000);
	CHECK(ok);
	for (int32_t i = 0; i < 2000; i++) {
		Error err = arr.insertAtIndex(i);
		CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
		setElement(i, i);
	}
	LONGS_EQUAL(2000, arr.getNumElements());
	LONGS_EQUAL(0, getElement(0));
	LONGS_EQUAL(999, getElement(999));
	LONGS_EQUAL(1999, getElement(1999));
}

// ════════════════════════════════════════════════════════════════════════════════
// OrderedResizeableArray stress tests
// ════════════════════════════════════════════════════════════════════════════════

TEST_GROUP(OrderedArrayStress) {
	OrderedResizeableArrayWith32bitKey arr{static_cast<int32_t>(sizeof(int32_t))};

	void teardown() override { arr.empty(); }
};

TEST(OrderedArrayStress, searchEmptyArray) {
	LONGS_EQUAL(0, arr.search(0, GREATER_OR_EQUAL));
	LONGS_EQUAL(-1, arr.search(0, LESS));
	LONGS_EQUAL(-1, arr.searchExact(0));
}

TEST(OrderedArrayStress, searchSingleElement) {
	arr.insertAtKey(42);
	LONGS_EQUAL(0, arr.search(42, GREATER_OR_EQUAL));
	LONGS_EQUAL(0, arr.searchExact(42));
	LONGS_EQUAL(-1, arr.search(42, LESS));
	LONGS_EQUAL(0, arr.search(0, GREATER_OR_EQUAL));
	LONGS_EQUAL(1, arr.search(100, GREATER_OR_EQUAL));
	LONGS_EQUAL(-1, arr.searchExact(41));
	LONGS_EQUAL(-1, arr.searchExact(43));
}

TEST(OrderedArrayStress, insert1000RandomOrderMaintainsSorted) {
	// Use a deterministic pseudo-random sequence
	int32_t seed = 12345;
	for (int32_t i = 0; i < 1000; i++) {
		seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
		int32_t key = seed % 100000;
		int32_t idx = arr.insertAtKey(key);
		CHECK(idx >= 0);
	}
	LONGS_EQUAL(1000, arr.getNumElements());

	// Verify sorted order
	for (int32_t i = 1; i < arr.getNumElements(); i++) {
		CHECK(arr.getKeyAtIndex(i) >= arr.getKeyAtIndex(i - 1));
	}
}

TEST(OrderedArrayStress, duplicateKeyHandling) {
	// Insert the same key 100 times
	for (int32_t i = 0; i < 100; i++) {
		int32_t idx = arr.insertAtKey(42);
		CHECK(idx >= 0);
	}
	LONGS_EQUAL(100, arr.getNumElements());

	// All keys should be 42
	for (int32_t i = 0; i < 100; i++) {
		LONGS_EQUAL(42, arr.getKeyAtIndex(i));
	}

	// searchExact should find one of them
	int32_t idx = arr.searchExact(42);
	CHECK(idx >= 0);
	CHECK(idx < 100);
}

TEST(OrderedArrayStress, duplicateKeyDeleteOne) {
	for (int32_t i = 0; i < 5; i++) {
		arr.insertAtKey(10);
	}
	arr.deleteAtKey(10);
	LONGS_EQUAL(4, arr.getNumElements());
	for (int32_t i = 0; i < 4; i++) {
		LONGS_EQUAL(10, arr.getKeyAtIndex(i));
	}
}

TEST(OrderedArrayStress, duplicateKeyDeleteAll) {
	for (int32_t i = 0; i < 5; i++) {
		arr.insertAtKey(10);
	}
	for (int32_t i = 0; i < 5; i++) {
		arr.deleteAtKey(10);
	}
	LONGS_EQUAL(0, arr.getNumElements());
}

TEST(OrderedArrayStress, searchExactAfterRandomInsertDelete) {
	// Insert 200 keys with known values, delete half, verify remaining searchExact
	for (int32_t i = 0; i < 200; i++) {
		arr.insertAtKey(i * 3);
	}

	// Delete even-indexed keys (0, 6, 12, ...)
	for (int32_t i = 198 * 3; i >= 0; i -= 6) {
		arr.deleteAtKey(i);
	}

	// Remaining should be odd-indexed: 3, 9, 15, ...
	for (int32_t i = 0; i < 200; i++) {
		int32_t key = i * 3;
		int32_t result = arr.searchExact(key);
		if (i % 2 == 0) {
			LONGS_EQUAL(-1, result); // deleted
		}
		else {
			CHECK(result >= 0); // should exist
		}
	}
}

TEST(OrderedArrayStress, sortedOrderAfterReverseInsertion) {
	// Insert keys in strictly descending order
	for (int32_t i = 999; i >= 0; i--) {
		arr.insertAtKey(i);
	}
	LONGS_EQUAL(1000, arr.getNumElements());
	for (int32_t i = 0; i < 1000; i++) {
		LONGS_EQUAL(i, arr.getKeyAtIndex(i));
	}
}

TEST(OrderedArrayStress, searchWithRangeEdgeCases) {
	for (int32_t i = 0; i < 100; i++) {
		arr.insertAtKey(i * 10);
	}

	// Search in zero-width range
	LONGS_EQUAL(50, arr.search(500, GREATER_OR_EQUAL, 50, 50));

	// Search in single-element range
	LONGS_EQUAL(50, arr.search(500, GREATER_OR_EQUAL, 50, 51));
	LONGS_EQUAL(51, arr.search(505, GREATER_OR_EQUAL, 50, 51));

	// Search at beginning of range
	LONGS_EQUAL(0, arr.search(0, GREATER_OR_EQUAL, 0, 100));

	// Search at end of range
	LONGS_EQUAL(100, arr.search(9999, GREATER_OR_EQUAL, 0, 100));
}

TEST(OrderedArrayStress, negativeKeysLargeScale) {
	for (int32_t i = -500; i < 500; i++) {
		arr.insertAtKey(i);
	}
	LONGS_EQUAL(1000, arr.getNumElements());
	for (int32_t i = 0; i < 1000; i++) {
		LONGS_EQUAL(i - 500, arr.getKeyAtIndex(i));
	}
}

TEST(OrderedArrayStress, interleavedInsertSearchDelete) {
	// Simulate a real workload: insert, search, delete in interleaved pattern
	for (int32_t phase = 0; phase < 10; phase++) {
		// Insert 50 keys
		for (int32_t i = 0; i < 50; i++) {
			arr.insertAtKey(phase * 1000 + i * 7);
		}

		// Search for all inserted keys
		for (int32_t i = 0; i < 50; i++) {
			int32_t key = phase * 1000 + i * 7;
			int32_t idx = arr.searchExact(key);
			CHECK(idx >= 0);
		}

		// Delete half
		for (int32_t i = 0; i < 50; i += 2) {
			arr.deleteAtKey(phase * 1000 + i * 7);
		}
	}

	// Verify sorted
	for (int32_t i = 1; i < arr.getNumElements(); i++) {
		CHECK(arr.getKeyAtIndex(i) >= arr.getKeyAtIndex(i - 1));
	}
}

TEST(OrderedArrayStress, generateRepeatsLargePattern) {
	// 16 notes in a bar (step sequencer style), repeat 8 times
	for (int32_t i = 0; i < 16; i++) {
		arr.insertAtKey(i * 24); // 24 ticks per step
	}

	int32_t barLength = 16 * 24; // 384
	bool ok = arr.generateRepeats(barLength, barLength * 8);
	CHECK(ok);

	LONGS_EQUAL(128, arr.getNumElements()); // 16 * 8

	// Verify pattern repeats
	for (int32_t rep = 0; rep < 8; rep++) {
		for (int32_t note = 0; note < 16; note++) {
			int32_t idx = rep * 16 + note;
			LONGS_EQUAL(rep * barLength + note * 24, arr.getKeyAtIndex(idx));
		}
	}
}

TEST(OrderedArrayStress, searchDualEdgeCases) {
	for (int32_t i = 0; i < 100; i++) {
		arr.insertAtKey(i * 10);
	}

	// Both terms before all keys
	int32_t terms1[2] = {-100, -50};
	int32_t results1[2];
	arr.searchDual(terms1, results1);
	LONGS_EQUAL(0, results1[0]);
	LONGS_EQUAL(0, results1[1]);

	// Both terms after all keys
	int32_t terms2[2] = {9999, 10000};
	int32_t results2[2];
	arr.searchDual(terms2, results2);
	LONGS_EQUAL(100, results2[0]);
	LONGS_EQUAL(100, results2[1]);

	// First and last key
	int32_t terms3[2] = {0, 990};
	int32_t results3[2];
	arr.searchDual(terms3, results3);
	LONGS_EQUAL(0, results3[0]);
	LONGS_EQUAL(99, results3[1]);
}

TEST(OrderedArrayStress, searchMultipleStress) {
	for (int32_t i = 0; i < 500; i++) {
		arr.insertAtKey(i * 10);
	}

	// Search for 10 terms at various positions
	int32_t terms[10] = {0, 50, 100, 250, 500, 1000, 2500, 4000, 4990, 5000};
	arr.searchMultiple(terms, 10);
	LONGS_EQUAL(0, terms[0]);    // key 0 at index 0
	LONGS_EQUAL(5, terms[1]);    // key 50 at index 5
	LONGS_EQUAL(10, terms[2]);   // key 100 at index 10
	LONGS_EQUAL(25, terms[3]);   // key 250 at index 25
	LONGS_EQUAL(50, terms[4]);   // key 500 at index 50
	LONGS_EQUAL(100, terms[5]);  // key 1000 at index 100
	LONGS_EQUAL(250, terms[6]);  // key 2500 at index 250
	LONGS_EQUAL(400, terms[7]);  // key 4000 at index 400
	LONGS_EQUAL(499, terms[8]);  // key 4990 at index 499
	LONGS_EQUAL(500, terms[9]);  // key 5000 past end
}

// ════════════════════════════════════════════════════════════════════════════════
// NoteVector stress tests — mimicking real sequencer patterns
// ════════════════════════════════════════════════════════════════════════════════

TEST_GROUP(NoteVectorStress){};

TEST(NoteVectorStress, fill128NotesAcross4Bars) {
	NoteVector vec;

	// 4 bars of 4/4 at 48 ticks per quarter note = 4 * 4 * 48 = 768 ticks
	// 128 evenly spaced notes = every 6 ticks
	for (int32_t i = 0; i < 128; i++) {
		int32_t pos = i * 6;
		int32_t idx = vec.insertAtKey(pos);
		CHECK(idx >= 0);
		Note* note = (Note*)vec.getElementAddress(idx);
		note->setLength(3);
		note->setVelocity(64 + (i % 64)); // varying velocity
	}

	CHECK_EQUAL(128, vec.getNumElements());

	// Verify sorted order
	for (int32_t i = 0; i < 127; i++) {
		CHECK(vec.getElement(i)->pos < vec.getElement(i + 1)->pos);
	}

	// Verify velocities
	for (int32_t i = 0; i < 128; i++) {
		Note* note = vec.getElement(i);
		CHECK_EQUAL(i * 6, note->pos);
		CHECK_EQUAL(3, note->getLength());
		CHECK_EQUAL(64 + (i % 64), note->getVelocity());
	}
}

TEST(NoteVectorStress, deleteEveryOtherNote) {
	NoteVector vec;

	for (int32_t i = 0; i < 64; i++) {
		int32_t idx = vec.insertAtKey(i * 24);
		Note* note = (Note*)vec.getElementAddress(idx);
		note->setLength(12);
		note->setVelocity(100);
	}

	// Delete every other note (simulate thinning)
	for (int32_t i = 62; i >= 0; i -= 2) {
		vec.deleteAtKey(i * 24);
	}

	CHECK_EQUAL(32, vec.getNumElements());

	// Remaining should be odd-indexed positions: 24, 72, 120, ...
	for (int32_t i = 0; i < 32; i++) {
		CHECK_EQUAL((2 * i + 1) * 24, vec.getElement(i)->pos);
	}
}

TEST(NoteVectorStress, searchBoundaryConditions) {
	NoteVector vec;

	// Insert notes at positions 0, 100, 200, ..., 900
	for (int32_t i = 0; i < 10; i++) {
		int32_t idx = vec.insertAtKey(i * 100);
		Note* note = (Note*)vec.getElementAddress(idx);
		note->setLength(50);
		note->setVelocity(80);
	}

	// Search for exact positions
	for (int32_t i = 0; i < 10; i++) {
		int32_t idx = vec.searchExact(i * 100);
		CHECK(idx >= 0);
		CHECK_EQUAL(i * 100, vec.getElement(idx)->pos);
	}

	// Search for positions between notes
	CHECK(vec.searchExact(50) < 0);
	CHECK(vec.searchExact(150) < 0);
	CHECK(vec.searchExact(999) < 0);
	CHECK(vec.searchExact(-1) < 0);

	// Search GREATER_OR_EQUAL boundary
	LONGS_EQUAL(0, vec.search(0, GREATER_OR_EQUAL));
	LONGS_EQUAL(1, vec.search(1, GREATER_OR_EQUAL));     // between 0 and 100
	LONGS_EQUAL(1, vec.search(99, GREATER_OR_EQUAL));    // just before 100
	LONGS_EQUAL(1, vec.search(100, GREATER_OR_EQUAL));   // exact match
	LONGS_EQUAL(10, vec.search(901, GREATER_OR_EQUAL));  // past end
	LONGS_EQUAL(10, vec.search(1000, GREATER_OR_EQUAL)); // past end
}

TEST(NoteVectorStress, insertReverseOrder) {
	NoteVector vec;

	// Insert notes in reverse position order — forces maximum shifting
	for (int32_t i = 255; i >= 0; i--) {
		int32_t idx = vec.insertAtKey(i * 12);
		CHECK(idx >= 0);
		Note* note = (Note*)vec.getElementAddress(idx);
		note->setLength(6);
		note->setVelocity(i % 128);
	}

	CHECK_EQUAL(256, vec.getNumElements());

	// Verify sorted
	for (int32_t i = 0; i < 256; i++) {
		CHECK_EQUAL(i * 12, vec.getElement(i)->pos);
	}
}

TEST(NoteVectorStress, bulkInsertDeleteMimicSequencer) {
	NoteVector vec;

	// Phase 1: Record 32 notes
	for (int32_t i = 0; i < 32; i++) {
		int32_t idx = vec.insertAtKey(i * 48);
		Note* note = (Note*)vec.getElementAddress(idx);
		note->setLength(24);
		note->setVelocity(80 + (i % 48));
	}

	// Phase 2: Delete a range (clear a section, like erasing a bar)
	// Delete notes at positions 192..383 (indices 4..7)
	for (int32_t pos = 7 * 48; pos >= 4 * 48; pos -= 48) {
		vec.deleteAtKey(pos);
	}
	CHECK_EQUAL(28, vec.getNumElements());

	// Phase 3: Re-record in the gap with different velocities
	for (int32_t i = 4; i < 8; i++) {
		int32_t idx = vec.insertAtKey(i * 48);
		Note* note = (Note*)vec.getElementAddress(idx);
		note->setLength(48);
		note->setVelocity(127);
	}
	CHECK_EQUAL(32, vec.getNumElements());

	// Verify re-recorded notes
	for (int32_t i = 4; i < 8; i++) {
		Note* note = vec.getElement(i);
		CHECK_EQUAL(i * 48, note->pos);
		CHECK_EQUAL(127, note->getVelocity());
		CHECK_EQUAL(48, note->getLength());
	}
}

TEST(NoteVectorStress, getLastOnLargeVector) {
	NoteVector vec;
	for (int32_t i = 0; i < 500; i++) {
		int32_t idx = vec.insertAtKey(i * 10);
		Note* note = (Note*)vec.getElementAddress(idx);
		note->setLength(5);
		note->setVelocity(100);
	}
	Note* last = vec.getLast();
	CHECK(last != nullptr);
	CHECK_EQUAL(499 * 10, last->pos);
}

TEST(NoteVectorStress, noteFieldsPreservedUnderGrowth) {
	NoteVector vec;

	// Insert notes with distinct fields, forcing multiple reallocations
	for (int32_t i = 0; i < 200; i++) {
		int32_t idx = vec.insertAtKey(i * 24);
		Note* note = (Note*)vec.getElementAddress(idx);
		note->setLength(i + 1);
		note->setVelocity(i % 128);
		note->setProbability(i % 21);
		note->setLift(i % 128);
	}

	// Verify all fields survived reallocations
	for (int32_t i = 0; i < 200; i++) {
		Note* note = vec.getElement(i);
		CHECK_EQUAL(i * 24, note->pos);
		CHECK_EQUAL(i + 1, note->getLength());
		CHECK_EQUAL(i % 128, note->getVelocity());
		CHECK_EQUAL(i % 21, note->getProbability());
		CHECK_EQUAL(i % 128, note->getLift());
	}
}

TEST(NoteVectorStress, duplicatePositionHandling) {
	NoteVector vec;

	// Insert multiple notes at the same position (e.g., chord in drum mode)
	for (int32_t i = 0; i < 10; i++) {
		int32_t idx = vec.insertAtKey(0);
		CHECK(idx >= 0);
		Note* note = (Note*)vec.getElementAddress(idx);
		note->setVelocity(i * 10);
		note->setLength(24);
	}

	CHECK_EQUAL(10, vec.getNumElements());
	// All should have position 0
	for (int32_t i = 0; i < 10; i++) {
		CHECK_EQUAL(0, vec.getElement(i)->pos);
	}
}

TEST(NoteVectorStress, emptyVectorSearchExact) {
	NoteVector vec;
	CHECK(vec.searchExact(0) < 0);
	CHECK(vec.searchExact(100) < 0);
	CHECK(vec.searchExact(-1) < 0);
}

TEST(NoteVectorStress, singleNoteVector) {
	NoteVector vec;
	int32_t idx = vec.insertAtKey(100);
	Note* note = (Note*)vec.getElementAddress(idx);
	note->setLength(50);
	note->setVelocity(127);

	CHECK_EQUAL(1, vec.getNumElements());
	CHECK_EQUAL(0, vec.searchExact(100));
	CHECK(vec.searchExact(99) < 0);
	CHECK(vec.searchExact(101) < 0);

	Note* last = vec.getLast();
	CHECK(last != nullptr);
	CHECK_EQUAL(100, last->pos);
}

// ════════════════════════════════════════════════════════════════════════════════
// CStringArray stress tests
// ════════════════════════════════════════════════════════════════════════════════

TEST_GROUP(CStringArrayStress) {
	CStringArray arr{static_cast<int32_t>(sizeof(char*))};

	void setup() override {
		shouldInterpretNoteNames = false;
		octaveStartsFromA = false;
	}

	void teardown() override { arr.empty(); }

	void insertString(const char* str) {
		int32_t idx = arr.getNumElements();
		Error err = arr.insertAtIndex(idx);
		CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
		char const** slot = (char const**)arr.getElementAddress(idx);
		*slot = str;
	}
};

TEST(CStringArrayStress, sortLargeArray) {
	// 50 filenames with natural numeric ordering
	static const char* files[] = {
	    "SYNTH050", "SYNTH001", "SYNTH025", "SYNTH010", "SYNTH100", "SYNTH002", "SYNTH099", "SYNTH003",
	    "SYNTH030", "SYNTH004", "SYNTH040", "SYNTH005", "SYNTH060", "SYNTH006", "SYNTH070", "SYNTH007",
	    "SYNTH080", "SYNTH008", "SYNTH090", "SYNTH009", "SYNTH011", "SYNTH012", "SYNTH013", "SYNTH014",
	    "SYNTH015", "SYNTH016", "SYNTH017", "SYNTH018", "SYNTH019", "SYNTH020", "SYNTH021", "SYNTH022",
	    "SYNTH023", "SYNTH024", "SYNTH026", "SYNTH027", "SYNTH028", "SYNTH029", "SYNTH031", "SYNTH032",
	    "SYNTH033", "SYNTH034", "SYNTH035", "SYNTH036", "SYNTH037", "SYNTH038", "SYNTH039", "SYNTH041",
	    "SYNTH042", "SYNTH043",
	};

	for (auto* f : files) {
		insertString(f);
	}
	arr.sortForStrings();

	// Verify sorted in natural numeric order
	for (int32_t i = 1; i < arr.getNumElements(); i++) {
		const char* prev = *(char const**)arr.getElementAddress(i - 1);
		const char* curr = *(char const**)arr.getElementAddress(i);
		CHECK(strcmpspecial(prev, curr) <= 0);
	}
}

TEST(CStringArrayStress, sortAlreadySorted) {
	static const char* sorted[] = {"a", "b", "c", "d", "e", "f", "g", "h"};
	for (auto* s : sorted) {
		insertString(s);
	}
	arr.sortForStrings();

	for (int32_t i = 0; i < 8; i++) {
		STRCMP_EQUAL(sorted[i], *(char const**)arr.getElementAddress(i));
	}
}

TEST(CStringArrayStress, sortReverseSorted) {
	static const char* reversed[] = {"h", "g", "f", "e", "d", "c", "b", "a"};
	for (auto* s : reversed) {
		insertString(s);
	}
	arr.sortForStrings();

	for (int32_t i = 0; i < 8; i++) {
		char expected[2] = {(char)('a' + i), '\0'};
		STRCMP_EQUAL(expected, *(char const**)arr.getElementAddress(i));
	}
}

TEST(CStringArrayStress, sortAllIdentical) {
	for (int32_t i = 0; i < 20; i++) {
		insertString("same");
	}
	arr.sortForStrings();

	CHECK_EQUAL(20, arr.getNumElements());
	for (int32_t i = 0; i < 20; i++) {
		STRCMP_EQUAL("same", *(char const**)arr.getElementAddress(i));
	}
}

TEST(CStringArrayStress, longFilenames) {
	static const char* longNames[] = {
	    "ThisIsAVeryLongSynthPatchNameThatMightOverflowSomeBuffers_v1",
	    "ThisIsAVeryLongSynthPatchNameThatMightOverflowSomeBuffers_v2",
	    "ThisIsAVeryLongSynthPatchNameThatMightOverflowSomeBuffers_v10",
	    "ThisIsAVeryLongSynthPatchNameThatMightOverflowSomeBuffers_v3",
	    "Short",
	    "A",
	};

	for (auto* n : longNames) {
		insertString(n);
	}
	arr.sortForStrings();

	// Verify sorted
	for (int32_t i = 1; i < arr.getNumElements(); i++) {
		const char* prev = *(char const**)arr.getElementAddress(i - 1);
		const char* curr = *(char const**)arr.getElementAddress(i);
		CHECK(strcmpspecial(prev, curr) <= 0);
	}
}

TEST(CStringArrayStress, specialCharacterFilenames) {
	static const char* specials[] = {
	    ".hidden",     "_underscore", "123numeric", "UPPER",    "lower",
	    "MiXeD_CaSe", "with spaces", "with-dash",  "with.dot", "file(1)",
	};

	for (auto* s : specials) {
		insertString(s);
	}
	arr.sortForStrings();

	// Just verify sorted invariant holds
	for (int32_t i = 1; i < arr.getNumElements(); i++) {
		const char* prev = *(char const**)arr.getElementAddress(i - 1);
		const char* curr = *(char const**)arr.getElementAddress(i);
		CHECK(strcmpspecial(prev, curr) <= 0);
	}
}

TEST(CStringArrayStress, searchAfterSortLargeSet) {
	// Insert 30 patch names, sort, then search for each
	static const char* patches[] = {
	    "Bass001",     "Bass002",     "Bass010",     "Lead001",     "Lead002",
	    "Lead010",     "Pad001",      "Pad002",      "Pad010",      "Keys001",
	    "Keys002",     "Keys010",     "FX001",       "FX002",       "FX010",
	    "Drum001",     "Drum002",     "Drum010",     "Vocal001",    "Vocal002",
	    "Strings001",  "Strings002",  "Brass001",    "Brass002",    "Wind001",
	    "Wind002",     "Pluck001",    "Pluck002",    "Ambient001",  "Ambient002",
	};

	for (auto* p : patches) {
		insertString(p);
	}
	arr.sortForStrings();

	// All should be findable
	for (auto* p : patches) {
		bool found = false;
		arr.search(p, &found);
		CHECK(found);
	}

	// Non-existent should not be found
	bool found = false;
	arr.search("NotHere", &found);
	CHECK_FALSE(found);
}

TEST(CStringArrayStress, searchSingleElement) {
	insertString("only");
	bool found = false;
	int32_t idx = arr.search("only", &found);
	CHECK(found);
	CHECK_EQUAL(0, idx);

	found = false;
	arr.search("other", &found);
	CHECK_FALSE(found);
}

TEST(CStringArrayStress, sortSingleElement) {
	insertString("alone");
	arr.sortForStrings();
	CHECK_EQUAL(1, arr.getNumElements());
	STRCMP_EQUAL("alone", *(char const**)arr.getElementAddress(0));
}

TEST(CStringArrayStress, sortTwoElements) {
	insertString("beta");
	insertString("alpha");
	arr.sortForStrings();
	STRCMP_EQUAL("alpha", *(char const**)arr.getElementAddress(0));
	STRCMP_EQUAL("beta", *(char const**)arr.getElementAddress(1));
}

TEST(CStringArrayStress, sortEmptyArray) {
	arr.sortForStrings(); // Should not crash
	CHECK_EQUAL(0, arr.getNumElements());
}

// ════════════════════════════════════════════════════════════════════════════════
// BidirectionalLinkedList stress tests
// ════════════════════════════════════════════════════════════════════════════════

class StressNode : public BidirectionalLinkedListNode {
public:
	int value;
	StressNode(int v = 0) : value(v) {}
};

TEST_GROUP(LinkedListStress){};

TEST(LinkedListStress, add200NodesTraverseForward) {
	BidirectionalLinkedList list;
	StressNode nodes[200];
	for (int i = 0; i < 200; i++) {
		nodes[i].value = i;
		list.addToEnd(&nodes[i]);
	}
	CHECK_EQUAL(200, list.getNum());

	BidirectionalLinkedListNode* cur = list.getFirst();
	for (int i = 0; i < 200; i++) {
		CHECK(cur != nullptr);
		CHECK_EQUAL(i, static_cast<StressNode*>(cur)->value);
		cur = list.getNext(cur);
	}
	POINTERS_EQUAL(nullptr, cur);
}

TEST(LinkedListStress, removeAlternateNodesVerifyTraversal) {
	BidirectionalLinkedList list;
	StressNode nodes[100];
	for (int i = 0; i < 100; i++) {
		nodes[i].value = i;
		list.addToEnd(&nodes[i]);
	}

	// Remove even-indexed nodes
	for (int i = 0; i < 100; i += 2) {
		nodes[i].remove();
	}
	CHECK_EQUAL(50, list.getNum());

	// Verify only odd values remain
	BidirectionalLinkedListNode* cur = list.getFirst();
	for (int i = 1; i < 100; i += 2) {
		CHECK(cur != nullptr);
		CHECK_EQUAL(i, static_cast<StressNode*>(cur)->value);
		cur = list.getNext(cur);
	}
	POINTERS_EQUAL(nullptr, cur);
}

TEST(LinkedListStress, removeAllVerifyEmpty) {
	BidirectionalLinkedList list;
	StressNode nodes[50];
	for (int i = 0; i < 50; i++) {
		nodes[i].value = i;
		list.addToEnd(&nodes[i]);
	}

	for (int i = 0; i < 50; i++) {
		nodes[i].remove();
	}
	CHECK_EQUAL(0, list.getNum());
	POINTERS_EQUAL(nullptr, list.getFirst());
}

TEST(LinkedListStress, removeInReverseOrder) {
	BidirectionalLinkedList list;
	StressNode nodes[100];
	for (int i = 0; i < 100; i++) {
		nodes[i].value = i;
		list.addToEnd(&nodes[i]);
	}

	for (int i = 99; i >= 0; i--) {
		nodes[i].remove();
		CHECK_EQUAL(i, list.getNum());
	}
	POINTERS_EQUAL(nullptr, list.getFirst());
}

TEST(LinkedListStress, addRemoveAddPattern) {
	// Simulate add/remove cycling like voice allocation
	BidirectionalLinkedList list;
	StressNode nodes[50];

	// Add all
	for (int i = 0; i < 50; i++) {
		nodes[i].value = i;
		list.addToEnd(&nodes[i]);
	}

	// Remove first 25
	for (int i = 0; i < 25; i++) {
		nodes[i].remove();
	}
	CHECK_EQUAL(25, list.getNum());

	// Re-add first 25 at end
	for (int i = 0; i < 25; i++) {
		list.addToEnd(&nodes[i]);
	}
	CHECK_EQUAL(50, list.getNum());

	// Verify: 25..49 then 0..24
	BidirectionalLinkedListNode* cur = list.getFirst();
	for (int i = 25; i < 50; i++) {
		CHECK(cur != nullptr);
		CHECK_EQUAL(i, static_cast<StressNode*>(cur)->value);
		cur = list.getNext(cur);
	}
	for (int i = 0; i < 25; i++) {
		CHECK(cur != nullptr);
		CHECK_EQUAL(i, static_cast<StressNode*>(cur)->value);
		cur = list.getNext(cur);
	}
	POINTERS_EQUAL(nullptr, cur);
}

TEST(LinkedListStress, insertBeforeEveryNode) {
	BidirectionalLinkedList list;
	StressNode originals[10];
	StressNode inserted[10];

	for (int i = 0; i < 10; i++) {
		originals[i].value = i * 2; // even: 0,2,4,...,18
		list.addToEnd(&originals[i]);
	}

	// Insert a node before each original
	for (int i = 0; i < 10; i++) {
		inserted[i].value = i * 2 - 1; // odd: -1,1,3,...,17
		originals[i].insertOtherNodeBefore(&inserted[i]);
	}

	CHECK_EQUAL(20, list.getNum());

	// Verify interleaved order: -1,0,1,2,3,4,...
	BidirectionalLinkedListNode* cur = list.getFirst();
	for (int i = 0; i < 10; i++) {
		CHECK(cur != nullptr);
		CHECK_EQUAL(i * 2 - 1, static_cast<StressNode*>(cur)->value);
		cur = list.getNext(cur);
		CHECK(cur != nullptr);
		CHECK_EQUAL(i * 2, static_cast<StressNode*>(cur)->value);
		cur = list.getNext(cur);
	}
}

TEST(LinkedListStress, moveBetweenListsStress) {
	BidirectionalLinkedList list1, list2;
	StressNode nodes[20];

	for (int i = 0; i < 20; i++) {
		nodes[i].value = i;
		list1.addToEnd(&nodes[i]);
	}

	// Move 10 nodes from list1 to list2, one at a time
	for (int i = 0; i < 10; i++) {
		BidirectionalLinkedListNode* first = list1.getFirst();
		CHECK(first != nullptr);
		int val = static_cast<StressNode*>(first)->value;
		first->remove();
		list2.addToEnd(first);
		CHECK_EQUAL(val, static_cast<StressNode*>(first)->value);
	}

	CHECK_EQUAL(10, list1.getNum());
	CHECK_EQUAL(10, list2.getNum());

	// list2 should have 0..9
	BidirectionalLinkedListNode* cur = list2.getFirst();
	for (int i = 0; i < 10; i++) {
		CHECK(cur != nullptr);
		CHECK_EQUAL(i, static_cast<StressNode*>(cur)->value);
		cur = list2.getNext(cur);
	}

	// list1 should have 10..19
	cur = list1.getFirst();
	for (int i = 10; i < 20; i++) {
		CHECK(cur != nullptr);
		CHECK_EQUAL(i, static_cast<StressNode*>(cur)->value);
		cur = list1.getNext(cur);
	}
}

TEST(LinkedListStress, isLastDuringRemoval) {
	BidirectionalLinkedList list;
	StressNode nodes[5];
	for (int i = 0; i < 5; i++) {
		nodes[i].value = i;
		list.addToEnd(&nodes[i]);
	}

	// Only last node should report isLast
	for (int i = 0; i < 4; i++) {
		CHECK_FALSE(nodes[i].isLast());
	}
	CHECK_TRUE(nodes[4].isLast());

	// Remove last, now nodes[3] should be last
	nodes[4].remove();
	CHECK_TRUE(nodes[3].isLast());

	// Remove nodes[3], now nodes[2] is last
	nodes[3].remove();
	CHECK_TRUE(nodes[2].isLast());
}

TEST(LinkedListStress, destructorAutoRemovesLargeScale) {
	BidirectionalLinkedList list;
	StressNode permanent(42);
	list.addToEnd(&permanent);

	{
		StressNode temps[50];
		for (int i = 0; i < 50; i++) {
			temps[i].value = i;
			list.addToEnd(&temps[i]);
		}
		CHECK_EQUAL(51, list.getNum());
	} // All temps destroyed here

	CHECK_EQUAL(1, list.getNum());
	CHECK_EQUAL(42, static_cast<StressNode*>(list.getFirst())->value);
}

TEST(LinkedListStress, testMethodOnLargeList) {
	BidirectionalLinkedList list;
	StressNode nodes[100];
	for (int i = 0; i < 100; i++) {
		nodes[i].value = i;
		list.addToEnd(&nodes[i]);
	}
	list.test(); // Should not crash
}

TEST(LinkedListStress, singleNodeOperations) {
	BidirectionalLinkedList list;
	StressNode single(99);

	list.addToEnd(&single);
	CHECK_EQUAL(1, list.getNum());
	CHECK_TRUE(single.isLast());
	CHECK_EQUAL(99, static_cast<StressNode*>(list.getFirst())->value);
	POINTERS_EQUAL(nullptr, list.getNext(&single));

	single.remove();
	CHECK_EQUAL(0, list.getNum());
	POINTERS_EQUAL(nullptr, list.getFirst());
}

TEST(LinkedListStress, rapidAddRemoveCycles) {
	// Simulate rapid voice steal/allocate patterns
	BidirectionalLinkedList list;
	StressNode nodes[8];
	for (int i = 0; i < 8; i++) {
		nodes[i].value = i;
	}

	for (int cycle = 0; cycle < 100; cycle++) {
		// Add all
		for (int i = 0; i < 8; i++) {
			list.addToEnd(&nodes[i]);
		}
		CHECK_EQUAL(8, list.getNum());

		// Remove all from front
		for (int i = 0; i < 8; i++) {
			BidirectionalLinkedListNode* first = list.getFirst();
			CHECK(first != nullptr);
			first->remove();
		}
		CHECK_EQUAL(0, list.getNum());
	}
}

TEST(LinkedListStress, addToStartMultiple) {
	BidirectionalLinkedList list;
	StressNode sentinel(-1);
	list.addToEnd(&sentinel);

	StressNode nodes[20];
	for (int i = 0; i < 20; i++) {
		nodes[i].value = i;
		list.addToStart(&nodes[i]);
	}
	CHECK_EQUAL(21, list.getNum());

	// First should be sentinel (addToStart inserts after first)
	BidirectionalLinkedListNode* cur = list.getFirst();
	CHECK_EQUAL(-1, static_cast<StressNode*>(cur)->value);
}

TEST(LinkedListStress, emptyListOperations) {
	BidirectionalLinkedList list;
	CHECK_EQUAL(0, list.getNum());
	POINTERS_EQUAL(nullptr, list.getFirst());
	list.test(); // Should not crash on empty list
}
