// Shadow header replacing firmware's processing/sound/sound.h
// The real header pulls in arpeggiator.h, mod_controllable_audio.h,
// sample_recorder.h, sidechain.h, source.h, and many more.
// envelope.cpp only calls sound->envelopeHasSustainCurrently().

#pragma once
#include <cstdint>

// Pull in DSP math utilities that envelope.cpp expects to be transitively
// available through the real sound.h → voice.h → modulation chain.
#include "io/debug/log.h"
#include "util/functions.h"

class ParamManagerForTimeline;

class Sound {
public:
	Sound() = default;
	// Default: sustain is present, so noteOff transitions to RELEASE normally.
	bool envelopeHasSustainCurrently(int32_t /*envelopeIndex*/, ParamManagerForTimeline* /*pm*/) { return true; }
};
