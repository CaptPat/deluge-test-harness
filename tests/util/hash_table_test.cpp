// Tests for util/container/hashtable/open_addressing_hash_table.cpp
//
// Full runtime tests enabled by hash_table_wrapper.cpp which fixes
// getBucketAddress() pointer casts from uint32_t to uintptr_t.

#include "CppUTest/TestHarness.h"
#include "util/container/hashtable/open_addressing_hash_table.h"

// ── 32-bit key table: construction & sentinels ───────────────────────────

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
}

TEST(HashTable32Test, setKeyAndGetKeyConsistent) {
	OpenAddressingHashTableWith32bitKey ht;
	uint32_t buf = 0;
	ht.setKeyAtAddress(12345, &buf);
	CHECK_EQUAL(12345u, ht.getKeyFromAddress(&buf));
}

TEST(HashTable32Test, getBucketIndexDeterministic) {
	OpenAddressingHashTableWith32bitKey ht;
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
	int32_t idx1 = ht.getBucketIndex(1);
	int32_t idx2 = ht.getBucketIndex(1000);
	CHECK(idx1 >= 0 && idx1 < 16);
	CHECK(idx2 >= 0 && idx2 < 16);
}

// ── 32-bit key table: runtime insert/lookup/remove ───────────────────────

TEST_GROUP(HashTable32Runtime){};

TEST(HashTable32Runtime, insertAndLookupSingleKey) {
	OpenAddressingHashTableWith32bitKey ht;
	void* addr = ht.insert(42);
	CHECK(addr != nullptr);
	CHECK_EQUAL(1, ht.numElements);
	CHECK_EQUAL(16, ht.numBuckets); // initialNumBuckets

	void* found = ht.lookup(42);
	CHECK(found != nullptr);
	CHECK_EQUAL(42u, ht.getKeyFromAddress(found));
}

TEST(HashTable32Runtime, insertAndLookupMultipleKeys) {
	OpenAddressingHashTableWith32bitKey ht;
	ht.insert(10);
	ht.insert(20);
	ht.insert(30);
	CHECK_EQUAL(3, ht.numElements);

	CHECK(ht.lookup(10) != nullptr);
	CHECK(ht.lookup(20) != nullptr);
	CHECK(ht.lookup(30) != nullptr);
	POINTERS_EQUAL(nullptr, ht.lookup(40));
}

TEST(HashTable32Runtime, lookupNonexistentReturnsNull) {
	OpenAddressingHashTableWith32bitKey ht;
	ht.insert(100);
	POINTERS_EQUAL(nullptr, ht.lookup(200));
}

TEST(HashTable32Runtime, removeSingleKey) {
	OpenAddressingHashTableWith32bitKey ht;
	ht.insert(42);
	CHECK_EQUAL(1, ht.numElements);

	CHECK_TRUE(ht.remove(42));
	CHECK_EQUAL(0, ht.numElements);
	POINTERS_EQUAL(nullptr, ht.lookup(42));
}

TEST(HashTable32Runtime, removeNonexistentReturnsFalse) {
	OpenAddressingHashTableWith32bitKey ht;
	ht.insert(42);
	CHECK_FALSE(ht.remove(99));
	CHECK_EQUAL(1, ht.numElements);
}

TEST(HashTable32Runtime, removeMiddleKeyPreservesOthers) {
	OpenAddressingHashTableWith32bitKey ht;
	ht.insert(10);
	ht.insert(20);
	ht.insert(30);

	CHECK_TRUE(ht.remove(20));
	CHECK_EQUAL(2, ht.numElements);

	CHECK(ht.lookup(10) != nullptr);
	POINTERS_EQUAL(nullptr, ht.lookup(20));
	CHECK(ht.lookup(30) != nullptr);
}

TEST(HashTable32Runtime, removeAllKeys) {
	OpenAddressingHashTableWith32bitKey ht;
	for (uint32_t i = 1; i <= 10; i++) {
		ht.insert(i);
	}
	CHECK_EQUAL(10, ht.numElements);

	for (uint32_t i = 1; i <= 10; i++) {
		CHECK_TRUE(ht.remove(i));
	}
	CHECK_EQUAL(0, ht.numElements);

	for (uint32_t i = 1; i <= 10; i++) {
		POINTERS_EQUAL(nullptr, ht.lookup(i));
	}
}

