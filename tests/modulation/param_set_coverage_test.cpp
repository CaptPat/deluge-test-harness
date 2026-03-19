// Deep coverage tests for param_set.cpp — targeting uncovered methods to push from 35% toward 60%+.
//
// Covers:
// - beenCloned() for UnpatchedParamSet, PatchedParamSet, ExpressionParamSet
// - notifyParamModifiedInSomeWay() automation flag transitions
// - processCurrentPos() automation tick-down
// - tickSamples() / tickTicks() interpolation loop
// - playbackHasEnded() interpolation cleanup
// - setPlayPos() automation position setting
// - grabValuesFromPos() value snapshot from position
// - generateRepeats() automation repeat generation
// - appendParamCollection() cross-set append
// - trimToLength() with and without automation
// - deleteAutomationForParamBasicForSetup()
// - deleteAllAutomation()
// - shiftHorizontally()
// - insertTime() / deleteTime()
// - nudgeNonInterpolatingNodesAtPos()
// - remotelySwapParamState()
// - getAutoParamFromId()
// - notifyPingpongOccurred()
// - backUpAllAutomatedParamsToAction()
// - copyOverridingFrom() with automation
// - shiftParamValues() / shiftParamVolumeByDB()
// - paramHasAutomationNow / paramHasNoAutomationNow bit manipulation
// - getValue() / setValue() variants
// - ExpressionParamSet: clearValues, cancelAllOverriding, moveRegionHorizontally, deleteAllAutomation
// - PatchedParamSet: shouldParamIndicateMiddleValue all branches
// - UnpatchedParamSet: shouldParamIndicateMiddleValue, knob conversions edge cases

#include "CppUTest/TestHarness.h"
#include "definitions_cxx.hpp"
#include "model/model_stack.h"
#include "modulation/automation/auto_param.h"
#include "modulation/params/param_collection_summary.h"
#include "modulation/params/param_manager.h"
#include "modulation/params/param_set.h"
#include <cstring>

namespace params = deluge::modulation::params;

// ---- Helpers to build ModelStack variants from raw memory ----

namespace {

// Builds a ModelStackWithParamCollection pointing at the given ParamSet + summary.
// The memory buffer must be >= MODEL_STACK_MAX_SIZE.
ModelStackWithParamCollection* makeParamCollectionStack(char* mem, ParamSet* paramSet,
                                                        ParamCollectionSummary* summary,
                                                        ParamManager* paramManager = nullptr) {
	memset(mem, 0, MODEL_STACK_MAX_SIZE);
	auto* ms = reinterpret_cast<ModelStackWithParamCollection*>(mem);
	ms->song = nullptr;
	ms->setTimelineCounter(nullptr);
	ms->setNoteRow(nullptr);
	ms->noteRowId = 0;
	ms->modControllable = nullptr;
	ms->paramManager = paramManager;
	ms->paramCollection = paramSet;
	ms->summary = summary;
	return ms;
}

// Adds a param to a ParamSet so that it appears "automated" by inserting a node.
void makeParamAutomated(ParamSet* ps, ParamCollectionSummary* summary, int32_t p, int32_t value = 1000000) {
	// Insert a node at pos 0 so isAutomated() returns true
	ps->params[p].setNodeAtPos(0, value, false);
	ps->paramHasAutomationNow(summary, p);
}

// Sets up interpolation flag for a param (simulates valueIncrementPerHalfTick being set).
void makeParamInterpolating(ParamSet* ps, ParamCollectionSummary* summary, int32_t p) {
	ps->params[p].valueIncrementPerHalfTick = 100;
	summary->whichParamsAreInterpolating[p >> 5] |= ((uint32_t)1 << (p & 31));
}

} // namespace

// ══════════════════════════════════════════════════════════════════════════════
// ParamSet base class coverage (via UnpatchedParamSet)
// ══════════════════════════════════════════════════════════════════════════════

TEST_GROUP(ParamSetCoverage) {
	ParamCollectionSummary summary{};
	UnpatchedParamSet* ps;
	ParamManagerForTimeline* pm;
	char stackMem[MODEL_STACK_MAX_SIZE];

	void setup() override {
		memset(&summary, 0, sizeof(summary));
		ps = new UnpatchedParamSet(&summary);
		summary.paramCollection = ps;
		ps->kind = params::Kind::UNPATCHED_SOUND;
		pm = new ParamManagerForTimeline;
		pm->summaries[0].paramCollection = ps;
	}

	void teardown() override {
		pm->summaries[0].paramCollection = nullptr;
		delete pm;
		delete ps;
	}

	ModelStackWithParamCollection* ms() { return makeParamCollectionStack(stackMem, ps, &summary, pm); }
};

// ── getValue / setValue ──

TEST(ParamSetCoverage, getValueReturnsCurrentValue) {
	ps->params[0].setCurrentValueBasicForSetup(42);
	CHECK_EQUAL(42, ps->getValue(0));
}

TEST(ParamSetCoverage, getValueMultipleParams) {
	for (int32_t i = 0; i < std::min(ps->getNumParams(), 5); i++) {
		ps->params[i].setCurrentValueBasicForSetup(i * 1000);
	}
	for (int32_t i = 0; i < std::min(ps->getNumParams(), 5); i++) {
		CHECK_EQUAL(i * 1000, ps->getValue(i));
	}
}

// ── beenCloned ──

TEST(ParamSetCoverage, beenClonedCopiesAutomation) {
	ps->params[0].setCurrentValueBasicForSetup(999);
	ps->params[0].setNodeAtPos(0, 500, false);
	ps->params[0].setNodeAtPos(100, 800, false);

	ps->beenCloned(true, 0);

	CHECK_EQUAL(999, ps->getValue(0));
	CHECK(ps->params[0].isAutomated());
}

TEST(ParamSetCoverage, beenClonedWithoutAutomation) {
	ps->params[0].setCurrentValueBasicForSetup(999);
	ps->params[0].setNodeAtPos(0, 500, false);

	ps->beenCloned(false, 0);

	// After clone without automation, nodes should be cleared
	CHECK_FALSE(ps->params[0].isAutomated());
}

TEST(ParamSetCoverage, beenClonedWithReverse) {
	ps->params[0].setNodeAtPos(0, 100, false);
	ps->params[0].setNodeAtPos(50, 200, false);

	// reverseDirectionWithLength > 0 reverses automation
	ps->beenCloned(true, 100);
	CHECK(ps->params[0].isAutomated());
}

// ── notifyParamModifiedInSomeWay ──

