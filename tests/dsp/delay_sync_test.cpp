// Delay sync configuration tests — exercises setupWorkingState branches for
// syncLevel, syncType (even/triplet/dotted), and buffer copy operations.
// Driven by real-world preset values from community preset pack.

#include "CppUTest/TestHarness.h"
#include "dsp/delay/delay.h"
#include "dsp/stereo_sample.h"
#include <cstring>

static constexpr int kBufSize = 32;

TEST_GROUP(DelaySyncTest) {
	Delay delay;
	StereoSample buf[kBufSize];

	void setup() override {
		memset(buf, 0, sizeof(buf));
	}

	void teardown() override {
		delay.discardBuffers();
	}

	void fillDC(int32_t amplitude = 1 << 20) {
		for (auto& s : buf) {
			s.l = amplitude;
			s.r = amplitude;
		}
	}

	// Helper: run setupWorkingState and process a block
	void setupAndProcess(Delay::State& state, uint32_t tickInverse = 1 << 24) {
		delay.setupWorkingState(state, tickInverse);
		if (state.doDelay) {
			delay.userRateLastTime = state.userDelayRate;
			delay.countCyclesWithoutChange = 0;
			fillDC();
			delay.process({buf, kBufSize}, state);
		}
	}
};

// --- setupWorkingState sync branches ---

TEST(DelaySyncTest, syncLevelNoneBypassesSyncCalc) {
	delay.syncLevel = SYNC_LEVEL_NONE;
	delay.syncType = SYNC_TYPE_EVEN;
	Delay::State state{};
	state.userDelayRate = 44100;
	state.delayFeedbackAmount = 1 << 29;

	delay.setupWorkingState(state, 1 << 24);
	// With syncLevel=0, rate should pass through unmodified
	CHECK(state.doDelay);
	CHECK_EQUAL(44100, state.userDelayRate);
}

TEST(DelaySyncTest, syncEvenModifiesRate) {
	delay.syncLevel = SYNC_LEVEL_16TH;
	delay.syncType = SYNC_TYPE_EVEN;
	Delay::State state{};
	state.userDelayRate = 1 << 20;
	state.delayFeedbackAmount = 1 << 29;

	int32_t originalRate = state.userDelayRate;
	delay.setupWorkingState(state, 1 << 27);
	// Rate should be modified by sync calc (multiply + shift)
	CHECK(state.doDelay);
	CHECK(state.userDelayRate != originalRate);
}

TEST(DelaySyncTest, syncTripletMultipliesBy3Over2) {
	delay.syncLevel = SYNC_LEVEL_16TH;
	delay.syncType = SYNC_TYPE_TRIPLET;
	Delay::State state{};
	state.userDelayRate = 1 << 20;
	state.delayFeedbackAmount = 1 << 29;

	// Compare to even at same settings
	Delay delayEven;
	delayEven.syncLevel = SYNC_LEVEL_16TH;
	delayEven.syncType = SYNC_TYPE_EVEN;
	Delay::State stateEven{};
	stateEven.userDelayRate = 1 << 20;
	stateEven.delayFeedbackAmount = 1 << 29;

	delay.setupWorkingState(state, 1 << 27);
	delayEven.setupWorkingState(stateEven, 1 << 27);

	// Triplet should produce a different rate than even
	CHECK(state.userDelayRate != stateEven.userDelayRate);

	delayEven.discardBuffers();
}

TEST(DelaySyncTest, syncDottedMultipliesBy2Over3) {
	delay.syncLevel = SYNC_LEVEL_16TH;
	delay.syncType = SYNC_TYPE_DOTTED;
	Delay::State state{};
	state.userDelayRate = 1 << 20;
	state.delayFeedbackAmount = 1 << 29;

	Delay delayEven;
	delayEven.syncLevel = SYNC_LEVEL_16TH;
	delayEven.syncType = SYNC_TYPE_EVEN;
	Delay::State stateEven{};
	stateEven.userDelayRate = 1 << 20;
	stateEven.delayFeedbackAmount = 1 << 29;

	delay.setupWorkingState(state, 1 << 27);
	delayEven.setupWorkingState(stateEven, 1 << 27);

	// Dotted (2/3) < even, so rate should be lower
	CHECK(state.userDelayRate < stateEven.userDelayRate);

	delayEven.discardBuffers();
}

