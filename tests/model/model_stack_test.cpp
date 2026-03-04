// ModelStack regression tests — exercises the firmware's real ModelStack
// hierarchy using only the inline builder methods from model_stack.h.

#include "CppUTest/TestHarness.h"
#include "model/model_stack.h"
#include "model/note/note_row.h"

TEST_GROUP(ModelStackTest) {};

TEST(ModelStackTest, setupAndChain) {
	char memory[MODEL_STACK_MAX_SIZE];

	// Build the stack: Song → TimelineCounter → NoteRow
	ModelStack* ms = setupModelStackWithSong(memory, nullptr);
	CHECK(ms != nullptr);
	CHECK(ms->song == nullptr);

	ModelStackWithTimelineCounter* msTc = ms->addTimelineCounter(nullptr);
	CHECK(msTc != nullptr);
	CHECK(msTc->getTimelineCounterAllowNull() == nullptr);

	ModelStackWithNoteRow* msNr = msTc->addNoteRow(42, nullptr);
	CHECK(msNr != nullptr);
	CHECK_EQUAL(42, msNr->noteRowId);
	CHECK(msNr->getNoteRowAllowNull() == nullptr);
}

TEST(ModelStackTest, nullSafety) {
	char memory[MODEL_STACK_MAX_SIZE];

	ModelStackWithTimelineCounter* msTc = setupModelStackWithTimelineCounter(memory, nullptr, nullptr);
	CHECK(!msTc->timelineCounterIsSet());
	CHECK(msTc->getTimelineCounterAllowNull() == nullptr);

	ModelStackWithNoteRow* msNr = msTc->addNoteRow(0, nullptr);
	CHECK(msNr->getNoteRowAllowNull() == nullptr);
}

TEST(ModelStackTest, paramBuilderChain) {
	char memory[MODEL_STACK_MAX_SIZE];

	ModelStackWithThreeMainThings* ms3 =
	    setupModelStackWithThreeMainThingsButNoNoteRow(memory, nullptr, nullptr, nullptr, nullptr);

	CHECK(ms3 != nullptr);
	CHECK(ms3->paramManager == nullptr);
	CHECK(ms3->modControllable == nullptr);

	// Continue chain to ParamCollection
	ModelStackWithParamCollection* msPc = ms3->addParamCollection(nullptr, nullptr);
	CHECK(msPc != nullptr);
	CHECK(msPc->paramCollection == nullptr);
	CHECK(msPc->summary == nullptr);

	// Continue to AutoParam
	ModelStackWithAutoParam* msAp = msPc->addAutoParam(99, nullptr);
	CHECK(msAp != nullptr);
	CHECK_EQUAL(99, msAp->paramId);
	CHECK(msAp->autoParam == nullptr);
}

TEST(ModelStackTest, memoryLayout) {
	// Verify the hierarchy sizes grow monotonically
	CHECK(sizeof(ModelStack) <= sizeof(ModelStackWithTimelineCounter));
	CHECK(sizeof(ModelStackWithTimelineCounter) <= sizeof(ModelStackWithNoteRowId));
	CHECK(sizeof(ModelStackWithNoteRowId) <= sizeof(ModelStackWithNoteRow));
	CHECK(sizeof(ModelStackWithNoteRow) <= sizeof(ModelStackWithModControllable));
	CHECK(sizeof(ModelStackWithModControllable) <= sizeof(ModelStackWithThreeMainThings));
	CHECK(sizeof(ModelStackWithThreeMainThings) <= sizeof(ModelStackWithParamCollection));
	CHECK(sizeof(ModelStackWithParamCollection) <= sizeof(ModelStackWithParamId));
	CHECK(sizeof(ModelStackWithParamId) <= sizeof(ModelStackWithAutoParam));

	// MODEL_STACK_MAX_SIZE must cover the largest
	CHECK(MODEL_STACK_MAX_SIZE >= sizeof(ModelStackWithAutoParam));
	CHECK(MODEL_STACK_MAX_SIZE >= sizeof(ModelStackWithSoundFlags));
}

TEST(ModelStackTest, timelineCounterAccessors) {
	char memory[MODEL_STACK_MAX_SIZE];

	// Use a fake pointer value (we never dereference it)
	int dummy;
	TimelineCounter* tc = (TimelineCounter*)&dummy;

	ModelStackWithTimelineCounter* msTc = setupModelStackWithTimelineCounter(memory, nullptr, tc);
	CHECK(msTc->timelineCounterIsSet());
	CHECK_EQUAL(tc, msTc->getTimelineCounter());

	// Change it
	msTc->setTimelineCounter(nullptr);
	CHECK(!msTc->timelineCounterIsSet());
}

TEST(ModelStackTest, noteRowAccessors) {
	char memory[MODEL_STACK_MAX_SIZE];

	NoteRow noteRow(60);

	ModelStackWithNoteRow* msNr =
	    setupModelStackWithSong(memory, nullptr)->addTimelineCounter(nullptr)->addNoteRow(5, &noteRow);

	CHECK_EQUAL(&noteRow, msNr->getNoteRow());
	CHECK_EQUAL(&noteRow, msNr->getNoteRowAllowNull());
	CHECK_EQUAL(5, msNr->noteRowId);

	// Change the note row
	msNr->setNoteRow(nullptr);
	CHECK(msNr->getNoteRowAllowNull() == nullptr);
}

TEST(ModelStackTest, copyModelStackBuffer) {
	char memory1[MODEL_STACK_MAX_SIZE];
	char memory2[MODEL_STACK_MAX_SIZE];

	int dummySong;
	ModelStack* ms = setupModelStackWithSong(memory1, (Song*)&dummySong);

	copyModelStack(memory2, memory1, sizeof(ModelStack));
	ModelStack* msCopy = (ModelStack*)memory2;
	CHECK_EQUAL(ms->song, msCopy->song);
}

TEST(ModelStackTest, setupHelpers) {
	char memory[MODEL_STACK_MAX_SIZE];

	int dummySong, dummyTc;

	// setupModelStackWithTimelineCounter
	ModelStackWithTimelineCounter* msTc =
	    setupModelStackWithTimelineCounter(memory, (Song*)&dummySong, (TimelineCounter*)&dummyTc);
	CHECK_EQUAL((Song*)&dummySong, msTc->song);
	CHECK_EQUAL((TimelineCounter*)&dummyTc, msTc->getTimelineCounter());

	// setupModelStackWithModControllable
	int dummyMc;
	ModelStackWithModControllable* msMc =
	    setupModelStackWithModControllable(memory, (Song*)&dummySong, (TimelineCounter*)&dummyTc,
	                                       (ModControllable*)&dummyMc);
	CHECK_EQUAL((ModControllable*)&dummyMc, msMc->modControllable);
}
