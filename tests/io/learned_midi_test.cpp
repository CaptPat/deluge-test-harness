// Tests for the real firmware LearnedMIDI class compiled against shadow headers.
// Covers construction, clear(), state queries, channel/MPE matching, and edge cases.

#include "CppUTest/TestHarness.h"
#include "io/midi/learned_midi.h"

// ── Test Group ──────────────────────────────────────────────────────────

TEST_GROUP(LearnedMIDI){};

// ── Construction and clear() ────────────────────────────────────────────

TEST(LearnedMIDI, DefaultConstructionIsCleared) {
	LearnedMIDI lm;
	CHECK_FALSE(lm.containsSomething());
	POINTERS_EQUAL(nullptr, lm.cable);
	CHECK_EQUAL(MIDI_CHANNEL_NONE, lm.channelOrZone);
	CHECK_EQUAL(255, lm.noteOrCC);
}

TEST(LearnedMIDI, ClearResetsToDefault) {
	LearnedMIDI lm;
	MIDICable cable;
	lm.cable = &cable;
	lm.channelOrZone = 5;
	lm.noteOrCC = 60;

	CHECK_TRUE(lm.containsSomething());

	lm.clear();

	CHECK_FALSE(lm.containsSomething());
	POINTERS_EQUAL(nullptr, lm.cable);
	CHECK_EQUAL(MIDI_CHANNEL_NONE, lm.channelOrZone);
	CHECK_EQUAL(255, lm.noteOrCC);
}

// ── containsSomething() / isForMPEZone() / getMasterChannel() ───────────

TEST(LearnedMIDI, ContainsSomethingWhenChannelSet) {
	LearnedMIDI lm;
	lm.channelOrZone = 0;
	CHECK_TRUE(lm.containsSomething());
}

TEST(LearnedMIDI, IsForMPEZoneLower) {
	LearnedMIDI lm;
	lm.channelOrZone = MIDI_CHANNEL_MPE_LOWER_ZONE;
	CHECK_TRUE(lm.isForMPEZone());
	CHECK_EQUAL(0, lm.getMasterChannel()); // lower zone master = channel 0
}

TEST(LearnedMIDI, IsForMPEZoneUpper) {
	LearnedMIDI lm;
	lm.channelOrZone = MIDI_CHANNEL_MPE_UPPER_ZONE;
	CHECK_TRUE(lm.isForMPEZone());
	CHECK_EQUAL(15, lm.getMasterChannel()); // upper zone master = channel 15
}

TEST(LearnedMIDI, IsNotMPEForNormalChannel) {
	LearnedMIDI lm;
	lm.channelOrZone = 5;
	CHECK_FALSE(lm.isForMPEZone());
}

// ── checkMatch() ────────────────────────────────────────────────────────

TEST(LearnedMIDI, CheckMatchChannelExact) {
	// Non-MPE: channelToZone returns the channel itself, so channelOrZone == channel.
	MIDICable cable;
	// Default MPE zones: lower=0(off), upper=15(off) => no MPE remapping
	LearnedMIDI lm;
	lm.cable = &cable;
	lm.channelOrZone = 5;

	MIDIMatchType result = lm.checkMatch(&cable, 5);
	CHECK_EQUAL((int)MIDIMatchType::CHANNEL, (int)result);
}

TEST(LearnedMIDI, CheckMatchNoMatchWrongChannel) {
	MIDICable cable;
	LearnedMIDI lm;
	lm.cable = &cable;
	lm.channelOrZone = 5;

	MIDIMatchType result = lm.checkMatch(&cable, 7);
	CHECK_EQUAL((int)MIDIMatchType::NO_MATCH, (int)result);
}

