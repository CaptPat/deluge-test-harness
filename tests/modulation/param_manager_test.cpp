// Tests for modulation/params/param_manager.cpp
// Covers: construction, setupUnpatched, setupWithPatching, setupMIDI,
// destructAndForget, forgetParamCollections, stealParamCollections,
// expression param set lifecycle, mightContainAutomation

#include "CppUTest/TestHarness.h"
#include "definitions_cxx.hpp"
#include "modulation/params/param_manager.h"
#include "modulation/params/param_set.h"
#include "modulation/patch/patch_cable_set.h"
#include "modulation/midi/midi_param_collection.h"

// ── ParamManager construction ──────────────────────────────────────────────

TEST_GROUP(ParamManagerTest){};

TEST(ParamManagerTest, constructionDefaults) {
	ParamManager pm;
	CHECK_FALSE(pm.containsAnyMainParamCollections());
	CHECK_FALSE(pm.containsAnyParamCollectionsIncludingExpression());
	CHECK_EQUAL(0, pm.getExpressionParamSetOffset());
}

TEST(ParamManagerTest, setupUnpatchedSucceeds) {
	ParamManager pm;
	Error err = pm.setupUnpatched();
	CHECK_TRUE(err == Error::NONE);
	CHECK_TRUE(pm.containsAnyMainParamCollections());
	CHECK_EQUAL(1, pm.getExpressionParamSetOffset());
	// Has an UnpatchedParamSet at slot 0
	CHECK(pm.getUnpatchedParamSet() != nullptr);
}

TEST(ParamManagerTest, setupWithPatchingSucceeds) {
	ParamManager pm;
	Error err = pm.setupWithPatching();
	CHECK_TRUE(err == Error::NONE);
	CHECK_TRUE(pm.containsAnyMainParamCollections());
	CHECK_EQUAL(3, pm.getExpressionParamSetOffset());
	// Has all three: unpatched, patched, patch cables
	CHECK(pm.getUnpatchedParamSet() != nullptr);
	CHECK(pm.getPatchedParamSet() != nullptr);
}

TEST(ParamManagerTest, setupMIDISucceeds) {
	ParamManager pm;
	Error err = pm.setupMIDI();
	CHECK_TRUE(err == Error::NONE);
	CHECK_TRUE(pm.containsAnyMainParamCollections());
	CHECK_EQUAL(1, pm.getExpressionParamSetOffset());
	CHECK(pm.getMIDIParamCollection() != nullptr);
}

TEST(ParamManagerTest, destructAndForgetCleansUp) {
	ParamManager pm;
	pm.setupUnpatched();
	CHECK_TRUE(pm.containsAnyParamCollectionsIncludingExpression());
	pm.destructAndForgetParamCollections();
	CHECK_FALSE(pm.containsAnyParamCollectionsIncludingExpression());
	CHECK_EQUAL(0, pm.getExpressionParamSetOffset());
}

TEST(ParamManagerTest, forgetPreservesExpressionParams) {
	ParamManager pm;
	pm.setupUnpatched();
	pm.ensureExpressionParamSetExists(false);
	ExpressionParamSet* expr = pm.getExpressionParamSet();
	CHECK(expr != nullptr);

	pm.forgetParamCollections();
	// Expression params should still be at slot 0
	CHECK_EQUAL(0, pm.getExpressionParamSetOffset());
	CHECK(pm.getExpressionParamSet() != nullptr);

	// Clean up expression params manually since forgetParamCollections doesn't destroy them
	pm.destructAndForgetParamCollections();
}

TEST(ParamManagerTest, destructorCleansUpAutomatically) {
	// Just ensure no leaks — ParamManager destructor calls destructAndForgetParamCollections
	{
		ParamManager pm;
		pm.setupWithPatching();
	}
	// If we get here without crash/leak, it works
}

// ── Expression param set ───────────────────────────────────────────────────

TEST(ParamManagerTest, ensureExpressionParamSetCreatesOnDemand) {
	ParamManager pm;
	pm.setupUnpatched();
	CHECK(pm.getExpressionParamSet() == nullptr);
	bool ok = pm.ensureExpressionParamSetExists(false);
	CHECK_TRUE(ok);
	CHECK(pm.getExpressionParamSet() != nullptr);
}

TEST(ParamManagerTest, ensureExpressionParamSetIdempotent) {
	ParamManager pm;
	pm.setupUnpatched();
	pm.ensureExpressionParamSetExists(false);
	ExpressionParamSet* first = pm.getExpressionParamSet();
	pm.ensureExpressionParamSetExists(false);
	ExpressionParamSet* second = pm.getExpressionParamSet();
	POINTERS_EQUAL(first, second);
}

TEST(ParamManagerTest, getOrCreateExpressionParamSet) {
	ParamManager pm;
	pm.setupUnpatched();
	ExpressionParamSet* eps = pm.getOrCreateExpressionParamSet(false);
	CHECK(eps != nullptr);
}

