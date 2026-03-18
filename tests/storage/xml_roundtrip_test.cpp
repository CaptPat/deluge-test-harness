// XML Serializer/Deserializer round-trip tests.
//
// Uses StringSerializer (write to std::string) paired with a minimal
// StringDeserializer (read from std::string) to verify that XML output
// can be parsed back correctly without any FatFS dependency.
//
// The StringDeserializer implements the Deserializer interface by
// reimplementing the XML tokenizer state machine for the subset of
// operations needed by round-trip testing: tag navigation, attribute
// reading, and value extraction.

#include "CppUTest/TestHarness.h"
#include "storage/storage_manager.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// ── StringSerializer: captures XML output into std::string ──────────────
// (Same as in serializer_format_test.cpp — duplicated here to keep the
//  test file self-contained. Could be extracted to a shared header later.)

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

	void insertCommaIfNeeded() override {}

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
	//   IN_ATTRIBUTE_VALUE → IN_TAG_PAST_NAME → BETWEEN_TAGS → IN_TAG_NAME
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

		// If we're still inside a tag's attributes, skip to end of tag
		if (area_ == IN_TAG_PAST_NAME || area_ == IN_ATTRIBUTE_VALUE) {
			skipUntilChar('>');
			area_ = BETWEEN_TAGS;
		}

		// Now skip content until we find the matching closing tag
		int depth = 1;
		while (depth > 0 && !atEnd()) {
			skipUntilChar('<');
			if (atEnd()) {
				break;
			}
			if (peek() == '/') {
				advance(); // skip '/'
				// Read the closing tag name
				std::string closeName;
				while (!atEnd() && peek() != '>') {
					closeName += advance();
				}
				if (!atEnd()) {
					advance(); // skip '>'
				}
				depth--;
				if (depth == 0) {
					tagDepthFile_--;
				}
			}
			else if (peek() == '?') {
				skipUntilChar('>');
			}
			else {
				// Opening tag — increase depth
				skipUntilChar('>');
				depth++;
			}
		}
		area_ = BETWEEN_TAGS;
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

// ════════════════════════════════════════════════════════════════════════
// Tests
// ════════════════════════════════════════════════════════════════════════

// ── StringDeserializer unit tests ───────────────────────────────────────

TEST_GROUP(StringDeserializer) {
	StringDeserializer d;
};

TEST(StringDeserializer, ReadSimpleTag) {
	d.loadXML("<name>hello</name>");
	char const* tag = d.readNextTagOrAttributeName();
	STRCMP_EQUAL("name", tag);
	char const* val = d.readTagOrAttributeValue();
	STRCMP_EQUAL("hello", val);
}

TEST(StringDeserializer, ReadTagInt) {
	d.loadXML("<volume>42</volume>");
	char const* tag = d.readNextTagOrAttributeName();
	STRCMP_EQUAL("volume", tag);
	int32_t val = d.readTagOrAttributeValueInt();
	CHECK_EQUAL(42, val);
}

TEST(StringDeserializer, ReadTagNegativeInt) {
	d.loadXML("<pan>-7</pan>");
	d.readNextTagOrAttributeName();
	CHECK_EQUAL(-7, d.readTagOrAttributeValueInt());
}

TEST(StringDeserializer, ReadAttribute) {
	d.loadXML("<sound name=\"synth1\">\n</sound>");
	char const* tag = d.readNextTagOrAttributeName();
	STRCMP_EQUAL("sound", tag);

	char const* attr = d.readNextTagOrAttributeName();
	STRCMP_EQUAL("name", attr);
	char const* val = d.readTagOrAttributeValue();
	STRCMP_EQUAL("synth1", val);
}

TEST(StringDeserializer, ReadMultipleAttributes) {
	d.loadXML("<osc type=\"square\" transpose=\"12\">\n</osc>");

	STRCMP_EQUAL("osc", d.readNextTagOrAttributeName());
	STRCMP_EQUAL("type", d.readNextTagOrAttributeName());
	STRCMP_EQUAL("square", d.readTagOrAttributeValue());
	STRCMP_EQUAL("transpose", d.readNextTagOrAttributeName());
	CHECK_EQUAL(12, d.readTagOrAttributeValueInt());
}

