// Phase 14: Tests for ClipInstance and Output consequence classes
// ConsequenceClipInstanceChange, ConsequenceClipInstanceExistence, ConsequenceOutputExistence

#include "CppUTest/TestHarness.h"
#include "model/clip/clip_instance.h"
#include "model/consequence/consequence_clip_instance_change.h"
#include "model/consequence/consequence_clip_instance_existence.h"
#include "model/consequence/consequence_output_existence.h"
#include "model/model_stack.h"
#include "model/output.h"
#include "model/song/song.h"
#include <cstring>

// Concrete Output subclass for testing (Output has pure virtuals)
class MockOutput : public Output {
public:
	MockOutput() : Output(OutputType::SYNTH) {}
	bool matchesPreset(OutputType, int32_t, int32_t, char const*, char const*) override { return false; }
	void renderOutput(ModelStack*, std::span<StereoSample>, int32_t*, int32_t, int32_t, bool, bool) override {}
	char const* getXMLTag() override { return "test"; }
	ModelStackWithAutoParam* getModelStackWithParam(ModelStackWithTimelineCounter*, Clip*, int32_t,
	                                                deluge::modulation::params::Kind, bool, bool) override {
		return nullptr;
	}
	Clip* createNewClipForArrangementRecording(ModelStack*) override { return nullptr; }
};

// Helper to insert a ClipInstance into the vector at a given pos
static ClipInstance* insertInstance(MockOutput& output, int32_t pos, int32_t length, Clip* clip) {
	int32_t i = output.clipInstances.insertAtKey(pos);
	ClipInstance* ci = output.clipInstances.getElement(i);
	if (ci) {
		ci->pos = pos;
		ci->length = length;
		ci->clip = clip;
	}
	return ci;
}

// ── ConsequenceClipInstanceChange ───────────────────────────────────────

TEST_GROUP(ConsequenceClipInstanceChange) {
	MockOutput output;
	Song song;
	char mem[MODEL_STACK_MAX_SIZE];
	ModelStack* modelStack;
	Clip clipA;
	Clip clipB;

	void setup() {
		memset(mem, 0, sizeof(mem));
		modelStack = (ModelStack*)mem;
		modelStack->song = &song;
	}
};

TEST(ConsequenceClipInstanceChange, constructorCapturesBeforeState) {
	ClipInstance* ci = insertInstance(output, 100, 200, &clipA);

	ConsequenceClipInstanceChange c(&output, ci, 150, 300, &clipB);
	CHECK_EQUAL(100, c.pos[BEFORE]);
	CHECK_EQUAL(200, c.length[BEFORE]);
	POINTERS_EQUAL(&clipA, c.clip[BEFORE]);
	CHECK_EQUAL(150, c.pos[AFTER]);
	CHECK_EQUAL(300, c.length[AFTER]);
	POINTERS_EQUAL(&clipB, c.clip[AFTER]);
}

TEST(ConsequenceClipInstanceChange, revertBeforeRestoresOriginal) {
	ClipInstance* ci = insertInstance(output, 100, 200, &clipA);

	ConsequenceClipInstanceChange c(&output, ci, 150, 300, &clipB);

	// Simulate the forward action: apply the "after" values
	ci->pos = 150;
	ci->length = 300;
	ci->clip = &clipB;

	// Revert to BEFORE
	Error err = c.revert(BEFORE, modelStack);
	CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));

	// The instance at pos 150 should now have BEFORE values
	ClipInstance* reverted = output.clipInstances.getElement(
	    output.clipInstances.search(100, GREATER_OR_EQUAL));
	CHECK(reverted != nullptr);
	CHECK_EQUAL(100, reverted->pos);
	CHECK_EQUAL(200, reverted->length);
	POINTERS_EQUAL(&clipA, reverted->clip);
}

TEST(ConsequenceClipInstanceChange, revertAfterReappliesChange) {
	ClipInstance* ci = insertInstance(output, 100, 200, &clipA);

	ConsequenceClipInstanceChange c(&output, ci, 150, 300, &clipB);

	// Instance still at original pos=100 (undo scenario)
	Error err = c.revert(AFTER, modelStack);
	CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));

	ClipInstance* changed = output.clipInstances.getElement(
	    output.clipInstances.search(150, GREATER_OR_EQUAL));
	CHECK(changed != nullptr);
	CHECK_EQUAL(150, changed->pos);
	CHECK_EQUAL(300, changed->length);
	POINTERS_EQUAL(&clipB, changed->clip);
}

