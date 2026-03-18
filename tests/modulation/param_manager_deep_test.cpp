// Deep coverage tests for modulation/params/param_manager.cpp
// Covers methods not exercised by param_manager_test.cpp:
// - setupWithPatching() / setupUnpatched() structural invariants
// - stealParamCollectionsFrom() with expression params
// - cloneParamCollectionsFrom() self-clone (beenCloned)
// - forgetParamCollections() vs destructAndForgetParamCollections()
// - Expression param set management across setup modes
// - ParamCollectionSummary state after various operations
// - ParamManagerForTimeline construction and mightContainAutomation flag tracking

#include "CppUTest/TestHarness.h"
#include "definitions_cxx.hpp"
#include "modulation/params/param_manager.h"
#include "modulation/params/param_set.h"
#include "modulation/patch/patch_cable_set.h"
#include "modulation/midi/midi_param_collection.h"
#include "modulation/params/param_collection_summary.h"

// ══════════════════════════════════════════════════════════════════════════
// setupWithPatching / setupUnpatched structural invariants
// ══════════════════════════════════════════════════════════════════════════

TEST_GROUP(ParamManagerDeepSetup){};

TEST(ParamManagerDeepSetup, setupWithPatchingCreatesThreeCollections) {
	ParamManager* pm = new ParamManager;
	Error err = pm->setupWithPatching();
	CHECK(err == Error::NONE);

	// Slots 0, 1, 2 should be populated; slot 3 should be the terminator (NULL)
	CHECK(pm->summaries[0].paramCollection != nullptr);
	CHECK(pm->summaries[1].paramCollection != nullptr);
	CHECK(pm->summaries[2].paramCollection != nullptr);
	CHECK(pm->summaries[3].paramCollection == nullptr);

	// Expression param offset should be 3 (after unpatched, patched, patch cables)
	CHECK_EQUAL(3, pm->getExpressionParamSetOffset());

	// No expression params yet
	CHECK(pm->getExpressionParamSet() == nullptr);

	delete pm;
}

TEST(ParamManagerDeepSetup, setupUnpatchedCreatesOneCollection) {
	ParamManager* pm = new ParamManager;
	Error err = pm->setupUnpatched();
	CHECK(err == Error::NONE);

	// Slot 0 populated, slot 1 is the terminator
	CHECK(pm->summaries[0].paramCollection != nullptr);
	CHECK(pm->summaries[1].paramCollection == nullptr);

	CHECK_EQUAL(1, pm->getExpressionParamSetOffset());

	delete pm;
}

TEST(ParamManagerDeepSetup, setupMIDICreatesOneCollectionShufflesExpression) {
	ParamManager* pm = new ParamManager;
	Error err = pm->setupMIDI();
	CHECK(err == Error::NONE);

	// Slot 0 has MIDIParamCollection
	CHECK(pm->summaries[0].paramCollection != nullptr);
	CHECK_EQUAL(1, pm->getExpressionParamSetOffset());

	delete pm;
}

TEST(ParamManagerDeepSetup, containsAnyMainParamCollectionsReflectsOffset) {
	ParamManager* pm = new ParamManager;

	// Fresh PM: expressionParamSetOffset == 0, so containsAnyMainParamCollections is false
	CHECK_FALSE(pm->containsAnyMainParamCollections());

	pm->setupUnpatched();
	// Now offset == 1, so there are main collections
	CHECK_TRUE(pm->containsAnyMainParamCollections());

	delete pm;
}

TEST(ParamManagerDeepSetup, containsAnyIncludingExpressionChecksSlotZero) {
	ParamManager* pm = new ParamManager;
	CHECK_FALSE(pm->containsAnyParamCollectionsIncludingExpression());

	pm->setupUnpatched();
	CHECK_TRUE(pm->containsAnyParamCollectionsIncludingExpression());

	delete pm;
}

// ══════════════════════════════════════════════════════════════════════════
// stealParamCollectionsFrom — deep scenarios
// ══════════════════════════════════════════════════════════════════════════

TEST_GROUP(ParamManagerDeepSteal){};