TEST(LearnedMIDI, CheckMatchMPEMember) {
	// Set up lower MPE zone: channels 0-3. Master=0, members=1-3.
	MIDICable cable;
	cable.ports[MIDI_DIRECTION_INPUT_TO_DELUGE].mpeLowerZoneLastMemberChannel = 3;

	LearnedMIDI lm;
	lm.cable = &cable;
	lm.channelOrZone = MIDI_CHANNEL_MPE_LOWER_ZONE; // learned as lower zone

	// Channel 2 is a member (not master channel 0)
	MIDIMatchType result = lm.checkMatch(&cable, 2);
	CHECK_EQUAL((int)MIDIMatchType::MPE_MEMBER, (int)result);
}

TEST(LearnedMIDI, CheckMatchMPEMaster) {
	// Lower MPE zone: channels 0-3. Master=0.
	MIDICable cable;
	cable.ports[MIDI_DIRECTION_INPUT_TO_DELUGE].mpeLowerZoneLastMemberChannel = 3;

	LearnedMIDI lm;
	lm.cable = &cable;
	lm.channelOrZone = MIDI_CHANNEL_MPE_LOWER_ZONE;

	// Channel 0 is the master
	MIDIMatchType result = lm.checkMatch(&cable, 0);
	CHECK_EQUAL((int)MIDIMatchType::MPE_MASTER, (int)result);
}

TEST(LearnedMIDI, CheckMatchNoMatchDifferentCable) {
	MIDICable cable1;
	MIDICable cable2;
	MIDIDeviceManager::differentiatingInputsByDevice = true;

	LearnedMIDI lm;
	lm.cable = &cable1;
	lm.channelOrZone = 5;

	MIDIMatchType result = lm.checkMatch(&cable2, 5);
	CHECK_EQUAL((int)MIDIMatchType::NO_MATCH, (int)result);

	MIDIDeviceManager::differentiatingInputsByDevice = false;
}

// ── equalsChannelAllowMPE() ─────────────────────────────────────────────

TEST(LearnedMIDI, EqualsChannelAllowMPE_NoneReturnsEarly) {
	LearnedMIDI lm; // channelOrZone == MIDI_CHANNEL_NONE
	MIDICable cable;
	CHECK_FALSE(lm.equalsChannelAllowMPE(&cable, 5));
}

TEST(LearnedMIDI, EqualsChannelAllowMPE_NoCableReturnsFalse) {
	LearnedMIDI lm;
	MIDICable cable;
	lm.cable = nullptr; // no cable set
	lm.channelOrZone = 5;
	CHECK_FALSE(lm.equalsChannelAllowMPE(&cable, 5));
}

TEST(LearnedMIDI, EqualsChannelAllowMPE_ExactChannelMatch) {
	MIDICable cable;
	LearnedMIDI lm;
	lm.cable = &cable;
	lm.channelOrZone = 5;
	CHECK_TRUE(lm.equalsChannelAllowMPE(&cable, 5));
}

TEST(LearnedMIDI, EqualsChannelAllowMPE_MPEMemberMapsToZone) {
	MIDICable cable;
	cable.ports[MIDI_DIRECTION_INPUT_TO_DELUGE].mpeLowerZoneLastMemberChannel = 4;

	LearnedMIDI lm;
	lm.cable = &cable;
	lm.channelOrZone = MIDI_CHANNEL_MPE_LOWER_ZONE;

	// Channel 2 is in lower zone => channelToZone returns MIDI_CHANNEL_MPE_LOWER_ZONE
	CHECK_TRUE(lm.equalsChannelAllowMPE(&cable, 2));
	// Channel 10 is NOT in lower zone
	CHECK_FALSE(lm.equalsChannelAllowMPE(&cable, 10));
}

// ── equalsNoteOrCC() ────────────────────────────────────────────────────

TEST(LearnedMIDI, EqualsNoteOrCC_ExactMatch) {
	MIDICable cable;
	LearnedMIDI lm;
	lm.cable = &cable;
	lm.channelOrZone = 3;
	lm.noteOrCC = 60;

	CHECK_TRUE(lm.equalsNoteOrCC(&cable, 3, 60));
}

