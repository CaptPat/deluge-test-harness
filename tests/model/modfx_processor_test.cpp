#include "CppUTest/TestHarness.h"
#include "model/mod_controllable/ModFXProcessor.h"

TEST_GROUP(ModFXProcessorTest) {
	ModFXProcessor proc;
};

TEST(ModFXProcessorTest, defaultState) {
	CHECK(proc.modFXBuffer == nullptr);
	CHECK_EQUAL(0, proc.modFXBufferWriteIndex);
	CHECK_EQUAL(0, proc.phaserMemory.l);
	CHECK_EQUAL(0, proc.phaserMemory.r);
}

TEST(ModFXProcessorTest, allpassMemoryZeroInit) {
	bool allZero = true;
	for (int i = 0; i < kNumAllpassFiltersPhaser; i++) {
		if (proc.allpassMemory[i].l != 0 || proc.allpassMemory[i].r != 0) {
			allZero = false;
			break;
		}
	}
	CHECK(allZero);
}

TEST(ModFXProcessorTest, resetMemory) {
	// Dirty the memory
	proc.phaserMemory.l = 12345;
	proc.phaserMemory.r = 67890;
	proc.resetMemory();
	CHECK_EQUAL(0, proc.phaserMemory.l);
	CHECK_EQUAL(0, proc.phaserMemory.r);
}

TEST(ModFXProcessorTest, setupAndDisableBuffer) {
	proc.setupBuffer();
	// After setup, buffer should be allocated (or nullptr if alloc fails in test)
	// Either way, disableBuffer should not crash
	proc.disableBuffer();
	CHECK(proc.modFXBuffer == nullptr);
}

TEST(ModFXProcessorTest, disableBufferWhenNull) {
	// Disabling when already null should be safe
	proc.disableBuffer();
	CHECK(proc.modFXBuffer == nullptr);
}

TEST(ModFXProcessorTest, tickLFO) {
	// Ticking the LFO should not crash
	proc.tickLFO(1, 1000);
}

// modfx::modFXToString and getModNames are defined in global_effectable.cpp
// which is too heavy to compile — skipping those tests.