TEST(ConsequenceClipInstanceChange, roundTripUndoRedo) {
	ClipInstance* ci = insertInstance(output, 0, 100, &clipA);
	ConsequenceClipInstanceChange c(&output, ci, 50, 150, &clipB);

	// Apply forward
	ci->pos = 50;
	ci->length = 150;
	ci->clip = &clipB;

	// Undo
	c.revert(BEFORE, modelStack);
	ClipInstance* r = output.clipInstances.getElement(
	    output.clipInstances.search(0, GREATER_OR_EQUAL));
	CHECK_EQUAL(0, r->pos);
	CHECK_EQUAL(100, r->length);

	// Redo
	c.revert(AFTER, modelStack);
	r = output.clipInstances.getElement(
	    output.clipInstances.search(50, GREATER_OR_EQUAL));
	CHECK_EQUAL(50, r->pos);
	CHECK_EQUAL(150, r->length);
}

// ── ConsequenceClipInstanceExistence ────────────────────────────────────

TEST_GROUP(ConsequenceClipInstanceExistence) {
	MockOutput output;
	Song song;
	char mem[MODEL_STACK_MAX_SIZE];
	ModelStack* modelStack;
	Clip clip;

	void setup() {
		memset(mem, 0, sizeof(mem));
		modelStack = (ModelStack*)mem;
		modelStack->song = &song;
	}
};

TEST(ConsequenceClipInstanceExistence, constructorCapturesState) {
	ClipInstance* ci = insertInstance(output, 200, 400, &clip);

	ConsequenceClipInstanceExistence c(&output, ci, ExistenceChangeType::CREATE);
	POINTERS_EQUAL(&output, c.output);
	POINTERS_EQUAL(&clip, c.clip);
	CHECK_EQUAL(200, c.pos);
	CHECK_EQUAL(400, c.length);
}

TEST(ConsequenceClipInstanceExistence, undoCreateDeletesInstance) {
	ClipInstance* ci = insertInstance(output, 100, 200, &clip);
	CHECK_EQUAL(1, output.clipInstances.getNumElements());

	// Consequence records that this was a CREATE
	ConsequenceClipInstanceExistence c(&output, ci, ExistenceChangeType::CREATE);

	// Undo the creation (revert to BEFORE) → should delete
	Error err = c.revert(BEFORE, modelStack);
	CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
	CHECK_EQUAL(0, output.clipInstances.getNumElements());
}

TEST(ConsequenceClipInstanceExistence, redoCreateReInsertsInstance) {
	ClipInstance* ci = insertInstance(output, 100, 200, &clip);
	ConsequenceClipInstanceExistence c(&output, ci, ExistenceChangeType::CREATE);

	// Undo (delete)
	c.revert(BEFORE, modelStack);
	CHECK_EQUAL(0, output.clipInstances.getNumElements());

	// Redo (re-create)
	Error err = c.revert(AFTER, modelStack);
	CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
	CHECK_EQUAL(1, output.clipInstances.getNumElements());

	ClipInstance* recreated = output.clipInstances.getElement(0);
	CHECK(recreated != nullptr);
	CHECK_EQUAL(100, recreated->pos);
	CHECK_EQUAL(200, recreated->length);
	POINTERS_EQUAL(&clip, recreated->clip);
}

TEST(ConsequenceClipInstanceExistence, undoDeleteReCreatesInstance) {
	ClipInstance* ci = insertInstance(output, 300, 100, &clip);
	ConsequenceClipInstanceExistence c(&output, ci, ExistenceChangeType::DELETE);

	// The instance has already been deleted by the action
	output.clipInstances.deleteAtIndex(0);
	CHECK_EQUAL(0, output.clipInstances.getNumElements());

	// Undo the deletion → should re-create
	Error err = c.revert(BEFORE, modelStack);
	CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
	CHECK_EQUAL(1, output.clipInstances.getNumElements());

	ClipInstance* restored = output.clipInstances.getElement(0);
	CHECK_EQUAL(300, restored->pos);
	CHECK_EQUAL(100, restored->length);
}

TEST(ConsequenceClipInstanceExistence, redoDeleteRemovesAgain) {
	ClipInstance* ci = insertInstance(output, 300, 100, &clip);
	ConsequenceClipInstanceExistence c(&output, ci, ExistenceChangeType::DELETE);

	// Delete was done
	output.clipInstances.deleteAtIndex(0);

	// Undo
	c.revert(BEFORE, modelStack);
	CHECK_EQUAL(1, output.clipInstances.getNumElements());

	// Redo (re-delete)
	Error err = c.revert(AFTER, modelStack);
	CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
	CHECK_EQUAL(0, output.clipInstances.getNumElements());
}

