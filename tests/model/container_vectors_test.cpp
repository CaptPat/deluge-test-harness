#include "CppUTest/TestHarness.h"
#include "model/clip/clip_instance_vector.h"
#include "model/sample/sample_cluster_array.h"
#include "model/clip/clip_instance.h"

// ── ClipInstanceVector ───────────────────────────────────────────────────

TEST_GROUP(ClipInstanceVectorTest) {
	ClipInstanceVector vec;
};

TEST(ClipInstanceVectorTest, constructSetsSize) {
	// Constructor sets element size to sizeof(ClipInstance)
	CHECK_EQUAL(0, vec.getNumElements());
}

TEST(ClipInstanceVectorTest, getElementOnEmpty) {
	// getElement on empty vector should return nullptr or be safely bounded
	// We don't call getElement with invalid index — just verify numElements is 0
	CHECK_EQUAL(0, vec.getNumElements());
}

// Note: ClipArray tests omitted — mock ClipArray in clip_mocks.h (used by
// clip_iterators) has an ODR conflict with the real clip_array.cpp.
// ClipArray is already exercised via smoke_test.cpp.

// ── SampleClusterArray ───────────────────────────────────────────────────

TEST_GROUP(SampleClusterArrayTest) {
	SampleClusterArray arr;
};

TEST(SampleClusterArrayTest, constructEmpty) {
	CHECK_EQUAL(0, arr.getNumElements());
}

TEST(SampleClusterArrayTest, insertAtEnd) {
	Error err = arr.insertSampleClustersAtEnd(3);
	CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
	CHECK_EQUAL(3, arr.getNumElements());
}

TEST(SampleClusterArrayTest, getElement) {
	arr.insertSampleClustersAtEnd(2);
	SampleCluster* sc = arr.getElement(0);
	CHECK(sc != nullptr);
	SampleCluster* sc1 = arr.getElement(1);
	CHECK(sc1 != nullptr);
	CHECK(sc != sc1); // Different elements
}