TEST(ParamManagerDeepSteal, stealFromPatchedTransfersAllThreeCollections) {
	ParamManager* src = new ParamManager;
	src->setupWithPatching();
	ParamCollection* origUnpatched = src->summaries[0].paramCollection;
	ParamCollection* origPatched = src->summaries[1].paramCollection;
	ParamCollection* origPatchCables = src->summaries[2].paramCollection;

	ParamManager* dst = new ParamManager;
	dst->stealParamCollectionsFrom(src);

	// dst now owns all three collections (same pointers)
	POINTERS_EQUAL(origUnpatched, dst->summaries[0].paramCollection);
	POINTERS_EQUAL(origPatched, dst->summaries[1].paramCollection);
	POINTERS_EQUAL(origPatchCables, dst->summaries[2].paramCollection);

	// src is emptied
	CHECK_FALSE(src->containsAnyMainParamCollections());
	CHECK_EQUAL(0, src->getExpressionParamSetOffset());

	// dst has correct offset
	CHECK_EQUAL(3, dst->getExpressionParamSetOffset());

	delete src;
	delete dst;
}

TEST(ParamManagerDeepSteal, stealWithExpressionParamsNotRequested) {
	ParamManager* src = new ParamManager;
	src->setupUnpatched();
	src->ensureExpressionParamSetExists(false);
	ExpressionParamSet* srcExpr = src->getExpressionParamSet();
	CHECK(srcExpr != nullptr);

	ParamManager* dst = new ParamManager;
	// stealExpressionParams = false (default)
	dst->stealParamCollectionsFrom(src, false);

	// dst should have the unpatched param set but NOT the expression params from src
	CHECK_TRUE(dst->containsAnyMainParamCollections());
	// Expression params stay with src? No — src is emptied, but expression params
	// are left on src since we didn't steal them. Let's verify src still has expression.
	// Actually, looking at the code: src->summaries[0] = src->summaries[stopAtOther] which is
	// the expression params (stopAtOther == mpeParamsOffsetOther == 1, and expression is at [1]).
	// So src gets its expression params back at slot 0.
	CHECK(src->getExpressionParamSet() != nullptr);

	// dst should NOT have expression params (didn't steal them, and didn't have any)
	CHECK(dst->getExpressionParamSet() == nullptr);

	delete src;
	delete dst;
}

TEST(ParamManagerDeepSteal, stealWithExpressionParamsRequested) {
	ParamManager* src = new ParamManager;
	src->setupUnpatched();
	src->ensureExpressionParamSetExists(false);
	ExpressionParamSet* srcExpr = src->getExpressionParamSet();
	CHECK(srcExpr != nullptr);

	ParamManager* dst = new ParamManager;
	// stealExpressionParams = true
	dst->stealParamCollectionsFrom(src, true);

	// dst gets both unpatched and expression
	CHECK_TRUE(dst->containsAnyMainParamCollections());
	CHECK(dst->getExpressionParamSet() != nullptr);

	// src is fully emptied
	CHECK_FALSE(src->containsAnyMainParamCollections());
	CHECK(src->getExpressionParamSet() == nullptr);

	delete src;
	delete dst;
}

TEST(ParamManagerDeepSteal, stealExpressionParamsWhenDstAlreadyHasThem) {
	// When dst already has expression params and we try to steal from src,
	// dst keeps its own and src's are destroyed.
	ParamManager* src = new ParamManager;
	src->setupUnpatched();
	src->ensureExpressionParamSetExists(false);

	ParamManager* dst = new ParamManager;
	dst->setupUnpatched();
	dst->ensureExpressionParamSetExists(false);
	ExpressionParamSet* dstExpr = dst->getExpressionParamSet();
	CHECK(dstExpr != nullptr);

	dst->stealParamCollectionsFrom(src, true);

	// dst still has expression params (its own, not src's)
	CHECK(dst->getExpressionParamSet() != nullptr);

	delete src;
	delete dst;
}

