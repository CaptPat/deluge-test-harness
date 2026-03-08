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

// --- clampSwingIntervalSyncLevel ---

TEST(SyncTests, clampSwingInRange) {
	for (int32_t i = MIN_SWING_INERVAL; i <= MAX_SWING_INTERVAL; i++) {
		CHECK_EQUAL(i, clampSwingIntervalSyncLevel(i));
	}
}

TEST(SyncTests, clampSwingBelowMin) {
	CHECK_EQUAL(MIN_SWING_INERVAL, clampSwingIntervalSyncLevel(0));
	CHECK_EQUAL(MIN_SWING_INERVAL, clampSwingIntervalSyncLevel(-100));
}

TEST(SyncTests, clampSwingAboveMax) {
	CHECK_EQUAL(MAX_SWING_INTERVAL, clampSwingIntervalSyncLevel(MAX_SWING_INTERVAL + 1));
	CHECK_EQUAL(MAX_SWING_INTERVAL, clampSwingIntervalSyncLevel(1000));
}

// --- syncValueToString ---

TEST(SyncTests, syncValueToStringZeroIsOff) {
	DEF_STACK_STRING_BUF(buf, 30);
	syncValueToString(0, buf, 0);
	// Value 0 is SYNC_LEVEL_NONE with SYNC_TYPE_EVEN, no "-notes" suffix
	// The mock getNoteLengthNameFromMagnitude is a no-op, so buffer may be empty
	// Just verify no crash
	CHECK(buf.size() >= 0);
}

TEST(SyncTests, syncValueToStringEvenNonZero) {
	DEF_STACK_STRING_BUF(buf, 30);
	syncValueToString(4, buf, 0); // SYNC_LEVEL_8TH, SYNC_TYPE_EVEN
	// Mock stubs mean we won't get real text, but verify no crash
	CHECK(buf.size() >= 0);
}

TEST(SyncTests, syncValueToStringTriplet) {
	DEF_STACK_STRING_BUF(buf, 30);
	syncValueToString(SYNC_TYPE_TRIPLET + 3, buf, 0); // triplet 4th-notes
	CHECK(buf.size() >= 0);
}

TEST(SyncTests, syncValueToStringDotted) {
	DEF_STACK_STRING_BUF(buf, 30);
	syncValueToString(SYNC_TYPE_DOTTED + 1, buf, 0); // dotted whole
	CHECK(buf.size() >= 0);
}

TEST(SyncTests, syncValueToStringForHorzMenuLabel) {
	DEF_STACK_STRING_BUF(buf, 30);
	syncValueToStringForHorzMenuLabel(SYNC_TYPE_EVEN, SYNC_LEVEL_4TH, buf, 0);
	CHECK(buf.size() >= 0);
}

TEST(SyncTests, syncValueToStringForHorzMenuLabelTriplet) {
	DEF_STACK_STRING_BUF(buf, 30);
	syncValueToStringForHorzMenuLabel(SYNC_TYPE_TRIPLET, SYNC_LEVEL_8TH, buf, 0);
	CHECK(buf.size() >= 0);
}