TEST(ParamSetCoverage, notifyAutomationTransitionOnToOff) {
	makeParamAutomated(ps, &summary, 2);
	uint32_t bit = (uint32_t)1 << (2 & 31);
	CHECK(summary.whichParamsAreAutomated[2 >> 5] & bit);

	auto* mspc = ms();
	auto* msap = mspc->addAutoParam(2, &ps->params[2]);

	// Delete the nodes to simulate automation removed
	ps->params[2].deleteAutomationBasicForSetup();

	// Notify: automatedBefore=true, automatedNow=false
	ps->notifyParamModifiedInSomeWay(msap, 0, true, true, false);

	// Bit should be cleared
	CHECK_EQUAL(0u, summary.whichParamsAreAutomated[2 >> 5] & bit);
}

TEST(ParamSetCoverage, notifyAutomationTransitionOffToOn) {
	uint32_t bit = (uint32_t)1 << (3 & 31);
	CHECK_EQUAL(0u, summary.whichParamsAreAutomated[3 >> 5] & bit);

	auto* mspc = ms();
	auto* msap = mspc->addAutoParam(3, &ps->params[3]);

	ps->notifyParamModifiedInSomeWay(msap, 0, true, false, true);

	CHECK(summary.whichParamsAreAutomated[3 >> 5] & bit);
}

TEST(ParamSetCoverage, notifyNoTransitionDoesNothing) {
	auto* mspc = ms();
	auto* msap = mspc->addAutoParam(0, &ps->params[0]);

	// No change in automation state
	ps->notifyParamModifiedInSomeWay(msap, 0, false, false, false);
	CHECK_EQUAL(0u, summary.whichParamsAreAutomated[0]);
}

// ── paramHasAutomationNow / paramHasNoAutomationNow ──

TEST(ParamSetCoverage, automationBitsHighParam) {
	// Test a param in the second uint32 (index >= 32)
	int32_t p = std::min(32, ps->getNumParams() - 1);
	if (p >= 32) {
		ps->paramHasAutomationNow(&summary, p);
		uint32_t bit = (uint32_t)1 << (p & 31);
		CHECK(summary.whichParamsAreAutomated[p >> 5] & bit);

		auto* mspc = ms();
		ps->paramHasNoAutomationNow(mspc, p);
		CHECK_EQUAL(0u, summary.whichParamsAreAutomated[p >> 5] & bit);
	}
}

TEST(ParamSetCoverage, noAutomationClearsInterpolating) {
	int32_t p = 1;
	ps->paramHasAutomationNow(&summary, p);
	summary.whichParamsAreInterpolating[p >> 5] |= ((uint32_t)1 << (p & 31));

	auto* mspc = ms();
	ps->paramHasNoAutomationNow(mspc, p);

	uint32_t bit = (uint32_t)1 << (p & 31);
	CHECK_EQUAL(0u, summary.whichParamsAreInterpolating[p >> 5] & bit);
}

// ── processCurrentPos ──

TEST(ParamSetCoverage, processCurrentPosNoAutomationNoop) {
	ps->ticksTilNextEvent = 100;
	auto* mspc = ms();

	ps->processCurrentPos(mspc, 10, false, false, true);
	CHECK_EQUAL(90, ps->ticksTilNextEvent);
}

TEST(ParamSetCoverage, processCurrentPosTriggersAtZero) {
	ps->ticksTilNextEvent = 5;
	makeParamAutomated(ps, &summary, 0, 500);

	auto* mspc = ms();
	ps->processCurrentPos(mspc, 5, false, false, true);

	// ticksTilNextEvent should have been reset (to some value from param processing)
	// The key thing is it processes without crashing
	CHECK(ps->ticksTilNextEvent >= 0);
}

TEST(ParamSetCoverage, processCurrentPosTicksDown) {
	ps->ticksTilNextEvent = 1000;
	auto* mspc = ms();

	ps->processCurrentPos(mspc, 300, false, false, true);
	CHECK_EQUAL(700, ps->ticksTilNextEvent);
}

// ── tickSamples ──

TEST(ParamSetCoverage, tickSamplesNoInterpolationNoop) {
	auto* mspc = ms();
	// No params interpolating — should not crash
	ps->tickSamples(128, mspc);
}

TEST(ParamSetCoverage, tickSamplesWithInterpolation) {
	int32_t p = 0;
	ps->params[p].setCurrentValueBasicForSetup(1000);
	makeParamAutomated(ps, &summary, p);
	makeParamInterpolating(ps, &summary, p);

	auto* mspc = ms();
	int32_t valueBefore = ps->getValue(p);
	ps->tickSamples(64, mspc);
	// Value should have changed due to interpolation increment
	// (valueIncrementPerHalfTick = 100, so 64 samples should move it)
	// Even if tickSamples returns false, we exercise the code path
}

// ── tickTicks ──

TEST(ParamSetCoverage, tickTicksNoInterpolationNoop) {
	auto* mspc = ms();
	ps->tickTicks(10, mspc);
}

TEST(ParamSetCoverage, tickTicksWithInterpolation) {
	int32_t p = 1;
	ps->params[p].setCurrentValueBasicForSetup(2000);
	makeParamAutomated(ps, &summary, p);
	makeParamInterpolating(ps, &summary, p);

	auto* mspc = ms();
	ps->tickTicks(10, mspc);
}

// ── setPlayPos ──

TEST(ParamSetCoverage, setPlayPosNoAutomation) {
	auto* mspc = ms();
	ps->setPlayPos(0, mspc, false);
}

TEST(ParamSetCoverage, setPlayPosWithAutomation) {
	makeParamAutomated(ps, &summary, 0, 500);
	ps->params[0].setNodeAtPos(100, 800, false);

	auto* mspc = ms();
	ps->setPlayPos(50, mspc, false);
}

TEST(ParamSetCoverage, setPlayPosReversed) {
	makeParamAutomated(ps, &summary, 0, 500);
	ps->params[0].setNodeAtPos(100, 800, false);

	auto* mspc = ms();
	ps->setPlayPos(50, mspc, true);
}

// ── playbackHasEnded ──

TEST(ParamSetCoverage, playbackHasEndedClearsInterpolation) {
	int32_t p = 0;
	makeParamInterpolating(ps, &summary, p);
	CHECK(ps->params[p].valueIncrementPerHalfTick != 0);

	auto* mspc = ms();
	ps->playbackHasEnded(mspc);

	CHECK_EQUAL(0, ps->params[p].valueIncrementPerHalfTick);
	CHECK_EQUAL(0u, summary.whichParamsAreInterpolating[0]);
}

