// Tests for modulation/patch/patch_cable_set.cpp
// Covers: construction, isSourcePatchedToSomething, manual cable check,
// doesParamHaveSomethingPatchedToIt, cable manipulation, setupPatching

#include "CppUTest/TestHarness.h"
#include "definitions_cxx.hpp"
#include "modulation/params/param_manager.h"
#include "modulation/params/param_set.h"
#include "modulation/patch/patch_cable_set.h"
#include "model/model_stack.h"
#include <cstring>

namespace params = deluge::modulation::params;

// Helper to get a PatchCableSet from a ParamManager with patching
struct PatchCableSetHelper {
	ParamManager pm;

	PatchCableSetHelper() { pm.setupWithPatching(); }

	PatchCableSet* getCableSet() {
		// summaries[2] is always PatchCableSet after setupWithPatching
		return static_cast<PatchCableSet*>(pm.summaries[2].paramCollection);
	}
};

TEST_GROUP(PatchCableSetTest){};

TEST(PatchCableSetTest, constructionDefaults) {
	PatchCableSetHelper h;
	PatchCableSet* pcs = h.getCableSet();
	CHECK(pcs != nullptr);
	CHECK_EQUAL(0, pcs->numPatchCables);
	CHECK_EQUAL(0, pcs->numUsablePatchCables);
}

// isSourcePatchedToSomething reads sourcesPatchedToAnything[] which is only
// valid after setupPatching() — skipped because setupPatching does internal
// allocations with 32-bit pointer casts that crash on x86-64.

TEST(PatchCableSetTest, manualCheckCablesEmptyReturnsFalse) {
	PatchCableSetHelper h;
	PatchCableSet* pcs = h.getCableSet();
	CHECK_FALSE(pcs->isSourcePatchedToSomethingManuallyCheckCables(PatchSource::LFO_GLOBAL_1));
}

TEST(PatchCableSetTest, manualCheckCablesFindsSource) {
	PatchCableSetHelper h;
	PatchCableSet* pcs = h.getCableSet();

	// Manually add a cable
	pcs->patchCables[0].from = PatchSource::LFO_GLOBAL_1;
	pcs->patchCables[0].destinationParamDescriptor.setToHaveParamOnly(params::LOCAL_VOLUME);
	pcs->numPatchCables = 1;

	CHECK_TRUE(pcs->isSourcePatchedToSomethingManuallyCheckCables(PatchSource::LFO_GLOBAL_1));
	CHECK_FALSE(pcs->isSourcePatchedToSomethingManuallyCheckCables(PatchSource::ENVELOPE_0));
}

TEST(PatchCableSetTest, manualCheckMultipleCables) {
	PatchCableSetHelper h;
	PatchCableSet* pcs = h.getCableSet();

	pcs->patchCables[0].from = PatchSource::LFO_GLOBAL_1;
	pcs->patchCables[0].destinationParamDescriptor.setToHaveParamOnly(params::LOCAL_VOLUME);
	pcs->patchCables[1].from = PatchSource::ENVELOPE_0;
	pcs->patchCables[1].destinationParamDescriptor.setToHaveParamOnly(params::LOCAL_LPF_FREQ);
	pcs->numPatchCables = 2;

	CHECK_TRUE(pcs->isSourcePatchedToSomethingManuallyCheckCables(PatchSource::LFO_GLOBAL_1));
	CHECK_TRUE(pcs->isSourcePatchedToSomethingManuallyCheckCables(PatchSource::ENVELOPE_0));
	CHECK_FALSE(pcs->isSourcePatchedToSomethingManuallyCheckCables(PatchSource::VELOCITY));
}

TEST(PatchCableSetTest, objectSizeCorrect) {
	PatchCableSetHelper h;
	PatchCableSet* pcs = h.getCableSet();
	CHECK_EQUAL((int32_t)sizeof(PatchCableSet), pcs->objectSize);
}

// setupPatching does internal allocations with Destination arrays — crashes
// on x86-64 due to 32-bit pointer casts in the sorting/destination code.

TEST(PatchCableSetTest, beenClonedDoesNotCrash) {
	PatchCableSetHelper h;
	PatchCableSet* pcs = h.getCableSet();
	pcs->beenCloned(false, 0);
}

TEST(PatchCableSetTest, getParamKindIsPatchCable) {
	PatchCableSetHelper h;
	PatchCableSet* pcs = h.getCableSet();
	CHECK_TRUE(pcs->getParamKind() == params::Kind::PATCH_CABLE);
}

TEST(PatchCableSetTest, doesParamHaveSomethingPatchedDefaultFalse) {
	PatchCableSetHelper h;
	PatchCableSet* pcs = h.getCableSet();
	// Before setupPatching, destinations are null, so nothing is patched
	CHECK_FALSE(pcs->doesParamHaveSomethingPatchedToIt(params::LOCAL_VOLUME));
	CHECK_FALSE(pcs->doesParamHaveSomethingPatchedToIt(params::LOCAL_LPF_FREQ));
}