TEST(ParamManagerDeepSteal, stealPreservesDstExpressionWhenNotStealing) {
	// dst has expression params; stealing without stealExpressionParams
	// should preserve dst's expression params
	ParamManager* src = new ParamManager;
	src->setupWithPatching();

	ParamManager* dst = new ParamManager;
	dst->setupUnpatched();
	dst->ensureExpressionParamSetExists(false);
	ExpressionParamSet* dstExpr = dst->getExpressionParamSet();

	dst->stealParamCollectionsFrom(src, false);

	// dst should have the 3 patched collections plus its own expression params
	CHECK_EQUAL(3, dst->getExpressionParamSetOffset());
	CHECK(dst->getExpressionParamSet() != nullptr);
	POINTERS_EQUAL(dstExpr, dst->getExpressionParamSet());

	delete src;
	delete dst;
}

TEST(ParamManagerDeepSteal, stealFromEmptySrcGivesDstNothing) {
	// src has no collections at all
	ParamManager* src = new ParamManager;
	ParamManager* dst = new ParamManager;

	dst->stealParamCollectionsFrom(src);

	CHECK_FALSE(dst->containsAnyMainParamCollections());
	CHECK_EQUAL(0, dst->getExpressionParamSetOffset());

	delete src;
	delete dst;
}

// ══════════════════════════════════════════════════════════════════════════
// cloneParamCollectionsFrom — deep scenarios
// ══════════════════════════════════════════════════════════════════════════

TEST_GROUP(ParamManagerDeepClone){};

TEST(ParamManagerDeepClone, cloneCreatesDistinctPointers) {
	ParamManager* src = new ParamManager;
	src->setupWithPatching();

	ParamManager* dst = new ParamManager;
	Error err = dst->cloneParamCollectionsFrom(src, false);
	CHECK(err == Error::NONE);

	// All three collections cloned but different pointers
	CHECK(dst->summaries[0].paramCollection != src->summaries[0].paramCollection);
	CHECK(dst->summaries[1].paramCollection != src->summaries[1].paramCollection);
	CHECK(dst->summaries[2].paramCollection != src->summaries[2].paramCollection);

	// Offset preserved
	CHECK_EQUAL(src->getExpressionParamSetOffset(), dst->getExpressionParamSetOffset());

	delete src;
	delete dst;
}

TEST(ParamManagerDeepClone, cloneExpressionParamsWhenRequested) {
	ParamManager* src = new ParamManager;
	src->setupUnpatched();
	src->ensureExpressionParamSetExists(false);

	ParamManager* dst = new ParamManager;
	Error err = dst->cloneParamCollectionsFrom(src, false, true);
	CHECK(err == Error::NONE);

	// dst has expression params (cloned, different pointer)
	CHECK(dst->getExpressionParamSet() != nullptr);
	CHECK(dst->getExpressionParamSet() != src->getExpressionParamSet());

	delete src;
	delete dst;
}

TEST(ParamManagerDeepClone, cloneSkipsExpressionIfDstAlreadyHasThem) {
	ParamManager* src = new ParamManager;
	src->setupUnpatched();
	src->ensureExpressionParamSetExists(false);

	ParamManager* dst = new ParamManager;
	dst->setupUnpatched();
	dst->ensureExpressionParamSetExists(false);
	ExpressionParamSet* dstOrigExpr = dst->getExpressionParamSet();

	Error err = dst->cloneParamCollectionsFrom(src, false, true);
	CHECK(err == Error::NONE);

	// dst keeps its own expression params (pointer preserved)
	CHECK(dst->getExpressionParamSet() != nullptr);
	POINTERS_EQUAL(dstOrigExpr, dst->getExpressionParamSet());

	delete src;
	delete dst;
}

TEST(ParamManagerDeepClone, cloneWithoutExpressionParamsLeavesNone) {
	ParamManager* src = new ParamManager;
	src->setupUnpatched();
	src->ensureExpressionParamSetExists(false);

	ParamManager* dst = new ParamManager;
	// cloneExpressionParams = false
	Error err = dst->cloneParamCollectionsFrom(src, false, false);
	CHECK(err == Error::NONE);

	// dst has unpatched but no expression params
	CHECK(dst->getUnpatchedParamSet() != nullptr);
	CHECK(dst->getExpressionParamSet() == nullptr);

	delete src;
	delete dst;
}

