#include "CppUTest/TestHarness.h"
#include "gui/colour/colour.h"

TEST_GROUP(RGBTest) {};

// --- Construction ---

TEST(RGBTest, defaultConstructor) {
	RGB c{};
	CHECK_EQUAL(0, c.r);
	CHECK_EQUAL(0, c.g);
	CHECK_EQUAL(0, c.b);
}

TEST(RGBTest, constructorValues) {
	RGB c{255, 128, 0};
	CHECK_EQUAL(255, c.r);
	CHECK_EQUAL(128, c.g);
	CHECK_EQUAL(0, c.b);
}

TEST(RGBTest, monochrome) {
	RGB c = RGB::monochrome(128);
	CHECK_EQUAL(128, c.r);
	CHECK_EQUAL(128, c.g);
	CHECK_EQUAL(128, c.b);
}

// --- Operators ---

TEST(RGBTest, equality) {
	CHECK(RGB(1, 2, 3) == RGB(1, 2, 3));
	CHECK(!(RGB(1, 2, 3) == RGB(4, 5, 6)));
}

TEST(RGBTest, indexOperator) {
	RGB c{10, 20, 30};
	CHECK_EQUAL(10, c[0]);
	CHECK_EQUAL(20, c[1]);
	CHECK_EQUAL(30, c[2]);
}

TEST(RGBTest, indexOperatorWrite) {
	RGB c{};
	c[0] = 100;
	c[1] = 200;
	c[2] = 50;
	CHECK_EQUAL(100, c.r);
	CHECK_EQUAL(200, c.g);
	CHECK_EQUAL(50, c.b);
}

// --- Transforms ---

TEST(RGBTest, dim) {
	RGB c{128, 64, 32};
	RGB d = c.dim();
	CHECK_EQUAL(64, d.r);
	CHECK_EQUAL(32, d.g);
	CHECK_EQUAL(16, d.b);
}

TEST(RGBTest, dimLevel3) {
	RGB c{128, 128, 128};
	RGB d = c.dim(3);
	CHECK_EQUAL(16, d.r);
	CHECK_EQUAL(16, d.g);
	CHECK_EQUAL(16, d.b);
}

TEST(RGBTest, dull) {
	RGB bright{255, 0, 100};
	RGB d = bright.dull();
	CHECK_EQUAL(50, d.r);
	CHECK_EQUAL(5, d.g);
	CHECK_EQUAL(50, d.b);
}

TEST(RGBTest, average) {
	RGB a{100, 200, 0};
	RGB b{200, 100, 0};
	RGB avg = RGB::average(a, b);
	CHECK_EQUAL(150, avg.r);
	CHECK_EQUAL(150, avg.g);
	CHECK_EQUAL(0, avg.b);
}

TEST(RGBTest, blendFullA) {
	RGB a{255, 0, 0};
	RGB b{0, 0, 255};
	RGB result = RGB::blend(a, b, 65535);
	// Full weight to A
	CHECK_EQUAL(255, result.r);
	CHECK_EQUAL(0, result.b);
}

TEST(RGBTest, blendMidpoint) {
	RGB a{200, 0, 0};
	RGB b{0, 0, 200};
	RGB result = RGB::blend(a, b, 32768); // ~50/50 blend
	// Both channels should have some contribution
	CHECK(result.r > 0);
	CHECK(result.b > 0);
}

TEST(RGBTest, adjustIntensity) {
	RGB c{255, 128, 64};
	RGB result = c.adjust(128, 1);
	// Each channel: channel * 128 / 255 / 1
	CHECK_EQUAL(128, result.r); // 255*128/255 = 128
	CHECK_EQUAL(64, result.g);  // 128*128/255 = 64
	CHECK_EQUAL(32, result.b);  // 64*128/255 = 32
}

TEST(RGBTest, rotate) {
	RGB c{200, 100, 50};
	RGB rotated = c.rotate();
	// Rotation should produce a different colour
	CHECK(!(rotated == c));
	// At least one channel should be non-zero
	CHECK(rotated.r > 0 || rotated.g > 0 || rotated.b > 0);
}

TEST(RGBTest, forTail) {
	RGB c{200, 100, 50};
	RGB tail = c.forTail();
	// Should produce a valid derived colour
	CHECK(tail.r > 0 || tail.g > 0 || tail.b > 0);
}

TEST(RGBTest, forBlur) {
	RGB c{200, 100, 50};
	RGB blur = c.forBlur();
	// Should produce a valid derived colour
	CHECK(blur.r > 0 || blur.g > 0 || blur.b > 0);
}

// --- Hue-based construction (DSP) ---

TEST(RGBTest, fromHueProducesColour) {
	RGB c = RGB::fromHue(0);
	CHECK(c.r > 0 || c.g > 0 || c.b > 0);
}

TEST(RGBTest, fromHueVariesWithInput) {
	RGB c1 = RGB::fromHue(0);
	RGB c2 = RGB::fromHue(64);
	RGB c3 = RGB::fromHue(128);
	// Different hue values should produce different colours
	CHECK(!(c1 == c2));
	CHECK(!(c2 == c3));
}

TEST(RGBTest, fromHuePastelProducesColour) {
	RGB c = RGB::fromHuePastel(0);
	CHECK(c.r > 0 || c.g > 0 || c.b > 0);
}

TEST(RGBTest, fromHuePastelBrighterThanHue) {
	// Pastel colours should generally be brighter (higher total channel values)
	for (int hue = 0; hue < 192; hue += 32) {
		RGB normal = RGB::fromHue(hue);
		RGB pastel = RGB::fromHuePastel(hue);
		int32_t normalBrightness = normal.r + normal.g + normal.b;
		int32_t pastelBrightness = pastel.r + pastel.g + pastel.b;
		CHECK(pastelBrightness >= normalBrightness);
	}
}

// --- Palette constants ---

TEST(RGBTest, paletteBlack) {
	CHECK_EQUAL(0, deluge::gui::colours::black.r);
	CHECK_EQUAL(0, deluge::gui::colours::black.g);
	CHECK_EQUAL(0, deluge::gui::colours::black.b);
}

TEST(RGBTest, paletteRed) {
	CHECK_EQUAL(255, deluge::gui::colours::red.r);
	CHECK_EQUAL(0, deluge::gui::colours::red.g);
	CHECK_EQUAL(0, deluge::gui::colours::red.b);
}
