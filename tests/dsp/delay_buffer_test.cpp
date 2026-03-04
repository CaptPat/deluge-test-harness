#include "CppUTest/TestHarness.h"
#include "dsp/delay/delay_buffer.h"
#include "dsp/stereo_sample.h"
#include <cstring>

TEST_GROUP(DelayBufferTest) {
	DelayBuffer buf;

	void teardown() override {
		buf.discard();
	}
};

TEST(DelayBufferTest, defaultState) {
	// Default DelayBuffer should not have a buffer allocated
	CHECK_EQUAL(0, buf.size());
}

TEST(DelayBufferTest, idealSizeFromRate) {
	// getIdealBufferSizeFromRate returns a size and precision flag
	auto [size, precise] = DelayBuffer::getIdealBufferSizeFromRate(44100);
	CHECK(size > 0);
	CHECK(size <= DelayBuffer::kMaxSize);
}

TEST(DelayBufferTest, idealSizeSmallRate) {
	auto [size, precise] = DelayBuffer::getIdealBufferSizeFromRate(1);
	CHECK(size >= DelayBuffer::kMinSize);
}

TEST(DelayBufferTest, idealSizeLargeRate) {
	auto [size, precise] = DelayBuffer::getIdealBufferSizeFromRate(0xFFFFFFFF);
	CHECK(size >= DelayBuffer::kMinSize);
	CHECK(size <= DelayBuffer::kMaxSize);
}

TEST(DelayBufferTest, initAllocates) {
	Error err = buf.init(44100);
	CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
	CHECK(buf.size() > 0);
}

TEST(DelayBufferTest, initAndClear) {
	buf.init(44100);
	buf.clear(); // Should not crash — zeros the buffer
	CHECK(buf.size() > 0);
}

TEST(DelayBufferTest, discardFreesBuffer) {
	buf.init(44100);
	CHECK(buf.isActive());
	buf.discard();
	CHECK_FALSE(buf.isActive());
}

TEST(DelayBufferTest, isActiveAfterInit) {
	buf.init(44100);
	CHECK(buf.isActive());
}

TEST(DelayBufferTest, notActiveAfterDiscard) {
	buf.init(44100);
	buf.discard();
	CHECK_FALSE(buf.isActive());
}