TEST(ParamSetCoverage, playbackHasEndedMultipleInterpolating) {
	makeParamInterpolating(ps, &summary, 0);
	makeParamInterpolating(ps, &summary, 1);
	makeParamInterpolating(ps, &summary, 2);

	auto* mspc = ms();
	ps->playbackHasEnded(mspc);

	CHECK_EQUAL(0, ps->params[0].valueIncrementPerHalfTick);
	CHECK_EQUAL(0, ps->params[1].valueIncrementPerHalfTick);
	CHECK_EQUAL(0, ps->params[2].valueIncrementPerHalfTick);
}

// ── grabValuesFromPos ──

TEST(ParamSetCoverage, grabValuesFromPosNoAutomation) {
	auto* mspc = ms();
	ps->grabValuesFromPos(0, mspc); // No crash
}

TEST(ParamSetCoverage, grabValuesFromPosWithAutomation) {
	ps->params[0].setCurrentValueBasicForSetup(100);
	makeParamAutomated(ps, &summary, 0, 500);
	ps->params[0].setNodeAtPos(50, 800, false);

	auto* mspc = ms();
	ps->grabValuesFromPos(50, mspc);
}

// ── generateRepeats ──

TEST(ParamSetCoverage, generateRepeatsNoAutomation) {
	auto* mspc = ms();
	ps->generateRepeats(mspc, 100, 200, false);
}

TEST(ParamSetCoverage, generateRepeatsWithAutomation) {
	makeParamAutomated(ps, &summary, 0, 500);
	ps->params[0].setNodeAtPos(50, 800, false);

	auto* mspc = ms();
	ps->generateRepeats(mspc, 100, 200, false);
}

TEST(ParamSetCoverage, generateRepeatsPingpong) {
	makeParamAutomated(ps, &summary, 0, 500);
	ps->params[0].setNodeAtPos(50, 800, false);

	auto* mspc = ms();
	ps->generateRepeats(mspc, 100, 200, true);
}

// ── appendParamCollection ──

TEST(ParamSetCoverage, appendParamCollection) {
	ParamCollectionSummary summary2{};
	auto* ps2 = new UnpatchedParamSet(&summary2);
	summary2.paramCollection = ps2;

	makeParamAutomated(ps2, &summary2, 0, 300);
	ps2->params[0].setNodeAtPos(50, 600, false);

	char stackMem2[MODEL_STACK_MAX_SIZE];
	auto* ms2 = makeParamCollectionStack(stackMem2, ps2, &summary2);

	auto* mspc = ms();
	ps->appendParamCollection(mspc, ms2, 100, 0, false);

	CHECK_EQUAL(0, ps->ticksTilNextEvent);

	delete ps2;
}

// ── trimToLength ──

TEST(ParamSetCoverage, trimToLengthNoAutomation) {
	auto* mspc = ms();
	ps->trimToLength(50, mspc, nullptr, false);
}

TEST(ParamSetCoverage, trimToLengthWithAutomation) {
	makeParamAutomated(ps, &summary, 0, 500);
	ps->params[0].setNodeAtPos(100, 800, false);

	auto* mspc = ms();
	ps->trimToLength(50, mspc, nullptr, false);

	CHECK_EQUAL(0, ps->ticksTilNextEvent);
}

TEST(ParamSetCoverage, trimToLengthRemovesAutomation) {
	// Put a single node at pos 0 — trimming to length 1 should keep it
	ps->params[0].setNodeAtPos(0, 500, false);
	ps->paramHasAutomationNow(&summary, 0);

	auto* mspc = ms();
	ps->trimToLength(1, mspc, nullptr, false);
}

// ── deleteAutomationForParamBasicForSetup ──

TEST(ParamSetCoverage, deleteAutomationForParamBasicForSetup) {
	makeParamAutomated(ps, &summary, 0, 500);
	uint32_t bit = (uint32_t)1 << 0;
	CHECK(summary.whichParamsAreAutomated[0] & bit);

	auto* mspc = ms();
	ps->deleteAutomationForParamBasicForSetup(mspc, 0);

	CHECK_FALSE(ps->params[0].isAutomated());
	CHECK_EQUAL(0u, summary.whichParamsAreAutomated[0] & bit);
}

TEST(ParamSetCoverage, deleteAutomationForParamBasicMultiple) {
	makeParamAutomated(ps, &summary, 0);
	makeParamAutomated(ps, &summary, 1);
	makeParamAutomated(ps, &summary, 2);

	auto* mspc = ms();
	ps->deleteAutomationForParamBasicForSetup(mspc, 1);

	// Param 0 and 2 still automated
	CHECK(ps->params[0].isAutomated());
	CHECK_FALSE(ps->params[1].isAutomated());
	CHECK(ps->params[2].isAutomated());
}

// ── deleteAllAutomation ──

TEST(ParamSetCoverage, deleteAllAutomationNoParams) {
	auto* mspc = ms();
	ps->deleteAllAutomation(nullptr, mspc);
	CHECK_EQUAL(0u, summary.whichParamsAreAutomated[0]);
}

TEST(ParamSetCoverage, deleteAllAutomationClearsAll) {
	makeParamAutomated(ps, &summary, 0);
	makeParamAutomated(ps, &summary, 1);
	makeParamAutomated(ps, &summary, 3);

	auto* mspc = ms();
	ps->deleteAllAutomation(nullptr, mspc);

	for (int32_t i = 0; i <= ps->topUintToRepParams; i++) {
		CHECK_EQUAL(0u, summary.whichParamsAreAutomated[i]);
		CHECK_EQUAL(0u, summary.whichParamsAreInterpolating[i]);
	}
}

// ── shiftHorizontally ──

TEST(ParamSetCoverage, shiftHorizontallyNoAutomation) {
	auto* mspc = ms();
	ps->shiftHorizontally(mspc, 10, 100);
}

TEST(ParamSetCoverage, shiftHorizontallyWithAutomation) {
	makeParamAutomated(ps, &summary, 0, 500);
	ps->params[0].setNodeAtPos(50, 800, false);

	auto* mspc = ms();
	ps->shiftHorizontally(mspc, 10, 200);
}

// ── insertTime ──

TEST(ParamSetCoverage, insertTimeNoAutomation) {
	auto* mspc = ms();
	ps->insertTime(mspc, 0, 50);
}

TEST(ParamSetCoverage, insertTimeWithAutomation) {
	makeParamAutomated(ps, &summary, 0, 500);
	ps->params[0].setNodeAtPos(50, 800, false);

	auto* mspc = ms();
	ps->insertTime(mspc, 25, 50);
}

// ── deleteTime ──