TEST(LearnedMIDI, EqualsNoteOrCC_WrongNote) {
	MIDICable cable;
	LearnedMIDI lm;
	lm.cable = &cable;
	lm.channelOrZone = 3;
	lm.noteOrCC = 60;

	CHECK_FALSE(lm.equalsNoteOrCC(&cable, 3, 61));
}

TEST(LearnedMIDI, EqualsNoteOrCC_WrongChannel) {
	MIDICable cable;
	LearnedMIDI lm;
	lm.cable = &cable;
	lm.channelOrZone = 3;
	lm.noteOrCC = 60;

	CHECK_FALSE(lm.equalsNoteOrCC(&cable, 4, 60));
}

// ── equalsChannelAllowMPEMasterChannels() ───────────────────────────────

TEST(LearnedMIDI, EqualsMPEMaster_NoneReturnsEarly) {
	LearnedMIDI lm; // channelOrZone == MIDI_CHANNEL_NONE
	MIDICable cable;
	CHECK_FALSE(lm.equalsChannelAllowMPEMasterChannels(&cable, 5));
}

TEST(LearnedMIDI, EqualsMPEMaster_NormalChannelExactMatch) {
	MIDICable cable;
	LearnedMIDI lm;
	lm.cable = &cable;
	lm.channelOrZone = 7;

	CHECK_TRUE(lm.equalsChannelAllowMPEMasterChannels(&cable, 7));
	CHECK_FALSE(lm.equalsChannelAllowMPEMasterChannels(&cable, 8));
}

TEST(LearnedMIDI, EqualsMPEMaster_LowerZoneMasterOnly) {
	// Lower zone: channels 0-4, master=0
	MIDICable cable;
	cable.ports[MIDI_DIRECTION_INPUT_TO_DELUGE].mpeLowerZoneLastMemberChannel = 4;

	LearnedMIDI lm;
	lm.cable = &cable;
	lm.channelOrZone = MIDI_CHANNEL_MPE_LOWER_ZONE;

	// Channel 0 is the master => getMasterChannel() for lower zone = 0
	CHECK_TRUE(lm.equalsChannelAllowMPEMasterChannels(&cable, 0));
	// Channel 2 is a member, not the master => false
	CHECK_FALSE(lm.equalsChannelAllowMPEMasterChannels(&cable, 2));
	// Channel 10 is outside the zone
	CHECK_FALSE(lm.equalsChannelAllowMPEMasterChannels(&cable, 10));
}

TEST(LearnedMIDI, EqualsMPEMaster_UpperZoneMasterOnly) {
	// Upper zone: channels 12-15, master=15
	MIDICable cable;
	cable.ports[MIDI_DIRECTION_INPUT_TO_DELUGE].mpeUpperZoneLastMemberChannel = 12;

	LearnedMIDI lm;
	lm.cable = &cable;
	lm.channelOrZone = MIDI_CHANNEL_MPE_UPPER_ZONE;

	// Channel 15 is the master => getMasterChannel() for upper zone = 15
	CHECK_TRUE(lm.equalsChannelAllowMPEMasterChannels(&cable, 15));
	// Channel 13 is a member, not master => false
	CHECK_FALSE(lm.equalsChannelAllowMPEMasterChannels(&cable, 13));
	// Channel 5 is outside the zone
	CHECK_FALSE(lm.equalsChannelAllowMPEMasterChannels(&cable, 5));
}

TEST(LearnedMIDI, EqualsMPEMaster_DifferentCableReturnsFalse) {
	MIDICable cable1;
	MIDICable cable2;
	MIDIDeviceManager::differentiatingInputsByDevice = true;

	LearnedMIDI lm;
	lm.cable = &cable1;
	lm.channelOrZone = 5;

	CHECK_FALSE(lm.equalsChannelAllowMPEMasterChannels(&cable2, 5));

	MIDIDeviceManager::differentiatingInputsByDevice = false;
}

// ── equalsCable() device differentiation ────────────────────────────────

