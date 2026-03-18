// Shared StringSerializer + StringDeserializer for test harness use.
//
// StringSerializer: Reimplements XMLSerializer's formatting logic but writes
// to a std::string instead of FatFS, enabling isolated testing.
//
// StringDeserializer: Minimal XML tokenizer implementing the Deserializer
// interface. Supports tag names, attribute names/values, tag content values,
// exitTag(), match(), nested tag navigation. Does NOT support firmware version
// parsing, hex byte reading via clusters, or full error recovery.

#pragma once

#include "storage/storage_manager.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

// ── StringSerializer: captures XML formatting output into std::string ───

class StringSerializer : public Serializer {
public:
	std::string output;
	uint8_t indentAmount = 0;

	void reset() override {
		output.clear();
		indentAmount = 0;
	}

	void write(char const* s) override { output += s; }

	void printIndents() override {
		for (uint8_t i = 0; i < indentAmount; i++) {
			write("\t");
		}
	}

	void insertCommaIfNeeded() override {
		// XML does not use commas
	}

	void writeTag(char const* tag, int32_t number, bool box = false) override {
		char buffer[12];
		std::snprintf(buffer, sizeof(buffer), "%d", number);
		writeTag(tag, buffer, box);
	}

	void writeTag(char const* tag, char const* contents, bool box = false, bool quote = true) override {
		printIndents();
		write("<");
		write(tag);
		write(">");
		write(contents);
		write("</");
		write(tag);
		write(">\n");
	}

	void writeAttribute(char const* name, int32_t number, bool onNewLine = true) override {
		char buffer[12];
		std::snprintf(buffer, sizeof(buffer), "%d", number);
		writeAttribute(name, buffer, onNewLine);
	}

	void writeAttributeHex(char const* name, int32_t number, int32_t numChars, bool onNewLine = true) override {
		char buffer[11];
		buffer[0] = '0';
		buffer[1] = 'x';
		intToHex(static_cast<uint32_t>(number), &buffer[2], numChars);
		writeAttribute(name, buffer, onNewLine);
	}

	void writeAttributeHexBytes(char const* name, uint8_t* data, int32_t numBytes, bool onNewLine = true) override {
		if (onNewLine) {
			write("\n");
			printIndents();
		}
		else {
			write(" ");
		}
		write(name);
		write("=\"");

		char buffer[3];
		for (int i = 0; i < numBytes; i++) {
			intToHex(data[i], &buffer[0], 2);
			write(buffer);
		}
		write("\"");
	}

	void writeAttribute(char const* name, char const* value, bool onNewLine = true) override {
		if (onNewLine) {
			write("\n");
			printIndents();
		}
		else {
			write(" ");
		}
		write(name);
		write("=\"");
		write(value);
		write("\"");
	}

	void writeTagNameAndSeperator(char const* tag) override {
		write(tag);
		write("=");
	}

	void writeOpeningTag(char const* tag, bool startNewLineAfter = true, bool box = false) override {
		writeOpeningTagBeginning(tag);
		writeOpeningTagEnd(startNewLineAfter);
	}

	void writeOpeningTagBeginning(char const* tag, bool box = false, bool newLineBefore = true) override {
		printIndents();
		write("<");
		write(tag);
		indentAmount++;
	}

	void writeOpeningTagEnd(bool startNewLineAfter = true) override {
		if (startNewLineAfter) {
			write(">\n");
		}
		else {
			write(">");
		}
	}

	void closeTag(bool box = false) override {
		write(" /");
		writeOpeningTagEnd();
		indentAmount--;
	}

	void writeClosingTag(char const* tag, bool shouldPrintIndents = true, bool box = false) override {
		indentAmount--;
		if (shouldPrintIndents) {
			printIndents();
		}
		write("</");
		write(tag);
		write(">\n");
	}

	void writeArrayStart(char const* tag, bool startNewLineAfter = true, bool box = false) override {
		writeOpeningTag(tag, startNewLineAfter);
	}

	void writeArrayEnding(char const* tag, bool shouldPrintIndents = true, bool box = false) override {
		writeClosingTag(tag, shouldPrintIndents);
	}

	Error closeFileAfterWriting(char const* path, char const* beginningString, char const* endString) override {
		return Error::NONE;
	}
};

// ── StringDeserializer: parses XML from a std::string ───────────────────
//
// Minimal XML tokenizer implementing the Deserializer interface.
// Supports: tag names, attribute names/values, tag content values,
// exitTag(), match(), nested tag navigation.
//
// Does NOT support: firmware version parsing, hex byte reading,
// cluster-based buffering, or the full error recovery of the firmware.

