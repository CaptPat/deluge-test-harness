// Deep coverage tests for Sync utilities and TimelineCounter
//
// Covers:
// - syncValueToSyncType: boundary values, all three type regions
// - syncValueToSyncLevel: inverse mapping, round-trip with type
// - wrapSwingIntervalSyncLevel: large positive/negative wrapping
// - clampSwingIntervalSyncLevel: edge values, INT32 extremes
// - SyncLevel enum ordering and arithmetic properties
// - Note length math: shift calculations, magnitude derivation
// - TimelineCounter: concrete subclass with controllable state,
//   loop length, position tracking, armedForRecording, virtual dispatch
// - Clip as TimelineCounter: loop length, tempo ratio scaling

#include "CppUTest/TestHarness.h"
#include "clip_mocks.h"
#include "model/sync.h"
#include "model/timeline_counter.h"

#include <climits>

// ============================================================================
// SyncType boundary tests
// ============================================================================

TEST_GROUP(SyncTypeDeep){};

TEST(SyncTypeDeep, boundaryAtTripletStart) {
	// Value SYNC_TYPE_TRIPLET (10) is the first triplet entry
	CHECK_EQUAL(SYNC_TYPE_TRIPLET, syncValueToSyncType(SYNC_TYPE_TRIPLET));
}

TEST(SyncTypeDeep, boundaryAtDottedStart) {
	// Value SYNC_TYPE_DOTTED (19) is the first dotted entry
	CHECK_EQUAL(SYNC_TYPE_DOTTED, syncValueToSyncType(SYNC_TYPE_DOTTED));
}

TEST(SyncTypeDeep, lastEvenBeforeTriplet) {
	CHECK_EQUAL(SYNC_TYPE_EVEN, syncValueToSyncType(SYNC_TYPE_TRIPLET - 1));
}

TEST(SyncTypeDeep, lastTripletBeforeDotted) {
	CHECK_EQUAL(SYNC_TYPE_TRIPLET, syncValueToSyncType(SYNC_TYPE_DOTTED - 1));
}

TEST(SyncTypeDeep, lastDottedValue) {
	CHECK_EQUAL(SYNC_TYPE_DOTTED, syncValueToSyncType(NUM_SYNC_VALUES - 1));
}

TEST(SyncTypeDeep, valueZeroIsEven) {
	CHECK_EQUAL(SYNC_TYPE_EVEN, syncValueToSyncType(0));
}

// ============================================================================
// SyncLevel inverse mapping tests
// ============================================================================

TEST_GROUP(SyncLevelDeep){};

// Verify that every valid sync value maps to a level in [0, MAX_SYNC_LEVEL]
TEST(SyncLevelDeep, allValuesInRange) {
	for (int32_t v = 0; v < NUM_SYNC_VALUES; v++) {
		SyncLevel level = syncValueToSyncLevel(v);
		CHECK(level <= MAX_SYNC_LEVEL);
	}
}

// Even region: value i maps to level i directly
TEST(SyncLevelDeep, evenRegionIdentity) {
	for (int32_t i = 0; i <= MAX_SYNC_LEVEL; i++) {
		CHECK_EQUAL(static_cast<SyncLevel>(i), syncValueToSyncLevel(i));
	}
}

// Triplet region: value (SYNC_TYPE_TRIPLET + k) maps to level (k + 1)
TEST(SyncLevelDeep, tripletRegionOffset) {
	for (int32_t k = 0; k < MAX_SYNC_LEVEL; k++) {
		int32_t value = SYNC_TYPE_TRIPLET + k;
		SyncLevel expected = static_cast<SyncLevel>(k + 1);
		CHECK_EQUAL(expected, syncValueToSyncLevel(value));
	}
}

// Dotted region: value (SYNC_TYPE_DOTTED + k) maps to level (k + 1)
TEST(SyncLevelDeep, dottedRegionOffset) {
	for (int32_t k = 0; k < MAX_SYNC_LEVEL; k++) {
		int32_t value = SYNC_TYPE_DOTTED + k;
		SyncLevel expected = static_cast<SyncLevel>(k + 1);
		CHECK_EQUAL(expected, syncValueToSyncLevel(value));
	}
}

