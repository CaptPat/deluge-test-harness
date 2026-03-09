// Shadow header replacing firmware's processing/sound/sound.h
// The real header pulls in arpeggiator.h, mod_controllable_audio.h,
// sample_recorder.h, sidechain.h, source.h, and many more.
// Phase H: expanded for patcher.cpp (getSmoothedPatchedParamValue).

#pragma once
#include <cstdint>

// Pull in DSP math utilities that envelope.cpp expects to be transitively
// available through the real sound.h → voice.h → modulation chain.
#include "io/debug/log.h"
#include "modulation/params/param_manager.h"
#include "modulation/params/param_set.h"
#include "modulation/patch/patcher.h"
#include "util/functions.h"

class ParamManagerForTimeline;

#define PARAM_LPF_OFF (-1)

struct ParamLPF {
	int32_t p{PARAM_LPF_OFF};
	int32_t currentValue{0};
};

class Sound {
public:
	Sound() = default;
	// Default: sustain is present, so noteOff transitions to RELEASE normally.
	bool envelopeHasSustainCurrently(int32_t /*envelopeIndex*/, ParamManagerForTimeline* /*pm*/) { return true; }

	// Phase H: patcher.cpp calls this to get smoothed param values
	[[gnu::always_inline]] int32_t getSmoothedPatchedParamValue(int32_t p, ParamManager& paramManager) const {
		if (paramLPF.p == p) {
			return paramLPF.currentValue;
		}
		return paramManager.getPatchedParamSet()->getValue(p);
	}

	// Phase I: param_set.cpp PatchedParamSet::notifyParamModifiedInSomeWay calls this
	void notifyValueChangeViaLPF(int32_t /*p*/, bool /*shouldDoParamLPF*/,
	                             ModelStackWithThreeMainThings const* /*modelStack*/, int32_t /*oldValue*/,
	                             int32_t /*newValue*/, bool /*fromAutomation*/) {}

	// Phase I2: patch_cable_set.cpp uses these
	PatchCableAcceptance maySourcePatchToParam(PatchSource /*s*/, uint8_t /*p*/, ParamManager* /*paramManager*/) {
		return PatchCableAcceptance::EDITABLE;
	}
	void recalculatePatchingToParam(uint8_t /*p*/, ParamManagerForTimeline* /*paramManager*/) {}

	ParamLPF paramLPF{};
};