class StringDeserializer : public Deserializer {
public:
	void loadXML(std::string const& xml) {
		data_ = xml;
		pos_ = 0;
		tagDepthFile_ = 0;
		tagDepthCaller_ = 0;
		area_ = BETWEEN_TAGS;
	}

	void reset() override {
		data_.clear();
		pos_ = 0;
		tagDepthFile_ = 0;
		tagDepthCaller_ = 0;
		area_ = BETWEEN_TAGS;
	}

	// ── readNextTagOrAttributeName ──────────────────────────────────────
	// Mirrors the firmware's fall-through state machine:
	//   IN_ATTRIBUTE_VALUE -> IN_TAG_PAST_NAME -> BETWEEN_TAGS -> IN_TAG_NAME
	char const* readNextTagOrAttributeName() override {
		int32_t tagDepthStart = tagDepthFile_;

		// If we were left in an attribute value (e.g. char-at-a-time read
		// that didn't consume the whole value), skip to closing quote
		if (area_ == IN_ATTRIBUTE_VALUE) {
			skipUntilChar('"');
			area_ = IN_TAG_PAST_NAME;
		}

		// Try reading an attribute name if we're inside a tag
		if (area_ == IN_TAG_PAST_NAME) {
			char const* attr = readNextAttributeName();
			if (*attr || tagDepthFile_ != tagDepthStart) {
				if (*attr) {
					tagDepthCaller_++;
				}
				return attr;
			}
			// Fall through: no more attributes, area_ is now BETWEEN_TAGS
		}

		// BETWEEN_TAGS: find the next '<'
		if (area_ == BETWEEN_TAGS) {
			skipWhitespace();
			while (!atEnd()) {
				if (peek() != '<') {
					advance();
					continue;
				}
				advance(); // skip '<'

				if (atEnd()) {
					return "";
				}

				// Check for closing tag
				if (peek() == '/') {
					advance();
					skipUntilChar('>');
					tagDepthFile_--;
					if (tagDepthFile_ < tagDepthStart) {
						return "";
					}
					continue;
				}

				// Check for XML declaration
				if (peek() == '?') {
					skipUntilChar('>');
					continue;
				}

				// Found an opening tag
				area_ = IN_TAG_NAME;
				break;
			}
			if (atEnd()) {
				return "";
			}
		}

		// IN_TAG_NAME: read the tag name
		if (area_ == IN_TAG_NAME) {
			char const* name = readTagName();
			if (*name) {
				tagDepthCaller_++;
			}
			return name;
		}

		return "";
	}

	// ── readTagOrAttributeValue ─────────────────────────────────────────
	char const* readTagOrAttributeValue() override {
		if (area_ == IN_ATTRIBUTE_VALUE) {
			return readAttributeValue();
		}
		if (area_ == BETWEEN_TAGS) {
			return readTagContent();
		}
		return "";
	}

	int32_t readTagOrAttributeValueInt() override {
		char const* val = readTagOrAttributeValue();
		if (val[0] == '\0') {
			return 0;
		}
		return std::atoi(val);
	}

	int32_t readTagOrAttributeValueHex(int32_t errorValue) override {
		char const* val = readTagOrAttributeValue();
		if (val[0] == '\0') {
			return errorValue;
		}
		// Skip optional "0x" prefix
		if (val[0] == '0' && (val[1] == 'x' || val[1] == 'X')) {
			val += 2;
		}
		return static_cast<int32_t>(std::strtoul(val, nullptr, 16));
	}

	int readTagOrAttributeValueHexBytes(uint8_t* bytes, int32_t maxLen) override {
		char const* val = readTagOrAttributeValue();
		int count = 0;
		while (*val && count < maxLen) {
			char hi = *val++;
			if (!*val) {
				break;
			}
			char lo = *val++;
			auto hexDigit = [](char c) -> uint8_t {
				if (c >= '0' && c <= '9') {
					return c - '0';
				}
				if (c >= 'A' && c <= 'F') {
					return 10 + c - 'A';
				}
				if (c >= 'a' && c <= 'f') {
					return 10 + c - 'a';
				}
				return 0;
			};
			bytes[count++] = (hexDigit(hi) << 4) | hexDigit(lo);
		}
		return count;
	}

	bool prepareToReadTagOrAttributeValueOneCharAtATime() override {
		// For string-based reading, always ready
		return true;
	}

	char readNextCharOfTagOrAttributeValue() override {
		if (area_ == IN_ATTRIBUTE_VALUE) {
			if (!atEnd() && peek() != '"') {
				return advance();
			}
			return 0;
		}
		if (area_ == BETWEEN_TAGS) {
			if (!atEnd() && peek() != '<') {
				return advance();
			}
			return 0;
		}
		return 0;
	}

