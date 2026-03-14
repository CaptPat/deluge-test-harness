// Tests for util/container/hashtable/open_addressing_hash_table.cpp
//
// LIMITATION: getBucketAddress() and secondaryMemoryGetBucketAddress() cast
// void* to uint32_t, which truncates 64-bit pointers. This means ALL operations
// that access bucket memory (insert, lookup, remove) segfault on x86-64.
// Only construction, sentinel checks, and compile-verification are testable here.
// Full runtime testing requires a 32-bit build or the actual ARM target.

#include "CppUTest/TestHarness.h"
#include "util/container/hashtable/open_addressing_hash_table.h"

// ── 32-bit key table ───────────────────────────────────────────────────────

TEST_GROUP(HashTable32Test){};

TEST(HashTable32Test, constructionDefaults) {
	OpenAddressingHashTableWith32bitKey ht;
	CHECK_EQUAL(0, ht.numBuckets);
	CHECK_EQUAL(0, ht.numElements);
	POINTERS_EQUAL(nullptr, ht.memory);
	POINTERS_EQUAL(nullptr, ht.secondaryMemory);
	CHECK_EQUAL(16, ht.initialNumBuckets);
	CHECK_EQUAL((int8_t)sizeof(uint32_t), ht.elementSize);
}

TEST(HashTable32Test, emptyBucketKeyIs0xFFFFFFFF) {
	OpenAddressingHashTableWith32bitKey ht;
	CHECK_TRUE(ht.doesKeyIndicateEmptyBucket(0xFFFFFFFF));
	CHECK_FALSE(ht.doesKeyIndicateEmptyBucket(0));
	CHECK_FALSE(ht.doesKeyIndicateEmptyBucket(42));
	CHECK_FALSE(ht.doesKeyIndicateEmptyBucket(0xFFFFFFFE));
}

TEST(HashTable32Test, lookupOnEmptyReturnsNull) {
	OpenAddressingHashTableWith32bitKey ht;
	POINTERS_EQUAL(nullptr, ht.lookup(42));
}

TEST(HashTable32Test, removeFromEmptyReturnsFalse) {
	OpenAddressingHashTableWith32bitKey ht;
	CHECK_FALSE(ht.remove(42));
}

TEST(HashTable32Test, emptyOnEmptyIsNoOp) {
	OpenAddressingHashTableWith32bitKey ht;
	ht.empty();
	CHECK_EQUAL(0, ht.numBuckets);
	CHECK_EQUAL(0, ht.numElements);
	POINTERS_EQUAL(nullptr, ht.memory);
}

TEST(HashTable32Test, destructorOnEmptyIsNoOp) {
	{ OpenAddressingHashTableWith32bitKey ht; }
	// No crash = success
}

TEST(HashTable32Test, setKeyAndGetKeyConsistent) {
	OpenAddressingHashTableWith32bitKey ht;
	uint32_t buf = 0;
	ht.setKeyAtAddress(12345, &buf);
	CHECK_EQUAL(12345u, ht.getKeyFromAddress(&buf));
}

TEST(HashTable32Test, getBucketIndexDeterministic) {
	OpenAddressingHashTableWith32bitKey ht;
	// Need numBuckets set for masking to work
	ht.numBuckets = 16;
	int32_t idx1 = ht.getBucketIndex(42);
	int32_t idx2 = ht.getBucketIndex(42);
	CHECK_EQUAL(idx1, idx2);
	CHECK(idx1 >= 0);
	CHECK(idx1 < 16);
}

TEST(HashTable32Test, getBucketIndexDifferentKeys) {
	OpenAddressingHashTableWith32bitKey ht;
	ht.numBuckets = 16;
	// Different keys should mostly hash to different buckets
	int32_t idx1 = ht.getBucketIndex(1);
	int32_t idx2 = ht.getBucketIndex(1000);
	// Not guaranteed different, but both in range
	CHECK(idx1 >= 0 && idx1 < 16);
	CHECK(idx2 >= 0 && idx2 < 16);
}

// ── 16-bit key table ───────────────────────────────────────────────────────

TEST_GROUP(HashTable16Test){};

TEST(HashTable16Test, constructionDefaults) {
	OpenAddressingHashTableWith16bitKey ht;
	CHECK_EQUAL((int8_t)sizeof(uint16_t), ht.elementSize);
	CHECK_EQUAL(0, ht.numBuckets);
	CHECK_EQUAL(0, ht.numElements);
}

TEST(HashTable16Test, emptyBucketKeyIs0xFFFF) {
	OpenAddressingHashTableWith16bitKey ht;
	CHECK_TRUE(ht.doesKeyIndicateEmptyBucket(0xFFFF));
	CHECK_FALSE(ht.doesKeyIndicateEmptyBucket(0));
	CHECK_FALSE(ht.doesKeyIndicateEmptyBucket(0xFFFE));
}

TEST(HashTable16Test, setKeyAndGetKeyConsistent) {
	OpenAddressingHashTableWith16bitKey ht;
	uint16_t buf = 0;
	ht.setKeyAtAddress(5000, &buf);
	CHECK_EQUAL(5000u, ht.getKeyFromAddress(&buf));
}

TEST(HashTable16Test, lookupOnEmptyReturnsNull) {
	OpenAddressingHashTableWith16bitKey ht;
	POINTERS_EQUAL(nullptr, ht.lookup(1000));
}

// ── 8-bit key table ────────────────────────────────────────────────────────

TEST_GROUP(HashTable8Test){};

TEST(HashTable8Test, constructionDefaults) {
	OpenAddressingHashTableWith8bitKey ht;
	CHECK_EQUAL((int8_t)sizeof(uint8_t), ht.elementSize);
	CHECK_EQUAL(0, ht.numBuckets);
}

TEST(HashTable8Test, emptyBucketKeyIs0xFF) {
	OpenAddressingHashTableWith8bitKey ht;
	CHECK_TRUE(ht.doesKeyIndicateEmptyBucket(0xFF));
	CHECK_FALSE(ht.doesKeyIndicateEmptyBucket(0));
	CHECK_FALSE(ht.doesKeyIndicateEmptyBucket(0xFE));
}

TEST(HashTable8Test, setKeyAndGetKeyConsistent) {
	OpenAddressingHashTableWith8bitKey ht;
	uint8_t buf = 0;
	ht.setKeyAtAddress(42, &buf);
	CHECK_EQUAL(42u, ht.getKeyFromAddress(&buf));
}

TEST(HashTable8Test, lookupOnEmptyReturnsNull) {
	OpenAddressingHashTableWith8bitKey ht;
	POINTERS_EQUAL(nullptr, ht.lookup(42));
}