TEST(HashTable32Runtime, emptyAfterInserts) {
	OpenAddressingHashTableWith32bitKey ht;
	ht.insert(1);
	ht.insert(2);
	ht.insert(3);
	ht.empty();
	CHECK_EQUAL(0, ht.numBuckets);
	CHECK_EQUAL(0, ht.numElements);
	POINTERS_EQUAL(nullptr, ht.memory);
}

TEST(HashTable32Runtime, destructorAfterInserts) {
	{
		OpenAddressingHashTableWith32bitKey ht;
		ht.insert(1);
		ht.insert(2);
		ht.insert(3);
	}
	// No crash/leak = success
}

TEST(HashTable32Runtime, insertOnlyIfNotAlreadyPresent) {
	OpenAddressingHashTableWith32bitKey ht;
	ht.insert(42);
	CHECK_EQUAL(1, ht.numElements);

	bool wasAlreadyPresent = false;
	void* addr = ht.insert(42, &wasAlreadyPresent);
	CHECK(addr != nullptr);
	CHECK_TRUE(wasAlreadyPresent);
	CHECK_EQUAL(1, ht.numElements); // not duplicated
}

TEST(HashTable32Runtime, insertDuplicateKeyWithoutFlag) {
	OpenAddressingHashTableWith32bitKey ht;
	ht.insert(42);
	ht.insert(42); // no onlyIfNotAlreadyPresent — inserts duplicate
	// Implementation doesn't prevent duplicates without the flag
	CHECK(ht.numElements >= 1);
}

// ── Rehash (grow) tests ─────────────────────────────────────────────────

TEST_GROUP(HashTable32Rehash){};

TEST(HashTable32Rehash, rehashTriggeredAt75Percent) {
	OpenAddressingHashTableWith32bitKey ht;
	// initialNumBuckets = 16, 75% = 12
	// Insert 12 elements — should trigger rehash on the 13th
	for (uint32_t i = 1; i <= 12; i++) {
		ht.insert(i);
	}
	CHECK_EQUAL(16, ht.numBuckets);
	CHECK_EQUAL(12, ht.numElements);

	// 13th insert triggers rehash to 32 buckets
	ht.insert(13);
	CHECK_EQUAL(32, ht.numBuckets);
	CHECK_EQUAL(13, ht.numElements);

	// All 13 elements still findable
	for (uint32_t i = 1; i <= 13; i++) {
		CHECK(ht.lookup(i) != nullptr);
	}
}

TEST(HashTable32Rehash, multipleRehashes) {
	OpenAddressingHashTableWith32bitKey ht;
	// Insert enough to trigger multiple rehashes: 16→32→64
	for (uint32_t i = 1; i <= 40; i++) {
		ht.insert(i);
	}
	CHECK(ht.numBuckets >= 64);
	CHECK_EQUAL(40, ht.numElements);

	// All elements still findable
	for (uint32_t i = 1; i <= 40; i++) {
		CHECK(ht.lookup(i) != nullptr);
	}
}

TEST(HashTable32Rehash, lookupAndRemoveAfterRehash) {
	OpenAddressingHashTableWith32bitKey ht;
	for (uint32_t i = 1; i <= 20; i++) {
		ht.insert(i);
	}

	// Remove half
	for (uint32_t i = 1; i <= 10; i++) {
		CHECK_TRUE(ht.remove(i));
	}
	CHECK_EQUAL(10, ht.numElements);

	// Remaining half still findable
	for (uint32_t i = 11; i <= 20; i++) {
		CHECK(ht.lookup(i) != nullptr);
	}
	// Removed half gone
	for (uint32_t i = 1; i <= 10; i++) {
		POINTERS_EQUAL(nullptr, ht.lookup(i));
	}
}

// ── Collision handling ──────────────────────────────────────────────────

TEST_GROUP(HashTable32Collision){};

