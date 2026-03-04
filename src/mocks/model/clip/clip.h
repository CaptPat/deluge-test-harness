// Shadow header replacing firmware's model/clip/clip.h
// The real clip.h pulls in gui/views/audio_clip_view.h (blocked).
// Redirect to the existing Clip mock.
#pragma once
#include "clip_mocks.h"
