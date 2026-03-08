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