// For non-zero even values, type+level reconstruct the original value
TEST(SyncLevelDeep, roundTripEven) {
	for (int32_t v = 1; v < SYNC_TYPE_TRIPLET; v++) {
		SyncType type = syncValueToSyncType(v);
		SyncLevel level = syncValueToSyncLevel(v);
		CHECK_EQUAL(SYNC_TYPE_EVEN, type);
		// Reconstruct: for even, value == level
		CHECK_EQUAL(v, static_cast<int32_t>(level));
	}
}

// For triplet values, type+level reconstruct the original value
TEST(SyncLevelDeep, roundTripTriplet) {
	for (int32_t v = SYNC_TYPE_TRIPLET; v < SYNC_TYPE_DOTTED; v++) {
		SyncType type = syncValueToSyncType(v);
		SyncLevel level = syncValueToSyncLevel(v);
		CHECK_EQUAL(SYNC_TYPE_TRIPLET, type);
		int32_t reconstructed = SYNC_TYPE_TRIPLET + static_cast<int32_t>(level) - 1;
		CHECK_EQUAL(v, reconstructed);
	}
}

// For dotted values, type+level reconstruct the original value
TEST(SyncLevelDeep, roundTripDotted) {
	for (int32_t v = SYNC_TYPE_DOTTED; v < NUM_SYNC_VALUES; v++) {
		SyncType type = syncValueToSyncType(v);
		SyncLevel level = syncValueToSyncLevel(v);
		CHECK_EQUAL(SYNC_TYPE_DOTTED, type);
		int32_t reconstructed = SYNC_TYPE_DOTTED + static_cast<int32_t>(level) - 1;
		CHECK_EQUAL(v, reconstructed);
	}
}

// ============================================================================
// SyncLevel enum ordering
// ============================================================================

TEST(SyncLevelDeep, enumValuesAreContiguous) {
	CHECK_EQUAL(0, SYNC_LEVEL_NONE);
	CHECK_EQUAL(1, SYNC_LEVEL_WHOLE);
	CHECK_EQUAL(2, SYNC_LEVEL_2ND);
	CHECK_EQUAL(3, SYNC_LEVEL_4TH);
	CHECK_EQUAL(4, SYNC_LEVEL_8TH);
	CHECK_EQUAL(5, SYNC_LEVEL_16TH);
	CHECK_EQUAL(6, SYNC_LEVEL_32ND);
	CHECK_EQUAL(7, SYNC_LEVEL_64TH);
	CHECK_EQUAL(8, SYNC_LEVEL_128TH);
	CHECK_EQUAL(9, SYNC_LEVEL_256TH);
}

TEST(SyncLevelDeep, maxSyncLevelMatchesEnum) {
	CHECK_EQUAL(SYNC_LEVEL_256TH, MAX_SYNC_LEVEL);
}

// Each level represents a finer subdivision — higher level = finer
TEST(SyncLevelDeep, finerSubdivisionOrdering) {
	CHECK(SYNC_LEVEL_WHOLE < SYNC_LEVEL_2ND);
	CHECK(SYNC_LEVEL_2ND < SYNC_LEVEL_4TH);
	CHECK(SYNC_LEVEL_4TH < SYNC_LEVEL_8TH);
	CHECK(SYNC_LEVEL_8TH < SYNC_LEVEL_16TH);
	CHECK(SYNC_LEVEL_16TH < SYNC_LEVEL_32ND);
	CHECK(SYNC_LEVEL_32ND < SYNC_LEVEL_64TH);
	CHECK(SYNC_LEVEL_64TH < SYNC_LEVEL_128TH);
	CHECK(SYNC_LEVEL_128TH < SYNC_LEVEL_256TH);
}

// ============================================================================
// Note length shift math
// ============================================================================

TEST_GROUP(NoteLengthMath){};

// The note length formula: noteLength = 3 << (SYNC_LEVEL_256TH - level)
// Verify this yields expected values for each level
TEST(NoteLengthMath, noteLengthDoublesPerLevel) {
	uint32_t prevLength = 0;
	for (int level = SYNC_LEVEL_256TH; level >= SYNC_LEVEL_WHOLE; level--) {
		uint32_t shift = SYNC_LEVEL_256TH - level;
		uint32_t noteLength = uint32_t{3} << shift;

		if (prevLength > 0) {
			// Each coarser level doubles the note length
			CHECK_EQUAL(prevLength * 2, noteLength);
		}
		prevLength = noteLength;
	}
}

