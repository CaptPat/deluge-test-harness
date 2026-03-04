// AutoParam data-structure regression tests.
// Tests the real data-structure implementations (ported from firmware)
// for node manipulation, cloning, shifting, etc.

#include "CppUTest/TestHarness.h"
#include "modulation/automation/auto_param.h"
#include "modulation/params/param_node.h"

TEST_GROUP(AutoParamTest) {};

TEST(AutoParamTest, constructorDefaults) {
	AutoParam p;
	CHECK_EQUAL(0, p.currentValue);
	CHECK_EQUAL(0, p.valueIncrementPerHalfTick);
	CHECK_EQUAL(0u, p.renewedOverridingAtTime);
	CHECK_FALSE(p.isAutomated());
}

TEST(AutoParamTest, containsSomethingEmpty) {
	AutoParam p;
	CHECK_FALSE(p.containsSomething(0));
}

TEST(AutoParamTest, containsSomethingNonZeroValue) {
	AutoParam p;
	p.currentValue = 42;
	CHECK(p.containsSomething(0));
}

TEST(AutoParamTest, containsSomethingNeutralMatch) {
	AutoParam p;
	p.currentValue = 42;
	// When neutralValue matches currentValue → not "something"
	CHECK_FALSE(p.containsSomething(42));
}

TEST(AutoParamTest, containsSomethingWithNodes) {
	AutoParam p;
	p.setNodeAtPos(100, 500, false);
	CHECK(p.containsSomething(0));
}

TEST(AutoParamTest, containedSomethingBefore) {
	CHECK(AutoParam::containedSomethingBefore(true, 0, 0));
	CHECK(AutoParam::containedSomethingBefore(false, 42, 0));
	CHECK_FALSE(AutoParam::containedSomethingBefore(false, 0, 0));
}

TEST(AutoParamTest, setNodeAtPos) {
	AutoParam p;
	int32_t idx = p.setNodeAtPos(100, 500, false);
	CHECK(idx >= 0);
	CHECK(p.isAutomated());
	CHECK_EQUAL(1, p.nodes.getNumElements());

	ParamNode* node = p.nodes.getElement(0);
	CHECK_EQUAL(100, node->pos);
	CHECK_EQUAL(500, node->value);
	CHECK_FALSE(node->interpolated);
}

TEST(AutoParamTest, setNodeMultipleOrdered) {
	AutoParam p;
	p.setNodeAtPos(300, 30, false);
	p.setNodeAtPos(100, 10, false);
	p.setNodeAtPos(200, 20, false);

	CHECK_EQUAL(3, p.nodes.getNumElements());

	// Verify sorted by position
	ParamNode* n0 = p.nodes.getElement(0);
	ParamNode* n1 = p.nodes.getElement(1);
	ParamNode* n2 = p.nodes.getElement(2);
	CHECK_EQUAL(100, n0->pos);
	CHECK_EQUAL(200, n1->pos);
	CHECK_EQUAL(300, n2->pos);
	CHECK_EQUAL(10, n0->value);
	CHECK_EQUAL(20, n1->value);
	CHECK_EQUAL(30, n2->value);
}

TEST(AutoParamTest, setNodeOverwrite) {
	AutoParam p;
	p.setNodeAtPos(100, 500, false);
	p.setNodeAtPos(100, 999, true); // overwrite same position

	CHECK_EQUAL(1, p.nodes.getNumElements());
	ParamNode* node = p.nodes.getElement(0);
	CHECK_EQUAL(999, node->value);
	CHECK(node->interpolated);
}

TEST(AutoParamTest, deleteAutomationBasic) {
	AutoParam p;
	p.setNodeAtPos(100, 500, false);
	p.setNodeAtPos(200, 600, false);
	CHECK(p.isAutomated());

	p.deleteAutomationBasicForSetup();
	CHECK_FALSE(p.isAutomated());
	CHECK_EQUAL(0, p.valueIncrementPerHalfTick);
	CHECK_EQUAL(0u, p.renewedOverridingAtTime);
}

TEST(AutoParamTest, cloneFromWithAutomation) {
	AutoParam src;
	src.currentValue = 42;
	src.setNodeAtPos(100, 500, false);
	src.setNodeAtPos(200, 600, true);

	AutoParam dst;
	dst.cloneFrom(&src, true);
	CHECK_EQUAL(42, dst.currentValue);
	CHECK_EQUAL(2, dst.nodes.getNumElements());
	CHECK_EQUAL(0u, dst.renewedOverridingAtTime);
}

