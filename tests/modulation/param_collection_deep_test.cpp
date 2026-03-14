// Deep coverage tests for modulation/params/param_collection.cpp
// Covers: knobPos↔paramValue roundtrips, boundary values, setPlayPos, notifyPingpong

#include "CppUTest/TestHarness.h"
#include "modulation/params/param_collection.h"
#include "modulation/params/param_collection_summary.h"

// Reuse the minimal concrete subclass pattern from param_collection_test.cpp
class TestPC : public ParamCollection {
public:
	TestPC(ParamCollectionSummary* summary) : ParamCollection(sizeof(TestPC), summary) {}

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

TEST_GROUP(ParamCollectionDeepTest) {
	ParamCollectionSummary summary;
};

// ── paramValueToKnobPos boundary tests ─────────────────────────────────────

TEST(ParamCollectionDeepTest, paramValueToKnobPosNearMax) {
	TestPC pc(&summary);
	// Values near INT32_MAX - (1<<24) should clamp to 64
	int32_t threshold = (int32_t)(0x80000000u - (1u << 24));
	CHECK_EQUAL(64, pc.paramValueToKnobPos(threshold, nullptr));
	CHECK_EQUAL(64, pc.paramValueToKnobPos(threshold + 1, nullptr));
	CHECK_EQUAL(64, pc.paramValueToKnobPos(2147483647, nullptr));
}

TEST(ParamCollectionDeepTest, paramValueToKnobPosBelowThreshold) {
	TestPC pc(&summary);
	int32_t threshold = (int32_t)(0x80000000u - (1u << 24));
	int32_t below = threshold - 1;
	int32_t result = pc.paramValueToKnobPos(below, nullptr);
	CHECK(result < 64);
}

TEST(ParamCollectionDeepTest, paramValueToKnobPosNegative) {
	TestPC pc(&summary);
	// Negative values: (negative + (1<<24)) >> 25
	int32_t result = pc.paramValueToKnobPos(-100000000, nullptr);
	CHECK(result < 0);
}

TEST(ParamCollectionDeepTest, paramValueToKnobPosOne) {
	TestPC pc(&summary);
	// knobPos = (1<<25 + 1<<24) >> 25 = (33554432 + 16777216) >> 25 = 1
	CHECK_EQUAL(1, pc.paramValueToKnobPos(1 << 25, nullptr));
}

// ── knobPosToParamValue boundary tests ─────────────────────────────────────

TEST(ParamCollectionDeepTest, knobPosToParamValueNegative) {
	TestPC pc(&summary);
	int32_t val = pc.knobPosToParamValue(-1, nullptr);
	CHECK(val < 0); // -1 << 25
}

TEST(ParamCollectionDeepTest, knobPosToParamValueLargePositive) {
	TestPC pc(&summary);
	// >= 64 returns INT32_MAX
	CHECK_EQUAL(2147483647, pc.knobPosToParamValue(64, nullptr));
	CHECK_EQUAL(2147483647, pc.knobPosToParamValue(100, nullptr));
	CHECK_EQUAL(2147483647, pc.knobPosToParamValue(1000, nullptr));
}

// ── roundtrip tests ────────────────────────────────────────────────────────

TEST(ParamCollectionDeepTest, knobPosRoundtripSmallValues) {
	TestPC pc(&summary);
	// For knobPos 0..63, converting to param and back should be identity
	for (int32_t knob = 0; knob <= 63; knob++) {
		int32_t param = pc.knobPosToParamValue(knob, nullptr);
		int32_t backToKnob = pc.paramValueToKnobPos(param, nullptr);
		CHECK_EQUAL(knob, backToKnob);
	}
}

TEST(ParamCollectionDeepTest, knobPosRoundtripMax) {
	TestPC pc(&summary);
	int32_t param = pc.knobPosToParamValue(64, nullptr);
	int32_t backToKnob = pc.paramValueToKnobPos(param, nullptr);
	CHECK_EQUAL(64, backToKnob);
}

// ── setPlayPos / notifyPingpong ────────────────────────────────────────────

TEST(ParamCollectionDeepTest, setPlayPosResetsTicksTilNextEvent) {
	TestPC pc(&summary);
	pc.ticksTilNextEvent = 99999;
	pc.setPlayPos(0, nullptr, false);
	CHECK_EQUAL(0, pc.ticksTilNextEvent);
}

TEST(ParamCollectionDeepTest, notifyPingpongResetsTicksTilNextEvent) {
	TestPC pc(&summary);
	pc.ticksTilNextEvent = 99999;
	pc.notifyPingpongOccurred(nullptr);
	CHECK_EQUAL(0, pc.ticksTilNextEvent);
}

TEST(ParamCollectionDeepTest, mayParamInterpolateAllIds) {
	TestPC pc(&summary);
	// Default implementation returns true for all param IDs
	for (int32_t i = 0; i < 100; i++) {
		CHECK_TRUE(pc.mayParamInterpolate(i));
	}
}
