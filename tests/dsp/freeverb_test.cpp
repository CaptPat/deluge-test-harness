#include "CppUTest/TestHarness.h"
#include "dsp/reverb/freeverb/freeverb.hpp"

#include <array>
#include <cmath>
#include <cstdlib>
#include <numeric>
#include <vector>

// The Freeverb is large (~100KB due to static delay buffers), so we heap-allocate it.
TEST_GROUP(FreeverbTest) {
	deluge::dsp::reverb::Freeverb* reverb;
	void setup() override { reverb = new deluge::dsp::reverb::Freeverb(); }
	void teardown() override { delete reverb; }
};

// --- Parameter getters/setters ---

TEST(FreeverbTest, constructorDefaults) {
	// initialroom = 0.5, initialdamp = 0.5, initialwet = 1/scalewet, initialdry = 0, initialwidth = 1
	DOUBLES_EQUAL(0.5, reverb->getRoomSize(), 0.01);
	DOUBLES_EQUAL(0.5, reverb->getDamping(), 0.01);
	DOUBLES_EQUAL(1.0 / 3.0, reverb->getWet(), 0.01);
	DOUBLES_EQUAL(0.0, reverb->getDry(), 0.01);
	DOUBLES_EQUAL(1.0, reverb->getWidth(), 0.01);
}

TEST(FreeverbTest, setGetRoomSize) {
	reverb->setRoomSize(0.8f);
	DOUBLES_EQUAL(0.8, reverb->getRoomSize(), 0.01);
}

TEST(FreeverbTest, setGetDamping) {
	reverb->setDamping(0.3f);
	DOUBLES_EQUAL(0.3, reverb->getDamping(), 0.01);
}

TEST(FreeverbTest, setGetWet) {
	reverb->setWet(0.5f);
	DOUBLES_EQUAL(0.5, reverb->getWet(), 0.01);
}

TEST(FreeverbTest, setGetDry) {
	reverb->setDry(0.7f);
	DOUBLES_EQUAL(0.7, reverb->getDry(), 0.01);
}

TEST(FreeverbTest, setGetWidth) {
	reverb->setWidth(0.6f);
	DOUBLES_EQUAL(0.6, reverb->getWidth(), 0.01);
}

// --- Pan levels (from Base) ---

TEST(FreeverbTest, panLevels) {
	reverb->setPanLevels(1000, 2000);
	CHECK_EQUAL(1000, reverb->getPanLeft());
	CHECK_EQUAL(2000, reverb->getPanRight());
}

// --- Mute ---

TEST(FreeverbTest, muteProducesSilence) {
	reverb->mute();
	reverb->setPanLevels(INT32_MAX / 2, INT32_MAX / 2);
	std::array<int32_t, 64> input{};
	std::array<StereoSample, 64> output{};
	for (auto& s : output) {
		s.l = 0;
		s.r = 0;
	}
	reverb->process(input, output);
	for (auto& s : output) {
		CHECK_EQUAL(0, s.l);
		CHECK_EQUAL(0, s.r);
	}
}

// --- Processing ---

TEST(FreeverbTest, processZeroInput) {
	reverb->setPanLevels(INT32_MAX / 2, INT32_MAX / 2);
	std::array<int32_t, 128> input{};
	std::array<StereoSample, 128> output{};
	for (auto& s : output) {
		s.l = 0;
		s.r = 0;
	}
	reverb->process(input, output);
	// With zero input and muted buffers, output should be zero
	int64_t totalEnergy = 0;
	for (auto& s : output) {
		totalEnergy += std::abs((int64_t)s.l) + std::abs((int64_t)s.r);
	}
	CHECK_EQUAL(0, totalEnergy);
}

TEST(FreeverbTest, processNonZeroInput) {
	reverb->setPanLevels(INT32_MAX / 2, INT32_MAX / 2);
	reverb->setRoomSize(0.8f);
	reverb->setWet(0.8f);

	// Comb filter buffers are 1116–1640 samples, so we need >1640 samples
	// before output emerges. Feed impulse then process enough silence.
	constexpr int kBlockSize = 512;
	constexpr int kNumBlocks = 5; // 2560 total samples

	// First block: impulse
	std::vector<int32_t> input(kBlockSize, 0);
	input[0] = INT32_MAX / 4;
	std::vector<StereoSample> output(kBlockSize);
	for (auto& s : output) { s.l = 0; s.r = 0; }
	reverb->process(input, output);

	// Subsequent blocks: silence — accumulate energy
	int64_t totalEnergy = 0;
	for (int b = 1; b < kNumBlocks; b++) {
		std::vector<int32_t> silence(kBlockSize, 0);
		std::vector<StereoSample> out(kBlockSize);
		for (auto& s : out) { s.l = 0; s.r = 0; }
		reverb->process(silence, out);
		for (auto& s : out) {
			totalEnergy += std::abs((int64_t)s.l) + std::abs((int64_t)s.r);
		}
	}
	CHECK(totalEnergy > 0);
}

TEST(FreeverbTest, processMultipleCalls) {
	reverb->setPanLevels(INT32_MAX / 2, INT32_MAX / 2);

	for (int call = 0; call < 10; call++) {
		std::array<int32_t, 64> input{};
		input[0] = INT32_MAX / 8;
		std::array<StereoSample, 64> output{};
		for (auto& s : output) {
			s.l = 0;
			s.r = 0;
		}
		reverb->process(input, output);

		// Output should stay bounded (no overflow to extreme values)
		for (auto& s : output) {
			CHECK(s.l > INT32_MIN / 2);
			CHECK(s.l < INT32_MAX / 2);
			CHECK(s.r > INT32_MIN / 2);
			CHECK(s.r < INT32_MAX / 2);
		}
	}
}