TEST(AutoParamTest, cloneFromWithoutAutomation) {
	AutoParam src;
	src.currentValue = 42;
	src.setNodeAtPos(100, 500, false);

	AutoParam dst;
	dst.cloneFrom(&src, false);
	CHECK_EQUAL(42, dst.currentValue);
	CHECK_FALSE(dst.isAutomated());
}

TEST(AutoParamTest, shiftValues) {
	AutoParam p;
	p.currentValue = 100;
	p.setNodeAtPos(0, 200, false);
	p.setNodeAtPos(10, 300, false);

	p.shiftValues(50);
	CHECK_EQUAL(150, p.currentValue);
	CHECK_EQUAL(250, p.nodes.getElement(0)->value);
	CHECK_EQUAL(350, p.nodes.getElement(1)->value);
}

TEST(AutoParamTest, shiftValuesSaturates) {
	AutoParam p;
	p.currentValue = 2147483647;
	p.shiftValues(100);
	CHECK_EQUAL(2147483647, p.currentValue); // saturated at max
}

TEST(AutoParamTest, tickTicksNoEffect) {
	AutoParam p;
	p.valueIncrementPerHalfTick = 0;
	bool changed = p.tickTicks(10);
	CHECK_FALSE(changed);
}

TEST(AutoParamTest, tickTicksApplies) {
	AutoParam p;
	p.currentValue = 0;
	p.valueIncrementPerHalfTick = 100;
	bool changed = p.tickTicks(5);
	CHECK(changed);
	// increment * numTicks * 2 = 100 * 5 * 2 = 1000
	CHECK_EQUAL(1000, p.currentValue);
}

TEST(AutoParamTest, tickSamplesNoEffect) {
	AutoParam p;
	p.valueIncrementPerHalfTick = 0;
	CHECK_FALSE(p.tickSamples(10));
}

TEST(AutoParamTest, tickSamplesApplies) {
	AutoParam p;
	p.currentValue = 0;
	p.valueIncrementPerHalfTick = 100;
	CHECK(p.tickSamples(5));
	CHECK_EQUAL(500, p.currentValue);
}

TEST(AutoParamTest, notifyPingpongOccurred) {
	AutoParam p;
	p.valueIncrementPerHalfTick = 100;
	p.notifyPingpongOccurred();
	CHECK_EQUAL(-100, p.valueIncrementPerHalfTick);
	p.notifyPingpongOccurred();
	CHECK_EQUAL(100, p.valueIncrementPerHalfTick);
}

TEST(AutoParamTest, insertTime) {
	AutoParam p;
	p.setNodeAtPos(100, 10, false);
	p.setNodeAtPos(200, 20, false);
	p.setNodeAtPos(300, 30, false);

	// Insert 50 ticks at position 150 — nodes at 200 and 300 shift right
	p.insertTime(150, 50);

	CHECK_EQUAL(100, p.nodes.getElement(0)->pos);
	CHECK_EQUAL(250, p.nodes.getElement(1)->pos);
	CHECK_EQUAL(350, p.nodes.getElement(2)->pos);
}

TEST(AutoParamTest, appendParam) {
	AutoParam a;
	a.setNodeAtPos(0, 10, false);
	a.setNodeAtPos(50, 20, false);

	AutoParam b;
	b.setNodeAtPos(0, 30, false);
	b.setNodeAtPos(25, 40, true);

	// Append b at oldLength=100
	a.appendParam(&b, 100, 0, false);

	CHECK_EQUAL(4, a.nodes.getNumElements());
	// Original nodes unchanged
	CHECK_EQUAL(0, a.nodes.getElement(0)->pos);
	CHECK_EQUAL(50, a.nodes.getElement(1)->pos);
	// Appended nodes offset by oldLength
	CHECK_EQUAL(100, a.nodes.getElement(2)->pos);
	CHECK_EQUAL(30, a.nodes.getElement(2)->value);
	CHECK_EQUAL(125, a.nodes.getElement(3)->pos);
	CHECK_EQUAL(40, a.nodes.getElement(3)->value);
}

TEST(AutoParamTest, init) {
	AutoParam p;
	p.setNodeAtPos(100, 500, false);
	p.currentValue = 42;
	p.init();
	CHECK_FALSE(p.isAutomated());
}
