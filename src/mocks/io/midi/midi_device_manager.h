// Shadow header replacing firmware's io/midi/midi_device_manager.h
// Provides only the MIDIDeviceManager globals referenced by inline code.

#pragma once

class MIDICable;
class Deserializer;

namespace MIDIDeviceManager {
inline bool differentiatingInputsByDevice = false;

// Stub for learned_midi.cpp readFromFile() — returns nullptr (no device loaded)
inline MIDICable* readDeviceReferenceFromFile(Deserializer& reader) {
	(void)reader;
	return nullptr;
}
} // namespace MIDIDeviceManager
