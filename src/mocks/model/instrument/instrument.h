// Shadow header replacing firmware's model/instrument/instrument.h
// The real header pulls in io/midi/learned_midi.h and clip_instance_vector.h.
// We provide the Output include since downstream code expects it transitively.
#pragma once
#include "model/output.h"
