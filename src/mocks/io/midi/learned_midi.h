// Shadow header replacing firmware's io/midi/learned_midi.h
// Provides the real LearnedMIDI class declaration with all methods needed by
// the real learned_midi.cpp, but replaces the heavy #includes (midi_device.h,
// midi_device_manager.h, storage_manager.h) with minimal shadow versions.

#pragma once

#include "io/midi/midi_device_manager.h" // shadow — MIDIDeviceManager::differentiatingInputsByDevice
#include "storage/storage_manager.h"     // shadow — Serializer/Deserializer mocks
#include <algorithm>
#include <cstdint>

#define MIDI_MESSAGE_NONE 0
#define MIDI_MESSAGE_NOTE 1
#define MIDI_MESSAGE_CC 2

// MIDI_CHANNEL_NONE, MIDI_CHANNEL_MPE_LOWER_ZONE, IS_A_CC etc. come from
// definitions_cxx.hpp, already included transitively by storage_manager.h shadow.

#define MIDI_DIRECTION_INPUT_TO_DELUGE 0
#define MIDI_DIRECTION_OUTPUT_FROM_DELUGE 1

// ── MIDIPort (minimal shadow) ───────────────────────────────────────────
class MIDICable;

class MIDIPort {
public:
	MIDIPort() : mpeLowerZoneLastMemberChannel(0), mpeUpperZoneLastMemberChannel(15) {}

	int32_t channelToZone(int32_t inputChannel) {
		if (mpeLowerZoneLastMemberChannel && mpeLowerZoneLastMemberChannel >= inputChannel) {
			return MIDI_CHANNEL_MPE_LOWER_ZONE;
		}
		if (mpeUpperZoneLastMemberChannel < 15 && mpeUpperZoneLastMemberChannel <= inputChannel) {
			return MIDI_CHANNEL_MPE_UPPER_ZONE;
		}
		return inputChannel;
	}

	bool isMasterChannel(int32_t inputChannel) {
		if (mpeLowerZoneLastMemberChannel && inputChannel == 0) {
			return true;
		}
		if (mpeUpperZoneLastMemberChannel < 15 && inputChannel == 15) {
			return true;
		}
		return false;
	}

	uint8_t mpeLowerZoneLastMemberChannel; // 0 means off
	uint8_t mpeUpperZoneLastMemberChannel; // 15 means off
};

// ── MIDICable (minimal shadow) ──────────────────────────────────────────
class MIDICable {
public:
	MIDICable() : defaultVelocityToLevel(0) {}

	bool hasDefaultVelocityToLevelSet() { return defaultVelocityToLevel != 0; }
	void writeReferenceToFile(Serializer& writer, char const* tagName = "device") {
		(void)writer;
		(void)tagName;
	}

	MIDIPort ports[2];
	int32_t defaultVelocityToLevel;
};

// ── MIDIMatchType ───────────────────────────────────────────────────────
enum class MIDIMatchType { NO_MATCH, CHANNEL, MPE_MEMBER, MPE_MASTER };

// ── LearnedMIDI ─────────────────────────────────────────────────────────
class LearnedMIDI {
public:
	LearnedMIDI();
	void clear();

	constexpr bool equalsCable(MIDICable* newCable) const {
		return (!MIDIDeviceManager::differentiatingInputsByDevice || !cable || newCable == cable);
	}

	constexpr bool equalsChannelOrZone(MIDICable* newCable, int32_t newChannelOrZone) const {
		return (newChannelOrZone == channelOrZone && equalsCable(newCable));
	}

	constexpr bool equalsNoteOrCC(MIDICable* newCable, int32_t newChannel, int32_t newNoteOrCC) const {
		return (newNoteOrCC == noteOrCC && equalsChannelOrZone(newCable, newChannel));
	}

	bool equalsChannelAllowMPE(MIDICable* newCable, int32_t newChannel);
	bool equalsChannelAllowMPEMasterChannels(MIDICable* newCable, int32_t newChannel);

	inline bool equalsNoteOrCCAllowMPE(MIDICable* newCable, int32_t newChannel, int32_t newNoteOrCC) {
		return (newNoteOrCC == noteOrCC && equalsChannelAllowMPE(newCable, newChannel));
	}

	inline bool equalsNoteOrCCAllowMPEMasterChannels(MIDICable* newCable, int32_t newChannel, int32_t newNoteOrCC) {
		return (newNoteOrCC == noteOrCC && equalsChannelAllowMPEMasterChannels(newCable, newChannel));
	}

	MIDIMatchType checkMatch(MIDICable* fromCable, int32_t channel);
	inline bool containsSomething() { return (channelOrZone != MIDI_CHANNEL_NONE); }
	inline bool isForMPEZone() { return (channelOrZone >= 16); }
	inline int32_t getMasterChannel() { return (channelOrZone - MIDI_CHANNEL_MPE_LOWER_ZONE) * 15; }

	void writeAttributesToFile(Serializer& writer, int32_t midiMessageType);
	void writeToFile(Serializer& writer, char const* commandName, int32_t midiMessageType);
	void readFromFile(Deserializer& reader, int32_t midiMessageType);

	inline void writeNoteToFile(Serializer& writer, char const* commandName) {
		writeToFile(writer, commandName, MIDI_MESSAGE_NOTE);
	}
	inline void writeCCToFile(Serializer& writer, char const* commandName) {
		writeToFile(writer, commandName, MIDI_MESSAGE_CC);
	}
	inline void writeChannelToFile(Serializer& writer, char const* commandName) {
		writeToFile(writer, commandName, MIDI_MESSAGE_NONE);
	}

	inline void readNoteFromFile(Deserializer& reader) { readFromFile(reader, MIDI_MESSAGE_NOTE); }
	inline void readCCFromFile(Deserializer& reader) { readFromFile(reader, MIDI_MESSAGE_CC); }
	inline void readChannelFromFile(Deserializer& reader) { readFromFile(reader, MIDI_MESSAGE_NONE); }

	void readMPEZone(Deserializer& reader);

	// Data members — must match real LearnedMIDI layout
	MIDICable* cable;
	uint8_t channelOrZone;
	uint8_t noteOrCC;
};
