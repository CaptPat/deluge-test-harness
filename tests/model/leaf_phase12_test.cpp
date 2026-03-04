#include "CppUTest/TestHarness.h"
#include "hid/display/numeric_layer/numeric_layer.h"
#include "storage/multi_range/multi_range.h"

// --- NumericLayer (abstract — need concrete subclass) ---

class TestNumericLayer : public NumericLayer {
public:
	TestNumericLayer() = default;
	explicit TestNumericLayer(uint8_t dot) : NumericLayer(dot) {}

	bool callBack() override { return false; }
	void render(uint8_t* returnSegments) override { (void)returnSegments; }
	void isNowOnTop() override {}
};

TEST_GROUP(NumericLayerTest) {};

TEST(NumericLayerTest, defaultFixedDot) {
	TestNumericLayer layer;
	CHECK_EQUAL(255, layer.fixedDot);
	CHECK(layer.next == nullptr);
}

TEST(NumericLayerTest, constructWithDot) {
	TestNumericLayer layer(3);
	CHECK_EQUAL(3, layer.fixedDot);
}

TEST(NumericLayerTest, nextChaining) {
	TestNumericLayer a, b;
	a.next = &b;
	CHECK(a.next == &b);
	CHECK(b.next == nullptr);
}

// --- MultiRange (abstract — need concrete subclass) ---

class TestMultiRange : public MultiRange {
public:
	AudioFileHolder* getAudioFileHolder() override { return nullptr; }
};

TEST_GROUP(MultiRangeTest) {};

TEST(MultiRangeTest, defaultTopNote) {
	TestMultiRange range;
	// topNote default from constructor
	(void)range.topNote; // Just verify field exists and is accessible
}

TEST(MultiRangeTest, setTopNote) {
	TestMultiRange range;
	range.topNote = 60; // Middle C
	CHECK_EQUAL(60, range.topNote);
}

TEST(MultiRangeTest, getAudioFileHolderReturnsNull) {
	TestMultiRange range;
	CHECK(range.getAudioFileHolder() == nullptr);
}