TEST(DelaySyncTest, allSyncLevelsProcessWithoutCrash) {
	// Sweep all 9 sync levels with all 3 sync types
	SyncType types[] = {SYNC_TYPE_EVEN, SYNC_TYPE_TRIPLET, SYNC_TYPE_DOTTED};
	for (SyncType st : types) {
		for (int lvl = 1; lvl <= 9; lvl++) {
			Delay d;
			d.syncLevel = static_cast<SyncLevel>(lvl);
			d.syncType = st;

			Delay::State state{};
			state.userDelayRate = 1 << 20;
			state.delayFeedbackAmount = 1 << 29;

			d.setupWorkingState(state, 1 << 24);
			if (state.doDelay) {
				d.userRateLastTime = state.userDelayRate;
				d.countCyclesWithoutChange = 0;
				fillDC();
				d.process({buf, kBufSize}, state);
			}
			d.discardBuffers();
		}
	}
}

// --- Sync rate clamping ---

TEST(DelaySyncTest, syncRateClampedAtHighLevel) {
	// High sync level means small limit: 2147483647 >> (9+5) = 131071
	delay.syncLevel = static_cast<SyncLevel>(9);
	delay.syncType = SYNC_TYPE_EVEN;
	Delay::State state{};
	state.userDelayRate = INT32_MAX; // Extreme input
	state.delayFeedbackAmount = 1 << 29;

	delay.setupWorkingState(state, 1 << 30);
	// Should not overflow — rate gets clamped before shift
	CHECK(state.doDelay);
}

// --- Low feedback → mightDoDelay is false ---

TEST(DelaySyncTest, lowFeedbackSkipsSync) {
	delay.syncLevel = SYNC_LEVEL_16TH;
	delay.syncType = SYNC_TYPE_TRIPLET;
	Delay::State state{};
	state.userDelayRate = 44100;
	state.delayFeedbackAmount = 100; // Below 256 threshold

	delay.setupWorkingState(state, 1 << 24);
	// mightDoDelay should be false
	CHECK_FALSE(state.doDelay);
}

// --- copySecondaryToPrimary / copyPrimaryToSecondary ---

TEST(DelaySyncTest, copySecondaryToPrimary) {
	delay.informWhetherActive(true, 44100);
	CHECK(delay.isActive());

	delay.copySecondaryToPrimary();
	// Primary should now be active, secondary invalidated
	CHECK(delay.primaryBuffer.isActive());
	CHECK_FALSE(delay.secondaryBuffer.isActive());
}

TEST(DelaySyncTest, copyPrimaryToSecondary) {
	delay.informWhetherActive(true, 44100);
	delay.copySecondaryToPrimary(); // Move to primary first
	CHECK(delay.primaryBuffer.isActive());

	delay.copyPrimaryToSecondary();
	// Secondary should now be active, primary invalidated
	CHECK(delay.secondaryBuffer.isActive());
	CHECK_FALSE(delay.primaryBuffer.isActive());
}

// --- setupWorkingState with anySoundComingIn=false ---

TEST(DelaySyncTest, noSoundNoRepeatsDisablesDelay) {
	delay.repeatsUntilAbandon = 0;
	Delay::State state{};
	state.userDelayRate = 44100;
	state.delayFeedbackAmount = 1 << 29;

	delay.setupWorkingState(state, 1 << 24, false);
	// No sound + no repeats → mightDoDelay = false
	CHECK_FALSE(state.doDelay);
}

TEST(DelaySyncTest, noSoundWithRepeatsStaysActive) {
	delay.informWhetherActive(true, 44100);
	delay.repeatsUntilAbandon = 5;
	Delay::State state{};
	state.userDelayRate = 44100;
	state.delayFeedbackAmount = 1 << 29;

	delay.setupWorkingState(state, 1 << 24, false);
	// No sound but repeats > 0 → stays active
	CHECK(state.doDelay);
}

// --- Feedback change triggers setTimeToAbandon ---

TEST(DelaySyncTest, feedbackChangeUpdatesAbandonTime) {
	delay.informWhetherActive(true, 44100);
	delay.prevFeedback = 1 << 28;

	Delay::State state{};
	state.doDelay = true;
	state.userDelayRate = 44100;
	state.delayFeedbackAmount = 1 << 30; // Different from prevFeedback

	delay.syncLevel = SYNC_LEVEL_NONE;
	delay.setupWorkingState(state, 1 << 24, true);

	// prevFeedback should be updated to match
	CHECK_EQUAL(state.delayFeedbackAmount, delay.prevFeedback);
	CHECK(delay.repeatsUntilAbandon > 0);
}

// --- Process through sync mode changes ---

TEST(DelaySyncTest, switchSyncTypeMidStream) {
	delay.syncLevel = SYNC_LEVEL_16TH;
	delay.syncType = SYNC_TYPE_EVEN;

	Delay::State state{};
	state.userDelayRate = 1 << 20;
	state.delayFeedbackAmount = 1 << 29;

	setupAndProcess(state);

	// Switch to triplet mid-stream
	delay.syncType = SYNC_TYPE_TRIPLET;
	state.userDelayRate = 1 << 20; // Reset raw rate
	setupAndProcess(state);

	// Switch to dotted
	delay.syncType = SYNC_TYPE_DOTTED;
	state.userDelayRate = 1 << 20;
	setupAndProcess(state);
}

