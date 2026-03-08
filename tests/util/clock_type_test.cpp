#include "CppUTest/TestHarness.h"
#include "OSLikeStuff/timers_interrupts/clock_type.h"
#include <cmath>

// Mirror of firmware/tests/unit/time_tests.cpp

namespace {

TEST_GROUP(ClockType){};

TEST(ClockType, doubleConversion) {
	Time one = 0.0;
	Time two = Time(0, 0);
	CHECK_EQUAL(double(one), double(two));
}

TEST(ClockType, doubleDoubleConversion) {
	Time one = 123.5;
	CHECK_EQUAL((double)one, 123.5);
}

TEST(ClockType, convertWithRolls) {
	double base = 1234.123456; // 9 clock rollovers
	Time one = base;
	double diff = std::abs((double)one - base);
	CHECK(diff < 0.00001);
}

} // namespace
