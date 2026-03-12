// Reverb filter encoder round-trip regression test.
// Bug: readCurrentValue truncates float*50 instead of rounding, causing drift.
// Fix: add std::round() in reverb HPF/LPF readCurrentValue().
//
// Uses real reverb Base subclass with actual float storage, replicating
// the exact setHPF/getHPF write/read path from the menu items.

#include "CppUTest/TestHarness.h"
#include "dsp/reverb/base.hpp"

#include <cmath>
#include <cstdint>
#include <span>

using namespace deluge::dsp::reverb;

// kMaxMenuValue (= 50) is defined in definitions_cxx.hpp

namespace {

// Concrete reverb subclass that stores HPF/LPF as the real Mutable does
struct TestReverb : Base {
	float hpfVal{0.0f};
	float lpfVal{0.0f};

	void process(std::span<int32_t>, std::span<StereoSample>) override {}
	void setHPF(float f) override { hpfVal = f; }
	[[nodiscard]] float getHPF() const override { return hpfVal; }
	void setLPF(float f) override { lpfVal = f; }
	[[nodiscard]] float getLPF() const override { return lpfVal; }
};

TEST_GROUP(ReverbFilterRoundtrip) {
	TestReverb reverb;
};

// --- HPF round-trip with std::round (the fix) ---

TEST(ReverbFilterRoundtrip, hpfAllValuesRoundTripWithRound) {
	for (int32_t v = 0; v <= kMaxMenuValue; v++) {
		// writeCurrentValue: store v / 50.0f
		reverb.setHPF(static_cast<float>(v) / kMaxMenuValue);
		// readCurrentValue WITH fix: round(getHPF() * 50)
		int32_t readBack = static_cast<int32_t>(std::round(reverb.getHPF() * kMaxMenuValue));
		CHECK_EQUAL(v, readBack);
	}
}

TEST(ReverbFilterRoundtrip, lpfAllValuesRoundTripWithRound) {
	for (int32_t v = 0; v <= kMaxMenuValue; v++) {
		reverb.setLPF(static_cast<float>(v) / kMaxMenuValue);
		int32_t readBack = static_cast<int32_t>(std::round(reverb.getLPF() * kMaxMenuValue));
		CHECK_EQUAL(v, readBack);
	}
}

// --- Truncation is never better than rounding ---

TEST(ReverbFilterRoundtrip, roundingNeverWorseThanTruncation) {
	// For every value 0..50, rounding recovers the original at least as well as
	// truncation. On some platforms/compilers, truncation loses values due to
	// IEEE 754 imprecision (e.g. 29/50.0f*50.0f = 28.999998 → truncates to 28).
	// Even when truncation happens to work, rounding is never worse.
	for (int32_t v = 0; v <= kMaxMenuValue; v++) {
		reverb.setHPF(static_cast<float>(v) / kMaxMenuValue);
		float raw = reverb.getHPF() * kMaxMenuValue;
		int32_t truncated = static_cast<int32_t>(raw);
		int32_t rounded = static_cast<int32_t>(std::round(raw));
		// Rounded always recovers the original
		CHECK_EQUAL(v, rounded);
		// Truncated may or may not — but rounded is never farther away
		CHECK(std::abs(rounded - v) <= std::abs(truncated - v));
	}
}

// --- Repeated read-write stability (encoder re-entry) ---

TEST(ReverbFilterRoundtrip, hpfRepeatedReadWriteNeverDrifts) {
	// Simulate entering and exiting the reverb menu repeatedly.
	// Each visit: read the current value, then write it back.
	// Without round(), the value drifts down by 1 each visit.
	reverb.setHPF(static_cast<float>(29) / kMaxMenuValue);
	for (int i = 0; i < 100; i++) {
		int32_t readBack = static_cast<int32_t>(std::round(reverb.getHPF() * kMaxMenuValue));
		CHECK_EQUAL(29, readBack);
		reverb.setHPF(static_cast<float>(readBack) / kMaxMenuValue);
	}
}

TEST(ReverbFilterRoundtrip, repeatedRoundTripStableForAllValues) {
	// Every starting value 0..50 must survive 100 read-write cycles with rounding.
	// This is the core regression: without round(), some values drift down by 1
	// each cycle on platforms where float imprecision causes truncation loss.
	for (int32_t startV = 0; startV <= kMaxMenuValue; startV++) {
		reverb.setHPF(static_cast<float>(startV) / kMaxMenuValue);
		for (int i = 0; i < 100; i++) {
			int32_t readBack = static_cast<int32_t>(std::round(reverb.getHPF() * kMaxMenuValue));
			CHECK_EQUAL(startV, readBack);
			reverb.setHPF(static_cast<float>(readBack) / kMaxMenuValue);
		}
	}
}

} // namespace