TEST(ParamManagerDeepClone, clonePreservesOffsetForPatched) {
	ParamManager* src = new ParamManager;
	src->setupWithPatching();

	ParamManager* dst = new ParamManager;
	Error err = dst->cloneParamCollectionsFrom(src, false);
	CHECK(err == Error::NONE);

	CHECK_EQUAL(3, dst->getExpressionParamSetOffset());

	delete src;
	delete dst;
}

TEST(ParamManagerDeepClone, cloneMIDIPreservesOffset) {
	ParamManager* src = new ParamManager;
	src->setupMIDI();

	ParamManager* dst = new ParamManager;
	Error err = dst->cloneParamCollectionsFrom(src, false);
	CHECK(err == Error::NONE);

	CHECK_EQUAL(1, dst->getExpressionParamSetOffset());
	CHECK(dst->getMIDIParamCollection() != nullptr);

	delete src;
	delete dst;
}

// ══════════════════════════════════════════════════════════════════════════
// beenCloned — self-clone used after InstrumentClip cloning
// ══════════════════════════════════════════════════════════════════════════

TEST(ParamManagerDeepClone, beenClonedSelfClonesCollections) {
	ParamManager* pm = new ParamManager;
	pm->setupUnpatched();

	ParamCollection* origUnpatched = pm->summaries[0].paramCollection;

	Error err = pm->beenCloned();
	CHECK(err == Error::NONE);

	// After self-clone, the collection should be a new allocation
	CHECK(pm->summaries[0].paramCollection != nullptr);
	CHECK(pm->summaries[0].paramCollection != origUnpatched);
	CHECK_EQUAL(1, pm->getExpressionParamSetOffset());

	delete pm;
}

TEST(ParamManagerDeepClone, beenClonedWithPatchingSelfClones) {
	ParamManager* pm = new ParamManager;
	pm->setupWithPatching();

	ParamCollection* origSlot0 = pm->summaries[0].paramCollection;
	ParamCollection* origSlot1 = pm->summaries[1].paramCollection;
	ParamCollection* origSlot2 = pm->summaries[2].paramCollection;

	Error err = pm->beenCloned();
	CHECK(err == Error::NONE);

	// All three should be new pointers
	CHECK(pm->summaries[0].paramCollection != origSlot0);
	CHECK(pm->summaries[1].paramCollection != origSlot1);
	CHECK(pm->summaries[2].paramCollection != origSlot2);

	// All should be non-null
	CHECK(pm->summaries[0].paramCollection != nullptr);
	CHECK(pm->summaries[1].paramCollection != nullptr);
	CHECK(pm->summaries[2].paramCollection != nullptr);

	// Terminator
	CHECK(pm->summaries[3].paramCollection == nullptr);

	CHECK_EQUAL(3, pm->getExpressionParamSetOffset());

	delete pm;
}

TEST(ParamManagerDeepClone, beenClonedWithExpressionParams) {
	ParamManager* pm = new ParamManager;
	pm->setupUnpatched();
	pm->ensureExpressionParamSetExists(false);

	ExpressionParamSet* origExpr = pm->getExpressionParamSet();
	ParamCollection* origUnpatched = pm->summaries[0].paramCollection;

	Error err = pm->beenCloned();
	CHECK(err == Error::NONE);

	// Both unpatched and expression should be new allocations
	CHECK(pm->summaries[0].paramCollection != origUnpatched);
	CHECK(pm->getExpressionParamSet() != nullptr);
	CHECK(pm->getExpressionParamSet() != origExpr);

	delete pm;
}

// ══════════════════════════════════════════════════════════════════════════
// forgetParamCollections vs destructAndForgetParamCollections
// ══════════════════════════════════════════════════════════════════════════

TEST_GROUP(ParamManagerDeepForget){};