TEST(HashTable32Collision, collisionResolution) {
	OpenAddressingHashTableWith32bitKey ht;
	// Insert keys that are likely to collide (same lower bits)
	// With 16 buckets, bucket = hash(key) & 15
	// We can't predict exact collisions, but inserting many keys
	// exercises the linear probing path
	for (uint32_t i = 0; i < 15; i++) {
		ht.insert(i * 16 + 1); // keys 1, 17, 33, 49, ... share low bits
	}
	CHECK_EQUAL(15, ht.numElements);

	// All should be findable
	for (uint32_t i = 0; i < 15; i++) {
		CHECK(ht.lookup(i * 16 + 1) != nullptr);
	}
}

TEST(HashTable32Collision, removeWithRehashFixup) {
	OpenAddressingHashTableWith32bitKey ht;
	// Fill table enough to have collision chains
	for (uint32_t i = 1; i <= 10; i++) {
		ht.insert(i);
	}

	// Remove from the middle — tests the backward-shift deletion
	CHECK_TRUE(ht.remove(5));
	CHECK_EQUAL(9, ht.numElements);

	// All others still findable
	for (uint32_t i = 1; i <= 10; i++) {
		if (i == 5) {
			POINTERS_EQUAL(nullptr, ht.lookup(i));
		}
		else {
			CHECK(ht.lookup(i) != nullptr);
		}
	}
}

TEST(HashTable32Collision, removeAndReinsert) {
	OpenAddressingHashTableWith32bitKey ht;
	ht.insert(42);
	CHECK_TRUE(ht.remove(42));
	POINTERS_EQUAL(nullptr, ht.lookup(42));

	ht.insert(42);
	CHECK(ht.lookup(42) != nullptr);
	CHECK_EQUAL(1, ht.numElements);
}

// ── Stress tests ────────────────────────────────────────────────────────

TEST_GROUP(HashTable32Stress){};

TEST(HashTable32Stress, insertAndRemove100Elements) {
	OpenAddressingHashTableWith32bitKey ht;

	// Insert 100 elements
	for (uint32_t i = 1; i <= 100; i++) {
		ht.insert(i);
	}
	CHECK_EQUAL(100, ht.numElements);
	CHECK(ht.numBuckets >= 128); // must have rehashed several times

	// Verify all present
	for (uint32_t i = 1; i <= 100; i++) {
		CHECK(ht.lookup(i) != nullptr);
	}

	// Remove all in reverse order
	for (uint32_t i = 100; i >= 1; i--) {
		CHECK_TRUE(ht.remove(i));
	}
	CHECK_EQUAL(0, ht.numElements);
}

TEST(HashTable32Stress, interleavedInsertAndRemove) {
	OpenAddressingHashTableWith32bitKey ht;

	// Insert 1..50
	for (uint32_t i = 1; i <= 50; i++) {
		ht.insert(i);
	}
	// Remove odd numbers
	for (uint32_t i = 1; i <= 50; i += 2) {
		CHECK_TRUE(ht.remove(i));
	}
	CHECK_EQUAL(25, ht.numElements);

	// Insert 51..75
	for (uint32_t i = 51; i <= 75; i++) {
		ht.insert(i);
	}
	CHECK_EQUAL(50, ht.numElements);

	// Verify even 2..50 and 51..75 are present, odd 1..49 are gone
	for (uint32_t i = 1; i <= 50; i++) {
		if (i % 2 == 0) {
			CHECK(ht.lookup(i) != nullptr);
		}
		else {
			POINTERS_EQUAL(nullptr, ht.lookup(i));
		}
	}
	for (uint32_t i = 51; i <= 75; i++) {
		CHECK(ht.lookup(i) != nullptr);
	}
}

TEST(HashTable32Stress, largeKeyValues) {
	OpenAddressingHashTableWith32bitKey ht;
	// Test with large key values near the sentinel boundary
	uint32_t keys[] = {1, 0x7FFFFFFF, 0xFFFFFFFE, 0x80000000, 0x12345678};
	for (auto key : keys) {
		ht.insert(key);
	}
	CHECK_EQUAL(5, ht.numElements);

	for (auto key : keys) {
		CHECK(ht.lookup(key) != nullptr);
	}
}

// ── 16-bit key table ────────────────────────────────────────────────────

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

