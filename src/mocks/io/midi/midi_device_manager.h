// Shadow header replacing firmware's io/midi/midi_device_manager.h
// Provides only the MIDIDeviceManager globals referenced by inline code.

#pragma once

namespace MIDIDeviceManager {
inline bool differentiatingInputsByDevice = false;
}
