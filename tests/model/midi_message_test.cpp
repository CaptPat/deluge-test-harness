#include "CppUTest/TestHarness.h"
#include "model/midi/message.h"

TEST_GROUP(MidiMessageTest) {};

// --- bytesPerStatusMessage: 1-byte messages ---

TEST(MidiMessageTest, sysex1Byte) {
	CHECK_EQUAL(1u, bytesPerStatusMessage(0xF0));
}

TEST(MidiMessageTest, tuneRequest1Byte) {
	CHECK_EQUAL(1u, bytesPerStatusMessage(0xF6));
}

TEST(MidiMessageTest, endOfExclusive1Byte) {
	CHECK_EQUAL(1u, bytesPerStatusMessage(0xF7));
}

TEST(MidiMessageTest, timingClock1Byte) {
	CHECK_EQUAL(1u, bytesPerStatusMessage(0xF8));
}

TEST(MidiMessageTest, start1Byte) {
	CHECK_EQUAL(1u, bytesPerStatusMessage(0xFA));
}

TEST(MidiMessageTest, continue1Byte) {
	CHECK_EQUAL(1u, bytesPerStatusMessage(0xFB));
}

TEST(MidiMessageTest, stop1Byte) {
	CHECK_EQUAL(1u, bytesPerStatusMessage(0xFC));
}

TEST(MidiMessageTest, activeSensing1Byte) {
	CHECK_EQUAL(1u, bytesPerStatusMessage(0xFE));
}

TEST(MidiMessageTest, reset1Byte) {
	CHECK_EQUAL(1u, bytesPerStatusMessage(0xFF));
}

// --- bytesPerStatusMessage: 2-byte messages ---

TEST(MidiMessageTest, timecode2Bytes) {
	CHECK_EQUAL(2u, bytesPerStatusMessage(0xF1));
}

TEST(MidiMessageTest, songSelect2Bytes) {
	CHECK_EQUAL(2u, bytesPerStatusMessage(0xF3));
}

TEST(MidiMessageTest, programChange2Bytes) {
	CHECK_EQUAL(2u, bytesPerStatusMessage(0xC0));
	CHECK_EQUAL(2u, bytesPerStatusMessage(0xCF));
}

TEST(MidiMessageTest, channelAftertouch2Bytes) {
	CHECK_EQUAL(2u, bytesPerStatusMessage(0xD0));
	CHECK_EQUAL(2u, bytesPerStatusMessage(0xDF));
}

// --- bytesPerStatusMessage: 3-byte messages ---

TEST(MidiMessageTest, noteOff3Bytes) {
	CHECK_EQUAL(3u, bytesPerStatusMessage(0x80));
	CHECK_EQUAL(3u, bytesPerStatusMessage(0x8F));
}

TEST(MidiMessageTest, noteOn3Bytes) {
	CHECK_EQUAL(3u, bytesPerStatusMessage(0x90));
	CHECK_EQUAL(3u, bytesPerStatusMessage(0x9F));
}

TEST(MidiMessageTest, controlChange3Bytes) {
	CHECK_EQUAL(3u, bytesPerStatusMessage(0xB0));
	CHECK_EQUAL(3u, bytesPerStatusMessage(0xBF));
}

TEST(MidiMessageTest, pitchBend3Bytes) {
	CHECK_EQUAL(3u, bytesPerStatusMessage(0xE0));
	CHECK_EQUAL(3u, bytesPerStatusMessage(0xEF));
}

// --- MIDIMessage struct factories ---

TEST(MidiMessageTest, noteOnFactory) {
	MIDIMessage msg = MIDIMessage::noteOn(5, 60, 100);
	CHECK_EQUAL(0b1001, msg.statusType);
	CHECK_EQUAL(5, msg.channel);
	CHECK_EQUAL(60, msg.data1);
	CHECK_EQUAL(100, msg.data2);
}

TEST(MidiMessageTest, noteOffFactory) {
	MIDIMessage msg = MIDIMessage::noteOff(3, 72, 64);
	CHECK_EQUAL(0b1000, msg.statusType);
	CHECK_EQUAL(3, msg.channel);
	CHECK_EQUAL(72, msg.data1);
	CHECK_EQUAL(64, msg.data2);
}

TEST(MidiMessageTest, ccFactory) {
	MIDIMessage msg = MIDIMessage::cc(0, 7, 127);
	CHECK_EQUAL(0b1011, msg.statusType);
	CHECK_EQUAL(0, msg.channel);
	CHECK_EQUAL(7, msg.data1);
	CHECK_EQUAL(127, msg.data2);
}

TEST(MidiMessageTest, pitchBendFactory) {
	MIDIMessage msg = MIDIMessage::pitchBend(0, 0x2000);
	CHECK_EQUAL(0b1110, msg.statusType);
	CHECK_EQUAL(0, msg.channel);
	CHECK_EQUAL(0x00, msg.data1);
	CHECK_EQUAL(0x40, msg.data2);
}

TEST(MidiMessageTest, programChangeFactory) {
	MIDIMessage msg = MIDIMessage::programChange(2, 42);
	CHECK_EQUAL(0b1100, msg.statusType);
	CHECK_EQUAL(2, msg.channel);
	CHECK_EQUAL(42, msg.data1);
}

TEST(MidiMessageTest, isSystemMessage) {
	CHECK(MIDIMessage::realtimeClock().isSystemMessage());
	CHECK(MIDIMessage::realtimeStart().isSystemMessage());
	CHECK(MIDIMessage::realtimeContinue().isSystemMessage());
	CHECK(MIDIMessage::realtimeStop().isSystemMessage());
	CHECK(!MIDIMessage::noteOn(0, 60, 100).isSystemMessage());
}

TEST(MidiMessageTest, systemPositionPointer) {
	MIDIMessage msg = MIDIMessage::systemPositionPointer(128);
	CHECK(msg.isSystemMessage());
	CHECK_EQUAL(0x02, msg.channel);
	CHECK_EQUAL(0, msg.data1);
	CHECK_EQUAL(1, msg.data2);
}

TEST(MidiMessageTest, structSize) {
	CHECK_EQUAL(4u, sizeof(MIDIMessage));
}