	int32_t getNumCharsRemainingInValueBeforeEndOfCluster() override {
		// No cluster concept — return remaining chars in value
		size_t remaining = 0;
		size_t p = pos_;
		if (area_ == IN_ATTRIBUTE_VALUE) {
			while (p < data_.size() && data_[p] != '"') {
				remaining++;
				p++;
			}
		}
		else if (area_ == BETWEEN_TAGS) {
			while (p < data_.size() && data_[p] != '<') {
				remaining++;
				p++;
			}
		}
		return static_cast<int32_t>(remaining);
	}

	char const* readNextCharsOfTagOrAttributeValue(int32_t numChars) override {
		int32_t count = 0;
		while (count < numChars && count < (int32_t)sizeof(valueBuf_) - 1) {
			char c = readNextCharOfTagOrAttributeValue();
			if (c == 0) {
				break;
			}
			valueBuf_[count++] = c;
		}
		valueBuf_[count] = '\0';
		return valueBuf_;
	}

	Error readTagOrAttributeValueString(String* string) override {
		// Not implemented for now — would need String::set()
		(void)string;
		return Error::NONE;
	}

	bool match(char const ch) override {
		if (!atEnd() && peek() == ch) {
			advance();
			return true;
		}
		return false;
	}

	void exitTag(char const* exitTagName = nullptr, bool closeObject = false) override {
		(void)closeObject;

		if (exitTagName != nullptr) {
			// Named exit: skip remaining attributes, child content, and
			// closing tag. Used by test code that reads some attributes
			// then wants to exit the whole tag scope.
			if (area_ == IN_ATTRIBUTE_VALUE) {
				skipUntilChar('"');
				area_ = IN_TAG_PAST_NAME;
			}
			if (area_ == IN_TAG_PAST_NAME) {
				// Consume remaining attributes until '>' or '/>'
				while (!atEnd() && area_ == IN_TAG_PAST_NAME) {
					readNextAttributeName();
					if (area_ == IN_ATTRIBUTE_VALUE) {
						skipUntilChar('"');
						area_ = IN_TAG_PAST_NAME;
					}
				}
			}
			// area_ is now BETWEEN_TAGS (or we hit a self-closing />).
			// If self-closing already decremented tagDepthFile_, we may
			// already be at the right depth. Otherwise skip child
			// content to find the matching closing tag.
			int32_t targetDepth = tagDepthFile_ - 1;
			while (tagDepthFile_ > targetDepth && !atEnd()) {
				while (!atEnd() && peek() != '<') {
					advance();
				}
				if (atEnd()) {
					break;
				}
				advance(); // skip '<'
				if (atEnd()) {
					break;
				}
				if (peek() == '/') {
					advance();
					skipUntilChar('>');
					tagDepthFile_--;
				}
				else if (peek() == '?') {
					skipUntilChar('>');
				}
				else {
					while (!atEnd()) {
						char c = peek();
						if (c == '>') {
							advance();
							tagDepthFile_++;
							break;
						}
						if (c == '/') {
							advance();
							skipUntilChar('>');
							break;
						}
						advance();
					}
				}
			}
			area_ = BETWEEN_TAGS;
			tagDepthCaller_ = tagDepthFile_;
		}
		else {
			// Unnamed exit (firmware readFromFile pattern):
			// When tagDepthFile_ < tagDepthCaller_ (after reading an
			// attribute), this is a no-op — just re-sync caller depth.
			// When tagDepthFile_ >= tagDepthCaller_ (after reading a
			// child tag), skip content until file depth < caller depth.
			while (tagDepthFile_ >= tagDepthCaller_ && !atEnd()) {
				if (area_ == IN_ATTRIBUTE_VALUE) {
					skipUntilChar('"');
					area_ = IN_TAG_PAST_NAME;
				}
				if (area_ == IN_TAG_PAST_NAME) {
					readNextAttributeName();
					if (area_ == IN_ATTRIBUTE_VALUE) {
						skipUntilChar('"');
						area_ = IN_TAG_PAST_NAME;
					}
					continue;
				}
				if (area_ == BETWEEN_TAGS) {
					if (tagDepthFile_ < tagDepthCaller_) {
						break;
					}
					while (!atEnd() && peek() != '<') {
						advance();
					}
					if (atEnd()) {
						break;
					}
					advance();
					if (atEnd()) {
						break;
					}
					if (peek() == '/') {
						advance();
						skipUntilChar('>');
						tagDepthFile_--;
					}
					else if (peek() == '?') {
						skipUntilChar('>');
					}
					else {
						while (!atEnd()) {
							char c = peek();
							if (c == '>') {
								advance();
								tagDepthFile_++;
								break;
							}
							if (c == '/') {
								advance();
								skipUntilChar('>');
								break;
							}
							advance();
						}
					}
				}
			}
			tagDepthCaller_ = tagDepthFile_;
		}
	}