TEST(NoteLengthMath, level256thGivesMinimumLength) {
	uint32_t shift = SYNC_LEVEL_256TH - SYNC_LEVEL_256TH; // 0
	uint32_t noteLength = uint32_t{3} << shift;
	CHECK_EQUAL(3u, noteLength);
}

TEST(NoteLengthMath, levelWholeGivesLargeLength) {
	uint32_t shift = SYNC_LEVEL_256TH - SYNC_LEVEL_WHOLE; // 8
	uint32_t noteLength = uint32_t{3} << shift;
	CHECK_EQUAL(3u * 256u, noteLength); // 768
}

// ============================================================================
// wrapSwingIntervalSyncLevel deep tests
// ============================================================================

TEST_GROUP(WrapSwingDeep){};

// Large positive values wrap correctly
TEST(WrapSwingDeep, largePositiveWrap) {
	// NUM_SWING_INTERVALS is 10 (SYNC_TYPE_TRIPLET), valid range is 1..9
	int32_t period = NUM_SWING_INTERVALS - 1; // 9
	for (int32_t mult = 1; mult <= 5; mult++) {
		int32_t val = 1 + mult * period; // Should wrap to 1
		CHECK_EQUAL(1, wrapSwingIntervalSyncLevel(val));
	}
}

// Large negative values wrap correctly
// Formula: mod(value - 1, 9) + 1
TEST(WrapSwingDeep, largeNegativeWrap) {
	int32_t period = NUM_SWING_INTERVALS - 1; // 9
	// -1: mod(-2, 9) + 1 = 7 + 1 = 8
	CHECK_EQUAL(period - 1, wrapSwingIntervalSyncLevel(-1));
	// -period (= -9): mod(-10, 9) + 1 = 8 + 1 = 9
	CHECK_EQUAL(period, wrapSwingIntervalSyncLevel(-period));
}

// Verify the wrap never returns zero
TEST(WrapSwingDeep, neverReturnsZero) {
	for (int32_t v = -100; v <= 100; v++) {
		int32_t result = wrapSwingIntervalSyncLevel(v);
		CHECK(result >= 1);
		CHECK(result <= NUM_SWING_INTERVALS - 1);
	}
}

// Wrap is idempotent for values already in range
TEST(WrapSwingDeep, idempotentInRange) {
	for (int32_t v = 1; v < NUM_SWING_INTERVALS; v++) {
		CHECK_EQUAL(v, wrapSwingIntervalSyncLevel(v));
	}
}

// ============================================================================
// clampSwingIntervalSyncLevel deep tests
// ============================================================================

TEST_GROUP(ClampSwingDeep){};

TEST(ClampSwingDeep, int32MinClampsToMin) {
	CHECK_EQUAL(MIN_SWING_INERVAL, clampSwingIntervalSyncLevel(INT32_MIN));
}

TEST(ClampSwingDeep, int32MaxClampsToMax) {
	CHECK_EQUAL(MAX_SWING_INTERVAL, clampSwingIntervalSyncLevel(INT32_MAX));
}

TEST(ClampSwingDeep, exactMinReturnsMin) {
	CHECK_EQUAL(MIN_SWING_INERVAL, clampSwingIntervalSyncLevel(MIN_SWING_INERVAL));
}

TEST(ClampSwingDeep, exactMaxReturnsMax) {
	CHECK_EQUAL(MAX_SWING_INTERVAL, clampSwingIntervalSyncLevel(MAX_SWING_INTERVAL));
}

TEST(ClampSwingDeep, justBelowMinClamped) {
	CHECK_EQUAL(MIN_SWING_INERVAL, clampSwingIntervalSyncLevel(MIN_SWING_INERVAL - 1));
}

TEST(ClampSwingDeep, justAboveMaxClamped) {
	CHECK_EQUAL(MAX_SWING_INTERVAL, clampSwingIntervalSyncLevel(MAX_SWING_INTERVAL + 1));
}

