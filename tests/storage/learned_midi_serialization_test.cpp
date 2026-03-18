// LearnedMIDI XML serialization round-trip tests.
//
// Tests that LearnedMIDI data survives write→read cycles through the
// StringSerializer/StringDeserializer pair, exercising writeToFile(),
// writeAttributesToFile(), readFromFile(), and readMPEZone().

#include "CppUTest/TestHarness.h"
#include "io/midi/learned_midi.h"
#include "string_serializer.h"

#include <cstring>
#include <string>

// ═══════════════════════════════════════════════════════════════════════
// Test Group
// ═══════════════════════════════════════════════════════════════════════

TEST_GROUP(LearnedMIDISerialization) {
	StringSerializer ser;
	StringDeserializer deser;
	LearnedMIDI lm;

	void setup() {
		ser.reset();
		lm.clear();
	}

	// Helper: serialize lm via writeToFile, then load into deser for reading
	void roundTrip(char const* tagName, int32_t midiMessageType) {
		lm.writeToFile(ser, tagName, midiMessageType);
		deser.loadXML(ser.output);
	}

	// Helper: read back into a fresh LearnedMIDI from whatever deser contains
	LearnedMIDI readBack(int32_t midiMessageType) {
		LearnedMIDI result;
		// Navigate into the opening tag — readFromFile expects to already
		// be "inside" the tag (it reads attributes + child tags)
		char const* tag = deser.readNextTagOrAttributeName();
		(void)tag; // consume the opening tag name
		result.readFromFile(deser, midiMessageType);
		return result;
	}
};

// ═══════════════════════════════════════════════════════════════════════
// CC Mapping Round-Trips
// ═══════════════════════════════════════════════════════════════════════

TEST(LearnedMIDISerialization, CCMappingChannel5CC64) {
	lm.channelOrZone = 5;
	lm.noteOrCC = 64;

	roundTrip("midiCC", MIDI_MESSAGE_CC);
	LearnedMIDI out = readBack(MIDI_MESSAGE_CC);

	CHECK_EQUAL(5, out.channelOrZone);
	CHECK_EQUAL(64, out.noteOrCC);
	POINTERS_EQUAL(nullptr, out.cable); // no cable written (stub returns nullptr)
}

TEST(LearnedMIDISerialization, CCMappingChannel0CC0) {
	lm.channelOrZone = 0;
	lm.noteOrCC = 0;

	roundTrip("midiCC", MIDI_MESSAGE_CC);
	LearnedMIDI out = readBack(MIDI_MESSAGE_CC);

	CHECK_EQUAL(0, out.channelOrZone);
	CHECK_EQUAL(0, out.noteOrCC);
}

TEST(LearnedMIDISerialization, CCMappingChannel15CC127) {
	lm.channelOrZone = 15;
	lm.noteOrCC = 127;

	roundTrip("midiCC", MIDI_MESSAGE_CC);
	LearnedMIDI out = readBack(MIDI_MESSAGE_CC);

	CHECK_EQUAL(15, out.channelOrZone);
	CHECK_EQUAL(127, out.noteOrCC);
}

// ═══════════════════════════════════════════════════════════════════════
// Note Mapping Round-Trips
// ═══════════════════════════════════════════════════════════════════════

TEST(LearnedMIDISerialization, NoteMappingChannel3Note60) {
	lm.channelOrZone = 3;
	lm.noteOrCC = 60;

	roundTrip("midiNote", MIDI_MESSAGE_NOTE);
	LearnedMIDI out = readBack(MIDI_MESSAGE_NOTE);

	CHECK_EQUAL(3, out.channelOrZone);
	CHECK_EQUAL(60, out.noteOrCC);
}

TEST(LearnedMIDISerialization, NoteMappingChannel0Note0) {
	lm.channelOrZone = 0;
	lm.noteOrCC = 0;

	roundTrip("midiNote", MIDI_MESSAGE_NOTE);
	LearnedMIDI out = readBack(MIDI_MESSAGE_NOTE);

	CHECK_EQUAL(0, out.channelOrZone);
	CHECK_EQUAL(0, out.noteOrCC);
}

