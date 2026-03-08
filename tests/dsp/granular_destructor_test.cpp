// GranularProcessor destructor test for PR #4362.
// Verifies that destruction with null grainBuffer doesn't crash.

#include "CppUTest/TestHarness.h"
#include "dsp/granular/GranularProcessor.h"

TEST_GROUP(GranularDestructorTest) {};

TEST(GranularDestructorTest, destructionWithNullBufferDoesNotCrash) {
	// Default-constructed processor has grainBuffer = nullptr.
	// The destructor must handle this without crashing.
	{
		GranularProcessor proc;
		// proc goes out of scope here — destructor runs
	}
	CHECK_TRUE(true); // If we got here, no crash
}

TEST(GranularDestructorTest, multipleDefaultConstructAndDestroy) {
	// Repeated construction/destruction should be safe.
	for (int i = 0; i < 10; i++) {
		GranularProcessor proc;
	}
	CHECK_TRUE(true);
}