// All in-range values pass through unchanged
TEST(ClampSwingDeep, allInRangePassThrough) {
	for (int32_t v = MIN_SWING_INERVAL; v <= MAX_SWING_INTERVAL; v++) {
		CHECK_EQUAL(v, clampSwingIntervalSyncLevel(v));
	}
}

// ============================================================================
// SyncType enum value consistency
// ============================================================================

TEST_GROUP(SyncTypeConstants){};

TEST(SyncTypeConstants, typeEnumValues) {
	CHECK_EQUAL(0, SYNC_TYPE_EVEN);
	CHECK_EQUAL(10, SYNC_TYPE_TRIPLET);
	CHECK_EQUAL(19, SYNC_TYPE_DOTTED);
}

TEST(SyncTypeConstants, swingIntervalConstants) {
	CHECK_EQUAL(1, MIN_SWING_INERVAL);
	CHECK_EQUAL(SYNC_TYPE_TRIPLET - 1, MAX_SWING_INTERVAL); // 9
	CHECK_EQUAL(SYNC_TYPE_TRIPLET, NUM_SWING_INTERVALS);     // 10
}

TEST(SyncTypeConstants, numSyncValues) {
	// 10 even (0-9) + 9 triplet (10-18) + 9 dotted (19-27) = 28
	CHECK_EQUAL(28, NUM_SYNC_VALUES);
}

// Each region has the expected number of entries
TEST(SyncTypeConstants, regionSizes) {
	int32_t evenCount = SYNC_TYPE_TRIPLET; // 0..9 = 10 entries
	int32_t tripletCount = SYNC_TYPE_DOTTED - SYNC_TYPE_TRIPLET; // 10..18 = 9 entries
	int32_t dottedCount = NUM_SYNC_VALUES - SYNC_TYPE_DOTTED; // 19..27 = 9 entries
	CHECK_EQUAL(10, evenCount);
	CHECK_EQUAL(9, tripletCount);
	CHECK_EQUAL(9, dottedCount);
}

// ============================================================================
// syncValueToString smoke tests (stubs return minimal data)
// ============================================================================

TEST_GROUP(SyncStringDeep){};

// Exercise all 28 sync values with various tick magnitudes
TEST(SyncStringDeep, allValuesAllMagnitudesNocrash) {
	for (int32_t tickMag = -4; tickMag <= 4; tickMag++) {
		for (uint32_t v = 0; v < NUM_SYNC_VALUES; v++) {
			DEF_STACK_STRING_BUF(buf, 40);
			syncValueToString(v, buf, tickMag);
			// With mock stubs, just verify no crash
			CHECK(buf.size() >= 0);
		}
	}
}

// Exercise horizontal menu label for all type/level combos
TEST(SyncStringDeep, horzMenuLabelAllCombosNocrash) {
	SyncType types[] = {SYNC_TYPE_EVEN, SYNC_TYPE_TRIPLET, SYNC_TYPE_DOTTED};
	for (auto type : types) {
		for (int level = SYNC_LEVEL_NONE; level <= SYNC_LEVEL_256TH; level++) {
			for (int32_t tickMag = -2; tickMag <= 2; tickMag++) {
				DEF_STACK_STRING_BUF(buf, 40);
				syncValueToStringForHorzMenuLabel(type, static_cast<SyncLevel>(level), buf, tickMag);
				CHECK(buf.size() >= 0);
			}
		}
	}
}

// ============================================================================
// TimelineCounter deep tests via concrete subclass
// ============================================================================

// A more controllable test subclass than the minimal one in leaf_models_test
class ConfigurableTimelineCounter : public TimelineCounter {
public:
	int32_t lastProcessedPos_ = 0;
	uint32_t livePos_ = 0;
	int32_t loopLength_ = 0;
	bool playingAutomation_ = false;
	bool backtrackLoop_ = false;
	int32_t cutPos_ = INT32_MAX;

	int32_t getLastProcessedPos() const override { return lastProcessedPos_; }
	uint32_t getLivePos() const override { return livePos_; }
	int32_t getLoopLength() const override { return loopLength_; }
	bool isPlayingAutomationNow() const override { return playingAutomation_; }
	bool backtrackingCouldLoopBackToEnd() const override { return backtrackLoop_; }
	int32_t getPosAtWhichPlaybackWillCut(ModelStackWithTimelineCounter const*) const override { return cutPos_; }
	void getActiveModControllable(ModelStackWithTimelineCounter*) override {}
	void expectEvent() override {}
	TimelineCounter* getTimelineCounterToRecordTo() override { return this; }
};