TEST(LearnedMIDISerialization, NoteMappingChannel15Note127) {
	lm.channelOrZone = 15;
	lm.noteOrCC = 127;

	roundTrip("midiNote", MIDI_MESSAGE_NOTE);
	LearnedMIDI out = readBack(MIDI_MESSAGE_NOTE);

	CHECK_EQUAL(15, out.channelOrZone);
	CHECK_EQUAL(127, out.noteOrCC);
}

// ═══════════════════════════════════════════════════════════════════════
// MPE Zone Round-Trips
// ═══════════════════════════════════════════════════════════════════════

TEST(LearnedMIDISerialization, MPELowerZoneRoundTrip) {
	lm.channelOrZone = MIDI_CHANNEL_MPE_LOWER_ZONE;
	lm.noteOrCC = 42;

	roundTrip("midiCC", MIDI_MESSAGE_CC);
	LearnedMIDI out = readBack(MIDI_MESSAGE_CC);

	CHECK_EQUAL(MIDI_CHANNEL_MPE_LOWER_ZONE, out.channelOrZone);
	CHECK_EQUAL(42, out.noteOrCC);
}

TEST(LearnedMIDISerialization, MPEUpperZoneRoundTrip) {
	lm.channelOrZone = MIDI_CHANNEL_MPE_UPPER_ZONE;
	lm.noteOrCC = 99;

	roundTrip("midiCC", MIDI_MESSAGE_CC);
	LearnedMIDI out = readBack(MIDI_MESSAGE_CC);

	CHECK_EQUAL(MIDI_CHANNEL_MPE_UPPER_ZONE, out.channelOrZone);
	CHECK_EQUAL(99, out.noteOrCC);
}

TEST(LearnedMIDISerialization, MPELowerZoneNoteRoundTrip) {
	lm.channelOrZone = MIDI_CHANNEL_MPE_LOWER_ZONE;
	lm.noteOrCC = 60;

	roundTrip("midiNote", MIDI_MESSAGE_NOTE);
	LearnedMIDI out = readBack(MIDI_MESSAGE_NOTE);

	CHECK_EQUAL(MIDI_CHANNEL_MPE_LOWER_ZONE, out.channelOrZone);
	CHECK_EQUAL(60, out.noteOrCC);
}

TEST(LearnedMIDISerialization, MPEUpperZoneNoteRoundTrip) {
	lm.channelOrZone = MIDI_CHANNEL_MPE_UPPER_ZONE;
	lm.noteOrCC = 0;

	roundTrip("midiNote", MIDI_MESSAGE_NOTE);
	LearnedMIDI out = readBack(MIDI_MESSAGE_NOTE);

	CHECK_EQUAL(MIDI_CHANNEL_MPE_UPPER_ZONE, out.channelOrZone);
	CHECK_EQUAL(0, out.noteOrCC);
}

// ═══════════════════════════════════════════════════════════════════════
// Channel-Only (MIDI_MESSAGE_NONE) Round-Trips
// ═══════════════════════════════════════════════════════════════════════

TEST(LearnedMIDISerialization, ChannelOnlyNoNoteOrCC) {
	lm.channelOrZone = 10;
	lm.noteOrCC = 77; // should NOT be written (messageType == NONE)

	roundTrip("midiChannel", MIDI_MESSAGE_NONE);
	LearnedMIDI out = readBack(MIDI_MESSAGE_NONE);

	CHECK_EQUAL(10, out.channelOrZone);
	// noteOrCC should remain at its default (255) since nothing was written
	CHECK_EQUAL(255, out.noteOrCC);
}

TEST(LearnedMIDISerialization, MPELowerZoneChannelOnly) {
	lm.channelOrZone = MIDI_CHANNEL_MPE_LOWER_ZONE;

	roundTrip("midiChannel", MIDI_MESSAGE_NONE);
	LearnedMIDI out = readBack(MIDI_MESSAGE_NONE);

	CHECK_EQUAL(MIDI_CHANNEL_MPE_LOWER_ZONE, out.channelOrZone);
}

