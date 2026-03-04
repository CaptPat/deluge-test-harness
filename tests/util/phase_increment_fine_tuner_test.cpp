#include "CppUTest/TestHarness.h"
#include "util/phase_increment_fine_tuner.h"

TEST_GROUP(PhaseIncrementFineTunerTest) {};

TEST(PhaseIncrementFineTunerTest, constructorNoDetune) {
	PhaseIncrementFineTuner tuner;
	int32_t input = 1000000;
	int32_t output = tuner.detune(input);
	// With no detune, output should be approximately equal to input
	// multiply_32x32_rshift32(x, 1073741824) << 2 = (x * 2^30 >> 32) << 2 = x
	CHECK_EQUAL(input, output);
}

TEST(PhaseIncrementFineTunerTest, setNoDetuneIdentity) {
	PhaseIncrementFineTuner tuner;
	tuner.setup(100000); // Apply some detune first
	tuner.setNoDetune();  // Then reset
	int32_t input = 1000000;
	CHECK_EQUAL(input, tuner.detune(input));
}

TEST(PhaseIncrementFineTunerTest, setupPositiveDetune) {
	PhaseIncrementFineTuner tuner;
	tuner.setup(50000000); // Large positive detune
	int32_t input = 1000000;
	int32_t output = tuner.detune(input);
	CHECK(output > input);
}

TEST(PhaseIncrementFineTunerTest, setupNegativeDetune) {
	PhaseIncrementFineTuner tuner;
	tuner.setup(-50000000); // Large negative detune
	int32_t input = 1000000;
	int32_t output = tuner.detune(input);
	CHECK(output < input);
}

TEST(PhaseIncrementFineTunerTest, setupZeroDetune) {
	PhaseIncrementFineTuner tuner;
	tuner.setup(0);
	int32_t input = 1000000;
	int32_t output = tuner.detune(input);
	// Zero detune should give approximately the identity
	int32_t tolerance = input / 100; // 1%
	CHECK(output >= input - tolerance);
	CHECK(output <= input + tolerance);
}

TEST(PhaseIncrementFineTunerTest, detunePreservesSign) {
	PhaseIncrementFineTuner tuner;
	int32_t negativeInput = -1000000;
	int32_t output = tuner.detune(negativeInput);
	CHECK(output < 0);
}