TEST_GROUP(TimelineCounterDeep){};

TEST(TimelineCounterDeep, defaultArmedForRecording) {
	ConfigurableTimelineCounter tc;
	CHECK(tc.armedForRecording);
}

TEST(TimelineCounterDeep, armedForRecordingCanBeToggled) {
	ConfigurableTimelineCounter tc;
	tc.armedForRecording = false;
	CHECK(!tc.armedForRecording);
	tc.armedForRecording = true;
	CHECK(tc.armedForRecording);
}

TEST(TimelineCounterDeep, loopLengthZero) {
	ConfigurableTimelineCounter tc;
	tc.loopLength_ = 0;
	CHECK_EQUAL(0, tc.getLoopLength());
}

TEST(TimelineCounterDeep, loopLengthTypicalBar) {
	ConfigurableTimelineCounter tc;
	// A typical 4/4 bar at default resolution: 3072 ticks per quarter * 4 = 12288
	tc.loopLength_ = 12288;
	CHECK_EQUAL(12288, tc.getLoopLength());
}

TEST(TimelineCounterDeep, loopLengthMaxInt) {
	ConfigurableTimelineCounter tc;
	tc.loopLength_ = INT32_MAX;
	CHECK_EQUAL(INT32_MAX, tc.getLoopLength());
}

TEST(TimelineCounterDeep, positionTracking) {
	ConfigurableTimelineCounter tc;
	tc.lastProcessedPos_ = 0;
	tc.livePos_ = 0;
	CHECK_EQUAL(0, tc.getLastProcessedPos());
	CHECK_EQUAL(0u, tc.getLivePos());

	tc.lastProcessedPos_ = 1024;
	tc.livePos_ = 1025;
	CHECK_EQUAL(1024, tc.getLastProcessedPos());
	CHECK_EQUAL(1025u, tc.getLivePos());
}

TEST(TimelineCounterDeep, positionNearWrap) {
	ConfigurableTimelineCounter tc;
	tc.loopLength_ = 4096;
	// Position at end of loop
	tc.livePos_ = 4095;
	CHECK_EQUAL(4095u, tc.getLivePos());
	// Position beyond loop (the counter itself doesn't enforce wrapping)
	tc.livePos_ = 4096;
	CHECK_EQUAL(4096u, tc.getLivePos());
}

TEST(TimelineCounterDeep, lastProcessedPosNegative) {
	ConfigurableTimelineCounter tc;
	tc.lastProcessedPos_ = -1;
	CHECK_EQUAL(-1, tc.getLastProcessedPos());
}

TEST(TimelineCounterDeep, automationState) {
	ConfigurableTimelineCounter tc;
	CHECK(!tc.isPlayingAutomationNow());
	tc.playingAutomation_ = true;
	CHECK(tc.isPlayingAutomationNow());
}

TEST(TimelineCounterDeep, backtrackState) {
	ConfigurableTimelineCounter tc;
	CHECK(!tc.backtrackingCouldLoopBackToEnd());
	tc.backtrackLoop_ = true;
	CHECK(tc.backtrackingCouldLoopBackToEnd());
}

TEST(TimelineCounterDeep, cutPosition) {
	ConfigurableTimelineCounter tc;
	CHECK_EQUAL(INT32_MAX, tc.getPosAtWhichPlaybackWillCut(nullptr));
	tc.cutPos_ = 2048;
	CHECK_EQUAL(2048, tc.getPosAtWhichPlaybackWillCut(nullptr));
}

TEST(TimelineCounterDeep, getTimelineCounterToRecordToReturnsSelf) {
	ConfigurableTimelineCounter tc;
	POINTERS_EQUAL(&tc, tc.getTimelineCounterToRecordTo());
}

TEST(TimelineCounterDeep, possiblyCloneDefaultReturnsFalse) {
	ConfigurableTimelineCounter tc;
	CHECK(!tc.possiblyCloneForArrangementRecording(nullptr));
}