TEST(StringDeserializer, ReadNestedTags) {
	d.loadXML("<outer>\n\t<inner>99</inner>\n</outer>");

	STRCMP_EQUAL("outer", d.readNextTagOrAttributeName());
	STRCMP_EQUAL("inner", d.readNextTagOrAttributeName());
	CHECK_EQUAL(99, d.readTagOrAttributeValueInt());
}

TEST(StringDeserializer, ExitTag) {
	d.loadXML("<root>\n\t<skip>\n\t\t<deep>1</deep>\n\t</skip>\n\t<keep>2</keep>\n</root>");

	STRCMP_EQUAL("root", d.readNextTagOrAttributeName());
	STRCMP_EQUAL("skip", d.readNextTagOrAttributeName());
	d.exitTag("skip");

	STRCMP_EQUAL("keep", d.readNextTagOrAttributeName());
	CHECK_EQUAL(2, d.readTagOrAttributeValueInt());
}

TEST(StringDeserializer, ReadHexValue) {
	d.loadXML("<color>0xFF00</color>");
	d.readNextTagOrAttributeName();
	CHECK_EQUAL(0xFF00, d.readTagOrAttributeValueHex(-1));
}

TEST(StringDeserializer, ReadHexAttribute) {
	d.loadXML("<data val=\"0xDEAD\">\n</data>");
	d.readNextTagOrAttributeName();
	d.readNextTagOrAttributeName();
	CHECK_EQUAL(0xDEAD, d.readTagOrAttributeValueHex(-1));
}

TEST(StringDeserializer, ReadHexBytes) {
	d.loadXML("<data bytes=\"DEADBEEF\">\n</data>");
	d.readNextTagOrAttributeName();
	d.readNextTagOrAttributeName();
	uint8_t bytes[4] = {};
	int count = d.readTagOrAttributeValueHexBytes(bytes, 4);
	CHECK_EQUAL(4, count);
	CHECK_EQUAL(0xDE, bytes[0]);
	CHECK_EQUAL(0xAD, bytes[1]);
	CHECK_EQUAL(0xBE, bytes[2]);
	CHECK_EQUAL(0xEF, bytes[3]);
}

TEST(StringDeserializer, EmptyInputReturnsEmpty) {
	d.loadXML("");
	STRCMP_EQUAL("", d.readNextTagOrAttributeName());
}

TEST(StringDeserializer, SkipsXmlDeclaration) {
	d.loadXML("<?xml version=\"1.0\"?>\n<root>hello</root>");
	STRCMP_EQUAL("root", d.readNextTagOrAttributeName());
	STRCMP_EQUAL("hello", d.readTagOrAttributeValue());
}

// ── Round-trip tests: Serialize then Deserialize ────────────────────────

TEST_GROUP(XmlRoundTrip) {
	StringSerializer ser;
	StringDeserializer deser;

	void setup() {
		ser.reset();
	}

	// Helper: feed serializer output into deserializer
	void feedToDeserializer() { deser.loadXML(ser.output); }
};

TEST(XmlRoundTrip, SimpleTag) {
	ser.writeTag("name", "hello");
	feedToDeserializer();

	STRCMP_EQUAL("name", deser.readNextTagOrAttributeName());
	STRCMP_EQUAL("hello", deser.readTagOrAttributeValue());
}

TEST(XmlRoundTrip, IntTag) {
	ser.writeTag("volume", 42);
	feedToDeserializer();

	STRCMP_EQUAL("volume", deser.readNextTagOrAttributeName());
	CHECK_EQUAL(42, deser.readTagOrAttributeValueInt());
}

TEST(XmlRoundTrip, NegativeIntTag) {
	ser.writeTag("pan", -128);
	feedToDeserializer();

	deser.readNextTagOrAttributeName();
	CHECK_EQUAL(-128, deser.readTagOrAttributeValueInt());
}

TEST(XmlRoundTrip, AttributeString) {
	ser.writeOpeningTagBeginning("sound");
	ser.writeAttribute("name", "pad1", false);
	ser.writeOpeningTagEnd();
	ser.writeClosingTag("sound");
	feedToDeserializer();

	STRCMP_EQUAL("sound", deser.readNextTagOrAttributeName());
	STRCMP_EQUAL("name", deser.readNextTagOrAttributeName());
	STRCMP_EQUAL("pad1", deser.readTagOrAttributeValue());
}

