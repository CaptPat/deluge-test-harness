// Shadow header for model/song/song.h
// The real header pulls in reverb DSP, MIDI, clip, kit — hundreds of transitive deps.
// Redirect to our lightweight Song mock.
// Include util/functions.h to provide transitive declarations (getParamFromUserValue,
// combineHitStrengths, multiply_32x32_rshift32, ONE_Q31) that firmware code expects.
#pragma once
#include "modulation/params/param.h"
#include "song_mock.h"
#include "util/functions.h"