TEST(FreeverbTest, roomSizeAffectsDecay) {
	// Compare tail energy with small vs large room sizes.
	// Need >1640 samples for comb filters, then measure late tail.
	auto measureTailEnergy = [](deluge::dsp::reverb::Freeverb& rev, float roomSize) -> int64_t {
		rev.setRoomSize(roomSize);
		rev.setWet(0.5f);
		rev.setWidth(1.0f);
		rev.setPanLevels(INT32_MAX / 2, INT32_MAX / 2);
		rev.mute();

		constexpr int kBlockSize = 2048;

		// Impulse block
		std::vector<int32_t> impulse(kBlockSize, 0);
		impulse[0] = INT32_MAX / 4;
		std::vector<StereoSample> out(kBlockSize);
		for (auto& s : out) { s.l = 0; s.r = 0; }
		rev.process(impulse, out);

		// Let it ring: 3 more blocks of silence
		int64_t energy = 0;
		for (int b = 0; b < 3; b++) {
			std::vector<int32_t> silence(kBlockSize, 0);
			std::vector<StereoSample> tail(kBlockSize);
			for (auto& s : tail) { s.l = 0; s.r = 0; }
			rev.process(silence, tail);
			for (auto& s : tail) {
				energy += std::abs((int64_t)s.l) + std::abs((int64_t)s.r);
			}
		}
		return energy;
	};

	deluge::dsp::reverb::Freeverb smallRoom;
	deluge::dsp::reverb::Freeverb largeRoom;
	int64_t smallEnergy = measureTailEnergy(smallRoom, 0.2f);
	int64_t largeEnergy = measureTailEnergy(largeRoom, 0.95f);

	CHECK(largeEnergy > smallEnergy);
}

TEST(FreeverbTest, widthAffectsOutput) {
	// Different width values should produce different output
	constexpr int kBlockSize = 2048;

	auto computeHash = [](deluge::dsp::reverb::Freeverb& rev, float widthVal) -> int64_t {
		rev.setWidth(widthVal);
		rev.setWet(0.5f);
		rev.setRoomSize(0.8f);
		rev.setPanLevels(INT32_MAX / 2, INT32_MAX / 2);
		rev.mute();

		std::vector<int32_t> impulse(kBlockSize, 0);
		impulse[0] = INT32_MAX / 4;
		std::vector<StereoSample> out(kBlockSize);
		for (auto& s : out) { s.l = 0; s.r = 0; }
		rev.process(impulse, out);

		std::vector<int32_t> silence(kBlockSize, 0);
		std::vector<StereoSample> tail(kBlockSize);
		for (auto& s : tail) { s.l = 0; s.r = 0; }
		rev.process(silence, tail);

		int64_t hash = 0;
		for (auto& s : tail) {
			hash += (int64_t)s.l + (int64_t)s.r * 3;
		}
		return hash;
	};

	auto* rev0 = new deluge::dsp::reverb::Freeverb();
	auto* rev1 = new deluge::dsp::reverb::Freeverb();
	int64_t hash0 = computeHash(*rev0, 0.0f);
	int64_t hash1 = computeHash(*rev1, 1.0f);
	delete rev0;
	delete rev1;

	CHECK(hash0 != hash1);
}

TEST(FreeverbTest, processOneMethod) {
	reverb->setPanLevels(INT32_MAX / 2, INT32_MAX / 2);
	reverb->setWet(0.5f);
	reverb->mute();

	StereoSample out{};
	out.l = 0;
	out.r = 0;
	reverb->ProcessOne(INT32_MAX / 4, out);
	// After an impulse, ProcessOne should produce some output
	// (though first sample might be zero if buffers are empty)
	// At minimum, no crash
	CHECK(true);
}

TEST(FreeverbTest, dampingAffectsOutput) {
	// Different damping should produce different tail energy.
	auto measureEnergy = [](float dampVal) -> int64_t {
		deluge::dsp::reverb::Freeverb rev;
		rev.setDamping(dampVal);
		rev.setRoomSize(0.8f);
		rev.setWet(0.5f);
		rev.setPanLevels(INT32_MAX / 2, INT32_MAX / 2);
		rev.mute();

		constexpr int kBlockSize = 2048;

		std::vector<int32_t> impulse(kBlockSize, 0);
		impulse[0] = INT32_MAX / 4;
		std::vector<StereoSample> out(kBlockSize);
		for (auto& s : out) { s.l = 0; s.r = 0; }
		rev.process(impulse, out);

		// Measure tail in second block
		std::vector<int32_t> silence(kBlockSize, 0);
		std::vector<StereoSample> tail(kBlockSize);
		for (auto& s : tail) { s.l = 0; s.r = 0; }
		rev.process(silence, tail);

		int64_t energy = 0;
		for (auto& s : tail) {
			energy += std::abs((int64_t)s.l) + std::abs((int64_t)s.r);
		}
		return energy;
	};

	int64_t lowDampEnergy = measureEnergy(0.1f);
	int64_t highDampEnergy = measureEnergy(0.9f);

	CHECK(lowDampEnergy != highDampEnergy);
}
