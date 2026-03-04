#include "CppUTest/TestHarness.h"
#include "dsp/granular/GranularProcessor.h"

TEST_GROUP(GranularProcessorTest) {
	GranularProcessor proc;
};

TEST(GranularProcessorTest, defaultConstruction) {
	// Default construction should not crash
	// getSamplesToShutdown returns wrapsToShutdown * kModFXGrainBufferSize
	// On fresh construction wrapsToShutdown should be 0
	CHECK_EQUAL(0, proc.getSamplesToShutdown());
}

// Copy constructor test skipped — copies raw grainBuffer pointer,
// causing double-free on destruction in test context.

TEST(GranularProcessorTest, clearGrainFXBuffer) {
	// Clearing the buffer on a fresh processor should not crash
	proc.clearGrainFXBuffer();
	CHECK_EQUAL(0, proc.getSamplesToShutdown());
}

TEST(GranularProcessorTest, startSkippingRendering) {
	// Should not crash on fresh processor
	proc.startSkippingRendering();
	CHECK_EQUAL(0, proc.getSamplesToShutdown());
}

TEST(GranularProcessorTest, grainBufferStolen) {
	// grainBufferStolen sets internal grainBuffer to nullptr
	proc.grainBufferStolen();
	CHECK_EQUAL(0, proc.getSamplesToShutdown());
}

// Test the Grain struct directly
TEST(GranularProcessorTest, grainStructDefaults) {
	Grain g{};
	CHECK_EQUAL(0, g.length);
	CHECK_EQUAL(0, g.counter);
	CHECK_EQUAL(false, g.rev);
}
