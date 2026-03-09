// Tests for modulation/params/param_set.cpp — ParamSet, UnpatchedParamSet, PatchedParamSet, ExpressionParamSet
#include "CppUTest/TestHarness.h"
#include "definitions_cxx.hpp"
#include "model/model_stack.h"
#include "modulation/params/param_manager.h"
#include "modulation/params/param_set.h"
#include "modulation/params/param_collection_summary.h"
#include "storage/storage_manager.h"
#include <cstring>

namespace params = deluge::modulation::params;

// Helper to set up a ParamManager with an UnpatchedParamSet
struct ParamSetTestHelper {
	ParamCollectionSummary summary{};
	UnpatchedParamSet unpatchedParams{&summary};
	ParamManagerForTimeline paramManager;

	ParamSetTestHelper() {
		summary.paramCollection = &unpatchedParams;
		paramManager.summaries[0] = summary;
	}
	~ParamSetTestHelper() {
		paramManager.summaries[0].paramCollection = nullptr;
	}
};

// ── ParamSet base class tests ──────────────────────────────────────────────

TEST_GROUP(ParamSetTest) {
	ParamSetTestHelper helper;
	void setup() override {}
};

TEST(ParamSetTest, constructionDefaults) {
	// UnpatchedParamSet inherits ParamSet and initialises params array
	CHECK(helper.unpatchedParams.getNumParams() > 0);
}

TEST(ParamSetTest, beenClonedResetsParamsPointer) {
	// beenCloned should re-point params to internal array and call base beenCloned
	int32_t numBefore = helper.unpatchedParams.getNumParams();
	helper.unpatchedParams.beenCloned(false, 0);
	CHECK_EQUAL(numBefore, helper.unpatchedParams.getNumParams());
}

TEST(ParamSetTest, copyOverridingFrom) {
	// Create another UnpatchedParamSet and copy values
	ParamCollectionSummary summary2{};
	UnpatchedParamSet other{&summary2};

	// Set a value in other
	other.params[params::UNPATCHED_STUTTER_RATE].setCurrentValueBasicForSetup(12345678);

	// Copy from other
	helper.unpatchedParams.copyOverridingFrom(&other);
	CHECK_EQUAL(12345678, helper.unpatchedParams.getValue(params::UNPATCHED_STUTTER_RATE));
}

TEST(ParamSetTest, paramHasAutomationTracking) {
	// Initially no params should be flagged as automated
	for (int32_t i = 0; i <= 1; i++) {
		CHECK_EQUAL(0u, helper.summary.whichParamsAreAutomated[i]);
	}

	// Mark a param as having automation
	helper.unpatchedParams.paramHasAutomationNow(&helper.summary, params::UNPATCHED_STUTTER_RATE);

	// Check that the bit is set
	int32_t p = params::UNPATCHED_STUTTER_RATE;
	uint32_t expected = (uint32_t)1 << (p & 31);
	CHECK_EQUAL(expected, helper.summary.whichParamsAreAutomated[p >> 5] & expected);
}

TEST(ParamSetTest, shiftParamValues) {
	helper.unpatchedParams.params[0].setCurrentValueBasicForSetup(100);
	helper.unpatchedParams.shiftParamValues(0, 50);
	// shiftValues adds offset to current value
	CHECK_EQUAL(150, helper.unpatchedParams.getValue(0));
}

TEST(ParamSetTest, writeAndReadParam) {
	// writeParamAsAttribute and readParam use Serializer/Deserializer
	// Just verify they don't crash with our mock serializer/deserializer
	Serializer writer;
	helper.unpatchedParams.writeParamAsAttribute(writer, "testParam", 0, false, false, nullptr);

	Deserializer reader;
	helper.unpatchedParams.readParam(reader, &helper.summary, 0, 0);
}

// ── UnpatchedParamSet tests ────────────────────────────────────────────────

TEST_GROUP(UnpatchedParamSetTest) {
	ParamSetTestHelper helper;
};