TEST(ParamManagerDeepForget, forgetPreservesExpressionResetOffset) {
	ParamManager* pm = new ParamManager;
	pm->setupWithPatching();
	pm->ensureExpressionParamSetExists(false);
	ExpressionParamSet* expr = pm->getExpressionParamSet();
	CHECK(expr != nullptr);

	// Before forget: offset=3, expression at slot 3
	CHECK_EQUAL(3, pm->getExpressionParamSetOffset());

	pm->forgetParamCollections();

	// After forget: offset=0, expression moves to slot 0
	CHECK_EQUAL(0, pm->getExpressionParamSetOffset());
	// Expression params pointer should now be at slot 0
	CHECK(pm->summaries[0].paramCollection != nullptr);
	POINTERS_EQUAL(expr, (ExpressionParamSet*)pm->summaries[0].paramCollection);

	// Main collections are forgotten (but memory leaked — caller's responsibility)
	// Clean up the remaining expression params
	pm->destructAndForgetParamCollections();
	delete pm;
}

TEST(ParamManagerDeepForget, forgetWithoutExpressionClearsEverything) {
	ParamManager* pm = new ParamManager;
	pm->setupWithPatching();

	pm->forgetParamCollections();

	CHECK_EQUAL(0, pm->getExpressionParamSetOffset());
	// Slot 0 should be NULL (no expression params to preserve)
	CHECK(pm->summaries[0].paramCollection == nullptr);
	CHECK_FALSE(pm->containsAnyParamCollectionsIncludingExpression());

	// NOTE: the 3 param collections are leaked here (forgetParamCollections
	// intentionally doesn't free them). This is the firmware's design —
	// caller is expected to have transferred ownership elsewhere first.
	// We can't clean them up from pm since the pointers are gone.
	delete pm;
}

TEST(ParamManagerDeepForget, destructAndForgetDestroysAll) {
	ParamManager* pm = new ParamManager;
	pm->setupWithPatching();
	pm->ensureExpressionParamSetExists(false);

	// Should have 4 collections total
	CHECK(pm->summaries[0].paramCollection != nullptr);
	CHECK(pm->summaries[1].paramCollection != nullptr);
	CHECK(pm->summaries[2].paramCollection != nullptr);
	CHECK(pm->summaries[3].paramCollection != nullptr); // expression

	pm->destructAndForgetParamCollections();

	// Everything gone
	CHECK(pm->summaries[0].paramCollection == nullptr);
	CHECK_EQUAL(0, pm->getExpressionParamSetOffset());
	CHECK_FALSE(pm->containsAnyParamCollectionsIncludingExpression());

	delete pm;
}

TEST(ParamManagerDeepForget, destructAndForgetOnEmptyIsNoOp) {
	ParamManager* pm = new ParamManager;
	// No setup at all — should be safe to call
	pm->destructAndForgetParamCollections();
	CHECK_FALSE(pm->containsAnyParamCollectionsIncludingExpression());

	delete pm;
}

TEST(ParamManagerDeepForget, destructorCallsDestructAndForget) {
	// Verify no crash when destructor runs after various states
	{
		ParamManager* pm = new ParamManager;
		pm->setupWithPatching();
		pm->ensureExpressionParamSetExists(false);
		delete pm; // should clean up all 4 collections
	}
	{
		ParamManager* pm = new ParamManager;
		// no setup at all
		delete pm; // should handle empty gracefully
	}
	{
		ParamManager* pm = new ParamManager;
		pm->setupUnpatched();
		pm->forgetParamCollections(); // leaks unpatched, but destructor should still work
		delete pm;
	}
}

// ══════════════════════════════════════════════════════════════════════════
// Expression param set edge cases
// ══════════════════════════════════════════════════════════════════════════

TEST_GROUP(ParamManagerDeepExpression){};

TEST(ParamManagerDeepExpression, expressionParamsOnPatchedSetup) {
	ParamManager* pm = new ParamManager;
	pm->setupWithPatching();

	// Expression params go at slot 3 for patched setup
	CHECK_EQUAL(3, pm->getExpressionParamSetOffset());
	CHECK(pm->getExpressionParamSet() == nullptr);

	bool ok = pm->ensureExpressionParamSetExists(false);
	CHECK_TRUE(ok);
	CHECK(pm->getExpressionParamSet() != nullptr);

	// Terminator should be at slot 4
	CHECK(pm->summaries[4].paramCollection == nullptr);

	delete pm;
}