TEST(XmlRoundTrip, AttributeInt) {
	ser.writeOpeningTagBeginning("osc");
	ser.writeAttribute("transpose", 7, false);
	ser.writeOpeningTagEnd();
	ser.writeClosingTag("osc");
	feedToDeserializer();

	deser.readNextTagOrAttributeName();
	deser.readNextTagOrAttributeName();
	CHECK_EQUAL(7, deser.readTagOrAttributeValueInt());
}

TEST(XmlRoundTrip, MultipleAttributes) {
	ser.writeOpeningTagBeginning("osc");
	ser.writeAttribute("type", "square", false);
	ser.writeAttribute("transpose", 12, false);
	ser.writeAttribute("cents", -5, false);
	ser.writeOpeningTagEnd();
	ser.writeClosingTag("osc");
	feedToDeserializer();

	STRCMP_EQUAL("osc", deser.readNextTagOrAttributeName());

	STRCMP_EQUAL("type", deser.readNextTagOrAttributeName());
	STRCMP_EQUAL("square", deser.readTagOrAttributeValue());

	STRCMP_EQUAL("transpose", deser.readNextTagOrAttributeName());
	CHECK_EQUAL(12, deser.readTagOrAttributeValueInt());

	STRCMP_EQUAL("cents", deser.readNextTagOrAttributeName());
	CHECK_EQUAL(-5, deser.readTagOrAttributeValueInt());
}

TEST(XmlRoundTrip, HexAttribute) {
	ser.writeOpeningTagBeginning("data");
	ser.writeAttributeHex("color", 0x00FF, 4, false);
	ser.writeOpeningTagEnd();
	ser.writeClosingTag("data");
	feedToDeserializer();

	deser.readNextTagOrAttributeName();
	deser.readNextTagOrAttributeName();
	CHECK_EQUAL(0x00FF, deser.readTagOrAttributeValueHex(-1));
}

TEST(XmlRoundTrip, HexBytesAttribute) {
	uint8_t original[] = {0xDE, 0xAD, 0xBE, 0xEF};

	ser.writeOpeningTagBeginning("data");
	ser.writeAttributeHexBytes("bytes", original, 4, false);
	ser.writeOpeningTagEnd();
	ser.writeClosingTag("data");
	feedToDeserializer();

	deser.readNextTagOrAttributeName();
	deser.readNextTagOrAttributeName();
	uint8_t decoded[4] = {};
	int count = deser.readTagOrAttributeValueHexBytes(decoded, 4);
	CHECK_EQUAL(4, count);
	MEMCMP_EQUAL(original, decoded, 4);
}

TEST(XmlRoundTrip, NestedTags) {
	ser.writeOpeningTag("song");
	ser.writeOpeningTag("instruments");
	ser.writeTag("name", "bass");
	ser.writeClosingTag("instruments");
	ser.writeClosingTag("song");
	feedToDeserializer();

	STRCMP_EQUAL("song", deser.readNextTagOrAttributeName());
	STRCMP_EQUAL("instruments", deser.readNextTagOrAttributeName());
	STRCMP_EQUAL("name", deser.readNextTagOrAttributeName());
	STRCMP_EQUAL("bass", deser.readTagOrAttributeValue());
}

TEST(XmlRoundTrip, NestedTagsWithAttributes) {
	ser.writeOpeningTag("song");
	ser.writeOpeningTagBeginning("sound");
	ser.writeAttribute("name", "synth1", false);
	ser.writeAttribute("polyphonic", "poly", false);
	ser.writeOpeningTagEnd();
	ser.writeTag("volume", 80);
	ser.writeTag("pan", -10);
	ser.writeClosingTag("sound");
	ser.writeClosingTag("song");
	feedToDeserializer();

	STRCMP_EQUAL("song", deser.readNextTagOrAttributeName());
	STRCMP_EQUAL("sound", deser.readNextTagOrAttributeName());

	STRCMP_EQUAL("name", deser.readNextTagOrAttributeName());
	STRCMP_EQUAL("synth1", deser.readTagOrAttributeValue());

	STRCMP_EQUAL("polyphonic", deser.readNextTagOrAttributeName());
	STRCMP_EQUAL("poly", deser.readTagOrAttributeValue());

	// After attributes exhausted, next call should get child tags
	STRCMP_EQUAL("volume", deser.readNextTagOrAttributeName());
	CHECK_EQUAL(80, deser.readTagOrAttributeValueInt());

	STRCMP_EQUAL("pan", deser.readNextTagOrAttributeName());
	CHECK_EQUAL(-10, deser.readTagOrAttributeValueInt());
}