TEST(UnpatchedParamSetTest, compressorThresholdKnobConversion) {
	// Compressor threshold has special knob<->param conversion
	ModelStackWithAutoParam mockStack;
	memset(&mockStack, 0, sizeof(mockStack));
	mockStack.paramId = params::UNPATCHED_COMPRESSOR_THRESHOLD;

	// knobPos 0 -> paramValue = (0+64)<<24 = 64<<24 = 1073741824
	int32_t paramVal = helper.unpatchedParams.knobPosToParamValue(0, &mockStack);
	CHECK_EQUAL((int32_t)(64 << 24), paramVal);

	// paramValue back to knobPos
	int32_t knobPos = helper.unpatchedParams.paramValueToKnobPos(paramVal, &mockStack);
	CHECK_EQUAL(0, knobPos);
}

TEST(UnpatchedParamSetTest, compressorThresholdMaxKnobPos) {
	ModelStackWithAutoParam mockStack;
	memset(&mockStack, 0, sizeof(mockStack));
	mockStack.paramId = params::UNPATCHED_COMPRESSOR_THRESHOLD;

	// knobPos 64 -> MAX_VALUE (clamped)
	int32_t paramVal = helper.unpatchedParams.knobPosToParamValue(64, &mockStack);
	CHECK_EQUAL(2147483647, paramVal);
}

TEST(UnpatchedParamSetTest, stutterRateDisallowsAutomation) {
	ModelStackWithParamId mockStack;
	memset(&mockStack, 0, sizeof(mockStack));
	mockStack.paramId = params::UNPATCHED_STUTTER_RATE;

	CHECK_FALSE(helper.unpatchedParams.doesParamIdAllowAutomation(&mockStack));
}

TEST(UnpatchedParamSetTest, otherParamAllowsAutomation) {
	ModelStackWithParamId mockStack;
	memset(&mockStack, 0, sizeof(mockStack));
	mockStack.paramId = params::UNPATCHED_BASS;

	CHECK_TRUE(helper.unpatchedParams.doesParamIdAllowAutomation(&mockStack));
}

// ── PatchedParamSet tests ──────────────────────────────────────────────────

struct PatchedParamSetTestHelper {
	ParamCollectionSummary summary{};
	PatchedParamSet patchedParams{&summary};
	ParamManagerForTimeline paramManager;

	PatchedParamSetTestHelper() {
		summary.paramCollection = &patchedParams;
		paramManager.summaries[0] = summary;
	}
	~PatchedParamSetTestHelper() {
		paramManager.summaries[0].paramCollection = nullptr;
	}
};

TEST_GROUP(PatchedParamSetTest) {
	PatchedParamSetTestHelper helper;
};

TEST(PatchedParamSetTest, phaseWidthKnobConversion) {
	ModelStackWithAutoParam mockStack;
	memset(&mockStack, 0, sizeof(mockStack));
	mockStack.paramId = params::LOCAL_OSC_A_PHASE_WIDTH;

	// knobPos 0 -> paramValue = (0+64)<<24 = 1073741824
	int32_t paramVal = helper.patchedParams.knobPosToParamValue(0, &mockStack);
	CHECK_EQUAL((int32_t)(64 << 24), paramVal);

	int32_t knobPos = helper.patchedParams.paramValueToKnobPos(paramVal, &mockStack);
	CHECK_EQUAL(0, knobPos);
}

TEST(PatchedParamSetTest, phaseWidthMaxKnobPos) {
	ModelStackWithAutoParam mockStack;
	memset(&mockStack, 0, sizeof(mockStack));
	mockStack.paramId = params::LOCAL_OSC_B_PHASE_WIDTH;

	int32_t paramVal = helper.patchedParams.knobPosToParamValue(64, &mockStack);
	CHECK_EQUAL(2147483647, paramVal);
}

TEST(PatchedParamSetTest, shouldIndicateMiddleForPan) {
	ModelStackWithParamId mockStack;
	memset(&mockStack, 0, sizeof(mockStack));
	mockStack.paramId = params::LOCAL_PAN;

	CHECK_TRUE(helper.patchedParams.shouldParamIndicateMiddleValue(&mockStack));
}