TEST(ParamManagerDeepExpression, expressionParamsOnUnpatchedSetup) {
	ParamManager* pm = new ParamManager;
	pm->setupUnpatched();

	// Expression params go at slot 1 for unpatched setup
	CHECK_EQUAL(1, pm->getExpressionParamSetOffset());

	bool ok = pm->ensureExpressionParamSetExists(false);
	CHECK_TRUE(ok);

	// Terminator should be at slot 2
	CHECK(pm->summaries[2].paramCollection == nullptr);

	delete pm;
}

TEST(ParamManagerDeepExpression, expressionParamsOnMIDISetup) {
	ParamManager* pm = new ParamManager;
	pm->setupMIDI();

	// Expression params go at slot 1 for MIDI setup
	CHECK_EQUAL(1, pm->getExpressionParamSetOffset());

	bool ok = pm->ensureExpressionParamSetExists(false);
	CHECK_TRUE(ok);
	CHECK(pm->getExpressionParamSet() != nullptr);

	delete pm;
}

TEST(ParamManagerDeepExpression, getOrCreateReturnsNullableOnFreshPM) {
	// On a fresh PM with no setup, offset=0, slot 0 is empty
	// ensureExpressionParamSetExists should create at slot 0
	ParamManager* pm = new ParamManager;
	ExpressionParamSet* expr = pm->getOrCreateExpressionParamSet(false);
	CHECK(expr != nullptr);

	// Now slot 0 has expression params, slot 1 should be terminator
	CHECK(pm->summaries[1].paramCollection == nullptr);

	delete pm;
}

TEST(ParamManagerDeepExpression, ensureExpressionIdempotentOnPatched) {
	ParamManager* pm = new ParamManager;
	pm->setupWithPatching();

	pm->ensureExpressionParamSetExists(false);
	ExpressionParamSet* first = pm->getExpressionParamSet();

	// Second call should return the same pointer
	pm->ensureExpressionParamSetExists(false);
	ExpressionParamSet* second = pm->getExpressionParamSet();

	POINTERS_EQUAL(first, second);

	delete pm;
}

// ══════════════════════════════════════════════════════════════════════════
// ParamCollectionSummary flags after clone/steal
// ══════════════════════════════════════════════════════════════════════════

TEST_GROUP(ParamManagerDeepSummaryFlags){};

TEST(ParamManagerDeepSummaryFlags, cloneCopiesAutomationFlags) {
	ParamManager* src = new ParamManager;
	src->setupUnpatched();

	// Set some automation flags on the source summary
	src->summaries[0].whichParamsAreAutomated[0] = 0xDEADBEEF;
	src->summaries[0].whichParamsAreInterpolating[0] = 0xCAFEBABE;

	ParamManager* dst = new ParamManager;
	Error err = dst->cloneParamCollectionsFrom(src, true);
	CHECK(err == Error::NONE);

	// Flags should be cloned
	CHECK_EQUAL(0xDEADBEEF, dst->summaries[0].whichParamsAreAutomated[0]);
	CHECK_EQUAL(0xCAFEBABE, dst->summaries[0].whichParamsAreInterpolating[0]);

	delete src;
	delete dst;
}

TEST(ParamManagerDeepSummaryFlags, freshSummaryHasNoAutomation) {
	ParamManager* pm = new ParamManager;
	pm->setupUnpatched();

	// Fresh param set should have no automation flags
	CHECK_FALSE(pm->summaries[0].containsAutomation());

	delete pm;
}

TEST(ParamManagerDeepSummaryFlags, terminatorSummaryIsAllZeroes) {
	ParamManager* pm = new ParamManager;
	pm->setupWithPatching();

	// The terminator at slot 3 should have all zeroes
	ParamCollectionSummary* terminator = &pm->summaries[3];
	CHECK(terminator->paramCollection == nullptr);
	CHECK_FALSE(terminator->containsAutomation());

	delete pm;
}

// ══════════════════════════════════════════════════════════════════════════
// ParamManagerForTimeline deep tests
// ══════════════════════════════════════════════════════════════════════════

