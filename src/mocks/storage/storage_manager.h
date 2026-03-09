// Shadow header replacing firmware's storage/storage_manager.h
// Provides concrete mock Serializer/Deserializer classes with no-op methods
// so downstream files (arpeggiator.cpp, drum.cpp, etc.) can compile and link.

#pragma once

#include "definitions_cxx.hpp"
#include "model/sync.h"
#include "util/firmware_version.h"
#include <cstdint>

class String;
class Song;

extern FirmwareVersion song_firmware_version;

class StorageManager {};

// ── Serializer (concrete mock) ──────────────────────────────────────────

class Serializer {
public:
	virtual ~Serializer() = default;

	virtual void writeAttribute(char const* name, int32_t number, bool onNewLine = true) { (void)name; (void)number; (void)onNewLine; }
	virtual void writeAttribute(char const* name, char const* value, bool onNewLine = true) { (void)name; (void)value; (void)onNewLine; }
	virtual void writeAttributeHex(char const* name, int32_t number, int32_t numChars, bool onNewLine = true) { (void)name; (void)number; (void)numChars; (void)onNewLine; }
	virtual void writeAttributeHexBytes(char const* name, uint8_t* data, int32_t numBytes, bool onNewLine = true) { (void)name; (void)data; (void)numBytes; (void)onNewLine; }
	virtual void writeTagNameAndSeperator(char const* tag) { (void)tag; }
	virtual void writeTag(char const* tag, int32_t number, bool box = false) { (void)tag; (void)number; (void)box; }
	virtual void writeTag(char const* tag, char const* contents, bool box = false, bool quote = true) { (void)tag; (void)contents; (void)box; (void)quote; }
	virtual void writeOpeningTag(char const* tag, bool startNewLineAfter = true, bool box = false) { (void)tag; (void)startNewLineAfter; (void)box; }
	virtual void writeOpeningTagBeginning(char const* tag, bool box = false, bool newLineBefore = true) { (void)tag; (void)box; (void)newLineBefore; }
	virtual void writeOpeningTagEnd(bool startNewLineAfter = true) { (void)startNewLineAfter; }
	virtual void closeTag(bool box = false) { (void)box; }
	virtual void writeClosingTag(char const* tag, bool shouldPrintIndents = true, bool box = false) { (void)tag; (void)shouldPrintIndents; (void)box; }
	virtual void writeArrayStart(char const* tag, bool startNewLineAfter = true, bool box = false) { (void)tag; (void)startNewLineAfter; (void)box; }
	virtual void writeArrayEnding(char const* tag, bool shouldPrintIndents = true, bool box = false) { (void)tag; (void)shouldPrintIndents; (void)box; }
	virtual void printIndents() {}
	virtual void insertCommaIfNeeded() {}
	virtual void write(char const* output) { (void)output; }
	virtual Error closeFileAfterWriting(char const* path = nullptr, char const* beginningString = nullptr,
	                                    char const* endString = nullptr) { (void)path; (void)beginningString; (void)endString; return Error::NONE; }
	virtual void reset() {}

	void writeFirmwareVersion() {}

	void writeEarliestCompatibleFirmwareVersion(char const* versionString) {
		writeAttribute("earliestCompatibleFirmware", versionString);
	}

	void writeSyncTypeToFile(Song* song, char const* name, SyncType value, bool onNewLine) {
		(void)song;
		writeAttribute(name, (int32_t)value, onNewLine);
	}

	void writeAbsoluteSyncLevelToFile(Song* song, char const* name, SyncLevel internalValue, bool onNewLine) {
		(void)song;
		writeAttribute(name, (int32_t)internalValue, onNewLine);
	}
};

// ── Deserializer (concrete mock) ────────────────────────────────────────

class Deserializer {
public:
	virtual ~Deserializer() = default;

	virtual bool prepareToReadTagOrAttributeValueOneCharAtATime() { return false; }
	virtual char readNextCharOfTagOrAttributeValue() { return 0; }
	virtual int32_t getNumCharsRemainingInValueBeforeEndOfCluster() { return 0; }

	virtual char const* readNextTagOrAttributeName() { return ""; }
	virtual char const* readTagOrAttributeValue() { return ""; }
	virtual int32_t readTagOrAttributeValueInt() { return 0; }
	virtual int32_t readTagOrAttributeValueHex(int32_t errorValue) { return errorValue; }
	virtual int readTagOrAttributeValueHexBytes(uint8_t* bytes, int32_t maxLen) { (void)bytes; (void)maxLen; return 0; }
	virtual Error tryReadingFirmwareTagFromFile(char const* tagName, bool ignoreIncorrectFirmware = false) { (void)tagName; (void)ignoreIncorrectFirmware; return Error::NONE; }

	virtual char const* readNextCharsOfTagOrAttributeValue(int32_t numChars) { (void)numChars; return ""; }
	virtual Error readTagOrAttributeValueString(String* string) { (void)string; return Error::NONE; }
	virtual bool match(char const ch) { (void)ch; return false; }
	virtual void exitTag(char const* exitTagName = nullptr, bool closeObject = false) { (void)exitTagName; (void)closeObject; }

	virtual void reset() {}
};
