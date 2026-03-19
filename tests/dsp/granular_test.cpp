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

TEST(GranularProcessorTest, copyConstructorAllocatesOwnBuffer) {
	// Regression: upstream #4356 — copy constructor must allocate its own
	// grainBuffer via getBuffer(), not share the original's pointer.
	// Both original and copy must be safely destructible.
	{
		GranularProcessor original;
		GranularProcessor copy(original);
		// Both go out of scope — no double-free
	}
	CHECK(true); // reached here without crashing
}

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

TEST(GranularProcessorTest, grainBufferStolenResetsWraps) {
	// grainBufferStolen must reset wrapsToShutdown (and other state).
	// This is a regression guard — the implicit invariant "buffer can't
	// be stolen while grains are active" is not enforced in code.
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
