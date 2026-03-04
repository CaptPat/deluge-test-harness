// Sync system regression tests — ported from firmware/tests/unit/sync_tests.cpp

#include "CppUTest/TestHarness.h"
#include "model/sync.h"

TEST_GROUP(SyncTests){};

TEST(SyncTests, wrapSwingIntervalSyncLevel) {
	for (int32_t i = 1; i < NUM_SWING_INTERVALS; i++) {
		CHECK_EQUAL(i, wrapSwingIntervalSyncLevel(i));
	}
	CHECK_EQUAL(NUM_SWING_INTERVALS - 2, wrapSwingIntervalSyncLevel(-1));
	CHECK_EQUAL(NUM_SWING_INTERVALS - 1, wrapSwingIntervalSyncLevel(0));
	CHECK_EQUAL(1, wrapSwingIntervalSyncLevel(NUM_SWING_INTERVALS));
	CHECK_EQUAL(2, wrapSwingIntervalSyncLevel(NUM_SWING_INTERVALS + 1));
}

TEST(SyncTests, syncValueToSyncType) {
	for (int32_t i = 0; i < SYNC_TYPE_TRIPLET; i++) {
		CHECK_EQUAL(SYNC_TYPE_EVEN, syncValueToSyncType(i));
	}
	for (int32_t i = SYNC_TYPE_TRIPLET + 1; i < SYNC_TYPE_DOTTED; i++) {
		CHECK_EQUAL(SYNC_TYPE_TRIPLET, syncValueToSyncType(i));
	}
	for (int32_t i = SYNC_TYPE_DOTTED + 1; i < NUM_SYNC_VALUES; i++) {
		CHECK_EQUAL(SYNC_TYPE_DOTTED, syncValueToSyncType(i));
	}
}

TEST(SyncTests, syncValueToSyncLevel) {
	for (int32_t i = 0; i <= MAX_SYNC_LEVEL; i++) {
		CHECK_EQUAL(i, syncValueToSyncLevel(i));
	}
	for (int32_t i = 0; i < MAX_SYNC_LEVEL; i++) {
		CHECK_EQUAL(i + 1, syncValueToSyncLevel(i + SYNC_TYPE_TRIPLET));
	}
	for (int32_t i = 0; i < MAX_SYNC_LEVEL; i++) {
		CHECK_EQUAL(i + 1, syncValueToSyncLevel(i + SYNC_TYPE_DOTTED));
	}
}
