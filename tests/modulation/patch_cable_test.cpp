// PatchCable regression tests.

#include "CppUTest/TestHarness.h"
#include "modulation/patch/patch_cable.h"
#include "storage/flash_storage.h"

TEST_GROUP(PatchCableTest) {
	void setup() override {
		// Reset flash defaults before each test
		FlashStorage::defaultPatchCablePolarity = Polarity::BIPOLAR;
		FlashStorage::defaultMagnitude = 0;
	}
};

TEST(PatchCableTest, stringToPolarityBipolar) {
	CHECK(Polarity::BIPOLAR == stringToPolarity("bipolar"));
}

TEST(PatchCableTest, stringToPolarityUnipolar) {
	CHECK(Polarity::UNIPOLAR == stringToPolarity("unipolar"));
}

TEST(PatchCableTest, stringToPolarityDefault) {
	// Unknown string → defaults to BIPOLAR
	CHECK(Polarity::BIPOLAR == stringToPolarity("unknown"));
	CHECK(Polarity::BIPOLAR == stringToPolarity(""));
}

TEST(PatchCableTest, polarityToStringRoundtrip) {
	CHECK(std::string_view("bipolar") == polarityToString(Polarity::BIPOLAR));
	CHECK(std::string_view("unipolar") == polarityToString(Polarity::UNIPOLAR));
}

TEST(PatchCableTest, polarityToStringShort) {
	CHECK(std::string_view("BPLR") == polarityToStringShort(Polarity::BIPOLAR));
	CHECK(std::string_view("UPLR") == polarityToStringShort(Polarity::UNIPOLAR));
}

TEST(PatchCableTest, hasPolarityAftertouch) {
	CHECK(PatchCable::hasPolarity(PatchSource::AFTERTOUCH));
}

TEST(PatchCableTest, hasPolarityXY) {
	CHECK_FALSE(PatchCable::hasPolarity(PatchSource::X));
	CHECK_FALSE(PatchCable::hasPolarity(PatchSource::Y));
}

TEST(PatchCableTest, hasPolarityLFO) {
	CHECK(PatchCable::hasPolarity(PatchSource::LFO_GLOBAL_1));
	CHECK(PatchCable::hasPolarity(PatchSource::LFO_LOCAL_1));
}

TEST(PatchCableTest, getDefaultPolarityAftertouch) {
	// Aftertouch always returns UNIPOLAR regardless of flash setting
	CHECK(Polarity::UNIPOLAR == PatchCable::getDefaultPolarity(PatchSource::AFTERTOUCH));
}

TEST(PatchCableTest, getDefaultPolarityXYSidechain) {
	// X, Y, SIDECHAIN always return BIPOLAR
	CHECK(Polarity::BIPOLAR == PatchCable::getDefaultPolarity(PatchSource::X));
	CHECK(Polarity::BIPOLAR == PatchCable::getDefaultPolarity(PatchSource::Y));
	CHECK(Polarity::BIPOLAR == PatchCable::getDefaultPolarity(PatchSource::SIDECHAIN));
}

TEST(PatchCableTest, getDefaultPolarityLFOUsesFlash) {
	// LFO sources use the FlashStorage default
	FlashStorage::defaultPatchCablePolarity = Polarity::BIPOLAR;
	CHECK(Polarity::BIPOLAR == PatchCable::getDefaultPolarity(PatchSource::LFO_GLOBAL_1));

	FlashStorage::defaultPatchCablePolarity = Polarity::UNIPOLAR;
	CHECK(Polarity::UNIPOLAR == PatchCable::getDefaultPolarity(PatchSource::LFO_GLOBAL_1));
}

TEST(PatchCableTest, setupSetsFields) {
	PatchCable cable;
	cable.setup(PatchSource::LFO_GLOBAL_1, 5, 100);
	CHECK(PatchSource::LFO_GLOBAL_1 == cable.from);
	CHECK_EQUAL(100, cable.param.currentValue);
}

TEST(PatchCableTest, isActiveEmpty) {
	PatchCable cable;
	cable.param.currentValue = 0;
	CHECK_FALSE(cable.isActive());
}

TEST(PatchCableTest, isActiveNonZero) {
	PatchCable cable;
	cable.param.currentValue = 42;
	CHECK(cable.isActive());
}

TEST(PatchCableTest, makeUnusable) {
	PatchCable cable;
	cable.setup(PatchSource::LFO_GLOBAL_1, 5, 100);
	cable.makeUnusable();
	CHECK(cable.destinationParamDescriptor.isNull());
}