// ═══════════════════════════════════════════════════════════════════════
// No Cable → Self-Closing Tag
// ═══════════════════════════════════════════════════════════════════════

TEST(LearnedMIDISerialization, NoCableProducesSelfClosingTag) {
	lm.channelOrZone = 7;
	lm.noteOrCC = 32;
	lm.cable = nullptr;

	roundTrip("midiCC", MIDI_MESSAGE_CC);

	// The output should be a self-closing tag (contains " />" from closeTag())
	CHECK(ser.output.find(" />") != std::string::npos);

	// It should NOT contain a <device> child tag
	CHECK(ser.output.find("<device") == std::string::npos);

	// Round-trip should still work
	LearnedMIDI out = readBack(MIDI_MESSAGE_CC);
	CHECK_EQUAL(7, out.channelOrZone);
	CHECK_EQUAL(32, out.noteOrCC);
	POINTERS_EQUAL(nullptr, out.cable);
}

// ═══════════════════════════════════════════════════════════════════════
// Nothing to Write (containsSomething() == false)
// ═══════════════════════════════════════════════════════════════════════

TEST(LearnedMIDISerialization, NothingWrittenWhenEmpty) {
	// lm is cleared in setup: channelOrZone == MIDI_CHANNEL_NONE
	CHECK_FALSE(lm.containsSomething());

	roundTrip("midiCC", MIDI_MESSAGE_CC);

	// writeToFile should produce empty output
	CHECK(ser.output.empty());
}

TEST(LearnedMIDISerialization, NothingWrittenForDefaultConstructed) {
	LearnedMIDI fresh;
	CHECK_FALSE(fresh.containsSomething());

	fresh.writeToFile(ser, "anything", MIDI_MESSAGE_NOTE);
	CHECK(ser.output.empty());
}

// ═══════════════════════════════════════════════════════════════════════
// Edge Cases
// ═══════════════════════════════════════════════════════════════════════

TEST(LearnedMIDISerialization, NoteOrCCClampedTo127OnRead) {
	// Manually craft XML with noteOrCC > 127 to test the clamping in readFromFile
	std::string xml = "<midiCC channel=\"5\" ccNumber=\"200\">\n</midiCC>";
	deser.loadXML(xml);

	LearnedMIDI out = readBack(MIDI_MESSAGE_CC);
	CHECK_EQUAL(5, out.channelOrZone);
	CHECK_EQUAL(127, out.noteOrCC); // clamped from 200 to 127
}

TEST(LearnedMIDISerialization, NoteClampedTo127OnRead) {
	std::string xml = "<midiNote channel=\"0\" note=\"255\">\n</midiNote>";
	deser.loadXML(xml);

	LearnedMIDI out = readBack(MIDI_MESSAGE_NOTE);
	CHECK_EQUAL(0, out.channelOrZone);
	CHECK_EQUAL(127, out.noteOrCC); // clamped from 255 to 127
}

TEST(LearnedMIDISerialization, UnknownAttributeIgnored) {
	// readFromFile should silently skip unknown attributes via exitTag()
	std::string xml = "<midiCC channel=\"9\" unknownAttr=\"foo\" ccNumber=\"44\">\n</midiCC>";
	deser.loadXML(xml);

	LearnedMIDI out = readBack(MIDI_MESSAGE_CC);
	CHECK_EQUAL(9, out.channelOrZone);
	CHECK_EQUAL(44, out.noteOrCC);
}

TEST(LearnedMIDISerialization, DifferentTagNamesRoundTrip) {
	// writeToFile uses the commandName for the tag — verify various names work
	lm.channelOrZone = 2;
	lm.noteOrCC = 10;

	// Tag name "sustainPedal"
	lm.writeToFile(ser, "sustainPedal", MIDI_MESSAGE_CC);
	deser.loadXML(ser.output);
	deser.readNextTagOrAttributeName(); // consume "sustainPedal"
	LearnedMIDI out;
	out.readFromFile(deser, MIDI_MESSAGE_CC);
	CHECK_EQUAL(2, out.channelOrZone);
	CHECK_EQUAL(10, out.noteOrCC);
}