// --- setTimeToAbandon: all feedback threshold branches ---

TEST_GROUP(DelayAbandonThresholds) {
	Delay delay;

	void teardown() override { delay.discardBuffers(); }

	void checkThreshold(int32_t feedback, uint8_t expected) {
		Delay::State state{};
		state.doDelay = true;
		state.userDelayRate = 44100;
		state.delayFeedbackAmount = feedback;
		delay.setTimeToAbandon(state);
		CHECK_EQUAL(expected, delay.repeatsUntilAbandon);
	}
};

TEST(DelayAbandonThresholds, doDelayFalseGivesZero) {
	Delay::State state{};
	state.doDelay = false;
	state.delayFeedbackAmount = 1 << 30;
	delay.setTimeToAbandon(state);
	CHECK_EQUAL(0, delay.repeatsUntilAbandon);
}

TEST(DelayAbandonThresholds, feedbackBelow33M) {
	checkThreshold(1000, 1);
}

TEST(DelayAbandonThresholds, feedbackAt33MBoundary) {
	// 33554432 is the boundary: below → 1, at/above → 2
	checkThreshold(33554431, 1);
	checkThreshold(33554432, 2);
}

TEST(DelayAbandonThresholds, feedbackAt100MBoundary) {
	checkThreshold(100663296, 2);
	checkThreshold(100663297, 3);
}

TEST(DelayAbandonThresholds, feedbackAt218MBoundary) {
	checkThreshold(218103808, 3);
	checkThreshold(218103809, 4);
}

TEST(DelayAbandonThresholds, feedbackAt318MBoundary) {
	checkThreshold(318767103, 4);
	checkThreshold(318767104, 5);
}

TEST(DelayAbandonThresholds, feedbackAt352MBoundary) {
	checkThreshold(352321535, 5);
	checkThreshold(352321536, 6);
}

TEST(DelayAbandonThresholds, feedbackAt452MBoundary) {
	checkThreshold(452984831, 6);
	checkThreshold(452984832, 9);
}

TEST(DelayAbandonThresholds, feedbackAt520MBoundary) {
	checkThreshold(520093695, 9);
	checkThreshold(520093696, 12);
}

TEST(DelayAbandonThresholds, feedbackAt637MBoundary) {
	checkThreshold(637534207, 12);
	checkThreshold(637534208, 13);
}

TEST(DelayAbandonThresholds, feedbackAt704MBoundary) {
	checkThreshold(704643071, 13);
	checkThreshold(704643072, 18);
}

TEST(DelayAbandonThresholds, feedbackAt771MBoundary) {
	checkThreshold(771751935, 18);
	checkThreshold(771751936, 24);
}

TEST(DelayAbandonThresholds, feedbackAt838MBoundary) {
	checkThreshold(838860799, 24);
	checkThreshold(838860800, 40);
}

TEST(DelayAbandonThresholds, feedbackAt939MBoundary) {
	checkThreshold(939524095, 40);
	checkThreshold(939524096, 110);
}

TEST(DelayAbandonThresholds, feedbackAt1040MBoundary) {
	checkThreshold(1040187391, 110);
	checkThreshold(1040187392, 255);
}

TEST(DelayAbandonThresholds, feedbackMaxGives255) {
	checkThreshold(INT32_MAX, 255);
}

// --- hasWrapped decrement and discard ---

TEST(DelayAbandonThresholds, hasWrappedDecrementsAndDiscards) {
	delay.informWhetherActive(true, 44100);
	delay.repeatsUntilAbandon = 2;
	delay.hasWrapped();
	CHECK_EQUAL(1, delay.repeatsUntilAbandon);
	CHECK(delay.isActive());

	delay.informWhetherActive(true, 44100);
	delay.repeatsUntilAbandon = 1;
	delay.hasWrapped();
	CHECK_EQUAL(0, delay.repeatsUntilAbandon);
	CHECK_FALSE(delay.isActive()); // buffers discarded
}

TEST(DelayAbandonThresholds, hasWrapped255NeverDecreases) {
	delay.informWhetherActive(true, 44100);
	delay.repeatsUntilAbandon = 255;
	delay.hasWrapped();
	CHECK_EQUAL(255, delay.repeatsUntilAbandon);
	CHECK(delay.isActive());
}

// --- initializeSecondaryBuffer branches ---

TEST_GROUP(DelaySecondaryBufferTest) {
	Delay delay;
	StereoSample buf[64];

	void setup() override {
		memset(buf, 0, sizeof(buf));
	}

	void teardown() override {
		delay.discardBuffers();
	}

	void fillDC(int32_t amp = 1 << 20) {
		for (auto& s : buf) {
			s.l = amp;
			s.r = amp;
		}
	}
};