TEST(TimelineCounterDeep, instrumentBeenEditedDoesNotCrash) {
	ConfigurableTimelineCounter tc;
	tc.instrumentBeenEdited(); // default no-op, should not crash
}

TEST(TimelineCounterDeep, virtualDispatchThroughBasePointer) {
	ConfigurableTimelineCounter tc;
	tc.loopLength_ = 9999;
	tc.livePos_ = 42;
	TimelineCounter* base = &tc;
	CHECK_EQUAL(9999, base->getLoopLength());
	CHECK_EQUAL(42u, base->getLivePos());
}

// ============================================================================
// Clip as TimelineCounter
// ============================================================================

TEST_GROUP(ClipAsTimelineCounter){};

TEST(ClipAsTimelineCounter, clipInheritsTimelineCounter) {
	Clip clip;
	TimelineCounter* base = &clip;
	CHECK(base != nullptr);
}

TEST(ClipAsTimelineCounter, loopLengthThroughBase) {
	Clip clip;
	clip.loopLength = 8192;
	TimelineCounter* base = &clip;
	CHECK_EQUAL(8192, base->getLoopLength());
}

TEST(ClipAsTimelineCounter, defaultLoopLengthZero) {
	Clip clip;
	CHECK_EQUAL(0, clip.getLoopLength());
}

TEST(ClipAsTimelineCounter, defaultArmedForRecording) {
	Clip clip;
	CHECK(clip.armedForRecording);
}

TEST(ClipAsTimelineCounter, getTimelineCounterToRecordToReturnsSelf) {
	Clip clip;
	POINTERS_EQUAL(&clip, clip.getTimelineCounterToRecordTo());
}

// ============================================================================
// Clip tempo ratio scaling (sync-adjacent functionality)
// ============================================================================

TEST_GROUP(ClipTempoRatio){};

TEST(ClipTempoRatio, unityRatioPassesThrough) {
	Clip clip;
	clip.tempoRatioNumerator = 1;
	clip.tempoRatioDenominator = 1;
	CHECK(!clip.hasTempoRatio());
	CHECK_EQUAL(100, clip.scaleGlobalToClipTicks(100));
	CHECK_EQUAL(100, clip.clipTicksToGlobalTicks(100));
}

TEST(ClipTempoRatio, doubleSpeedRatio) {
	Clip clip;
	clip.tempoRatioNumerator = 2;
	clip.tempoRatioDenominator = 1;
	CHECK(clip.hasTempoRatio());
	// 100 global ticks -> 200 clip ticks at 2x speed
	CHECK_EQUAL(200, clip.scaleGlobalToClipTicks(100));
}

TEST(ClipTempoRatio, halfSpeedRatio) {
	Clip clip;
	clip.tempoRatioNumerator = 1;
	clip.tempoRatioDenominator = 2;
	CHECK(clip.hasTempoRatio());
	// 100 global ticks -> 50 clip ticks at 0.5x speed
	CHECK_EQUAL(50, clip.scaleGlobalToClipTicks(100));
}

TEST(ClipTempoRatio, clipTicksToGlobalInverse) {
	Clip clip;
	clip.tempoRatioNumerator = 3;
	clip.tempoRatioDenominator = 2;
	CHECK(clip.hasTempoRatio());
	// 90 clip ticks at 3:2 ratio -> ceil(90 * 2 / 3) = 60
	CHECK_EQUAL(60, clip.clipTicksToGlobalTicks(90));
}

TEST(ClipTempoRatio, negativeTicksScaleCorrectly) {
	Clip clip;
	clip.tempoRatioNumerator = 2;
	clip.tempoRatioDenominator = 1;
	int32_t result = clip.scaleGlobalToClipTicks(-50);
	CHECK_EQUAL(-100, result);
}

TEST(ClipTempoRatio, zeroTicksReturnZero) {
	Clip clip;
	clip.tempoRatioNumerator = 3;
	clip.tempoRatioDenominator = 4;
	CHECK_EQUAL(0, clip.scaleGlobalToClipTicks(0));
	CHECK_EQUAL(0, clip.clipTicksToGlobalTicks(0));
}

// ============================================================================
// Sync magnitude relationships
// ============================================================================

TEST_GROUP(SyncMagnitude){};

