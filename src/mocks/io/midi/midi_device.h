// Shadow header replacing firmware's io/midi/midi_device.h
// The real header pulls in heavy MIDI infrastructure.
// We include learned_midi.h for LearnedMIDI/MIDICable used by patch_cable_set.cpp.
#pragma once
#include "io/midi/learned_midi.h"
