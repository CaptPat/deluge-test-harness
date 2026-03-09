// Shadow header replacing firmware's io/midi/learned_midi.h
// The real header includes midi_device_manager.h and storage_manager.h (both deep).
// NoteRow embeds LearnedMIDI by value, so we must provide the full class
// with matching data layout but without the heavy dependencies.

#pragma once

#include <cstdint>

#define MIDI_MESSAGE_NONE 0
#define MIDI_MESSAGE_NOTE 1
#define MIDI_MESSAGE_CC 2

class Serializer;
class Deserializer;

// Minimal MIDICable for patch_cable_set.cpp (grabVelocityToLevelFromMIDIInput)
class MIDICable {
public:
	bool hasDefaultVelocityToLevelSet() { return defaultVelocityToLevel != 0; }
	int32_t defaultVelocityToLevel{0};
};

enum class MIDIMatchType { NO_MATCH, CHANNEL, MPE_MEMBER, MPE_MASTER };

class LearnedMIDI {
public:
	LearnedMIDI() : cable(nullptr), channelOrZone(255), noteOrCC(0) {}
	void clear() {
		cable = nullptr;
		channelOrZone = 255;
		noteOrCC = 0;
	}

	inline bool containsSomething() { return (channelOrZone != 255); }
	inline bool isForMPEZone() { return (channelOrZone >= 16); }

	// Data members — must match real LearnedMIDI layout
	MIDICable* cable;
	uint8_t channelOrZone;
	uint8_t noteOrCC;
};