	Error tryReadingFirmwareTagFromFile(char const* tagName, bool ignoreIncorrectFirmware = false) override {
		(void)tagName;
		(void)ignoreIncorrectFirmware;
		return Error::RESULT_TAG_UNUSED;
	}

private:
	enum {
		BETWEEN_TAGS = 0,
		IN_TAG_NAME = 1,
		IN_TAG_PAST_NAME = 2,
		IN_ATTRIBUTE_NAME = 3,
		PAST_ATTRIBUTE_NAME = 4,
		PAST_EQUALS_SIGN = 5,
		IN_ATTRIBUTE_VALUE = 6,
	};

	std::string data_;
	size_t pos_ = 0;
	int32_t tagDepthFile_ = 0;
	int32_t tagDepthCaller_ = 0;
	int area_ = BETWEEN_TAGS;
	char nameBuf_[256] = {};
	char valueBuf_[256] = {};

	bool atEnd() const { return pos_ >= data_.size(); }
	char peek() const { return data_[pos_]; }
	char advance() { return data_[pos_++]; }

	void skipWhitespace() {
		while (!atEnd() && (peek() == ' ' || peek() == '\t' || peek() == '\r' || peek() == '\n')) {
			advance();
		}
	}

	void skipUntilChar(char c) {
		while (!atEnd() && peek() != c) {
			advance();
		}
		if (!atEnd()) {
			advance(); // skip the char itself
		}
	}

	char const* readTagName() {
		int i = 0;
		while (!atEnd() && i < (int)sizeof(nameBuf_) - 1) {
			char c = peek();
			if (c == '>' || c == ' ' || c == '\t' || c == '\r' || c == '\n') {
				break;
			}
			if (c == '/') {
				// Self-closing tag like <tag/>
				advance();
				skipUntilChar('>');
				area_ = BETWEEN_TAGS;
				nameBuf_[i] = '\0';
				// Don't increment tagDepthFile_ since it immediately closes
				return nameBuf_;
			}
			nameBuf_[i++] = advance();
		}
		nameBuf_[i] = '\0';

		if (i > 0) {
			tagDepthFile_++;
		}

		// Determine what follows
		if (!atEnd()) {
			char c = peek();
			if (c == '>') {
				advance();
				area_ = BETWEEN_TAGS;
			}
			else {
				area_ = IN_TAG_PAST_NAME;
			}
		}

		return nameBuf_;
	}

	char const* readNextAttributeName() {
		// Skip whitespace
		skipWhitespace();

		if (atEnd()) {
			return "";
		}

		char c = peek();
		if (c == '>') {
			advance();
			area_ = BETWEEN_TAGS;
			return "";
		}
		if (c == '/') {
			advance();
			skipUntilChar('>');
			tagDepthFile_--;
			area_ = BETWEEN_TAGS;
			return "";
		}

		// Read the attribute name
		int i = 0;
		while (!atEnd() && i < (int)sizeof(nameBuf_) - 1) {
			c = peek();
			if (c == '=' || c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '>') {
				break;
			}
			nameBuf_[i++] = advance();
		}
		nameBuf_[i] = '\0';

		if (i == 0) {
			return "";
		}

		// Skip to '='
		skipWhitespace();
		if (!atEnd() && peek() == '=') {
			advance();
		}

		// Skip to opening quote
		skipWhitespace();
		if (!atEnd() && peek() == '"') {
			advance();
			area_ = IN_ATTRIBUTE_VALUE;
		}

		return nameBuf_;
	}

	char const* readAttributeValue() {
		int i = 0;
		while (!atEnd() && peek() != '"' && i < (int)sizeof(valueBuf_) - 1) {
			valueBuf_[i++] = advance();
		}
		valueBuf_[i] = '\0';

		// Skip closing quote
		if (!atEnd() && peek() == '"') {
			advance();
		}

		area_ = IN_TAG_PAST_NAME;
		return valueBuf_;
	}

	char const* readTagContent() {
		int i = 0;
		while (!atEnd() && peek() != '<' && i < (int)sizeof(valueBuf_) - 1) {
			valueBuf_[i++] = advance();
		}
		valueBuf_[i] = '\0';

		// Now we should be at '<' — check for closing tag
		if (!atEnd() && peek() == '<') {
			advance();
			if (!atEnd() && peek() == '/') {
				advance();
				// Skip closing tag name and '>'
				skipUntilChar('>');
				tagDepthFile_--;
				area_ = BETWEEN_TAGS;
			}
		}

		return valueBuf_;
	}
};
