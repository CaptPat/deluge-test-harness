#include "CppUTest/TestHarness.h"
#include "dsp/envelope_follower/absolute_value.h"

#include <cmath>
#include <vector>

TEST_GROUP(AbsValueFollowerTest) {
	AbsValueFollower follower;
	void setup() override { follower.reset(); }
};

TEST(AbsValueFollowerTest, defaultConstruction) {
	AbsValueFollower f;
	CHECK_EQUAL(0, f.getAttack());
	CHECK_EQUAL(0, f.getRelease());
}

TEST(AbsValueFollowerTest, setupSetsParameters) {
	follower.setup(ONE_Q31 / 4, ONE_Q31 / 2);
	CHECK(follower.getAttack() > 0);
	CHECK(follower.getRelease() > 0);
}

TEST(AbsValueFollowerTest, setAttackReturnsMS) {
	int32_t ms = follower.setAttack(ONE_Q31 / 2);
	CHECK(ms > 0);
	CHECK_EQUAL(ms, follower.getAttackMS());
}

TEST(AbsValueFollowerTest, setReleaseReturnsMS) {
	int32_t ms = follower.setRelease(ONE_Q31 / 2);
	CHECK(ms > 0);
	CHECK_EQUAL(ms, follower.getReleaseMS());
}

TEST(AbsValueFollowerTest, attackIncreasesWithKnob) {
	int32_t msLow = follower.setAttack(ONE_Q31 / 10);
	int32_t msHigh = follower.setAttack(ONE_Q31 - 1);
	CHECK(msHigh > msLow);
}

TEST(AbsValueFollowerTest, releaseIncreasesWithKnob) {
	int32_t msLow = follower.setRelease(ONE_Q31 / 10);
	int32_t msHigh = follower.setRelease(ONE_Q31 - 1);
	CHECK(msHigh > msLow);
}

TEST(AbsValueFollowerTest, calcApproxRMSSilence) {
	follower.setup(ONE_Q31 / 4, ONE_Q31 / 4);
	std::vector<StereoSample> buf(128, StereoSample{0, 0});
	StereoFloatSample result = follower.calcApproxRMS(std::span<StereoSample>(buf));
	// log(1e-24) is approximately -55.3
	CHECK(result.l < -50.0f);
	CHECK(result.r < -50.0f);
}

TEST(AbsValueFollowerTest, calcApproxRMSNonZero) {
	follower.setup(ONE_Q31 / 4, ONE_Q31 / 4);
	std::vector<StereoSample> buf(128);
	for (auto& s : buf) {
		s.l = 10000000; // keep small enough to avoid int32 overflow in sum (128*10M < INT32_MAX)
		s.r = 10000000;
	}
	StereoFloatSample result = follower.calcApproxRMS(std::span<StereoSample>(buf));
	// Should be much larger than silence
	CHECK(result.l > -50.0f);
	CHECK(result.r > -50.0f);
}

TEST(AbsValueFollowerTest, calcApproxRMSTracking) {
	follower.setup(ONE_Q31 / 4, ONE_Q31 / 4);

	// First: quiet signal
	std::vector<StereoSample> quietBuf(128);
	for (auto& s : quietBuf) {
		s.l = 1000;
		s.r = 1000;
	}
	StereoFloatSample quietResult = follower.calcApproxRMS(std::span<StereoSample>(quietBuf));

	follower.reset();

	// Second: loud signal (keep under INT32_MAX/128 to avoid overflow in sum)
	std::vector<StereoSample> loudBuf(128);
	for (auto& s : loudBuf) {
		s.l = 15000000;
		s.r = 15000000;
	}
	StereoFloatSample loudResult = follower.calcApproxRMS(std::span<StereoSample>(loudBuf));

	// Loud should produce higher RMS than quiet
	CHECK(loudResult.l > quietResult.l);
	CHECK(loudResult.r > quietResult.r);
}

TEST(AbsValueFollowerTest, resetClearsState) {
	follower.setup(ONE_Q31 / 4, ONE_Q31 / 4);
	std::vector<StereoSample> buf(128);
	for (auto& s : buf) {
		s.l = 15000000;
		s.r = 15000000;
	}
	follower.calcApproxRMS(std::span<StereoSample>(buf));

	follower.reset();

	// After reset, silence should give near-silence RMS
	std::vector<StereoSample> silentBuf(128, StereoSample{0, 0});
	StereoFloatSample result = follower.calcApproxRMS(std::span<StereoSample>(silentBuf));
	CHECK(result.l < -50.0f);
}
