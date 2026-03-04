// Shadow header replacing firmware's model/voice/voice.h
// The real header pulls in dsp/filter, sample_playback, voice_unison_part, etc.
// envelope.cpp only accesses voice->paramFinalValues[envelopeIndex], so we
// provide a minimal class with just that array.

#pragma once
#include "definitions_cxx.hpp"
#include "modulation/params/param.h"
#include <cstdint>

class Voice {
public:
	int32_t paramFinalValues[deluge::modulation::params::LOCAL_LAST]{};
};