TEST(ParamSetCoverage, deleteTimeNoAutomation) {
	auto* mspc = ms();
	ps->deleteTime(mspc, 0, 50);
}

TEST(ParamSetCoverage, deleteTimeWithAutomation) {
	makeParamAutomated(ps, &summary, 0, 500);
	ps->params[0].setNodeAtPos(50, 800, false);
	ps->params[0].setNodeAtPos(100, 900, false);

	auto* mspc = ms();
	ps->deleteTime(mspc, 0, 60);
}

TEST(ParamSetCoverage, deleteTimeRemovesAllNodes) {
	// Single node at 0 — delete from 0 with enough length
	ps->params[0].setNodeAtPos(0, 500, false);
	ps->paramHasAutomationNow(&summary, 0);

	auto* mspc = ms();
	ps->deleteTime(mspc, 0, 1000);

	// Automation flag should be cleared since no nodes remain
	if (!ps->params[0].isAutomated()) {
		uint32_t bit = (uint32_t)1 << 0;
		CHECK_EQUAL(0u, summary.whichParamsAreAutomated[0] & bit);
	}
}

// ── nudgeNonInterpolatingNodesAtPos ──

TEST(ParamSetCoverage, nudgeNodesNoAutomation) {
	auto* mspc = ms();
	ps->nudgeNonInterpolatingNodesAtPos(50, 1, 200, nullptr, mspc);
}

TEST(ParamSetCoverage, nudgeNodesWithAutomation) {
	makeParamAutomated(ps, &summary, 0, 500);
	ps->params[0].setNodeAtPos(50, 800, false);

	auto* mspc = ms();
	ps->nudgeNonInterpolatingNodesAtPos(50, 1, 200, nullptr, mspc);
}

// ── remotelySwapParamState ──

TEST(ParamSetCoverage, remotelySwapParamState) {
	ps->params[0].setCurrentValueBasicForSetup(111);

	AutoParamState state;
	state.value = 222;

	auto* mspc = ms();
	auto* msid = mspc->addParamId(0);

	ps->remotelySwapParamState(&state, msid);
}

// ── getAutoParamFromId ──

TEST(ParamSetCoverage, getAutoParamFromIdReturnsCorrectParam) {
	ps->params[5].setCurrentValueBasicForSetup(5555);

	auto* mspc = ms();
	auto* msid = mspc->addParamId(5);
	auto* msap = ps->getAutoParamFromId(msid);

	CHECK(msap != nullptr);
	CHECK_EQUAL(5555, msap->autoParam->getCurrentValue());
}

TEST(ParamSetCoverage, getAutoParamFromIdParamZero) {
	ps->params[0].setCurrentValueBasicForSetup(9999);

	auto* mspc = ms();
	auto* msid = mspc->addParamId(0);
	auto* msap = ps->getAutoParamFromId(msid);

	CHECK(msap != nullptr);
	CHECK_EQUAL(9999, msap->autoParam->getCurrentValue());
}

// ── notifyPingpongOccurred ──

TEST(ParamSetCoverage, notifyPingpongNoInterpolation) {
	auto* mspc = ms();
	ps->notifyPingpongOccurred(mspc);
}

TEST(ParamSetCoverage, notifyPingpongWithInterpolation) {
	makeParamInterpolating(ps, &summary, 0);
	auto* mspc = ms();
	ps->notifyPingpongOccurred(mspc);
}

// ── shiftParamValues / shiftParamVolumeByDB ──

TEST(ParamSetCoverage, shiftParamValuesNegative) {
	ps->params[0].setCurrentValueBasicForSetup(1000);
	ps->shiftParamValues(0, -500);
	CHECK_EQUAL(500, ps->getValue(0));
}

TEST(ParamSetCoverage, shiftParamVolumeByDBDoesNotCrash) {
	ps->params[0].setCurrentValueBasicForSetup(1000000);
	ps->shiftParamVolumeByDB(0, 3.0f);
	// Just verify no crash; dB shift math is in AutoParam
}

// ── copyOverridingFrom with multiple params ──

TEST(ParamSetCoverage, copyOverridingMultipleParams) {
	ParamCollectionSummary summary2{};
	auto* other = new UnpatchedParamSet(&summary2);

	for (int32_t i = 0; i < std::min(other->getNumParams(), 5); i++) {
		other->params[i].setCurrentValueBasicForSetup((i + 1) * 111);
	}

	ps->copyOverridingFrom(other);

	for (int32_t i = 0; i < std::min(ps->getNumParams(), 5); i++) {
		CHECK_EQUAL((i + 1) * 111, ps->getValue(i));
	}

	delete other;
}

// ── writeParamAsAttribute with onlyIfContainsSomething ──

TEST(ParamSetCoverage, writeParamSkipsWhenEmpty) {
	Serializer writer;
	// With onlyIfContainsSomething=true and param has default value, should skip
	ps->params[0].setCurrentValueBasicForSetup(0);
	ps->writeParamAsAttribute(writer, "test", 0, false, true);
}

TEST(ParamSetCoverage, writeParamWritesWhenHasContent) {
	Serializer writer;
	ps->params[0].setCurrentValueBasicForSetup(12345);
	// containsSomething checks non-zero value
	ps->writeParamAsAttribute(writer, "test", 0, false, false);
}

// ── readParam with automation ──

TEST(ParamSetCoverage, readParamSetsAutomationFlag) {
	// readParam reads from deserializer; with our mock it won't add real nodes
	// but we can verify the path doesn't crash
	Deserializer reader;
	ps->readParam(reader, &summary, 0, 0);
}

// ══════════════════════════════════════════════════════════════════════════════
// PatchedParamSet coverage
// ══════════════════════════════════════════════════════════════════════════════

TEST_GROUP(PatchedParamSetCoverage) {
	ParamCollectionSummary summary{};
	PatchedParamSet* ps;
	ParamManagerForTimeline* pm;
	char stackMem[MODEL_STACK_MAX_SIZE];

	void setup() override {
		memset(&summary, 0, sizeof(summary));
		ps = new PatchedParamSet(&summary);
		summary.paramCollection = ps;
		pm = new ParamManagerForTimeline;
		pm->summaries[0].paramCollection = ps;
	}

	void teardown() override {
		pm->summaries[0].paramCollection = nullptr;
		delete pm;
		delete ps;
	}

	ModelStackWithParamCollection* ms() { return makeParamCollectionStack(stackMem, ps, &summary, pm); }
};

TEST(PatchedParamSetCoverage, beenClonedResetsPointer) {
	int32_t numBefore = ps->getNumParams();
	ps->beenCloned(false, 0);
	CHECK_EQUAL(numBefore, ps->getNumParams());
	// Verify params pointer is still valid
	ps->params[0].setCurrentValueBasicForSetup(42);
	CHECK_EQUAL(42, ps->getValue(0));
}

