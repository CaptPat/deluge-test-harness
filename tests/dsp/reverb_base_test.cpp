#include "CppUTest/TestHarness.h"
#include "dsp/reverb/base.hpp"
#include <cmath>

using namespace deluge::dsp::reverb;

namespace {

TEST_GROUP(ReverbBase){};

// --- Pan levels ---

TEST(ReverbBase, panLevelsDefault) {
	// Need a concrete subclass since Base is abstract
	struct TestReverb : Base {
		void process(std::span<int32_t>, std::span<StereoSample>) override {}
	};
	TestReverb rev;
	CHECK_EQUAL(0, rev.getPanLeft());
	CHECK_EQUAL(0, rev.getPanRight());
}

TEST(ReverbBase, setPanLevels) {
	struct TestReverb : Base {
		void process(std::span<int32_t>, std::span<StereoSample>) override {}
	};
	TestReverb rev;
	rev.setPanLevels(100, 200);
	CHECK_EQUAL(100, rev.getPanLeft());
	CHECK_EQUAL(200, rev.getPanRight());
}

// --- calcFilterCutoff ---

TEST(ReverbBase, lpfCutoffAtZero) {
	float wc = Base::calcFilterCutoff<Base::FilterType::LowPass>(0.0f);
	// At f=0: fc_hz = 0 + (exp(0) - 1) * 5083.74 = 0
	// wc = 0 / (1 + 0) = 0
	DOUBLES_EQUAL(0.0, wc, 0.0001);
}

TEST(ReverbBase, lpfCutoffAtOne) {
	float wc = Base::calcFilterCutoff<Base::FilterType::LowPass>(1.0f);
	// At f=1: fc_hz = (exp(1.5) - 1) * 5083.74 ≈ 3.48 * 5083.74 ≈ 17691
	// fc = 17691 / 44100 ≈ 0.401
	// wc = 0.401 / 1.401 ≈ 0.286
	CHECK(wc > 0.0f);
	CHECK(wc < 1.0f);
}

TEST(ReverbBase, lpfCutoffMonotonic) {
	float prev = 0.0f;
	for (float f = 0.1f; f <= 1.0f; f += 0.1f) {
		float wc = Base::calcFilterCutoff<Base::FilterType::LowPass>(f);
		CHECK(wc >= prev);
		prev = wc;
	}
}

TEST(ReverbBase, hpfCutoffAtZero) {
	float wc = Base::calcFilterCutoff<Base::FilterType::HighPass>(0.0f);
	// At f=0: fc_hz = 20 + (exp(0) - 1) * 150 = 20
	// fc = 20 / 44100 ≈ 0.000454
	// wc ≈ 0.000454
	CHECK(wc > 0.0f);
	CHECK(wc < 0.01f);
}

TEST(ReverbBase, hpfCutoffAtOne) {
	float wc = Base::calcFilterCutoff<Base::FilterType::HighPass>(1.0f);
	// At f=1: fc_hz = 20 + (exp(1.5) - 1) * 150 ≈ 20 + 522 = 542
	CHECK(wc > 0.0f);
	CHECK(wc < 1.0f);
}

TEST(ReverbBase, hpfCutoffMonotonic) {
	float prev = 0.0f;
	for (float f = 0.1f; f <= 1.0f; f += 0.1f) {
		float wc = Base::calcFilterCutoff<Base::FilterType::HighPass>(f);
		CHECK(wc >= prev);
		prev = wc;
	}
}

// --- Dummy virtual getters return defaults ---

TEST(ReverbBase, dummyGettersReturnZero) {
	struct TestReverb : Base {
		void process(std::span<int32_t>, std::span<StereoSample>) override {}
	};
	TestReverb rev;
	DOUBLES_EQUAL(0.0f, rev.getRoomSize(), 0.001f);
	DOUBLES_EQUAL(0.0f, rev.getHPF(), 0.001f);
	DOUBLES_EQUAL(0.0f, rev.getLPF(), 0.001f);
	DOUBLES_EQUAL(0.0f, rev.getDamping(), 0.001f);
	DOUBLES_EQUAL(0.0f, rev.getWidth(), 0.001f);
}

// --- Dummy setters don't crash ---

TEST(ReverbBase, dummySettersDontCrash) {
	struct TestReverb : Base {
		void process(std::span<int32_t>, std::span<StereoSample>) override {}
	};
	TestReverb rev;
	rev.setRoomSize(0.5f);
	rev.setHPF(0.3f);
	rev.setLPF(0.7f);
	rev.setDamping(0.2f);
	rev.setWidth(1.0f);
	// Just verify no crash
	CHECK(true);
}

} // namespace