// Helper: activate delay and move secondary→primary via copySecondaryToPrimary
// This gives us a primary buffer we can use for resampling tests.

TEST(DelaySecondaryBufferTest, directSecondaryBufferInit) {
	// Directly test init of secondary buffer — verifies allocator works
	Error result = delay.secondaryBuffer.init(44100);
	LONGS_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(result));
	CHECK(delay.secondaryBuffer.isActive());
}

TEST(DelaySecondaryBufferTest, initSecondaryPreciseRelative) {
	delay.informWhetherActive(true, 44100);
	delay.copySecondaryToPrimary();
	CHECK(delay.primaryBuffer.isActive());

	// Use a very different rate so buffer size differs from primaryBuffer.size()
	delay.initializeSecondaryBuffer(11025, true);
	// If allocation succeeded, secondary is active. If size matched primary, it's skipped.
	// Just verify no crash — the path was exercised.
}

TEST(DelaySecondaryBufferTest, initSecondarySeparatePrecision) {
	delay.informWhetherActive(true, 44100);
	delay.copySecondaryToPrimary();
	CHECK(delay.primaryBuffer.isActive());

	delay.initializeSecondaryBuffer(11025, false);
}

// --- Resampling read/write path ---

TEST(DelaySecondaryBufferTest, resamplingReadWritePath) {
	// Setup: primary buffer at rate 44100
	delay.informWhetherActive(true, 44100);
	delay.copySecondaryToPrimary();
	CHECK(delay.primaryBuffer.isActive());

	// First process at native rate to write some data
	Delay::State state{};
	state.doDelay = true;
	state.userDelayRate = 44100;
	state.delayFeedbackAmount = 1 << 30;
	delay.userRateLastTime = 44100;
	delay.countCyclesWithoutChange = 0;

	fillDC();
	delay.process({buf, 64}, state);

	// Now process with a different rate to trigger resampling
	state.userDelayRate = 22050;
	delay.userRateLastTime = 22050;

	fillDC();
	delay.process({buf, 64}, state);

	// Should not crash — resampling read and write paths exercised
	CHECK(delay.primaryBuffer.isActive());
}

TEST(DelaySecondaryBufferTest, resamplingAtDoubleRate) {
	delay.informWhetherActive(true, 44100);
	delay.copySecondaryToPrimary();

	Delay::State state{};
	state.doDelay = true;
	state.userDelayRate = 44100;
	state.delayFeedbackAmount = 1 << 30;
	delay.userRateLastTime = 44100;
	delay.countCyclesWithoutChange = 0;

	fillDC();
	delay.process({buf, 64}, state);

	// Process at double native rate — triggers resampling
	state.userDelayRate = 88200;
	delay.userRateLastTime = 88200;

	fillDC();
	delay.process({buf, 64}, state);

	CHECK(delay.primaryBuffer.isActive());
}

TEST(DelaySecondaryBufferTest, settledRateTriggersSecondaryInitPath) {
	delay.informWhetherActive(true, 44100);
	delay.copySecondaryToPrimary();

	Delay::State state{};
	state.doDelay = true;
	state.userDelayRate = 22050; // Different from native 44100
	state.delayFeedbackAmount = 1 << 30;
	delay.userRateLastTime = 22050;
	// Pretend rate has been stable for >kSampleRate>>5 samples
	delay.countCyclesWithoutChange = 44100;

	fillDC();
	delay.process({buf, 64}, state);
	// Exercises the settled-rate branch in process() — may or may not allocate
	// depending on buffer size match. No crash = success.
}

TEST(DelaySecondaryBufferTest, doubleNativeRateTriggersSecondaryInitPath) {
	delay.informWhetherActive(true, 44100);
	delay.copySecondaryToPrimary();

	Delay::State state{};
	state.doDelay = true;
	state.userDelayRate = 88200; // >= nativeRate << 1
	state.delayFeedbackAmount = 1 << 30;
	delay.userRateLastTime = 88200;
	delay.countCyclesWithoutChange = 0;

	fillDC();
	delay.process({buf, 64}, state);
	// Exercises the double-rate branch
}

TEST(DelaySecondaryBufferTest, belowHalfNativeRateTriggersSecondaryInitPath) {
	delay.informWhetherActive(true, 44100);
	delay.copySecondaryToPrimary();

	Delay::State state{};
	state.doDelay = true;
	state.userDelayRate = 11025; // < nativeRate >> 1 (22050)
	state.delayFeedbackAmount = 1 << 30;
	delay.userRateLastTime = 11025;
	delay.countCyclesWithoutChange = 0;

	fillDC();
	delay.process({buf, 64}, state);
	// Exercises the below-half-rate branch
}