// The shift from SYNC_LEVEL_256TH to a given level determines note length
// Verify the shift is monotonic
TEST(SyncMagnitude, shiftDecreasesWithFinerLevel) {
	for (int level = SYNC_LEVEL_WHOLE; level < SYNC_LEVEL_256TH; level++) {
		uint32_t shiftCoarser = SYNC_LEVEL_256TH - level;
		uint32_t shiftFiner = SYNC_LEVEL_256TH - (level + 1);
		CHECK(shiftCoarser > shiftFiner);
	}
}

// Level SYNC_LEVEL_256TH has shift 0
TEST(SyncMagnitude, finestLevelShiftIsZero) {
	uint32_t shift = SYNC_LEVEL_256TH - SYNC_LEVEL_256TH;
	CHECK_EQUAL(0u, shift);
}

// Level SYNC_LEVEL_WHOLE has shift 8
TEST(SyncMagnitude, coarsestLevelShiftIs8) {
	uint32_t shift = SYNC_LEVEL_256TH - SYNC_LEVEL_WHOLE;
	CHECK_EQUAL(8u, shift);
}

// Each type region has exactly MAX_SYNC_LEVEL non-NONE levels (1..9)
TEST(SyncMagnitude, tripletAndDottedLevelRange) {
	for (int32_t v = SYNC_TYPE_TRIPLET; v < SYNC_TYPE_DOTTED; v++) {
		SyncLevel level = syncValueToSyncLevel(v);
		CHECK(level >= SYNC_LEVEL_WHOLE);
		CHECK(level <= SYNC_LEVEL_256TH);
	}
	for (int32_t v = SYNC_TYPE_DOTTED; v < NUM_SYNC_VALUES; v++) {
		SyncLevel level = syncValueToSyncLevel(v);
		CHECK(level >= SYNC_LEVEL_WHOLE);
		CHECK(level <= SYNC_LEVEL_256TH);
	}
}

// ============================================================================
// Edge case: multiple TimelineCounter instances
// ============================================================================

TEST_GROUP(MultipleTimelineCounters){};

TEST(MultipleTimelineCounters, independentState) {
	ConfigurableTimelineCounter a, b;
	a.loopLength_ = 1000;
	a.livePos_ = 500;
	b.loopLength_ = 2000;
	b.livePos_ = 1500;

	CHECK_EQUAL(1000, a.getLoopLength());
	CHECK_EQUAL(2000, b.getLoopLength());
	CHECK_EQUAL(500u, a.getLivePos());
	CHECK_EQUAL(1500u, b.getLivePos());
}

TEST(MultipleTimelineCounters, recordToTargetIndependent) {
	ConfigurableTimelineCounter a, b;
	POINTERS_EQUAL(&a, a.getTimelineCounterToRecordTo());
	POINTERS_EQUAL(&b, b.getTimelineCounterToRecordTo());
	CHECK(a.getTimelineCounterToRecordTo() != b.getTimelineCounterToRecordTo());
}

// ============================================================================
// Wrap/clamp consistency
// ============================================================================

TEST_GROUP(WrapClampConsistency){};

// For values in the valid range, wrap and clamp agree
TEST(WrapClampConsistency, agreeInRange) {
	for (int32_t v = MIN_SWING_INERVAL; v <= MAX_SWING_INTERVAL; v++) {
		CHECK_EQUAL(clampSwingIntervalSyncLevel(v), wrapSwingIntervalSyncLevel(v));
	}
}

// Both always produce values in [MIN_SWING_INERVAL, MAX_SWING_INTERVAL]
TEST(WrapClampConsistency, bothInValidRange) {
	int32_t testValues[] = {-1000, -1, 0, 1, 5, 9, 10, 11, 50, 100, 1000};
	for (int32_t v : testValues) {
		int32_t wrapped = wrapSwingIntervalSyncLevel(v);
		int32_t clamped = clampSwingIntervalSyncLevel(v);
		CHECK(wrapped >= MIN_SWING_INERVAL);
		CHECK(wrapped <= MAX_SWING_INTERVAL);
		CHECK(clamped >= MIN_SWING_INERVAL);
		CHECK(clamped <= MAX_SWING_INTERVAL);
	}
}
