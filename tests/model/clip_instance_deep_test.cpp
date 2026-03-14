// Deep tests for ClipInstance and ClipInstanceVector.

#include "CppUTest/TestHarness.h"
#include "model/clip/clip_instance.h"
#include "model/clip/clip_instance_vector.h"
#include "song_mock.h"

TEST_GROUP(ClipInstanceDeep) {
	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
	}
};

TEST(ClipInstanceDeep, constructionDefaults) {
	ClipInstance ci;
	// Positionable fields
	CHECK_EQUAL(0, ci.pos);
	CHECK_EQUAL(0, ci.length);
	POINTERS_EQUAL(nullptr, ci.clip);
}

TEST(ClipInstanceDeep, changeWithoutAction) {
	ClipInstance ci;
	ci.change(nullptr, nullptr, 100, 200, nullptr);
	CHECK_EQUAL(100, ci.pos);
	CHECK_EQUAL(200, ci.length);
	POINTERS_EQUAL(nullptr, ci.clip);
}

TEST(ClipInstanceDeep, changeMultipleTimes) {
	ClipInstance ci;
	ci.change(nullptr, nullptr, 10, 20, nullptr);
	CHECK_EQUAL(10, ci.pos);
	ci.change(nullptr, nullptr, 30, 40, nullptr);
	CHECK_EQUAL(30, ci.pos);
	CHECK_EQUAL(40, ci.length);
}

TEST(ClipInstanceDeep, getColourNoClip) {
	ClipInstance ci;
	ci.clip = nullptr;
	RGB colour = ci.getColour();
	// No clip → monochrome grey (128,128,128)
	CHECK_EQUAL(128, colour.r);
	CHECK_EQUAL(128, colour.g);
	CHECK_EQUAL(128, colour.b);
}

// ── ClipInstanceVector deep tests ────────────────────────────────────────

TEST_GROUP(ClipInstanceVectorDeep) {
	void setup() {
		currentSong = &testSong;
		preLoadedSong = nullptr;
	}
};

TEST(ClipInstanceVectorDeep, constructEmpty) {
	ClipInstanceVector vec;
	CHECK_EQUAL(0, vec.getNumElements());
}

TEST(ClipInstanceVectorDeep, getElementOutOfBoundsNegative) {
	ClipInstanceVector vec;
	POINTERS_EQUAL(nullptr, vec.getElement(-1));
}

TEST(ClipInstanceVectorDeep, getElementOutOfBoundsEmpty) {
	ClipInstanceVector vec;
	POINTERS_EQUAL(nullptr, vec.getElement(0));
}

TEST(ClipInstanceVectorDeep, getElementOutOfBoundsLarge) {
	ClipInstanceVector vec;
	POINTERS_EQUAL(nullptr, vec.getElement(1000));
}

TEST(ClipInstanceVectorDeep, insertAndRetrieve) {
	ClipInstanceVector vec;
	// insertAtKey inserts sorted by key (position)
	int32_t idx = vec.insertAtKey(100);
	CHECK(idx >= 0);
	CHECK_EQUAL(1, vec.getNumElements());

	ClipInstance* ci = vec.getElement(idx);
	CHECK(ci != nullptr);
}

TEST(ClipInstanceVectorDeep, insertMultipleSorted) {
	ClipInstanceVector vec;
	vec.insertAtKey(300);
	vec.insertAtKey(100);
	vec.insertAtKey(200);

	CHECK_EQUAL(3, vec.getNumElements());

	// Should be sorted by key (position)
	int32_t k0 = vec.getKeyAtIndex(0);
	int32_t k1 = vec.getKeyAtIndex(1);
	int32_t k2 = vec.getKeyAtIndex(2);
	CHECK(k0 <= k1);
	CHECK(k1 <= k2);
}

TEST(ClipInstanceVectorDeep, searchExact) {
	ClipInstanceVector vec;
	vec.insertAtKey(100);
	vec.insertAtKey(200);
	vec.insertAtKey(300);

	int32_t idx = vec.searchExact(200);
	CHECK(idx >= 0);
	CHECK_EQUAL(200, vec.getKeyAtIndex(idx));
}

TEST(ClipInstanceVectorDeep, searchExactNotFound) {
	ClipInstanceVector vec;
	vec.insertAtKey(100);
	vec.insertAtKey(300);

	int32_t idx = vec.searchExact(200);
	CHECK(idx < 0); // not found
}

TEST(ClipInstanceVectorDeep, deleteAtIndex) {
	ClipInstanceVector vec;
	vec.insertAtKey(100);
	vec.insertAtKey(200);
	vec.insertAtKey(300);

	vec.deleteAtIndex(1); // remove middle
	CHECK_EQUAL(2, vec.getNumElements());
	CHECK_EQUAL(100, vec.getKeyAtIndex(0));
	CHECK_EQUAL(300, vec.getKeyAtIndex(1));
}

TEST(ClipInstanceVectorDeep, insertAndDeleteStress) {
	ClipInstanceVector vec;
	// Insert 20 clip instances at various positions
	for (int32_t i = 0; i < 20; i++) {
		vec.insertAtKey(i * 100);
	}
	CHECK_EQUAL(20, vec.getNumElements());

	// Delete every other one
	for (int i = 9; i >= 0; i--) {
		vec.deleteAtIndex(i * 2);
	}
	CHECK_EQUAL(10, vec.getNumElements());

	// Remaining should be sorted
	for (int i = 0; i < 9; i++) {
		CHECK(vec.getKeyAtIndex(i) < vec.getKeyAtIndex(i + 1));
	}
}
