// Shadow header replacing firmware's model/clip/instrument_clip.h
// The real instrument_clip.h pulls in gui/ui/keyboard/* (blocked).
// Redirect to the existing InstrumentClip mock.
#pragma once
#include "clip_mocks.h"