// ═══════════════════════════════════════════════════════════════════════
// writeAttributesToFile Direct Tests
// ═══════════════════════════════════════════════════════════════════════

TEST(LearnedMIDISerialization, WriteAttributesChannelAndCC) {
	lm.channelOrZone = 3;
	lm.noteOrCC = 64;

	// Wrap in a tag so we can parse it
	ser.writeOpeningTagBeginning("test");
	lm.writeAttributesToFile(ser, MIDI_MESSAGE_CC);
	ser.closeTag();

	deser.loadXML(ser.output);
	deser.readNextTagOrAttributeName(); // "test"

	// Read channel attribute
	char const* attr = deser.readNextTagOrAttributeName();
	STRCMP_EQUAL("channel", attr);
	CHECK_EQUAL(3, deser.readTagOrAttributeValueInt());

	// Read ccNumber attribute
	attr = deser.readNextTagOrAttributeName();
	STRCMP_EQUAL("ccNumber", attr);
	CHECK_EQUAL(64, deser.readTagOrAttributeValueInt());
}

TEST(LearnedMIDISerialization, WriteAttributesMPEZoneLower) {
	lm.channelOrZone = MIDI_CHANNEL_MPE_LOWER_ZONE;
	lm.noteOrCC = 10;

	ser.writeOpeningTagBeginning("test");
	lm.writeAttributesToFile(ser, MIDI_MESSAGE_CC);
	ser.closeTag();

	deser.loadXML(ser.output);
	deser.readNextTagOrAttributeName(); // "test"

	char const* attr = deser.readNextTagOrAttributeName();
	STRCMP_EQUAL("mpeZone", attr);
	STRCMP_EQUAL("lower", deser.readTagOrAttributeValue());
}

TEST(LearnedMIDISerialization, WriteAttributesMPEZoneUpper) {
	lm.channelOrZone = MIDI_CHANNEL_MPE_UPPER_ZONE;
	lm.noteOrCC = 10;

	ser.writeOpeningTagBeginning("test");
	lm.writeAttributesToFile(ser, MIDI_MESSAGE_CC);
	ser.closeTag();

	deser.loadXML(ser.output);
	deser.readNextTagOrAttributeName(); // "test"

	char const* attr = deser.readNextTagOrAttributeName();
	STRCMP_EQUAL("mpeZone", attr);
	STRCMP_EQUAL("upper", deser.readTagOrAttributeValue());
}

TEST(LearnedMIDISerialization, WriteAttributesNoteMessage) {
	lm.channelOrZone = 8;
	lm.noteOrCC = 60;

	ser.writeOpeningTagBeginning("test");
	lm.writeAttributesToFile(ser, MIDI_MESSAGE_NOTE);
	ser.closeTag();

	deser.loadXML(ser.output);
	deser.readNextTagOrAttributeName(); // "test"

	char const* attr = deser.readNextTagOrAttributeName();
	STRCMP_EQUAL("channel", attr);
	CHECK_EQUAL(8, deser.readTagOrAttributeValueInt());

	attr = deser.readNextTagOrAttributeName();
	STRCMP_EQUAL("note", attr);
	CHECK_EQUAL(60, deser.readTagOrAttributeValueInt());
}

TEST(LearnedMIDISerialization, WriteAttributesNoMessageType) {
	lm.channelOrZone = 12;
	lm.noteOrCC = 99; // should not be written

	ser.writeOpeningTagBeginning("test");
	lm.writeAttributesToFile(ser, MIDI_MESSAGE_NONE);
	ser.closeTag();

	// Output should contain channel but NOT ccNumber or note
	CHECK(ser.output.find("channel") != std::string::npos);
	CHECK(ser.output.find("ccNumber") == std::string::npos);
	CHECK(ser.output.find("note") == std::string::npos);
}

// ═══════════════════════════════════════════════════════════════════════
// Convenience Wrappers
// ═══════════════════════════════════════════════════════════════════════