TEST(XmlRoundTrip, ExitTagSkipsChildren) {
	ser.writeOpeningTag("song");
	ser.writeOpeningTag("skip");
	ser.writeTag("a", 1);
	ser.writeTag("b", 2);
	ser.writeClosingTag("skip");
	ser.writeTag("keep", 99);
	ser.writeClosingTag("song");
	feedToDeserializer();

	STRCMP_EQUAL("song", deser.readNextTagOrAttributeName());
	STRCMP_EQUAL("skip", deser.readNextTagOrAttributeName());
	deser.exitTag("skip");

	STRCMP_EQUAL("keep", deser.readNextTagOrAttributeName());
	CHECK_EQUAL(99, deser.readTagOrAttributeValueInt());
}

TEST(XmlRoundTrip, SelfClosingTag) {
	ser.writeOpeningTagBeginning("sample");
	ser.writeAttribute("path", "/SAMPLES/kick.wav", false);
	ser.closeTag();
	feedToDeserializer();

	STRCMP_EQUAL("sample", deser.readNextTagOrAttributeName());
	STRCMP_EQUAL("path", deser.readNextTagOrAttributeName());
	STRCMP_EQUAL("/SAMPLES/kick.wav", deser.readTagOrAttributeValue());
}

TEST(XmlRoundTrip, MultipleTopLevelSiblings) {
	// Wrap in a root tag as XML requires
	ser.writeOpeningTag("root");
	ser.writeTag("a", 1);
	ser.writeTag("b", 2);
	ser.writeTag("c", 3);
	ser.writeClosingTag("root");
	feedToDeserializer();

	STRCMP_EQUAL("root", deser.readNextTagOrAttributeName());

	STRCMP_EQUAL("a", deser.readNextTagOrAttributeName());
	CHECK_EQUAL(1, deser.readTagOrAttributeValueInt());

	STRCMP_EQUAL("b", deser.readNextTagOrAttributeName());
	CHECK_EQUAL(2, deser.readTagOrAttributeValueInt());

	STRCMP_EQUAL("c", deser.readNextTagOrAttributeName());
	CHECK_EQUAL(3, deser.readTagOrAttributeValueInt());
}

// ── Realistic Deluge preset fragment ────────────────────────────────────

