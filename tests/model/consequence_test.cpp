// Phase 10A: Consequence system tests
// Tests consequence subclasses: swing change, tempo change, scale add note,
// param change, begin playback

#include "CppUTest/TestHarness.h"
#include "model/consequence/consequence.h"
#include "model/consequence/consequence_begin_playback.h"
#include "model/consequence/consequence_param_change.h"
#include "model/consequence/consequence_scale_add_note.h"
#include "model/consequence/consequence_swing_change.h"
#include "model/consequence/consequence_tempo_change.h"
#include "model/model_stack.h"
#include "model/song/song.h"
#include "playback/playback_handler.h"
#include <cstring>

// ── Base Consequence ────────────────────────────────────────────────────

TEST_GROUP(ConsequenceBase){};

TEST(ConsequenceBase, constructorSetsTypeZero) {
	// Test via a concrete subclass (Consequence has pure virtual revert)
	ConsequenceSwingChange c(0, 0);
	CHECK_EQUAL(0, c.type);
}

// ── ConsequenceSwingChange ──────────────────────────────────────────────

TEST_GROUP(ConsequenceSwing){};

TEST(ConsequenceSwing, constructorStoresSwingValues) {
	ConsequenceSwingChange c(10, 25);
	CHECK_EQUAL(10, c.swing[BEFORE]);
	CHECK_EQUAL(25, c.swing[AFTER]);
}

TEST(ConsequenceSwing, constructorNegativeValues) {
	ConsequenceSwingChange c(-50, -10);
	CHECK_EQUAL(-50, c.swing[BEFORE]);
	CHECK_EQUAL(-10, c.swing[AFTER]);
}

TEST(ConsequenceSwing, revertBeforeSetsSwing) {
	Song song;
	song.swingAmount = 99;

	ModelStack modelStack;
	modelStack.song = &song;

	ConsequenceSwingChange c(10, 25);
	Error err = c.revert(BEFORE, &modelStack);

	CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
	CHECK_EQUAL(10, song.swingAmount);
}

TEST(ConsequenceSwing, revertAfterSetsSwing) {
	Song song;
	song.swingAmount = 0;

	ModelStack modelStack;
	modelStack.song = &song;

	ConsequenceSwingChange c(10, 25);
	Error err = c.revert(AFTER, &modelStack);

	CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
	CHECK_EQUAL(25, song.swingAmount);
}

TEST(ConsequenceSwing, revertRoundTrip) {
	Song song;
	song.swingAmount = 50;

	ModelStack modelStack;
	modelStack.song = &song;

	ConsequenceSwingChange c(10, 50);

	c.revert(BEFORE, &modelStack);
	CHECK_EQUAL(10, song.swingAmount);

	c.revert(AFTER, &modelStack);
	CHECK_EQUAL(50, song.swingAmount);
}

// ── ConsequenceTempoChange ──────────────────────────────────────────────

TEST_GROUP(ConsequenceTempo){};

TEST(ConsequenceTempo, constructorStoresTimePerBig) {
	ConsequenceTempoChange c(1000ULL, 2000ULL);
	CHECK_EQUAL(1000ULL, c.timePerBig[BEFORE]);
	CHECK_EQUAL(2000ULL, c.timePerBig[AFTER]);
}

TEST(ConsequenceTempo, constructorLargeValues) {
	uint64_t big1 = 0xFFFFFFFF00000000ULL;
	uint64_t big2 = 0x0000000100000000ULL;
	ConsequenceTempoChange c(big1, big2);
	CHECK(c.timePerBig[BEFORE] == big1);
	CHECK(c.timePerBig[AFTER] == big2);
}

TEST(ConsequenceTempo, revertBeforeReturnsNone) {
	Song song;
	ModelStack modelStack;
	modelStack.song = &song;

	ConsequenceTempoChange c(1000ULL, 2000ULL);
	Error err = c.revert(BEFORE, &modelStack);
	CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
}

