// Shadow header replacing firmware's io/midi/midi_device.h
// The real header pulls in heavy MIDI infrastructure.
// Our learned_midi.h shadow already defines MIDICable, MIDIPort, and all
// constants needed by code that includes midi_device.h.
#pragma once
#include "io/midi/learned_midi.h"