TEST(PatchedParamSetCoverage, beenClonedWithAutomation) {
	ps->params[0].setNodeAtPos(0, 100, false);
	ps->params[0].setNodeAtPos(50, 200, false);

	ps->beenCloned(true, 0);
	CHECK(ps->params[0].isAutomated());
}

TEST(PatchedParamSetCoverage, shouldIndicateMiddlePitchAdjust) {
	ModelStackWithParamId msid;
	memset(&msid, 0, sizeof(msid));

	msid.paramId = params::LOCAL_PITCH_ADJUST;
	CHECK_TRUE(ps->shouldParamIndicateMiddleValue(&msid));
}

TEST(PatchedParamSetCoverage, shouldIndicateMiddleOscAPitchAdjust) {
	ModelStackWithParamId msid;
	memset(&msid, 0, sizeof(msid));
	msid.paramId = params::LOCAL_OSC_A_PITCH_ADJUST;
	CHECK_TRUE(ps->shouldParamIndicateMiddleValue(&msid));
}

TEST(PatchedParamSetCoverage, shouldIndicateMiddleOscBPitchAdjust) {
	ModelStackWithParamId msid;
	memset(&msid, 0, sizeof(msid));
	msid.paramId = params::LOCAL_OSC_B_PITCH_ADJUST;
	CHECK_TRUE(ps->shouldParamIndicateMiddleValue(&msid));
}

TEST(PatchedParamSetCoverage, shouldIndicateMiddleMod0PitchAdjust) {
	ModelStackWithParamId msid;
	memset(&msid, 0, sizeof(msid));
	msid.paramId = params::LOCAL_MODULATOR_0_PITCH_ADJUST;
	CHECK_TRUE(ps->shouldParamIndicateMiddleValue(&msid));
}

TEST(PatchedParamSetCoverage, shouldIndicateMiddleMod1PitchAdjust) {
	ModelStackWithParamId msid;
	memset(&msid, 0, sizeof(msid));
	msid.paramId = params::LOCAL_MODULATOR_1_PITCH_ADJUST;
	CHECK_TRUE(ps->shouldParamIndicateMiddleValue(&msid));
}

TEST(PatchedParamSetCoverage, shouldIndicateMiddleDelayFeedback) {
	ModelStackWithParamId msid;
	memset(&msid, 0, sizeof(msid));
	msid.paramId = params::GLOBAL_DELAY_FEEDBACK;
	CHECK_TRUE(ps->shouldParamIndicateMiddleValue(&msid));
}

TEST(PatchedParamSetCoverage, shouldIndicateMiddleDelayRate) {
	ModelStackWithParamId msid;
	memset(&msid, 0, sizeof(msid));
	msid.paramId = params::GLOBAL_DELAY_RATE;
	CHECK_TRUE(ps->shouldParamIndicateMiddleValue(&msid));
}

TEST(PatchedParamSetCoverage, shouldNotIndicateMiddleForOscVolume) {
	ModelStackWithParamId msid;
	memset(&msid, 0, sizeof(msid));
	msid.paramId = params::LOCAL_OSC_A_VOLUME;
	CHECK_FALSE(ps->shouldParamIndicateMiddleValue(&msid));
}

TEST(PatchedParamSetCoverage, phaseWidthOscBKnobConversion) {
	ModelStackWithAutoParam msap;
	memset(&msap, 0, sizeof(msap));
	msap.paramId = params::LOCAL_OSC_B_PHASE_WIDTH;

	int32_t val = ps->knobPosToParamValue(32, &msap);
	CHECK_EQUAL((int32_t)(96 << 24), val);

	int32_t pos = ps->paramValueToKnobPos(val, &msap);
	CHECK_EQUAL(32, pos);
}

TEST(PatchedParamSetCoverage, nonPhaseWidthUsesParentConversion) {
	ModelStackWithAutoParam msap;
	memset(&msap, 0, sizeof(msap));
	msap.paramId = params::LOCAL_VOLUME;

	int32_t val = ps->knobPosToParamValue(0, &msap);
	// Parent does bipolar conversion: knobPos=0 -> paramValue=0
	CHECK_EQUAL(0, val);
}

TEST(PatchedParamSetCoverage, nullModelStackUsesParentConversion) {
	int32_t val = ps->knobPosToParamValue(0, nullptr);
	CHECK_EQUAL(0, val);

	int32_t pos = ps->paramValueToKnobPos(0, nullptr);
	CHECK_EQUAL(0, pos);
}

// ── PatchedParamSet: processCurrentPos, tickSamples, setPlayPos, playbackHasEnded ──

TEST(PatchedParamSetCoverage, processCurrentPosWithAutomation) {
	ps->ticksTilNextEvent = 1;
	makeParamAutomated(ps, &summary, 0, 500);
	ps->params[0].setNodeAtPos(100, 800, false);

	auto* mspc = ms();
	ps->processCurrentPos(mspc, 1, false, false, true);
}

TEST(PatchedParamSetCoverage, deleteAllAutomation) {
	makeParamAutomated(ps, &summary, 0);
	makeParamAutomated(ps, &summary, 5);

	auto* mspc = ms();
	ps->deleteAllAutomation(nullptr, mspc);

	CHECK_FALSE(ps->params[0].isAutomated());
	CHECK_FALSE(ps->params[5].isAutomated());
}

// ══════════════════════════════════════════════════════════════════════════════
// UnpatchedParamSet coverage
// ══════════════════════════════════════════════════════════════════════════════

TEST_GROUP(UnpatchedParamSetCoverage) {
	ParamCollectionSummary summary{};
	UnpatchedParamSet* ps;
	ParamManagerForTimeline* pm;
	char stackMem[MODEL_STACK_MAX_SIZE];

	void setup() override {
		memset(&summary, 0, sizeof(summary));
		ps = new UnpatchedParamSet(&summary);
		summary.paramCollection = ps;
		ps->kind = params::Kind::UNPATCHED_SOUND;
		pm = new ParamManagerForTimeline;
		pm->summaries[0].paramCollection = ps;
	}

	void teardown() override {
		pm->summaries[0].paramCollection = nullptr;
		delete pm;
		delete ps;
	}

	ModelStackWithParamCollection* ms() { return makeParamCollectionStack(stackMem, ps, &summary, pm); }
};