TEST(ConsequenceTempo, revertAfterReturnsNone) {
	Song song;
	ModelStack modelStack;
	modelStack.song = &song;

	ConsequenceTempoChange c(1000ULL, 2000ULL);
	Error err = c.revert(AFTER, &modelStack);
	CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
}

// ── ConsequenceScaleAddNote ─────────────────────────────────────────────

TEST_GROUP(ConsequenceScaleAddNote){};

TEST(ConsequenceScaleAddNote, constructorStoresNote) {
	ConsequenceScaleAddNote c(7);
	CHECK_EQUAL(7, c.noteWithinOctave);
}

TEST(ConsequenceScaleAddNote, constructorZero) {
	ConsequenceScaleAddNote c(0);
	CHECK_EQUAL(0, c.noteWithinOctave);
}

TEST(ConsequenceScaleAddNote, revertBeforeReturnsNone) {
	Song song;
	ModelStack modelStack;
	modelStack.song = &song;

	ConsequenceScaleAddNote c(5);
	Error err = c.revert(BEFORE, &modelStack);
	CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
}

TEST(ConsequenceScaleAddNote, revertAfterReturnsNone) {
	Song song;
	ModelStack modelStack;
	modelStack.song = &song;

	ConsequenceScaleAddNote c(5);
	Error err = c.revert(AFTER, &modelStack);
	CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
}

// ── ConsequenceParamChange ──────────────────────────────────────────────

TEST_GROUP(ConsequenceParamChange){};

TEST(ConsequenceParamChange, typeIsParamChange) {
	char mem[MODEL_STACK_MAX_SIZE];
	memset(mem, 0, sizeof(mem));
	auto* ms = (ModelStackWithAutoParam*)mem;

	AutoParam ap;
	ms->autoParam = &ap;
	ms->paramCollection = nullptr;
	ms->summary = nullptr;
	ms->paramId = 0;
	ms->song = nullptr;

	ConsequenceParamChange c(ms, false);
	CHECK_EQUAL(Consequence::PARAM_CHANGE, c.type);
}

// ── ConsequenceBeginPlayback ────────────────────────────────────────────

TEST_GROUP(ConsequenceBeginPlayback){
    void setup(){
        playbackHandler.playbackState = 0;
playbackHandler.recording = RecordingMode::OFF;
}
}
;

TEST(ConsequenceBeginPlayback, constructorCreatesInstance) {
	ConsequenceBeginPlayback c;
	CHECK_EQUAL(0, c.type);
}

TEST(ConsequenceBeginPlayback, revertBeforeEndsPlaybackIfActive) {
	playbackHandler.playbackState = 1;

	Song song;
	ModelStack modelStack;
	modelStack.song = &song;

	ConsequenceBeginPlayback c;
	c.revert(BEFORE, &modelStack);

	CHECK_EQUAL(0, playbackHandler.playbackState);
}

TEST(ConsequenceBeginPlayback, revertBeforeNoOpIfStopped) {
	playbackHandler.playbackState = 0;

	Song song;
	ModelStack modelStack;
	modelStack.song = &song;

	ConsequenceBeginPlayback c;
	c.revert(BEFORE, &modelStack);

	CHECK_EQUAL(0, playbackHandler.playbackState);
}

TEST(ConsequenceBeginPlayback, revertAfterStartsPlayback) {
	playbackHandler.playbackState = 0;

	Song song;
	ModelStack modelStack;
	modelStack.song = &song;

	ConsequenceBeginPlayback c;
	c.revert(AFTER, &modelStack);

	CHECK_EQUAL(1, playbackHandler.playbackState);
}

TEST(ConsequenceBeginPlayback, revertAfterNoOpIfAlreadyPlaying) {
	playbackHandler.playbackState = 1;

	Song song;
	ModelStack modelStack;
	modelStack.song = &song;

	ConsequenceBeginPlayback c;
	c.revert(AFTER, &modelStack);

	CHECK_EQUAL(1, playbackHandler.playbackState);
}