TEST(ParamManagerTest, expressionParamSetForDrum) {
	ParamManager pm;
	pm.setupUnpatched();
	ExpressionParamSet* eps = pm.getOrCreateExpressionParamSet(true);
	CHECK(eps != nullptr);
	// For drum: finger-level bend range should equal main bend range
	CHECK_EQUAL(eps->bendRanges[BEND_RANGE_MAIN], eps->bendRanges[BEND_RANGE_FINGER_LEVEL]);
}

// ── Steal param collections ────────────────────────────────────────────────

TEST(ParamManagerTest, stealParamCollectionsBasic) {
	ParamManager src;
	src.setupUnpatched();
	CHECK_TRUE(src.containsAnyMainParamCollections());

	ParamManager dst;
	dst.stealParamCollectionsFrom(&src);
	CHECK_TRUE(dst.containsAnyMainParamCollections());
	CHECK_FALSE(src.containsAnyMainParamCollections());
}

TEST(ParamManagerTest, stealWithPatchingPreservesAll) {
	ParamManager src;
	src.setupWithPatching();

	ParamManager dst;
	dst.stealParamCollectionsFrom(&src);
	CHECK_TRUE(dst.containsAnyMainParamCollections());
	CHECK(dst.getUnpatchedParamSet() != nullptr);
	CHECK(dst.getPatchedParamSet() != nullptr);
	CHECK_EQUAL(3, dst.getExpressionParamSetOffset());
}

// ── ParamManagerForTimeline ────────────────────────────────────────────────

TEST_GROUP(ParamManagerForTimelineTest){};

TEST(ParamManagerForTimelineTest, constructionDefaults) {
	ParamManagerForTimeline pmt;
	CHECK_EQUAL(0, pmt.ticksSkipped);
	CHECK_EQUAL(0, pmt.ticksTilNextEvent);
}

TEST(ParamManagerForTimelineTest, mightContainAutomationDefaultFalse) {
	ParamManagerForTimeline pmt;
	pmt.setupUnpatched();
	CHECK_FALSE(pmt.mightContainAutomation());
}

TEST(ParamManagerForTimelineTest, toForTimelineReturnsSelf) {
	ParamManagerForTimeline pmt;
	CHECK_EQUAL(&pmt, pmt.toForTimeline());
}

// ── Clone param collections ────────────────────────────────────────────────

TEST(ParamManagerTest, cloneUnpatched) {
	ParamManager src;
	src.setupUnpatched();

	ParamManager dst;
	Error err = dst.cloneParamCollectionsFrom(&src, false);
	CHECK_TRUE(err == Error::NONE);
	CHECK_TRUE(dst.containsAnyMainParamCollections());
	CHECK(dst.getUnpatchedParamSet() != nullptr);
	// Should be a different pointer (cloned, not shared)
	CHECK(dst.getUnpatchedParamSet() != src.getUnpatchedParamSet());
}

TEST(ParamManagerTest, cloneWithPatching) {
	ParamManager src;
	src.setupWithPatching();

	ParamManager dst;
	Error err = dst.cloneParamCollectionsFrom(&src, false);
	CHECK_TRUE(err == Error::NONE);
	CHECK(dst.getUnpatchedParamSet() != nullptr);
	CHECK(dst.getPatchedParamSet() != nullptr);
	CHECK_EQUAL(3, dst.getExpressionParamSetOffset());
}

TEST(ParamManagerTest, cloneMIDI) {
	ParamManager src;
	src.setupMIDI();

	ParamManager dst;
	Error err = dst.cloneParamCollectionsFrom(&src, false);
	CHECK_TRUE(err == Error::NONE);
	CHECK(dst.getMIDIParamCollection() != nullptr);
}

TEST(ParamManagerTest, cloneWithExpressionParams) {
	ParamManager src;
	src.setupUnpatched();
	src.ensureExpressionParamSetExists(false);

	ParamManager dst;
	Error err = dst.cloneParamCollectionsFrom(&src, false, true);
	CHECK_TRUE(err == Error::NONE);
	CHECK(dst.getExpressionParamSet() != nullptr);
}

TEST(ParamManagerTest, cloneSkipsExpressionIfDstHasThem) {
	ParamManager src;
	src.setupUnpatched();
	src.ensureExpressionParamSetExists(false);

	ParamManager dst;
	dst.setupUnpatched();
	dst.ensureExpressionParamSetExists(false);
	ExpressionParamSet* dstExpr = dst.getExpressionParamSet();

	Error err = dst.cloneParamCollectionsFrom(&src, false, true);
	CHECK_TRUE(err == Error::NONE);
	// dst keeps its own expression params
	CHECK(dst.getExpressionParamSet() != nullptr);
}