TEST(LearnedMIDI, EqualsCable_IgnoredWhenNotDifferentiating) {
	MIDICable cable1;
	MIDICable cable2;
	MIDIDeviceManager::differentiatingInputsByDevice = false;

	LearnedMIDI lm;
	lm.cable = &cable1;
	lm.channelOrZone = 3;

	// Different cable but not differentiating => still matches
	CHECK_TRUE(lm.equalsCable(&cable2));
}

TEST(LearnedMIDI, EqualsCable_NullCableAlwaysMatches) {
	MIDICable cable;
	MIDIDeviceManager::differentiatingInputsByDevice = true;

	LearnedMIDI lm;
	lm.cable = nullptr; // no cable assigned
	lm.channelOrZone = 3;

	CHECK_TRUE(lm.equalsCable(&cable));

	MIDIDeviceManager::differentiatingInputsByDevice = false;
}

// ── IS_A_CC range ───────────────────────────────────────────────────────

TEST(LearnedMIDI, EqualsMPEMaster_ISACCChannelExactMatch) {
	// channelOrZone > IS_A_CC should use exact match path
	MIDICable cable;
	LearnedMIDI lm;
	lm.cable = &cable;
	lm.channelOrZone = IS_A_CC + 3; // CC on channel 3

	CHECK_TRUE(lm.equalsChannelAllowMPEMasterChannels(&cable, IS_A_CC + 3));
	CHECK_FALSE(lm.equalsChannelAllowMPEMasterChannels(&cable, IS_A_CC + 4));
}

// ── MIDIPort channelToZone/isMasterChannel ──────────────────────────────

TEST(LearnedMIDI, PortChannelToZone_NoMPEReturnsChannel) {
	MIDIPort port; // default: lower=0(off), upper=15(off)
	CHECK_EQUAL(5, port.channelToZone(5));
	CHECK_EQUAL(0, port.channelToZone(0));
	CHECK_EQUAL(15, port.channelToZone(15));
}

TEST(LearnedMIDI, PortChannelToZone_LowerZone) {
	MIDIPort port;
	port.mpeLowerZoneLastMemberChannel = 5;

	CHECK_EQUAL(MIDI_CHANNEL_MPE_LOWER_ZONE, port.channelToZone(0));
	CHECK_EQUAL(MIDI_CHANNEL_MPE_LOWER_ZONE, port.channelToZone(3));
	CHECK_EQUAL(MIDI_CHANNEL_MPE_LOWER_ZONE, port.channelToZone(5));
	CHECK_EQUAL(6, port.channelToZone(6)); // outside zone
}

TEST(LearnedMIDI, PortChannelToZone_UpperZone) {
	MIDIPort port;
	port.mpeUpperZoneLastMemberChannel = 12;

	CHECK_EQUAL(MIDI_CHANNEL_MPE_UPPER_ZONE, port.channelToZone(12));
	CHECK_EQUAL(MIDI_CHANNEL_MPE_UPPER_ZONE, port.channelToZone(14));
	CHECK_EQUAL(MIDI_CHANNEL_MPE_UPPER_ZONE, port.channelToZone(15));
	CHECK_EQUAL(11, port.channelToZone(11)); // outside zone
}

TEST(LearnedMIDI, PortIsMasterChannel_LowerZone) {
	MIDIPort port;
	port.mpeLowerZoneLastMemberChannel = 4;

	CHECK_TRUE(port.isMasterChannel(0));
	CHECK_FALSE(port.isMasterChannel(1));
	CHECK_FALSE(port.isMasterChannel(15));
}

TEST(LearnedMIDI, PortIsMasterChannel_UpperZone) {
	MIDIPort port;
	port.mpeUpperZoneLastMemberChannel = 12;

	CHECK_TRUE(port.isMasterChannel(15));
	CHECK_FALSE(port.isMasterChannel(14));
	CHECK_FALSE(port.isMasterChannel(0));
}

TEST(LearnedMIDI, PortIsMasterChannel_NoZone) {
	MIDIPort port; // default: no zones active
	CHECK_FALSE(port.isMasterChannel(0));
	CHECK_FALSE(port.isMasterChannel(15));
}