TEST(ConsequenceClipInstanceExistence, multipleInstancesOrdering) {
	insertInstance(output, 0, 100, nullptr);
	ClipInstance* ci2 = insertInstance(output, 200, 100, &clip);
	insertInstance(output, 400, 100, nullptr);
	CHECK_EQUAL(3, output.clipInstances.getNumElements());

	// Delete the middle one
	ConsequenceClipInstanceExistence c(&output, ci2, ExistenceChangeType::DELETE);
	output.clipInstances.deleteAtIndex(1);
	CHECK_EQUAL(2, output.clipInstances.getNumElements());

	// Undo → middle one comes back
	c.revert(BEFORE, modelStack);
	CHECK_EQUAL(3, output.clipInstances.getNumElements());

	// Verify ordering is correct
	CHECK_EQUAL(0, output.clipInstances.getElement(0)->pos);
	CHECK_EQUAL(200, output.clipInstances.getElement(1)->pos);
	CHECK_EQUAL(400, output.clipInstances.getElement(2)->pos);
	POINTERS_EQUAL(&clip, output.clipInstances.getElement(1)->clip);
}

// ── ConsequenceOutputExistence ──────────────────────────────────────────

TEST_GROUP(ConsequenceOutputExistence) {
	Song song;
	char mem[MODEL_STACK_MAX_SIZE];
	ModelStack* modelStack;
	MockOutput outputA;
	MockOutput outputB;
	MockOutput outputC;

	void setup() {
		memset(mem, 0, sizeof(mem));
		modelStack = (ModelStack*)mem;
		modelStack->song = &song;
		song.firstOutput = nullptr;
		outputA.next = nullptr;
		outputB.next = nullptr;
		outputC.next = nullptr;
	}
};

TEST(ConsequenceOutputExistence, constructorStoresFields) {
	ConsequenceOutputExistence c(&outputA, ExistenceChangeType::CREATE);
	POINTERS_EQUAL(&outputA, c.output);
}

TEST(ConsequenceOutputExistence, undoCreateRemovesOutput) {
	song.addOutput(&outputA);
	CHECK(song.firstOutput == &outputA);

	ConsequenceOutputExistence c(&outputA, ExistenceChangeType::CREATE);

	// Undo the creation → should remove from song
	Error err = c.revert(BEFORE, modelStack);
	CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
	POINTERS_EQUAL(nullptr, song.firstOutput);
}

TEST(ConsequenceOutputExistence, redoCreateReAddsOutput) {
	ConsequenceOutputExistence c(&outputA, ExistenceChangeType::CREATE);

	// Redo → add back
	Error err = c.revert(AFTER, modelStack);
	CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
	POINTERS_EQUAL(&outputA, song.firstOutput);
}

TEST(ConsequenceOutputExistence, undoDeleteReAddsOutput) {
	// Output was deleted from song
	ConsequenceOutputExistence c(&outputA, ExistenceChangeType::DELETE);

	// Undo → re-add
	Error err = c.revert(BEFORE, modelStack);
	CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
	POINTERS_EQUAL(&outputA, song.firstOutput);
}

TEST(ConsequenceOutputExistence, redoDeleteRemovesOutput) {
	song.addOutput(&outputA);
	ConsequenceOutputExistence c(&outputA, ExistenceChangeType::DELETE);

	// Redo the deletion
	Error err = c.revert(AFTER, modelStack);
	CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
	POINTERS_EQUAL(nullptr, song.firstOutput);
}

TEST(ConsequenceOutputExistence, roundTripCreateUndoRedo) {
	song.addOutput(&outputA);
	ConsequenceOutputExistence c(&outputA, ExistenceChangeType::CREATE);

	// Undo
	c.revert(BEFORE, modelStack);
	POINTERS_EQUAL(nullptr, song.firstOutput);

	// Redo
	c.revert(AFTER, modelStack);
	POINTERS_EQUAL(&outputA, song.firstOutput);
}

TEST(ConsequenceOutputExistence, multipleOutputsRemoveMiddle) {
	song.addOutput(&outputA);
	song.addOutput(&outputB);
	song.addOutput(&outputC);
	// List is C -> B -> A

	ConsequenceOutputExistence c(&outputB, ExistenceChangeType::CREATE);

	// Undo removes B from middle
	c.revert(BEFORE, modelStack);
	POINTERS_EQUAL(&outputC, song.firstOutput);
	POINTERS_EQUAL(&outputA, outputC.next);
	POINTERS_EQUAL(nullptr, outputA.next);

	// Redo adds B back (at start)
	c.revert(AFTER, modelStack);
	POINTERS_EQUAL(&outputB, song.firstOutput);
}
