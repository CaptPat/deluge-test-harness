// Shadow header: forward-declares ArpeggiatorSettings before
// the real mod_controllable_audio.h uses it as a pointer parameter.
#pragma once

class ArpeggiatorSettings;

// The real header is found via the firmware include paths.
// We use #include_next to skip this shadow and find the real one.
#include_next "model/mod_controllable/mod_controllable_audio.h"
