// Phase 11: Consequence clip/noterow tests
// Tests ConsequenceClipHorizontalShift, ConsequenceClipLength, ConsequenceNoteRowLength

#include "CppUTest/TestHarness.h"
#include "model/consequence/consequence_clip_horizontal_shift.h"
#include "model/consequence/consequence_clip_length.h"
#include "model/consequence/consequence_note_row_length.h"
#include "model/model_stack.h"
#include "model/note/note_row.h"
#include "model/song/song.h"
#include <cstring>

// ── Helper: track shiftHorizontally calls ──────────────────────────────

static int32_t lastShiftAmount;
static bool lastShiftAutomation;
static bool lastShiftSequenceAndMPE;

class TrackingClip : public Clip {
public:
	void shiftHorizontally(ModelStackWithTimelineCounter* ms, int32_t amount, bool shiftAutomation,
	                       bool shiftSequenceAndMPE) override {
		(void)ms;
		lastShiftAmount = amount;
		lastShiftAutomation = shiftAutomation;
		lastShiftSequenceAndMPE = shiftSequenceAndMPE;
	}
};

// ── ConsequenceClipHorizontalShift ─────────────────────────────────────

TEST_GROUP(ConsequenceClipHShift) {
	Song song;
	TrackingClip clip;
	char mem[MODEL_STACK_MAX_SIZE];
	ModelStack* modelStack;

	void setup() {
		memset(mem, 0, sizeof(mem));
		modelStack = (ModelStack*)mem;
		song.currentClip = &clip;
		modelStack->song = &song;
		lastShiftAmount = 0;
		lastShiftAutomation = false;
		lastShiftSequenceAndMPE = false;
	}
};

TEST(ConsequenceClipHShift, constructorStoresFields) {
	ConsequenceClipHorizontalShift c(100, true, false);
	CHECK_EQUAL(100, c.amount);
	CHECK(c.shiftAutomation);
	CHECK(!c.shiftSequenceAndMPE);
}

TEST(ConsequenceClipHShift, revertAfterShiftsPositive) {
	ConsequenceClipHorizontalShift c(42, true, true);
	c.revert(AFTER, modelStack);
	CHECK_EQUAL(42, lastShiftAmount);
	CHECK(lastShiftAutomation);
	CHECK(lastShiftSequenceAndMPE);
}

TEST(ConsequenceClipHShift, revertBeforeShiftsNegated) {
	ConsequenceClipHorizontalShift c(42, false, true);
	c.revert(BEFORE, modelStack);
	CHECK_EQUAL(-42, lastShiftAmount);
	CHECK(!lastShiftAutomation);
	CHECK(lastShiftSequenceAndMPE);
}

TEST(ConsequenceClipHShift, revertReturnsNone) {
	ConsequenceClipHorizontalShift c(1, false, false);
	Error err = c.revert(AFTER, modelStack);
	CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
}

// ── ConsequenceClipLength ──────────────────────────────────────────────

TEST_GROUP(ConsequenceClipLength) {
	Song song;
	Clip clip;
	ModelStack modelStack;

	void setup() {
		clip.loopLength = 0;
		song.currentClip = &clip;
		modelStack.song = &song;
	}
};

TEST(ConsequenceClipLength, constructorStoresFields) {
	ConsequenceClipLength c(&clip, 128);
	CHECK_EQUAL(Consequence::CLIP_LENGTH, c.type);
	POINTERS_EQUAL(&clip, c.clip);
	CHECK_EQUAL(128, c.lengthToRevertTo);
	POINTERS_EQUAL(nullptr, c.pointerToMarkerValue);
}

TEST(ConsequenceClipLength, revertSwapsLength) {
	clip.loopLength = 200;
	ConsequenceClipLength c(&clip, 100);

	c.revert(BEFORE, &modelStack);
	// setClipLength sets clip.loopLength to 100, lengthToRevertTo becomes 200
	CHECK_EQUAL(100, clip.loopLength);
	CHECK_EQUAL(200, c.lengthToRevertTo);
}

TEST(ConsequenceClipLength, revertRoundTrip) {
	clip.loopLength = 200;
	ConsequenceClipLength c(&clip, 100);

	c.revert(BEFORE, &modelStack);
	CHECK_EQUAL(100, clip.loopLength);

	c.revert(AFTER, &modelStack);
	CHECK_EQUAL(200, clip.loopLength);
}

TEST(ConsequenceClipLength, revertReturnsNone) {
	clip.loopLength = 50;
	ConsequenceClipLength c(&clip, 25);
	Error err = c.revert(BEFORE, &modelStack);
	CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
}

// ── ConsequenceNoteRowLength ───────────────────────────────────────────

TEST_GROUP(ConsequenceNoteRowLength){};

TEST(ConsequenceNoteRowLength, constructorStoresFields) {
	ConsequenceNoteRowLength c(5, 128);
	CHECK_EQUAL(5, c.noteRowId);
	CHECK_EQUAL(128, c.backedUpLength);
}

TEST(ConsequenceNoteRowLength, performChangeSwapsLength) {
	NoteRow nr(60);
	nr.loopLengthIfIndependent = 200;

	char mem[MODEL_STACK_MAX_SIZE];
	memset(mem, 0, sizeof(mem));
	auto* ms = (ModelStackWithNoteRow*)mem;
	ms->setNoteRow(&nr);

	ConsequenceNoteRowLength c(0, 100);
	c.performChange(ms, nullptr, 0, false);

	// setLength sets loopLengthIfIndependent to 100, backedUpLength becomes 200
	CHECK_EQUAL(100, nr.loopLengthIfIndependent);
	CHECK_EQUAL(200, c.backedUpLength);
}

TEST(ConsequenceNoteRowLength, performChangeRoundTrip) {
	NoteRow nr(60);
	nr.loopLengthIfIndependent = 200;

	char mem[MODEL_STACK_MAX_SIZE];
	memset(mem, 0, sizeof(mem));
	auto* ms = (ModelStackWithNoteRow*)mem;
	ms->setNoteRow(&nr);

	ConsequenceNoteRowLength c(0, 100);

	c.performChange(ms, nullptr, 0, false);
	CHECK_EQUAL(100, nr.loopLengthIfIndependent);

	c.performChange(ms, nullptr, 0, false);
	CHECK_EQUAL(200, nr.loopLengthIfIndependent);
}
