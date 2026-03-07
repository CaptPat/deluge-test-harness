// Tests for ConsequencePerformanceViewPress:
// Stores before/after FXColumnPress state for a single column,
// and reverts by copying the appropriate state back.

#include "CppUTest/TestHarness.h"
#include "model/consequence/consequence_performance_view_press.h"
#include "model/model_stack.h"
#include "song_mock.h"

static uint8_t modelStackMemory[MODEL_STACK_MAX_SIZE] __attribute__((aligned(4)));

static ModelStack* makeModelStack() {
	auto* ms = (ModelStack*)modelStackMemory;
	ms->song = currentSong;
	return ms;
}

TEST_GROUP(ConsequencePerformanceViewPress) {
	FXColumnPress before[kDisplayWidth];
	FXColumnPress after[kDisplayWidth];

	void setup() {
		memset(before, 0, sizeof(before));
		memset(after, 0, sizeof(after));
		memset(performanceView.fxPress, 0, sizeof(performanceView.fxPress));
	}
};

TEST(ConsequencePerformanceViewPress, revertBeforeRestoresBeforeState) {
	before[3].currentKnobPosition = 42;
	before[3].yDisplay = 5;
	after[3].currentKnobPosition = 99;
	after[3].yDisplay = 7;

	ConsequencePerformanceViewPress consequence(before, after, 3);

	ModelStack* ms = makeModelStack();
	Error err = consequence.revert(BEFORE, ms);
	CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
	CHECK_EQUAL(42, performanceView.fxPress[3].currentKnobPosition);
	CHECK_EQUAL(5, performanceView.fxPress[3].yDisplay);
}

TEST(ConsequencePerformanceViewPress, revertAfterRestoresAfterState) {
	before[3].currentKnobPosition = 42;
	after[3].currentKnobPosition = 99;
	after[3].yDisplay = 7;

	ConsequencePerformanceViewPress consequence(before, after, 3);

	ModelStack* ms = makeModelStack();
	Error err = consequence.revert(AFTER, ms);
	CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
	CHECK_EQUAL(99, performanceView.fxPress[3].currentKnobPosition);
	CHECK_EQUAL(7, performanceView.fxPress[3].yDisplay);
}

TEST(ConsequencePerformanceViewPress, undoRedoCycleRestoresState) {
	before[0].currentKnobPosition = 10;
	before[0].previousKnobPosition = 5;
	after[0].currentKnobPosition = 80;
	after[0].previousKnobPosition = 40;

	ConsequencePerformanceViewPress consequence(before, after, 0);

	ModelStack* ms = makeModelStack();

	consequence.revert(BEFORE, ms);
	CHECK_EQUAL(10, performanceView.fxPress[0].currentKnobPosition);
	CHECK_EQUAL(5, performanceView.fxPress[0].previousKnobPosition);

	consequence.revert(AFTER, ms);
	CHECK_EQUAL(80, performanceView.fxPress[0].currentKnobPosition);
	CHECK_EQUAL(40, performanceView.fxPress[0].previousKnobPosition);
}

TEST(ConsequencePerformanceViewPress, differentColumnsAreIndependent) {
	before[2].currentKnobPosition = 20;
	after[2].currentKnobPosition = 60;
	performanceView.fxPress[5].currentKnobPosition = 999;

	ConsequencePerformanceViewPress consequence(before, after, 2);

	ModelStack* ms = makeModelStack();
	consequence.revert(BEFORE, ms);
	CHECK_EQUAL(20, performanceView.fxPress[2].currentKnobPosition);
	// Column 5 should be untouched
	CHECK_EQUAL(999, performanceView.fxPress[5].currentKnobPosition);
}

TEST(ConsequencePerformanceViewPress, padPressHeldIsPreserved) {
	before[1].padPressHeld = true;
	after[1].padPressHeld = false;

	ConsequencePerformanceViewPress consequence(before, after, 1);

	ModelStack* ms = makeModelStack();
	consequence.revert(BEFORE, ms);
	CHECK(performanceView.fxPress[1].padPressHeld);

	consequence.revert(AFTER, ms);
	CHECK_FALSE(performanceView.fxPress[1].padPressHeld);
}