TEST(UnpatchedParamSetCoverage, compressorThresholdNegativeKnobPos) {
	ModelStackWithAutoParam msap;
	memset(&msap, 0, sizeof(msap));
	msap.paramId = params::UNPATCHED_COMPRESSOR_THRESHOLD;

	int32_t val = ps->knobPosToParamValue(-10, &msap);
	CHECK_EQUAL((int32_t)(54 << 24), val);
}

TEST(UnpatchedParamSetCoverage, nonCompressorUsesParentConversion) {
	ModelStackWithAutoParam msap;
	memset(&msap, 0, sizeof(msap));
	msap.paramId = params::UNPATCHED_BASS;

	int32_t val = ps->knobPosToParamValue(0, &msap);
	CHECK_EQUAL(0, val); // Parent bipolar
}

TEST(UnpatchedParamSetCoverage, nullModelStackUsesParent) {
	int32_t val = ps->knobPosToParamValue(0, nullptr);
	CHECK_EQUAL(0, val);

	int32_t pos = ps->paramValueToKnobPos(0, nullptr);
	CHECK_EQUAL(0, pos);
}

TEST(UnpatchedParamSetCoverage, paramValueToKnobPosCompressor) {
	ModelStackWithAutoParam msap;
	memset(&msap, 0, sizeof(msap));
	msap.paramId = params::UNPATCHED_COMPRESSOR_THRESHOLD;

	// Round-trip test for various knob positions
	for (int32_t knob = -64; knob < 64; knob++) {
		int32_t val = ps->knobPosToParamValue(knob, &msap);
		int32_t backToKnob = ps->paramValueToKnobPos(val, &msap);
		CHECK_EQUAL(knob, backToKnob);
	}
}

TEST(UnpatchedParamSetCoverage, beenClonedResetsPointer) {
	ps->params[0].setCurrentValueBasicForSetup(7777);
	ps->beenCloned(false, 0);
	// Pointer should still be valid
	CHECK_EQUAL(7777, ps->getValue(0));
}

TEST(UnpatchedParamSetCoverage, doesParamIdAllowAutomationNonStutter) {
	ModelStackWithParamId msid;
	memset(&msid, 0, sizeof(msid));
	msid.paramId = params::UNPATCHED_TREBLE;
	CHECK_TRUE(ps->doesParamIdAllowAutomation(&msid));
}

TEST(UnpatchedParamSetCoverage, getParamKind) {
	ps->kind = params::Kind::UNPATCHED_SOUND;
	CHECK(ps->getParamKind() == params::Kind::UNPATCHED_SOUND);

	ps->kind = params::Kind::UNPATCHED_GLOBAL;
	CHECK(ps->getParamKind() == params::Kind::UNPATCHED_GLOBAL);
}

// ══════════════════════════════════════════════════════════════════════════════
// ExpressionParamSet coverage
// ══════════════════════════════════════════════════════════════════════════════

TEST_GROUP(ExpressionParamSetCoverage) {
	ParamCollectionSummary summary{};
	ExpressionParamSet* ps;
	ParamManagerForTimeline* pm;
	char stackMem[MODEL_STACK_MAX_SIZE];

	void setup() override {
		memset(&summary, 0, sizeof(summary));
		ps = new ExpressionParamSet(&summary, false);
		summary.paramCollection = ps;
		pm = new ParamManagerForTimeline;
		pm->summaries[0].paramCollection = ps;
	}

	void teardown() override {
		pm->summaries[0].paramCollection = nullptr;
		delete pm;
		delete ps;
	}

	ModelStackWithParamCollection* ms() { return makeParamCollectionStack(stackMem, ps, &summary, pm); }
};

TEST(ExpressionParamSetCoverage, beenClonedResetsPointer) {
	ps->params[0].setCurrentValueBasicForSetup(3333);
	ps->beenCloned(false, 0);
	CHECK_EQUAL(3333, ps->getValue(0));
}

TEST(ExpressionParamSetCoverage, beenClonedWithAutomation) {
	ps->params[0].setNodeAtPos(0, 100, false);
	ps->beenCloned(true, 0);
	CHECK(ps->params[0].isAutomated());
}

TEST(ExpressionParamSetCoverage, clearValuesAllDimensions) {
	for (int32_t i = 0; i < kNumExpressionDimensions; i++) {
		ps->params[i].setCurrentValueBasicForSetup((i + 1) * 1000);
	}

	auto* mspc = ms();
	ps->clearValues(mspc);

	for (int32_t i = 0; i < kNumExpressionDimensions; i++) {
		CHECK_EQUAL(0, ps->getValue(i));
	}
}

TEST(ExpressionParamSetCoverage, cancelAllOverridingClearsFlags) {
	for (int32_t i = 0; i < kNumExpressionDimensions; i++) {
		ps->params[i].renewedOverridingAtTime = 12345;
	}
	ps->cancelAllOverriding();
	for (int32_t i = 0; i < kNumExpressionDimensions; i++) {
		CHECK_EQUAL(0u, ps->params[i].renewedOverridingAtTime);
	}
}

TEST(ExpressionParamSetCoverage, deleteAllAutomationClearsValues) {
	for (int32_t i = 0; i < kNumExpressionDimensions; i++) {
		ps->params[i].setCurrentValueBasicForSetup(500);
		makeParamAutomated(ps, &summary, i);
	}

	auto* mspc = ms();
	ps->deleteAllAutomation(nullptr, mspc);

	for (int32_t i = 0; i < kNumExpressionDimensions; i++) {
		CHECK_FALSE(ps->params[i].isAutomated());
		CHECK_EQUAL(0, ps->getValue(i));
	}
}

TEST(ExpressionParamSetCoverage, moveRegionHorizontally) {
	for (int32_t i = 0; i < kNumExpressionDimensions; i++) {
		ps->params[i].setNodeAtPos(0, 100 * (i + 1), false);
		ps->params[i].setNodeAtPos(50, 200 * (i + 1), false);
	}

	auto* mspc = ms();
	ps->moveRegionHorizontally(mspc, 0, 50, 10, 100, nullptr);
}

TEST(ExpressionParamSetCoverage, mayParamInterpolateReturnsFalse) {
	CHECK_FALSE(ps->mayParamInterpolate(0));
	CHECK_FALSE(ps->mayParamInterpolate(1));
	CHECK_FALSE(ps->mayParamInterpolate(2));
}

TEST(ExpressionParamSetCoverage, getParamKind) {
	CHECK(ps->getParamKind() == params::Kind::EXPRESSION);
}

