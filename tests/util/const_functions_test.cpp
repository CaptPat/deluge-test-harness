#include "CppUTest/TestHarness.h"
#include "util/const_functions.h"

// Mirror of firmware/tests/unit/function_tests.cpp

namespace {

TEST_GROUP(ConstFunctions){};

TEST(ConstFunctions, modNegativeInputs) {
	CHECK_EQUAL(0, mod(-3, 3));
	CHECK_EQUAL(1, mod(-2, 3));
	CHECK_EQUAL(2, mod(-1, 3));
}

TEST(ConstFunctions, modZeroAndPositive) {
	CHECK_EQUAL(0, mod(0, 3));
	CHECK_EQUAL(1, mod(1, 3));
	CHECK_EQUAL(2, mod(2, 3));
	CHECK_EQUAL(0, mod(3, 3));
}

TEST(ConstFunctions, modLargeValues) {
	CHECK_EQUAL(0, mod(12, 4));
	CHECK_EQUAL(1, mod(-11, 4));
	CHECK_EQUAL(3, mod(-1, 4));
}

} // namespace