TEST_GROUP(HashTable16Runtime){};

TEST(HashTable16Runtime, insertAndLookup) {
	OpenAddressingHashTableWith16bitKey ht;
	ht.insert(1000);
	ht.insert(2000);
	ht.insert(3000);
	CHECK_EQUAL(3, ht.numElements);

	CHECK(ht.lookup(1000) != nullptr);
	CHECK(ht.lookup(2000) != nullptr);
	CHECK(ht.lookup(3000) != nullptr);
	POINTERS_EQUAL(nullptr, ht.lookup(4000));
}

TEST(HashTable16Runtime, removeKey) {
	OpenAddressingHashTableWith16bitKey ht;
	ht.insert(500);
	ht.insert(600);
	CHECK_TRUE(ht.remove(500));
	CHECK_EQUAL(1, ht.numElements);
	POINTERS_EQUAL(nullptr, ht.lookup(500));
	CHECK(ht.lookup(600) != nullptr);
}

TEST(HashTable16Runtime, rehashWorks) {
	OpenAddressingHashTableWith16bitKey ht;
	// Fill past 75% of initial 16 buckets
	for (uint32_t i = 1; i <= 13; i++) {
		ht.insert(i);
	}
	CHECK(ht.numBuckets >= 32);
	for (uint32_t i = 1; i <= 13; i++) {
		CHECK(ht.lookup(i) != nullptr);
	}
}

TEST(HashTable16Runtime, maxValidKeys) {
	OpenAddressingHashTableWith16bitKey ht;
	// 0xFFFE is the maximum valid key (0xFFFF is sentinel)
	ht.insert(0xFFFE);
	ht.insert(0);
	ht.insert(1);
	CHECK_EQUAL(3, ht.numElements);
	CHECK(ht.lookup(0xFFFE) != nullptr);
	CHECK(ht.lookup(0) != nullptr);
	CHECK(ht.lookup(1) != nullptr);
}

// ── 8-bit key table ─────────────────────────────────────────────────────

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

TEST_GROUP(HashTable8Runtime){};

TEST(HashTable8Runtime, insertAndLookup) {
	OpenAddressingHashTableWith8bitKey ht;
	ht.insert(10);
	ht.insert(20);
	ht.insert(30);
	CHECK_EQUAL(3, ht.numElements);

	CHECK(ht.lookup(10) != nullptr);
	CHECK(ht.lookup(20) != nullptr);
	CHECK(ht.lookup(30) != nullptr);
	POINTERS_EQUAL(nullptr, ht.lookup(40));
}

TEST(HashTable8Runtime, removeKey) {
	OpenAddressingHashTableWith8bitKey ht;
	ht.insert(42);
	ht.insert(43);
	CHECK_TRUE(ht.remove(42));
	POINTERS_EQUAL(nullptr, ht.lookup(42));
	CHECK(ht.lookup(43) != nullptr);
}

TEST(HashTable8Runtime, insertAllValidKeys) {
	OpenAddressingHashTableWith8bitKey ht;
	// Insert all 255 valid keys (0..254, 0xFF=255 is sentinel)
	for (uint32_t i = 0; i < 255; i++) {
		ht.insert(i);
	}
	CHECK_EQUAL(255, ht.numElements);

	// All findable
	for (uint32_t i = 0; i < 255; i++) {
		CHECK(ht.lookup(i) != nullptr);
	}
}

TEST(HashTable8Runtime, removeAllValidKeys) {
	OpenAddressingHashTableWith8bitKey ht;
	for (uint32_t i = 0; i < 255; i++) {
		ht.insert(i);
	}

	// Remove all in forward order
	for (uint32_t i = 0; i < 255; i++) {
		CHECK_TRUE(ht.remove(i));
	}
	CHECK_EQUAL(0, ht.numElements);
}

TEST(HashTable8Runtime, rehashWorks) {
	OpenAddressingHashTableWith8bitKey ht;
	for (uint32_t i = 0; i < 20; i++) {
		ht.insert(i);
	}
	CHECK(ht.numBuckets >= 32);
	for (uint32_t i = 0; i < 20; i++) {
		CHECK(ht.lookup(i) != nullptr);
	}
}
