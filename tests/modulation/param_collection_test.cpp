// ParamCollection regression tests.
// ParamCollection is abstract — we create a minimal concrete subclass.

#include "CppUTest/TestHarness.h"
#include "modulation/params/param_collection.h"
#include "modulation/params/param_collection_summary.h"

// Minimal concrete subclass implementing all pure virtuals as no-ops.
class TestParamCollection : public ParamCollection {
public:
	TestParamCollection(ParamCollectionSummary* summary) : ParamCollection(sizeof(TestParamCollection), summary) {}

	void beenCloned(bool, int32_t) override {}
	void tickSamples(int32_t, ModelStackWithParamCollection*) override {}
	void tickTicks(int32_t, ModelStackWithParamCollection*) override {}
	void playbackHasEnded(ModelStackWithParamCollection*) override {}
	void grabValuesFromPos(uint32_t, ModelStackWithParamCollection*) override {}
	void generateRepeats(ModelStackWithParamCollection*, uint32_t, uint32_t, bool) override {}
	void appendParamCollection(ModelStackWithParamCollection*, ModelStackWithParamCollection*, int32_t, int32_t,
	                           bool) override {}
	void trimToLength(uint32_t, ModelStackWithParamCollection*, Action*, bool) override {}
	void shiftHorizontally(ModelStackWithParamCollection*, int32_t, int32_t) override {}
	void processCurrentPos(ModelStackWithParamCollection*, int32_t, bool, bool, bool) override {}
	void remotelySwapParamState(AutoParamState*, ModelStackWithParamId*) override {}
	void deleteAllAutomation(Action*, ModelStackWithParamCollection*) override {}
	void nudgeNonInterpolatingNodesAtPos(int32_t, int32_t, int32_t, Action*, ModelStackWithParamCollection*) override {}
	ModelStackWithAutoParam* getAutoParamFromId(ModelStackWithParamId*, bool) override { return nullptr; }
	deluge::modulation::params::Kind getParamKind() override { return deluge::modulation::params::Kind::PATCHED; }
};

TEST_GROUP(ParamCollectionTest) {
	ParamCollectionSummary summary;
};

TEST(ParamCollectionTest, constructorClearsFlags) {
	TestParamCollection pc(&summary);
	for (int32_t i = 0; i < kMaxNumUnsignedIntegerstoRepAllParams; i++) {
		CHECK_EQUAL(0u, summary.whichParamsAreAutomated[i]);
		CHECK_EQUAL(0u, summary.whichParamsAreInterpolating[i]);
	}
}

TEST(ParamCollectionTest, objectSizeStored) {
	TestParamCollection pc(&summary);
	CHECK_EQUAL((int32_t)sizeof(TestParamCollection), pc.objectSize);
}

TEST(ParamCollectionTest, paramValueToKnobPosZero) {
	TestParamCollection pc(&summary);
	int32_t knobPos = pc.paramValueToKnobPos(0, nullptr);
	CHECK_EQUAL(0, knobPos);
}

TEST(ParamCollectionTest, paramValueToKnobPosMax) {
	TestParamCollection pc(&summary);
	// Near INT32_MAX → should return 64
	int32_t knobPos = pc.paramValueToKnobPos(2147483647, nullptr);
	CHECK_EQUAL(64, knobPos);
}

TEST(ParamCollectionTest, paramValueToKnobPosMid) {
	TestParamCollection pc(&summary);
	// knobPos = (paramValue + (1<<24)) >> 25
	// For paramValue = 32 << 25 = 1073741824: knobPos = (1073741824 + 16777216) >> 25 = 32
	int32_t knobPos = pc.paramValueToKnobPos(32 << 25, nullptr);
	CHECK_EQUAL(32, knobPos);
}

TEST(ParamCollectionTest, knobPosToParamValueZero) {
	TestParamCollection pc(&summary);
	int32_t val = pc.knobPosToParamValue(0, nullptr);
	CHECK_EQUAL(0, val);
}

TEST(ParamCollectionTest, knobPosToParamValueMax) {
	TestParamCollection pc(&summary);
	int32_t val = pc.knobPosToParamValue(64, nullptr);
	CHECK_EQUAL(2147483647, val);
}

TEST(ParamCollectionTest, knobPosToParamValueMid) {
	TestParamCollection pc(&summary);
	// knobPos 32 → 32 << 25 = 1073741824
	int32_t val = pc.knobPosToParamValue(32, nullptr);
	CHECK_EQUAL(32 << 25, val);
}

TEST(ParamCollectionTest, mayParamInterpolateDefault) {
	TestParamCollection pc(&summary);
	CHECK(pc.mayParamInterpolate(0));
	CHECK(pc.mayParamInterpolate(42));
}