TEST(XmlRoundTrip, DelugePresetFragment) {
	// Serialize a fragment resembling a real Deluge preset
	ser.writeOpeningTag("sound");
	ser.writeOpeningTagBeginning("osc1");
	ser.writeAttribute("type", "analogSquare", false);
	ser.writeAttribute("transpose", 0, false);
	ser.writeAttribute("cents", 0, false);
	ser.writeOpeningTagEnd();
	ser.writeTag("volume", "0x7FFFFFFF");
	ser.writeClosingTag("osc1");

	ser.writeOpeningTagBeginning("osc2");
	ser.writeAttribute("type", "square", false);
	ser.writeAttribute("transpose", -12, false);
	ser.writeOpeningTagEnd();
	ser.writeTag("volume", "0x50000000");
	ser.writeClosingTag("osc2");

	ser.writeOpeningTagBeginning("lfo1");
	ser.writeAttribute("type", "triangle", false);
	ser.writeAttribute("syncLevel", 0, false);
	ser.writeOpeningTagEnd();
	ser.writeTag("rate", "0x30000000");
	ser.writeClosingTag("lfo1");

	ser.writeClosingTag("sound");
	feedToDeserializer();

	// Parse it back
	STRCMP_EQUAL("sound", deser.readNextTagOrAttributeName());

	// osc1
	STRCMP_EQUAL("osc1", deser.readNextTagOrAttributeName());
	STRCMP_EQUAL("type", deser.readNextTagOrAttributeName());
	STRCMP_EQUAL("analogSquare", deser.readTagOrAttributeValue());
	STRCMP_EQUAL("transpose", deser.readNextTagOrAttributeName());
	CHECK_EQUAL(0, deser.readTagOrAttributeValueInt());
	STRCMP_EQUAL("cents", deser.readNextTagOrAttributeName());
	CHECK_EQUAL(0, deser.readTagOrAttributeValueInt());
	// Skip remaining osc1 children (volume tag)
	deser.exitTag("osc1");

	// osc2
	STRCMP_EQUAL("osc2", deser.readNextTagOrAttributeName());
	STRCMP_EQUAL("type", deser.readNextTagOrAttributeName());
	STRCMP_EQUAL("square", deser.readTagOrAttributeValue());
	STRCMP_EQUAL("transpose", deser.readNextTagOrAttributeName());
	CHECK_EQUAL(-12, deser.readTagOrAttributeValueInt());
	deser.exitTag("osc2");

	// lfo1
	STRCMP_EQUAL("lfo1", deser.readNextTagOrAttributeName());
	STRCMP_EQUAL("type", deser.readNextTagOrAttributeName());
	STRCMP_EQUAL("triangle", deser.readTagOrAttributeValue());
	STRCMP_EQUAL("syncLevel", deser.readNextTagOrAttributeName());
	CHECK_EQUAL(0, deser.readTagOrAttributeValueInt());
	STRCMP_EQUAL("rate", deser.readNextTagOrAttributeName());
	STRCMP_EQUAL("0x30000000", deser.readTagOrAttributeValue());
}

TEST(XmlRoundTrip, AttributesOnNewLines) {
	// Test that newline-formatted attributes (the default) also round-trip
	ser.writeOpeningTagBeginning("osc1");
	ser.writeAttribute("type", "saw"); // onNewLine=true (default)
	ser.writeAttribute("transpose", 5);
	ser.writeOpeningTagEnd();
	ser.writeClosingTag("osc1");
	feedToDeserializer();

	STRCMP_EQUAL("osc1", deser.readNextTagOrAttributeName());
	STRCMP_EQUAL("type", deser.readNextTagOrAttributeName());
	STRCMP_EQUAL("saw", deser.readTagOrAttributeValue());
	STRCMP_EQUAL("transpose", deser.readNextTagOrAttributeName());
	CHECK_EQUAL(5, deser.readTagOrAttributeValueInt());
}

TEST(XmlRoundTrip, ArrayWithElements) {
	ser.writeArrayStart("noteRows");
	ser.writeOpeningTagBeginning("noteRow");
	ser.writeAttribute("y", 60, false);
	ser.writeOpeningTagEnd();
	ser.writeClosingTag("noteRow");
	ser.writeOpeningTagBeginning("noteRow");
	ser.writeAttribute("y", 64, false);
	ser.writeOpeningTagEnd();
	ser.writeClosingTag("noteRow");
	ser.writeArrayEnding("noteRows");
	feedToDeserializer();

	STRCMP_EQUAL("noteRows", deser.readNextTagOrAttributeName());

	STRCMP_EQUAL("noteRow", deser.readNextTagOrAttributeName());
	STRCMP_EQUAL("y", deser.readNextTagOrAttributeName());
	CHECK_EQUAL(60, deser.readTagOrAttributeValueInt());
	deser.exitTag("noteRow");

	STRCMP_EQUAL("noteRow", deser.readNextTagOrAttributeName());
	STRCMP_EQUAL("y", deser.readNextTagOrAttributeName());
	CHECK_EQUAL(64, deser.readTagOrAttributeValueInt());
}

TEST(XmlRoundTrip, CharAtATimeReading) {
	ser.writeTag("path", "/SAMPLES/kick.wav");
	feedToDeserializer();

	deser.readNextTagOrAttributeName();
	CHECK(deser.prepareToReadTagOrAttributeValueOneCharAtATime());

	std::string result;
	char c;
	while ((c = deser.readNextCharOfTagOrAttributeValue()) != 0) {
		result += c;
	}
	STRCMP_EQUAL("/SAMPLES/kick.wav", result.c_str());
}