TEST_GROUP(ParamManagerForTimelineDeep){};

TEST(ParamManagerForTimelineDeep, constructionInitializesTickFields) {
	ParamManagerForTimeline* pmt = new ParamManagerForTimeline;
	CHECK_EQUAL(0, pmt->ticksSkipped);
	CHECK_EQUAL(0, pmt->ticksTilNextEvent);

	delete pmt;
}

TEST(ParamManagerForTimelineDeep, mightContainAutomationWithFlags) {
	ParamManagerForTimeline* pmt = new ParamManagerForTimeline;
	pmt->setupUnpatched();

	// Initially no automation
	CHECK_FALSE(pmt->mightContainAutomation());

	// Set automation flag
	pmt->summaries[0].whichParamsAreAutomated[0] = 1;
	CHECK_TRUE(pmt->mightContainAutomation());

	// Clear it
	pmt->summaries[0].whichParamsAreAutomated[0] = 0;
	CHECK_FALSE(pmt->mightContainAutomation());

	delete pmt;
}

TEST(ParamManagerForTimelineDeep, mightContainAutomationMultipleCollections) {
	ParamManagerForTimeline* pmt = new ParamManagerForTimeline;
	pmt->setupWithPatching();

	CHECK_FALSE(pmt->mightContainAutomation());

	// Set automation on the second collection only (patched)
	pmt->summaries[1].whichParamsAreAutomated[0] = 0x10;
	CHECK_TRUE(pmt->mightContainAutomation());

	// Clear patched, set on patch cables
	pmt->summaries[1].whichParamsAreAutomated[0] = 0;
	pmt->summaries[2].whichParamsAreAutomated[0] = 0x20;
	CHECK_TRUE(pmt->mightContainAutomation());

	delete pmt;
}

TEST(ParamManagerForTimelineDeep, mightContainAutomationEmpty) {
	ParamManagerForTimeline* pmt = new ParamManagerForTimeline;
	// No setup — no collections
	CHECK_FALSE(pmt->mightContainAutomation());

	delete pmt;
}

TEST(ParamManagerForTimelineDeep, setupThenSteal) {
	ParamManagerForTimeline* src = new ParamManagerForTimeline;
	src->setupWithPatching();
	src->summaries[0].whichParamsAreAutomated[0] = 0xFF;

	ParamManagerForTimeline* dst = new ParamManagerForTimeline;
	dst->stealParamCollectionsFrom(src);

	// dst should report automation (the summary flags move with the collection)
	// Note: stealParamCollectionsFrom copies summaries wholesale, so flags come along
	CHECK_TRUE(dst->mightContainAutomation());
	CHECK_FALSE(src->containsAnyMainParamCollections());

	delete src;
	delete dst;
}

TEST(ParamManagerForTimelineDeep, setupThenCloneThenCheckAutomation) {
	ParamManagerForTimeline* src = new ParamManagerForTimeline;
	src->setupUnpatched();
	src->summaries[0].whichParamsAreAutomated[0] = 0xAB;

	ParamManagerForTimeline* dst = new ParamManagerForTimeline;
	Error err = dst->cloneParamCollectionsFrom(src, true);
	CHECK(err == Error::NONE);

	CHECK_TRUE(dst->mightContainAutomation());
	CHECK_EQUAL(0xAB, dst->summaries[0].whichParamsAreAutomated[0]);

	delete src;
	delete dst;
}

// ══════════════════════════════════════════════════════════════════════════
// Multiple setup modes on the same ParamManager
// ══════════════════════════════════════════════════════════════════════════

TEST_GROUP(ParamManagerDeepMultiSetup){};

TEST(ParamManagerDeepMultiSetup, destructThenResetupUnpatched) {
	ParamManager* pm = new ParamManager;
	pm->setupWithPatching();
	CHECK_EQUAL(3, pm->getExpressionParamSetOffset());

	pm->destructAndForgetParamCollections();
	CHECK_FALSE(pm->containsAnyParamCollectionsIncludingExpression());

	// Re-setup as unpatched
	Error err = pm->setupUnpatched();
	CHECK(err == Error::NONE);
	CHECK_EQUAL(1, pm->getExpressionParamSetOffset());
	CHECK(pm->getUnpatchedParamSet() != nullptr);

	delete pm;
}

