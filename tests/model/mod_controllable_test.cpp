// Phase 10B: ModControllable tests
// Tests default implementations in the ModControllable base class

#include "CppUTest/TestHarness.h"
#include "model/mod_controllable/mod_controllable.h"
#include "model/model_stack.h"
#include <cstring>

// Concrete subclass for testing (ModControllable has pure virtuals in some contexts,
// but the methods we test are non-virtual defaults)
class TestModControllable : public ModControllable {};

TEST_GROUP(ModControllable){};

TEST(ModControllable, getKnobPosForNonExistentParamReturnsMinus64) {
	TestModControllable mc;

	char mem[MODEL_STACK_MAX_SIZE];
	memset(mem, 0, sizeof(mem));
	auto* ms = (ModelStackWithAutoParam*)mem;

	int32_t result = mc.getKnobPosForNonExistentParam(0, ms);
	CHECK_EQUAL(-64, result);
}

TEST(ModControllable, getKnobPosForNonExistentParamDifferentEncoder) {
	TestModControllable mc;

	char mem[MODEL_STACK_MAX_SIZE];
	memset(mem, 0, sizeof(mem));
	auto* ms = (ModelStackWithAutoParam*)mem;

	int32_t result = mc.getKnobPosForNonExistentParam(1, ms);
	CHECK_EQUAL(-64, result);
}

TEST(ModControllable, getParamFromModEncoderReturnsNullAutoParam) {
	TestModControllable mc;

	char mem[MODEL_STACK_MAX_SIZE];
	memset(mem, 0, sizeof(mem));
	auto* ms3 = (ModelStackWithThreeMainThings*)mem;

	ModelStackWithAutoParam* result = mc.getParamFromModEncoder(0, ms3, false);
	POINTERS_EQUAL(nullptr, result->autoParam);
}

TEST(ModControllable, getParamFromModEncoderWithCreation) {
	TestModControllable mc;

	char mem[MODEL_STACK_MAX_SIZE];
	memset(mem, 0, sizeof(mem));
	auto* ms3 = (ModelStackWithThreeMainThings*)mem;

	ModelStackWithAutoParam* result = mc.getParamFromModEncoder(0, ms3, true);
	POINTERS_EQUAL(nullptr, result->autoParam);
}

TEST(ModControllable, getModKnobModeReturnsNull) {
	TestModControllable mc;
	POINTERS_EQUAL(nullptr, mc.getModKnobMode());
}
