// Tests for ConsequenceInstrumentClipMultiply:
// Undo halves clip length and calls halveNoteRowsWithIndependentLength.
// Redo doubles clip length via Song::doubleClipLength.

#include "CppUTest/TestHarness.h"
#include "model/consequence/consequence_instrument_clip_multiply.h"
#include "model/model_stack.h"
#include "clip_mocks.h"
#include "song_mock.h"

static uint8_t modelStackMemory[MODEL_STACK_MAX_SIZE] __attribute__((aligned(4)));

static ModelStack* makeModelStack() {
	auto* ms = (ModelStack*)modelStackMemory;
	ms->song = currentSong;
	return ms;
}

TEST_GROUP(ConsequenceInstrumentClipMultiply) {
	InstrumentClip clip;

	void setup() {
		clip.loopLength = 1000;
		clip.halveNoteRowsCalled = false;
		currentSong->currentClip = &clip;
	}
};

TEST(ConsequenceInstrumentClipMultiply, undoHalvesClipLength) {
	ConsequenceInstrumentClipMultiply consequence;
	ModelStack* ms = makeModelStack();

	Error err = consequence.revert(BEFORE, ms);
	CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
	CHECK_EQUAL(500, clip.loopLength);
}

TEST(ConsequenceInstrumentClipMultiply, undoCallsHalveNoteRows) {
	ConsequenceInstrumentClipMultiply consequence;
	ModelStack* ms = makeModelStack();

	consequence.revert(BEFORE, ms);
	CHECK(clip.halveNoteRowsCalled);
}

TEST(ConsequenceInstrumentClipMultiply, redoDoublesClipLength) {
	ConsequenceInstrumentClipMultiply consequence;
	ModelStack* ms = makeModelStack();

	Error err = consequence.revert(AFTER, ms);
	CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
	CHECK_EQUAL(2000, clip.loopLength);
}

TEST(ConsequenceInstrumentClipMultiply, undoThenRedoRestoresOriginalLength) {
	ConsequenceInstrumentClipMultiply consequence;
	ModelStack* ms = makeModelStack();

	consequence.revert(BEFORE, ms); // 1000 → 500
	CHECK_EQUAL(500, clip.loopLength);

	consequence.revert(AFTER, ms); // 500 → 1000
	CHECK_EQUAL(1000, clip.loopLength);
}

TEST(ConsequenceInstrumentClipMultiply, undoWithOddLengthTruncates) {
	clip.loopLength = 999;
	ConsequenceInstrumentClipMultiply consequence;
	ModelStack* ms = makeModelStack();

	consequence.revert(BEFORE, ms);
	CHECK_EQUAL(499, clip.loopLength); // 999 >> 1 = 499
}