TEST(ParamManagerDeepMultiSetup, destructThenResetupWithPatching) {
	ParamManager* pm = new ParamManager;
	pm->setupUnpatched();
	CHECK_EQUAL(1, pm->getExpressionParamSetOffset());

	pm->destructAndForgetParamCollections();

	Error err = pm->setupWithPatching();
	CHECK(err == Error::NONE);
	CHECK_EQUAL(3, pm->getExpressionParamSetOffset());
	CHECK(pm->getUnpatchedParamSet() != nullptr);
	CHECK(pm->getPatchedParamSet() != nullptr);

	delete pm;
}

TEST(ParamManagerDeepMultiSetup, destructThenResetupMIDI) {
	ParamManager* pm = new ParamManager;
	pm->setupWithPatching();

	pm->destructAndForgetParamCollections();

	Error err = pm->setupMIDI();
	CHECK(err == Error::NONE);
	CHECK_EQUAL(1, pm->getExpressionParamSetOffset());
	CHECK(pm->getMIDIParamCollection() != nullptr);

	delete pm;
}

// ══════════════════════════════════════════════════════════════════════════
// Steal + clone round-trip: verify data integrity
// ══════════════════════════════════════════════════════════════════════════

TEST_GROUP(ParamManagerDeepRoundTrip){};

TEST(ParamManagerDeepRoundTrip, stealThenCloneBack) {
	// Original PM with patching
	ParamManager* original = new ParamManager;
	original->setupWithPatching();

	// Steal into holder
	ParamManager* holder = new ParamManager;
	holder->stealParamCollectionsFrom(original);
	CHECK_FALSE(original->containsAnyMainParamCollections());

	// Clone from holder back to original
	Error err = original->cloneParamCollectionsFrom(holder, false);
	CHECK(err == Error::NONE);

	// Both should now have valid patched setups
	CHECK(original->getUnpatchedParamSet() != nullptr);
	CHECK(original->getPatchedParamSet() != nullptr);
	CHECK(holder->getUnpatchedParamSet() != nullptr);
	CHECK(holder->getPatchedParamSet() != nullptr);

	// But different pointers
	CHECK(original->getUnpatchedParamSet() != holder->getUnpatchedParamSet());
	CHECK(original->getPatchedParamSet() != holder->getPatchedParamSet());

	delete original;
	delete holder;
}

TEST(ParamManagerDeepRoundTrip, cloneThenDestructOriginal) {
	ParamManager* src = new ParamManager;
	src->setupWithPatching();
	src->ensureExpressionParamSetExists(false);

	ParamManager* dst = new ParamManager;
	Error err = dst->cloneParamCollectionsFrom(src, false, true);
	CHECK(err == Error::NONE);

	// Destroy source — dst should still be valid
	delete src;

	CHECK(dst->getUnpatchedParamSet() != nullptr);
	CHECK(dst->getPatchedParamSet() != nullptr);
	CHECK(dst->getExpressionParamSet() != nullptr);
	CHECK_EQUAL(3, dst->getExpressionParamSetOffset());

	delete dst;
}

TEST(ParamManagerDeepRoundTrip, stealChainThreeManagers) {
	// PM A → steal to B → steal to C
	ParamManager* a = new ParamManager;
	a->setupUnpatched();
	ParamCollection* origCollection = a->summaries[0].paramCollection;

	ParamManager* b = new ParamManager;
	b->stealParamCollectionsFrom(a);
	CHECK_FALSE(a->containsAnyMainParamCollections());
	POINTERS_EQUAL(origCollection, b->summaries[0].paramCollection);

	ParamManager* c = new ParamManager;
	c->stealParamCollectionsFrom(b);
	CHECK_FALSE(b->containsAnyMainParamCollections());
	POINTERS_EQUAL(origCollection, c->summaries[0].paramCollection);

	// Only C should have the collection
	CHECK_TRUE(c->containsAnyMainParamCollections());

	delete a;
	delete b;
	delete c;
}