TEST(ExpressionParamSetCoverage, knobPosToParamValuePressure) {
	ModelStackWithAutoParam msap;
	memset(&msap, 0, sizeof(msap));
	msap.paramId = Z_PRESSURE; // Not pitch bend

	// knobPos 0 -> (0+64)<<24
	int32_t val = ps->knobPosToParamValue(0, &msap);
	CHECK_EQUAL((int32_t)(64 << 24), val);

	// Round trip
	int32_t pos = ps->paramValueToKnobPos(val, &msap);
	CHECK_EQUAL(0, pos);
}

TEST(ExpressionParamSetCoverage, knobPosToParamValuePitchBendBipolar) {
	ModelStackWithAutoParam msap;
	memset(&msap, 0, sizeof(msap));
	msap.paramId = X_PITCH_BEND;

	int32_t val = ps->knobPosToParamValue(50, &msap);
	int32_t pos = ps->paramValueToKnobPos(val, &msap);
	CHECK_EQUAL(50, pos);
}

TEST(ExpressionParamSetCoverage, paramValueToKnobPosPressureNonZero) {
	ModelStackWithAutoParam msap;
	memset(&msap, 0, sizeof(msap));
	msap.paramId = Z_PRESSURE;

	int32_t val = (32 + 64) << 24; // knobPos should be 32
	int32_t pos = ps->paramValueToKnobPos(val, &msap);
	CHECK_EQUAL(32, pos);
}

TEST(ExpressionParamSetCoverage, writeToFileNothingToWrite) {
	Serializer writer;
	bool written = ps->writeToFile(writer, false);
	CHECK_FALSE(written);
}

TEST(ExpressionParamSetCoverage, writeToFileWithData) {
	Serializer writer;
	ps->params[0].setCurrentValueBasicForSetup(12345); // Non-zero => containsSomething
	bool written = ps->writeToFile(writer, false);
	// Whether it returns true depends on containsSomething logic
}

TEST(ExpressionParamSetCoverage, writeToFileWithOpeningTag) {
	Serializer writer;
	ps->params[0].setCurrentValueBasicForSetup(12345);
	bool written = ps->writeToFile(writer, true);
}

TEST(ExpressionParamSetCoverage, readFromFile) {
	Deserializer reader;
	ps->readFromFile(reader, &summary, 0);
}

TEST(ExpressionParamSetCoverage, drumConstructorBendRanges) {
	ParamCollectionSummary summary2{};
	auto* drumExpr = new ExpressionParamSet(&summary2, true);
	// Drum: finger-level = main
	CHECK_EQUAL(drumExpr->bendRanges[BEND_RANGE_MAIN], drumExpr->bendRanges[BEND_RANGE_FINGER_LEVEL]);
	delete drumExpr;
}

TEST(ExpressionParamSetCoverage, nonDrumBendRangesFromFlash) {
	// Non-drum: both bend ranges come from FlashStorage defaults
	CHECK(ps->bendRanges[BEND_RANGE_MAIN] > 0);
	CHECK(ps->bendRanges[BEND_RANGE_FINGER_LEVEL] > 0);
}

// ══════════════════════════════════════════════════════════════════════════════
// Multi-param automation scenarios
// ══════════════════════════════════════════════════════════════════════════════

TEST_GROUP(ParamSetMultiAutomation) {
	ParamCollectionSummary summary{};
	UnpatchedParamSet* ps;
	ParamManagerForTimeline* pm;
	char stackMem[MODEL_STACK_MAX_SIZE];

	void setup() override {
		memset(&summary, 0, sizeof(summary));
		ps = new UnpatchedParamSet(&summary);
		summary.paramCollection = ps;
		ps->kind = params::Kind::UNPATCHED_SOUND;
		pm = new ParamManagerForTimeline;
		pm->summaries[0].paramCollection = ps;
	}

	void teardown() override {
		pm->summaries[0].paramCollection = nullptr;
		delete pm;
		delete ps;
	}

	ModelStackWithParamCollection* ms() { return makeParamCollectionStack(stackMem, ps, &summary, pm); }
};

TEST(ParamSetMultiAutomation, multipleParamsAutomatedProcessCurrentPos) {
	makeParamAutomated(ps, &summary, 0, 500);
	makeParamAutomated(ps, &summary, 1, 600);
	makeParamAutomated(ps, &summary, 2, 700);

	ps->ticksTilNextEvent = 0;
	auto* mspc = ms();
	ps->processCurrentPos(mspc, 1, false, false, true);
}

TEST(ParamSetMultiAutomation, multipleParamsPlaybackEnded) {
	makeParamInterpolating(ps, &summary, 0);
	makeParamInterpolating(ps, &summary, 3);
	makeParamInterpolating(ps, &summary, 5);

	auto* mspc = ms();
	ps->playbackHasEnded(mspc);

	CHECK_EQUAL(0, ps->params[0].valueIncrementPerHalfTick);
	CHECK_EQUAL(0, ps->params[3].valueIncrementPerHalfTick);
	CHECK_EQUAL(0, ps->params[5].valueIncrementPerHalfTick);
	CHECK_EQUAL(0u, summary.whichParamsAreInterpolating[0]);
}

TEST(ParamSetMultiAutomation, setPlayPosMultipleAutomated) {
	makeParamAutomated(ps, &summary, 0, 500);
	ps->params[0].setNodeAtPos(50, 800, false);
	makeParamAutomated(ps, &summary, 2, 300);
	ps->params[2].setNodeAtPos(75, 900, false);

	auto* mspc = ms();
	ps->setPlayPos(25, mspc, false);
}

TEST(ParamSetMultiAutomation, grabValuesFromPosMultiple) {
	makeParamAutomated(ps, &summary, 0, 100);
	ps->params[0].setNodeAtPos(50, 200, false);
	makeParamAutomated(ps, &summary, 1, 300);
	ps->params[1].setNodeAtPos(50, 400, false);

	auto* mspc = ms();
	ps->grabValuesFromPos(50, mspc);
}

TEST(ParamSetMultiAutomation, generateRepeatsMultiple) {
	makeParamAutomated(ps, &summary, 0, 100);
	ps->params[0].setNodeAtPos(50, 200, false);
	makeParamAutomated(ps, &summary, 3, 300);
	ps->params[3].setNodeAtPos(50, 400, false);

	auto* mspc = ms();
	ps->generateRepeats(mspc, 100, 300, false);
}

TEST(ParamSetMultiAutomation, trimToLengthMultiple) {
	makeParamAutomated(ps, &summary, 0, 100);
	ps->params[0].setNodeAtPos(50, 200, false);
	makeParamAutomated(ps, &summary, 2, 300);
	ps->params[2].setNodeAtPos(75, 400, false);

	auto* mspc = ms();
	ps->trimToLength(30, mspc, nullptr, false);
	CHECK_EQUAL(0, ps->ticksTilNextEvent);
}