TEST(PatchedParamSetTest, shouldNotIndicateMiddleForVolume) {
	ModelStackWithParamId mockStack;
	memset(&mockStack, 0, sizeof(mockStack));
	mockStack.paramId = params::LOCAL_VOLUME;

	CHECK_FALSE(helper.patchedParams.shouldParamIndicateMiddleValue(&mockStack));
}

// ── ExpressionParamSet tests ───────────────────────────────────────────────

struct ExpressionTestHelper {
	ParamCollectionSummary summary{};
	ExpressionParamSet exprParams{&summary, false};
	ParamManagerForTimeline paramManager;

	ExpressionTestHelper() {
		summary.paramCollection = &exprParams;
		paramManager.summaries[0] = summary;
	}
	~ExpressionTestHelper() {
		paramManager.summaries[0].paramCollection = nullptr;
	}
};

TEST_GROUP(ExpressionParamSetTest) {
	ExpressionTestHelper helper;
};

TEST(ExpressionParamSetTest, defaultBendRanges) {
	// Non-drum: should get defaults from FlashStorage
	CHECK(helper.exprParams.bendRanges[BEND_RANGE_MAIN] > 0);
	CHECK(helper.exprParams.bendRanges[BEND_RANGE_FINGER_LEVEL] > 0);
}

TEST(ExpressionParamSetTest, drumBendRangesSame) {
	ParamCollectionSummary summary2{};
	ExpressionParamSet drumExpr{&summary2, true};
	// For drum: finger-level bend range = main bend range
	CHECK_EQUAL(drumExpr.bendRanges[BEND_RANGE_MAIN], drumExpr.bendRanges[BEND_RANGE_FINGER_LEVEL]);
}

TEST(ExpressionParamSetTest, pitchBendKnobConversionBipolar) {
	ModelStackWithAutoParam mockStack;
	memset(&mockStack, 0, sizeof(mockStack));
	mockStack.paramId = X_PITCH_BEND;

	// Pitch bend uses bipolar (parent) conversion
	int32_t paramVal = helper.exprParams.knobPosToParamValue(0, &mockStack);
	// Parent does standard conversion (knobPos=0 maps to 0)
	CHECK_EQUAL(0, paramVal);
}

TEST(ExpressionParamSetTest, nonPitchBendKnobConversionUnipolar) {
	ModelStackWithAutoParam mockStack;
	memset(&mockStack, 0, sizeof(mockStack));
	mockStack.paramId = Y_SLIDE_TIMBRE; // Not pitch bend

	// knobPos 64 -> MAX_VALUE (2147483647)
	int32_t paramVal = helper.exprParams.knobPosToParamValue(64, &mockStack);
	CHECK_EQUAL(2147483647, paramVal);

	// knobPos 0 -> (0+64)<<24
	int32_t paramVal2 = helper.exprParams.knobPosToParamValue(0, &mockStack);
	CHECK_EQUAL((int32_t)(64 << 24), paramVal2);
}

TEST(ExpressionParamSetTest, writeAndReadDoNotCrash) {
	Serializer writer;
	bool written = helper.exprParams.writeToFile(writer, false);
	// No params contain anything, so nothing written
	CHECK_FALSE(written);

	Deserializer reader;
	helper.exprParams.readFromFile(reader, &helper.summary, 0);
}

TEST(ExpressionParamSetTest, clearValuesZerosAll) {
	// Set a value first
	helper.exprParams.params[0].setCurrentValueBasicForSetup(12345);

	// Create a minimal ModelStackWithParamCollection for clearValues
	// clearValues iterates kNumExpressionDimensions params
	char stackMem[MODEL_STACK_MAX_SIZE];
	memset(stackMem, 0, sizeof(stackMem));
	auto* modelStack = reinterpret_cast<ModelStackWithParamCollection*>(stackMem);

	helper.exprParams.clearValues(modelStack);
	CHECK_EQUAL(0, helper.exprParams.getValue(0));
}

TEST(ExpressionParamSetTest, cancelAllOverridingDoesNotCrash) {
	helper.exprParams.cancelAllOverriding();
}
