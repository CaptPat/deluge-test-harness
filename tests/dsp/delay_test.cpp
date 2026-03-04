#include "CppUTest/TestHarness.h"
#include "dsp/delay/delay.h"

TEST_GROUP(DelayTest) {
	Delay delay;

	void teardown() override {
		delay.discardBuffers();
	}
};

TEST(DelayTest, defaultInactive) {
	CHECK_FALSE(delay.isActive());
}

TEST(DelayTest, informWhetherActiveAllocates) {
	delay.informWhetherActive(true, 44100);
	// After enabling, secondary buffer should be set up
	// (primary won't be active until writing begins)
	CHECK(delay.isActive());
}

TEST(DelayTest, informWhetherActiveDeactivates) {
	delay.informWhetherActive(true, 44100);
	CHECK(delay.isActive());
	delay.informWhetherActive(false, 44100);
	CHECK_FALSE(delay.isActive());
}

TEST(DelayTest, discardBuffersResetsState) {
	delay.informWhetherActive(true, 44100);
	delay.discardBuffers();
	CHECK_FALSE(delay.isActive());
}

TEST(DelayTest, setTimeToAbandonZeroFeedback) {
	delay.informWhetherActive(true, 44100);
	Delay::State state;
	state.doDelay = true;
	state.delayFeedbackAmount = 0;
	state.userDelayRate = 44100;
	delay.setTimeToAbandon(state);
	// Zero feedback should give minimal repeats
	CHECK(delay.repeatsUntilAbandon <= 1);
}

TEST(DelayTest, setTimeToAbandonHighFeedback) {
	delay.informWhetherActive(true, 44100);
	Delay::State state;
	state.doDelay = true;
	state.delayFeedbackAmount = 1100000000; // Very high
	state.userDelayRate = 44100;
	delay.setTimeToAbandon(state);
	// High feedback should give many repeats
	CHECK(delay.repeatsUntilAbandon > 100);
}

TEST(DelayTest, hasWrappedDecrementsRepeats) {
	delay.informWhetherActive(true, 44100);
	delay.repeatsUntilAbandon = 3;
	delay.hasWrapped();
	CHECK_EQUAL(2, delay.repeatsUntilAbandon);
}

TEST(DelayTest, hasWrappedMaxDoesNotDecrement) {
	delay.repeatsUntilAbandon = 255;
	delay.hasWrapped();
	CHECK_EQUAL(255, delay.repeatsUntilAbandon);
}

TEST(DelayTest, hasWrappedDiscardsAtZero) {
	delay.informWhetherActive(true, 44100);
	delay.repeatsUntilAbandon = 1;
	delay.hasWrapped();
	CHECK_EQUAL(0, delay.repeatsUntilAbandon);
	CHECK_FALSE(delay.isActive());
}