TEST(LearnedMIDISerialization, WriteCCToFileWrapper) {
	lm.channelOrZone = 1;
	lm.noteOrCC = 74;

	lm.writeCCToFile(ser, "midiCC");
	deser.loadXML(ser.output);

	LearnedMIDI out;
	deser.readNextTagOrAttributeName();
	out.readCCFromFile(deser);

	CHECK_EQUAL(1, out.channelOrZone);
	CHECK_EQUAL(74, out.noteOrCC);
}

TEST(LearnedMIDISerialization, WriteNoteToFileWrapper) {
	lm.channelOrZone = 9;
	lm.noteOrCC = 36;

	lm.writeNoteToFile(ser, "midiNote");
	deser.loadXML(ser.output);

	LearnedMIDI out;
	deser.readNextTagOrAttributeName();
	out.readNoteFromFile(deser);

	CHECK_EQUAL(9, out.channelOrZone);
	CHECK_EQUAL(36, out.noteOrCC);
}

TEST(LearnedMIDISerialization, WriteChannelToFileWrapper) {
	lm.channelOrZone = 14;

	lm.writeChannelToFile(ser, "midiInput");
	deser.loadXML(ser.output);

	LearnedMIDI out;
	deser.readNextTagOrAttributeName();
	out.readChannelFromFile(deser);

	CHECK_EQUAL(14, out.channelOrZone);
	// noteOrCC not written/read when using channel-only wrappers
	CHECK_EQUAL(255, out.noteOrCC);
}

// ═══════════════════════════════════════════════════════════════════════
// readMPEZone Direct Tests
// ═══════════════════════════════════════════════════════════════════════

TEST(LearnedMIDISerialization, ReadMPEZoneLowerDirect) {
	std::string xml = "<test mpeZone=\"lower\">\n</test>";
	deser.loadXML(xml);

	deser.readNextTagOrAttributeName(); // "test"
	char const* attr = deser.readNextTagOrAttributeName(); // "mpeZone"
	STRCMP_EQUAL("mpeZone", attr);

	LearnedMIDI out;
	out.readMPEZone(deser);
	CHECK_EQUAL(MIDI_CHANNEL_MPE_LOWER_ZONE, out.channelOrZone);
}

TEST(LearnedMIDISerialization, ReadMPEZoneUpperDirect) {
	std::string xml = "<test mpeZone=\"upper\">\n</test>";
	deser.loadXML(xml);

	deser.readNextTagOrAttributeName(); // "test"
	char const* attr = deser.readNextTagOrAttributeName(); // "mpeZone"
	STRCMP_EQUAL("mpeZone", attr);

	LearnedMIDI out;
	out.readMPEZone(deser);
	CHECK_EQUAL(MIDI_CHANNEL_MPE_UPPER_ZONE, out.channelOrZone);
}

TEST(LearnedMIDISerialization, ReadMPEZoneUnknownValueNoChange) {
	std::string xml = "<test mpeZone=\"middle\">\n</test>";
	deser.loadXML(xml);

	deser.readNextTagOrAttributeName();
	deser.readNextTagOrAttributeName();

	LearnedMIDI out;
	out.channelOrZone = 42; // set to something
	out.readMPEZone(deser);
	// "middle" is neither "lower" nor "upper", so channelOrZone should be unchanged
	CHECK_EQUAL(42, out.channelOrZone);
}

// ═══════════════════════════════════════════════════════════════════════
// All Channel Values (boundary sweep)
// ═══════════════════════════════════════════════════════════════════════

TEST(LearnedMIDISerialization, AllNormalChannelsRoundTrip) {
	for (int ch = 0; ch <= 15; ch++) {
		ser.reset();
		lm.clear();
		lm.channelOrZone = static_cast<uint8_t>(ch);
		lm.noteOrCC = 64;

		roundTrip("midiCC", MIDI_MESSAGE_CC);
		LearnedMIDI out = readBack(MIDI_MESSAGE_CC);

		CHECK_EQUAL(ch, out.channelOrZone);
		CHECK_EQUAL(64, out.noteOrCC);
	}
}
