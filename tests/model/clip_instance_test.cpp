// ClipInstance tests: constructor, change(), getColour()

#include "CppUTest/TestHarness.h"
#include "model/clip/clip_instance.h"
#include "clip_mocks.h"

TEST_GROUP(ClipInstance){};

TEST(ClipInstance, defaultConstructor) {
	ClipInstance ci;
	CHECK_EQUAL(0, ci.pos);
	CHECK_EQUAL(0, ci.length);
	POINTERS_EQUAL(nullptr, ci.clip);
}

TEST(ClipInstance, changeWithNullActionSetsFields) {
	ClipInstance ci;
	Clip mockClip(1);

	ci.change(nullptr, nullptr, 100, 200, &mockClip);

	CHECK_EQUAL(100, ci.pos);
	CHECK_EQUAL(200, ci.length);
	POINTERS_EQUAL(&mockClip, ci.clip);
}

TEST(ClipInstance, changeOverwritesPreviousValues) {
	ClipInstance ci;
	Clip clip1(1);
	Clip clip2(2);

	ci.change(nullptr, nullptr, 10, 20, &clip1);
	ci.change(nullptr, nullptr, 30, 40, &clip2);

	CHECK_EQUAL(30, ci.pos);
	CHECK_EQUAL(40, ci.length);
	POINTERS_EQUAL(&clip2, ci.clip);
}

TEST(ClipInstance, changeToNullClip) {
	ClipInstance ci;
	Clip clip1(1);

	ci.change(nullptr, nullptr, 10, 20, &clip1);
	ci.change(nullptr, nullptr, 0, 0, nullptr);

	CHECK_EQUAL(0, ci.pos);
	CHECK_EQUAL(0, ci.length);
	POINTERS_EQUAL(nullptr, ci.clip);
}

TEST(ClipInstance, getColourNullClipReturnsGrey) {
	ClipInstance ci;
	ci.clip = nullptr;

	RGB colour = ci.getColour();
	CHECK_EQUAL(128, colour.r);
	CHECK_EQUAL(128, colour.g);
	CHECK_EQUAL(128, colour.b);
}

TEST(ClipInstance, getColourWithClipReturnsSectionColour) {
	ClipInstance ci;
	Clip mockClip(1);
	mockClip.section = 0;
	ci.clip = &mockClip;

	RGB colour = ci.getColour();
	// defaultClipSectionColours[0] is RGB::monochrome(128) in our mock stubs
	CHECK_EQUAL(128, colour.r);
	CHECK_EQUAL(128, colour.g);
	CHECK_EQUAL(128, colour.b);
}

TEST(ClipInstance, changeWithNegativePos) {
	ClipInstance ci;
	ci.change(nullptr, nullptr, -500, 1000, nullptr);

	CHECK_EQUAL(-500, ci.pos);
	CHECK_EQUAL(1000, ci.length);
}