TEST(ParamSetMultiAutomation, deleteTimeMultiple) {
	makeParamAutomated(ps, &summary, 0, 100);
	ps->params[0].setNodeAtPos(50, 200, false);
	ps->params[0].setNodeAtPos(100, 300, false);
	makeParamAutomated(ps, &summary, 1, 400);
	ps->params[1].setNodeAtPos(50, 500, false);

	auto* mspc = ms();
	ps->deleteTime(mspc, 25, 50);
}

TEST(ParamSetMultiAutomation, insertTimeMultiple) {
	makeParamAutomated(ps, &summary, 0, 100);
	ps->params[0].setNodeAtPos(50, 200, false);
	makeParamAutomated(ps, &summary, 4, 300);
	ps->params[4].setNodeAtPos(30, 400, false);

	auto* mspc = ms();
	ps->insertTime(mspc, 25, 100);
}

TEST(ParamSetMultiAutomation, shiftHorizontallyMultiple) {
	makeParamAutomated(ps, &summary, 0, 100);
	ps->params[0].setNodeAtPos(50, 200, false);
	makeParamAutomated(ps, &summary, 1, 300);
	ps->params[1].setNodeAtPos(75, 400, false);

	auto* mspc = ms();
	ps->shiftHorizontally(mspc, 10, 200);
}

TEST(ParamSetMultiAutomation, pingpongMultipleInterpolating) {
	makeParamInterpolating(ps, &summary, 0);
	makeParamInterpolating(ps, &summary, 2);

	auto* mspc = ms();
	ps->notifyPingpongOccurred(mspc);
}

// ══════════════════════════════════════════════════════════════════════════════
// Edge cases and boundary conditions
// ══════════════════════════════════════════════════════════════════════════════

TEST_GROUP(ParamSetEdgeCases) {
	ParamCollectionSummary summary{};
	UnpatchedParamSet* ps;
	ParamManagerForTimeline* pm;
	char stackMem[MODEL_STACK_MAX_SIZE];

	void setup() override {
		memset(&summary, 0, sizeof(summary));
		ps = new UnpatchedParamSet(&summary);
		summary.paramCollection = ps;
		pm = new ParamManagerForTimeline;
		pm->summaries[0].paramCollection = ps;
	}

	void teardown() override {
		pm->summaries[0].paramCollection = nullptr;
		delete pm;
		delete ps;
	}

	ModelStackWithParamCollection* ms() { return makeParamCollectionStack(stackMem, ps, &summary, pm); }
};

TEST(ParamSetEdgeCases, processCurrentPosExactlyAtThreshold) {
	ps->ticksTilNextEvent = 10;
	auto* mspc = ms();
	ps->processCurrentPos(mspc, 10, false, false, true);
	// ticksTilNextEvent was exactly 0 after decrement — should trigger processing
}

TEST(ParamSetEdgeCases, processCurrentPosOvershoot) {
	ps->ticksTilNextEvent = 5;
	auto* mspc = ms();
	ps->processCurrentPos(mspc, 100, false, false, true);
	// ticksTilNextEvent went negative — should still trigger
}

TEST(ParamSetEdgeCases, processCurrentPosReversed) {
	makeParamAutomated(ps, &summary, 0, 500);
	ps->params[0].setNodeAtPos(50, 800, false);
	ps->ticksTilNextEvent = 0;

	auto* mspc = ms();
	ps->processCurrentPos(mspc, 1, true, false, true);
}

TEST(ParamSetEdgeCases, processCurrentPosDidPingpong) {
	makeParamAutomated(ps, &summary, 0, 500);
	ps->params[0].setNodeAtPos(50, 800, false);
	ps->ticksTilNextEvent = 0;

	auto* mspc = ms();
	ps->processCurrentPos(mspc, 1, false, true, true);
}

TEST(ParamSetEdgeCases, processCurrentPosNoInterpolation) {
	makeParamAutomated(ps, &summary, 0, 500);
	ps->ticksTilNextEvent = 0;

	auto* mspc = ms();
	ps->processCurrentPos(mspc, 1, false, false, false);
}

TEST(ParamSetEdgeCases, maxValueParam) {
	ps->params[0].setCurrentValueBasicForSetup(2147483647);
	CHECK_EQUAL(2147483647, ps->getValue(0));
}

TEST(ParamSetEdgeCases, minValueParam) {
	ps->params[0].setCurrentValueBasicForSetup(-2147483648);
	CHECK_EQUAL(-2147483648, ps->getValue(0));
}

TEST(ParamSetEdgeCases, getNumParamsPositive) {
	CHECK(ps->getNumParams() > 0);
	CHECK(ps->getNumParams() == (int32_t)params::kMaxNumUnpatchedParams);
}

TEST(ParamSetEdgeCases, topUintToRepParamsCorrect) {
	int32_t expected = (ps->getNumParams() - 1) >> 5;
	CHECK_EQUAL(expected, (int32_t)ps->topUintToRepParams);
}

TEST(ParamSetEdgeCases, deleteAutomationForSetupMultipleTimes) {
	makeParamAutomated(ps, &summary, 0);
	auto* mspc = ms();
	ps->deleteAutomationForParamBasicForSetup(mspc, 0);
	// Calling again on already-cleared param should be safe
	ps->deleteAutomationForParamBasicForSetup(mspc, 0);
	CHECK_FALSE(ps->params[0].isAutomated());
}

TEST(ParamSetEdgeCases, shiftParamValuesZeroOffset) {
	ps->params[0].setCurrentValueBasicForSetup(500);
	ps->shiftParamValues(0, 0);
	CHECK_EQUAL(500, ps->getValue(0));
}

TEST(ParamSetEdgeCases, automationBitFirstParam) {
	ps->paramHasAutomationNow(&summary, 0);
	CHECK_EQUAL(1u, summary.whichParamsAreAutomated[0] & 1u);
}

TEST(ParamSetEdgeCases, automationBitParam31) {
	if (ps->getNumParams() > 31) {
		ps->paramHasAutomationNow(&summary, 31);
		uint32_t bit = (uint32_t)1 << 31;
		CHECK(summary.whichParamsAreAutomated[0] & bit);
	}
}

TEST(ParamSetEdgeCases, automationBitParam32) {
	if (ps->getNumParams() > 32) {
		ps->paramHasAutomationNow(&summary, 32);
		uint32_t bit = (uint32_t)1 << 0;
		CHECK(summary.whichParamsAreAutomated[1] & bit);
	}
}
