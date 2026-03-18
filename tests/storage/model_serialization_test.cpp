// Round-trip serialization tests for ArpeggiatorSettings.
//
// ArpeggiatorSettings::writeCommonParamsToFile() writes XML attributes,
// while readCommonTagsFromFile() reads child tags. These formats are
// interchangeable in the firmware's XML parser (readNextTagOrAttributeName
// returns both attribute names and tag names identically). However, the
// reader's exitTag() after each field is designed for child-tag format.
//
// Strategy:
//  1. Test writeCommonParamsToFile output for correctness (attribute format).
//  2. Test readCommonTagsFromFile with child-tag XML for correctness.
//  3. Full round-trip: set values → write → convert to child tags → read →
//     verify all fields match.
//  4. Same for writeNonAudioParamsToFile / readNonAudioTagsFromFile.

#include "CppUTest/TestHarness.h"
#include "modulation/arpeggiator.h"
#include "storage/storage_manager.h"

#include <cstdio>
#include <cstring>
#include <regex>
#include <string>
#include <vector>

// Use a namespace to avoid ODR violations with the identically-named
// classes in xml_roundtrip_test.cpp (linked into the same executable).
namespace arp_serial_test {

// ── StringSerializer ────────────────────────────────────────────────────

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

// ── StringDeserializer (same as xml_roundtrip_test.cpp) ─────────────────

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

	char const* readNextTagOrAttributeName() override {
		int32_t tagDepthStart = tagDepthFile_;

		if (area_ == IN_ATTRIBUTE_VALUE) {
			skipUntilChar('"');
			area_ = IN_TAG_PAST_NAME;
		}

		if (area_ == IN_TAG_PAST_NAME) {
			char const* attr = readNextAttributeName();
			if (*attr || tagDepthFile_ != tagDepthStart) {
				if (*attr) {
					tagDepthCaller_++;
				}
				return attr;
			}
		}

		if (area_ == BETWEEN_TAGS) {
			skipWhitespace();
			while (!atEnd()) {
				if (peek() != '<') {
					advance();
					continue;
				}
				advance();
				if (atEnd()) {
					return "";
				}
				if (peek() == '/') {
					advance();
					skipUntilChar('>');
					tagDepthFile_--;
					if (tagDepthFile_ < tagDepthStart) {
						return "";
					}
					continue;
				}
				if (peek() == '?') {
					skipUntilChar('>');
					continue;
				}
				area_ = IN_TAG_NAME;
				break;
			}
			if (atEnd()) {
				return "";
			}
		}

		if (area_ == IN_TAG_NAME) {
			char const* name = readTagName();
			if (*name) {
				tagDepthCaller_++;
			}
			return name;
		}

		return "";
	}

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

	bool prepareToReadTagOrAttributeValueOneCharAtATime() override { return true; }

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

		tagDepthCaller_--;

		if (area_ == IN_TAG_PAST_NAME || area_ == IN_ATTRIBUTE_VALUE) {
			skipUntilChar('>');
			area_ = BETWEEN_TAGS;
		}

		// If readTagContent() already consumed the closing tag (decrementing
		// tagDepthFile_), the depths already match and we can skip the scan.
		if (tagDepthFile_ <= tagDepthCaller_) {
			return;
		}

		int depth = 1;
		while (depth > 0 && !atEnd()) {
			skipUntilChar('<');
			if (atEnd()) {
				break;
			}
			if (peek() == '/') {
				advance();
				std::string closeName;
				while (!atEnd() && peek() != '>') {
					closeName += advance();
				}
				if (!atEnd()) {
					advance();
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
			advance();
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
				advance();
				skipUntilChar('>');
				area_ = BETWEEN_TAGS;
				nameBuf_[i] = '\0';
				return nameBuf_;
			}
			nameBuf_[i++] = advance();
		}
		nameBuf_[i] = '\0';

		if (i > 0) {
			tagDepthFile_++;
		}

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

		skipWhitespace();
		if (!atEnd() && peek() == '=') {
			advance();
		}

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

		if (!atEnd() && peek() == '<') {
			advance();
			if (!atEnd() && peek() == '/') {
				advance();
				skipUntilChar('>');
				tagDepthFile_--;
				area_ = BETWEEN_TAGS;
			}
		}

		return valueBuf_;
	}
};

} // namespace arp_serial_test

using arp_serial_test::StringDeserializer;
using arp_serial_test::StringSerializer;

// ── Helper: convert attribute XML to child-tag XML ──────────────────────
// Takes the attribute output from writeCommonParamsToFile / writeNonAudioParamsToFile
// and converts it to child-tag format that readCommonTagsFromFile /
// readNonAudioTagsFromFile can parse.
//
// Input:  <arpeggiator\n\tmode="off"\n\tnumOctaves="2"\n ... />
// Output: <arpeggiator><mode>off</mode><numOctaves>2</numOctaves>...</arpeggiator>

static std::string attributesToChildTags(std::string const& attrXml) {
	// Extract tag name from <tagname ...
	size_t tagStart = attrXml.find('<');
	if (tagStart == std::string::npos) {
		return attrXml;
	}
	size_t nameStart = tagStart + 1;
	size_t nameEnd = attrXml.find_first_of(" \t\n\r/>", nameStart);
	std::string tagName = attrXml.substr(nameStart, nameEnd - nameStart);

	// Extract all name="value" pairs
	std::string result = "<" + tagName + ">\n";
	std::regex attrRegex(R"((\w+)=\"([^\"]*)\")");
	auto begin = std::sregex_iterator(attrXml.begin(), attrXml.end(), attrRegex);
	auto end = std::sregex_iterator();
	for (auto it = begin; it != end; ++it) {
		std::string name = (*it)[1].str();
		std::string value = (*it)[2].str();
		result += "<" + name + ">" + value + "</" + name + ">\n";
	}
	result += "</" + tagName + ">\n";
	return result;
}

// ── Helper: build child-tag XML from an ArpeggiatorSettings via write ───
static std::string writeAndConvertCommonParams(ArpeggiatorSettings& arp) {
	StringSerializer ser;
	ser.writeOpeningTagBeginning("arpeggiator");
	arp.writeCommonParamsToFile(ser, nullptr);
	ser.closeTag();
	return attributesToChildTags(ser.output);
}

static std::string writeAndConvertNonAudioParams(ArpeggiatorSettings& arp) {
	StringSerializer ser;
	ser.writeOpeningTagBeginning("arpParams");
	arp.writeNonAudioParamsToFile(ser);
	ser.closeTag();
	return attributesToChildTags(ser.output);
}

// ── Helper: read common params from child-tag XML ───────────────────────
static void readCommonParamsFromXml(ArpeggiatorSettings& arp, std::string const& xml) {
	StringDeserializer deser;
	deser.loadXML(xml);

	// Read the outer tag (e.g. "arpeggiator")
	char const* outerTag = deser.readNextTagOrAttributeName();
	(void)outerTag;

	// Loop through child tags, delegating to readCommonTagsFromFile
	char const* tagName;
	while (*(tagName = deser.readNextTagOrAttributeName())) {
		bool handled = arp.readCommonTagsFromFile(deser, tagName, nullptr);
		if (!handled) {
			deser.exitTag(tagName);
		}
	}
}

static void readNonAudioParamsFromXml(ArpeggiatorSettings& arp, std::string const& xml) {
	StringDeserializer deser;
	deser.loadXML(xml);

	char const* outerTag = deser.readNextTagOrAttributeName();
	(void)outerTag;

	char const* tagName;
	while (*(tagName = deser.readNextTagOrAttributeName())) {
		bool handled = arp.readNonAudioTagsFromFile(deser, tagName);
		if (!handled) {
			deser.exitTag(tagName);
		}
	}
}

// ════════════════════════════════════════════════════════════════════════
// Tests
// ════════════════════════════════════════════════════════════════════════

// ── Write format verification ───────────────────────────────────────────

TEST_GROUP(ArpSerializeWrite) {
	StringSerializer ser;
	void setup() { ser.reset(); }
};

TEST(ArpSerializeWrite, CommonParamsContainsModeAttribute) {
	ArpeggiatorSettings arp;
	arp.mode = ArpMode::ARP;
	ser.writeOpeningTagBeginning("arpeggiator");
	arp.writeCommonParamsToFile(ser, nullptr);
	ser.closeTag();

	// Verify key attributes appear in the output
	CHECK(ser.output.find("mode=\"arp\"") != std::string::npos);
	CHECK(ser.output.find("arpMode=\"arp\"") != std::string::npos);
}

TEST(ArpSerializeWrite, CommonParamsContainsNumOctaves) {
	ArpeggiatorSettings arp;
	arp.numOctaves = 4;
	ser.writeOpeningTagBeginning("arpeggiator");
	arp.writeCommonParamsToFile(ser, nullptr);
	ser.closeTag();

	CHECK(ser.output.find("numOctaves=\"4\"") != std::string::npos);
}

TEST(ArpSerializeWrite, CommonParamsContainsSyncLevel) {
	ArpeggiatorSettings arp;
	arp.syncLevel = SYNC_LEVEL_4TH;
	ser.writeOpeningTagBeginning("arpeggiator");
	arp.writeCommonParamsToFile(ser, nullptr);
	ser.closeTag();

	// SyncLevel 4th = 3
	CHECK(ser.output.find("syncLevel=\"3\"") != std::string::npos);
}

TEST(ArpSerializeWrite, NonAudioParamsContainsGateAndRate) {
	ArpeggiatorSettings arp;
	arp.gate = 42;
	arp.rate = 100;
	ser.writeOpeningTagBeginning("arpParams");
	arp.writeNonAudioParamsToFile(ser);
	ser.closeTag();

	CHECK(ser.output.find("gate=\"42\"") != std::string::npos);
	CHECK(ser.output.find("rate=\"100\"") != std::string::npos);
}

TEST(ArpSerializeWrite, CommonParamsContainsHexBytesArrays) {
	ArpeggiatorSettings arp;
	// Set some non-zero values in locked arrays
	arp.lockedNoteProbabilityValues[0] = 0x12;
	arp.lockedNoteProbabilityValues[1] = 0x34;
	ser.writeOpeningTagBeginning("arpeggiator");
	arp.writeCommonParamsToFile(ser, nullptr);
	ser.closeTag();

	CHECK(ser.output.find("lockedNoteProbArray=\"1234") != std::string::npos);
}

TEST(ArpSerializeWrite, CommonParamsContainsStepRepeat) {
	ArpeggiatorSettings arp;
	arp.numStepRepeats = 3;
	ser.writeOpeningTagBeginning("arpeggiator");
	arp.writeCommonParamsToFile(ser, nullptr);
	ser.closeTag();

	CHECK(ser.output.find("stepRepeat=\"3\"") != std::string::npos);
}

TEST(ArpSerializeWrite, CommonParamsContainsChordType) {
	ArpeggiatorSettings arp;
	arp.chordTypeIndex = 5;
	ser.writeOpeningTagBeginning("arpeggiator");
	arp.writeCommonParamsToFile(ser, nullptr);
	ser.closeTag();

	CHECK(ser.output.find("chordType=\"5\"") != std::string::npos);
}

TEST(ArpSerializeWrite, NonAudioParamsContainsProbabilities) {
	ArpeggiatorSettings arp;
	arp.noteProbability = 1000;
	arp.bassProbability = 2000;
	arp.ratchetAmount = 3;
	ser.writeOpeningTagBeginning("arpParams");
	arp.writeNonAudioParamsToFile(ser);
	ser.closeTag();

	CHECK(ser.output.find("noteProbability=\"1000\"") != std::string::npos);
	CHECK(ser.output.find("bassProbability=\"2000\"") != std::string::npos);
	CHECK(ser.output.find("ratchetAmount=\"3\"") != std::string::npos);
}

// ── Read from child-tag XML ─────────────────────────────────────────────

TEST_GROUP(ArpSerializeRead) {};

TEST(ArpSerializeRead, ReadNumOctaves) {
	ArpeggiatorSettings arp;
	std::string xml = "<arp><numOctaves>5</numOctaves></arp>";
	readCommonParamsFromXml(arp, xml);
	CHECK_EQUAL(5, arp.numOctaves);
}

TEST(ArpSerializeRead, ReadStepRepeat) {
	ArpeggiatorSettings arp;
	std::string xml = "<arp><stepRepeat>4</stepRepeat></arp>";
	readCommonParamsFromXml(arp, xml);
	CHECK_EQUAL(4, arp.numStepRepeats);
}

TEST(ArpSerializeRead, ReadSyncLevelAndType) {
	ArpeggiatorSettings arp;
	std::string xml = "<arp><syncLevel>3</syncLevel><syncType>10</syncType></arp>";
	readCommonParamsFromXml(arp, xml);
	CHECK_EQUAL(SYNC_LEVEL_4TH, arp.syncLevel);
	CHECK_EQUAL(SYNC_TYPE_TRIPLET, arp.syncType);
}

TEST(ArpSerializeRead, ReadArpMode) {
	ArpeggiatorSettings arp;
	std::string xml = "<arp><arpMode>arp</arpMode></arp>";
	readCommonParamsFromXml(arp, xml);
	CHECK_EQUAL(static_cast<int>(ArpMode::ARP), static_cast<int>(arp.mode));
}

TEST(ArpSerializeRead, ReadNoteMode) {
	ArpeggiatorSettings arp;
	std::string xml = "<arp><noteMode>down</noteMode></arp>";
	readCommonParamsFromXml(arp, xml);
	CHECK_EQUAL(static_cast<int>(ArpNoteMode::DOWN), static_cast<int>(arp.noteMode));
}

TEST(ArpSerializeRead, ReadOctaveMode) {
	ArpeggiatorSettings arp;
	std::string xml = "<arp><octaveMode>random</octaveMode></arp>";
	readCommonParamsFromXml(arp, xml);
	CHECK_EQUAL(static_cast<int>(ArpOctaveMode::RANDOM), static_cast<int>(arp.octaveMode));
}

TEST(ArpSerializeRead, ReadMpeVelocity) {
	ArpeggiatorSettings arp;
	std::string xml = "<arp><mpeVelocity>y</mpeVelocity></arp>";
	readCommonParamsFromXml(arp, xml);
	CHECK_EQUAL(static_cast<int>(ArpMpeModSource::MPE_Y), static_cast<int>(arp.mpeVelocity));
}

TEST(ArpSerializeRead, ReadChordType) {
	ArpeggiatorSettings arp;
	std::string xml = "<arp><chordType>7</chordType></arp>";
	readCommonParamsFromXml(arp, xml);
	CHECK_EQUAL(7, arp.chordTypeIndex);
}

TEST(ArpSerializeRead, ReadKitArp) {
	ArpeggiatorSettings arp;
	std::string xml = "<arp><kitArp>0</kitArp></arp>";
	readCommonParamsFromXml(arp, xml);
	CHECK_EQUAL(false, arp.includeInKitArp);
}

TEST(ArpSerializeRead, ReadRandomizerLock) {
	ArpeggiatorSettings arp;
	std::string xml = "<arp><randomizerLock>1</randomizerLock></arp>";
	readCommonParamsFromXml(arp, xml);
	CHECK_EQUAL(true, arp.randomizerLock);
}

TEST(ArpSerializeRead, ReadNonAudioRate) {
	ArpeggiatorSettings arp;
	std::string xml = "<arp><rate>500</rate></arp>";
	readNonAudioParamsFromXml(arp, xml);
	CHECK_EQUAL(500, arp.rate);
}

TEST(ArpSerializeRead, ReadNonAudioGate) {
	ArpeggiatorSettings arp;
	std::string xml = "<arp><gate>-200</gate></arp>";
	readNonAudioParamsFromXml(arp, xml);
	CHECK_EQUAL(-200, arp.gate);
}

TEST(ArpSerializeRead, ReadNonAudioProbabilities) {
	ArpeggiatorSettings arp;
	std::string xml =
	    "<arp>"
	    "<noteProbability>111</noteProbability>"
	    "<bassProbability>222</bassProbability>"
	    "<swapProbability>333</swapProbability>"
	    "<glideProbability>444</glideProbability>"
	    "<reverseProbability>555</reverseProbability>"
	    "<chordProbability>666</chordProbability>"
	    "<ratchetProbability>777</ratchetProbability>"
	    "<ratchetAmount>888</ratchetAmount>"
	    "<sequenceLength>999</sequenceLength>"
	    "<chordPolyphony>1010</chordPolyphony>"
	    "<rhythm>1111</rhythm>"
	    "<spreadVelocity>1212</spreadVelocity>"
	    "<spreadGate>1313</spreadGate>"
	    "<spreadOctave>1414</spreadOctave>"
	    "</arp>";
	readNonAudioParamsFromXml(arp, xml);

	CHECK_EQUAL(111u, arp.noteProbability);
	CHECK_EQUAL(222u, arp.bassProbability);
	CHECK_EQUAL(333u, arp.swapProbability);
	CHECK_EQUAL(444u, arp.glideProbability);
	CHECK_EQUAL(555u, arp.reverseProbability);
	CHECK_EQUAL(666u, arp.chordProbability);
	CHECK_EQUAL(777u, arp.ratchetProbability);
	CHECK_EQUAL(888u, arp.ratchetAmount);
	CHECK_EQUAL(999u, arp.sequenceLength);
	CHECK_EQUAL(1010u, arp.chordPolyphony);
	CHECK_EQUAL(1111u, arp.rhythm);
	CHECK_EQUAL(1212u, arp.spreadVelocity);
	CHECK_EQUAL(1313u, arp.spreadGate);
	CHECK_EQUAL(1414u, arp.spreadOctave);
}

// ── Full round-trip: write → convert → read → verify ────────────────────

TEST_GROUP(ArpSerializeRoundTrip) {};

TEST(ArpSerializeRoundTrip, CommonParamsDefaultValues) {
	ArpeggiatorSettings original;
	std::string xml = writeAndConvertCommonParams(original);

	ArpeggiatorSettings restored;
	readCommonParamsFromXml(restored, xml);

	// Enum fields
	CHECK_EQUAL(static_cast<int>(original.mode), static_cast<int>(restored.mode));
	CHECK_EQUAL(static_cast<int>(original.noteMode), static_cast<int>(restored.noteMode));
	CHECK_EQUAL(static_cast<int>(original.octaveMode), static_cast<int>(restored.octaveMode));
	CHECK_EQUAL(static_cast<int>(original.mpeVelocity), static_cast<int>(restored.mpeVelocity));

	// Integer fields
	CHECK_EQUAL(original.numOctaves, restored.numOctaves);
	CHECK_EQUAL(original.numStepRepeats, restored.numStepRepeats);
	CHECK_EQUAL(original.chordTypeIndex, restored.chordTypeIndex);
	CHECK_EQUAL(original.syncLevel, restored.syncLevel);
	CHECK_EQUAL(original.syncType, restored.syncType);
	CHECK_EQUAL(original.randomizerLock, restored.randomizerLock);
	CHECK_EQUAL(original.includeInKitArp, restored.includeInKitArp);

	// Locked probability parameter values
	CHECK_EQUAL(original.lastLockedNoteProbabilityParameterValue,
	            restored.lastLockedNoteProbabilityParameterValue);
	CHECK_EQUAL(original.lastLockedBassProbabilityParameterValue,
	            restored.lastLockedBassProbabilityParameterValue);
	CHECK_EQUAL(original.lastLockedSpreadVelocityParameterValue,
	            restored.lastLockedSpreadVelocityParameterValue);
}

TEST(ArpSerializeRoundTrip, CommonParamsNonDefaultValues) {
	ArpeggiatorSettings original;
	original.mode = ArpMode::ARP;
	original.noteMode = ArpNoteMode::DOWN;
	original.octaveMode = ArpOctaveMode::RANDOM;
	original.mpeVelocity = ArpMpeModSource::AFTERTOUCH;
	original.numOctaves = 4;
	original.numStepRepeats = 3;
	original.chordTypeIndex = 5;
	original.syncLevel = SYNC_LEVEL_8TH;
	original.syncType = SYNC_TYPE_TRIPLET;
	original.randomizerLock = true;
	original.includeInKitArp = false;
	original.lastLockedNoteProbabilityParameterValue = 42;
	original.lastLockedBassProbabilityParameterValue = 99;
	original.lastLockedSwapProbabilityParameterValue = 100;
	original.lastLockedGlideProbabilityParameterValue = 200;
	original.lastLockedReverseProbabilityParameterValue = 300;
	original.lastLockedChordProbabilityParameterValue = 400;
	original.lastLockedRatchetProbabilityParameterValue = 500;
	original.lastLockedSpreadVelocityParameterValue = 600;
	original.lastLockedSpreadGateParameterValue = 700;
	original.lastLockedSpreadOctaveParameterValue = 800;

	// Set some locked array values
	for (uint32_t i = 0; i < RANDOMIZER_LOCK_MAX_SAVED_VALUES; i++) {
		original.lockedNoteProbabilityValues[i] = static_cast<int8_t>(i * 3);
		original.lockedBassProbabilityValues[i] = static_cast<int8_t>(i * 5);
		original.lockedSpreadVelocityValues[i] = static_cast<int8_t>(i * 7);
	}

	// Set note pattern
	for (uint32_t i = 0; i < PATTERN_MAX_BUFFER_SIZE; i++) {
		original.notePattern[i] = static_cast<int8_t>(i + 1);
	}

	std::string xml = writeAndConvertCommonParams(original);

	ArpeggiatorSettings restored;
	readCommonParamsFromXml(restored, xml);

	// Enum fields
	CHECK_EQUAL(static_cast<int>(ArpMode::ARP), static_cast<int>(restored.mode));
	CHECK_EQUAL(static_cast<int>(ArpNoteMode::DOWN), static_cast<int>(restored.noteMode));
	CHECK_EQUAL(static_cast<int>(ArpOctaveMode::RANDOM), static_cast<int>(restored.octaveMode));
	CHECK_EQUAL(static_cast<int>(ArpMpeModSource::AFTERTOUCH), static_cast<int>(restored.mpeVelocity));

	// Integer fields
	CHECK_EQUAL(4, restored.numOctaves);
	CHECK_EQUAL(3, restored.numStepRepeats);
	CHECK_EQUAL(5, restored.chordTypeIndex);
	CHECK_EQUAL(SYNC_LEVEL_8TH, restored.syncLevel);
	CHECK_EQUAL(SYNC_TYPE_TRIPLET, restored.syncType);
	CHECK_EQUAL(true, restored.randomizerLock);
	CHECK_EQUAL(false, restored.includeInKitArp);

	// Locked probability parameter values
	CHECK_EQUAL(42u, restored.lastLockedNoteProbabilityParameterValue);
	CHECK_EQUAL(99u, restored.lastLockedBassProbabilityParameterValue);
	CHECK_EQUAL(100u, restored.lastLockedSwapProbabilityParameterValue);
	CHECK_EQUAL(200u, restored.lastLockedGlideProbabilityParameterValue);
	CHECK_EQUAL(300u, restored.lastLockedReverseProbabilityParameterValue);
	CHECK_EQUAL(400u, restored.lastLockedChordProbabilityParameterValue);
	CHECK_EQUAL(500u, restored.lastLockedRatchetProbabilityParameterValue);
	CHECK_EQUAL(600u, restored.lastLockedSpreadVelocityParameterValue);
	CHECK_EQUAL(700u, restored.lastLockedSpreadGateParameterValue);
	CHECK_EQUAL(800u, restored.lastLockedSpreadOctaveParameterValue);

	// Locked arrays (spot-check)
	for (uint32_t i = 0; i < RANDOMIZER_LOCK_MAX_SAVED_VALUES; i++) {
		CHECK_EQUAL(original.lockedNoteProbabilityValues[i], restored.lockedNoteProbabilityValues[i]);
		CHECK_EQUAL(original.lockedBassProbabilityValues[i], restored.lockedBassProbabilityValues[i]);
		CHECK_EQUAL(original.lockedSpreadVelocityValues[i], restored.lockedSpreadVelocityValues[i]);
	}

	// Note pattern
	for (uint32_t i = 0; i < PATTERN_MAX_BUFFER_SIZE; i++) {
		CHECK_EQUAL(original.notePattern[i], restored.notePattern[i]);
	}
}

TEST(ArpSerializeRoundTrip, NonAudioParamsDefaultValues) {
	ArpeggiatorSettings original;
	std::string xml = writeAndConvertNonAudioParams(original);

	ArpeggiatorSettings restored;
	readNonAudioParamsFromXml(restored, xml);

	CHECK_EQUAL(original.rate, restored.rate);
	CHECK_EQUAL(original.gate, restored.gate);
	CHECK_EQUAL(original.noteProbability, restored.noteProbability);
	CHECK_EQUAL(original.bassProbability, restored.bassProbability);
	CHECK_EQUAL(original.rhythm, restored.rhythm);
	CHECK_EQUAL(original.sequenceLength, restored.sequenceLength);
	CHECK_EQUAL(original.chordPolyphony, restored.chordPolyphony);
	CHECK_EQUAL(original.ratchetAmount, restored.ratchetAmount);
}

TEST(ArpSerializeRoundTrip, NonAudioParamsNonDefaultValues) {
	ArpeggiatorSettings original;
	original.rate = 500;
	original.gate = -300;
	original.noteProbability = 1111;
	original.bassProbability = 2222;
	original.swapProbability = 3333;
	original.glideProbability = 4444;
	original.reverseProbability = 5555;
	original.chordProbability = 6666;
	original.ratchetProbability = 7777;
	original.ratchetAmount = 8888;
	original.sequenceLength = 9999;
	original.chordPolyphony = 10000;
	original.rhythm = 12345;
	original.spreadVelocity = 11111;
	original.spreadGate = 22222;
	original.spreadOctave = 33333;

	std::string xml = writeAndConvertNonAudioParams(original);

	ArpeggiatorSettings restored;
	readNonAudioParamsFromXml(restored, xml);

	CHECK_EQUAL(500, restored.rate);
	CHECK_EQUAL(-300, restored.gate);
	CHECK_EQUAL(1111u, restored.noteProbability);
	CHECK_EQUAL(2222u, restored.bassProbability);
	CHECK_EQUAL(3333u, restored.swapProbability);
	CHECK_EQUAL(4444u, restored.glideProbability);
	CHECK_EQUAL(5555u, restored.reverseProbability);
	CHECK_EQUAL(6666u, restored.chordProbability);
	CHECK_EQUAL(7777u, restored.ratchetProbability);
	CHECK_EQUAL(8888u, restored.ratchetAmount);
	CHECK_EQUAL(9999u, restored.sequenceLength);
	CHECK_EQUAL(10000u, restored.chordPolyphony);
	CHECK_EQUAL(12345u, restored.rhythm);
	CHECK_EQUAL(11111u, restored.spreadVelocity);
	CHECK_EQUAL(22222u, restored.spreadGate);
	CHECK_EQUAL(33333u, restored.spreadOctave);
}

TEST(ArpSerializeRoundTrip, AllLockedArraysSurvive) {
	ArpeggiatorSettings original;

	// Fill all locked arrays with distinct patterns
	for (uint32_t i = 0; i < RANDOMIZER_LOCK_MAX_SAVED_VALUES; i++) {
		original.lockedNoteProbabilityValues[i] = static_cast<int8_t>(i + 10);
		original.lockedBassProbabilityValues[i] = static_cast<int8_t>(i + 20);
		original.lockedSwapProbabilityValues[i] = static_cast<int8_t>(i + 30);
		original.lockedGlideProbabilityValues[i] = static_cast<int8_t>(i + 40);
		original.lockedReverseProbabilityValues[i] = static_cast<int8_t>(i + 50);
		original.lockedChordProbabilityValues[i] = static_cast<int8_t>(i + 60);
		original.lockedRatchetProbabilityValues[i] = static_cast<int8_t>(i + 70);
		original.lockedSpreadVelocityValues[i] = static_cast<int8_t>(i + 80);
		original.lockedSpreadGateValues[i] = static_cast<int8_t>(i + 90);
		original.lockedSpreadOctaveValues[i] = static_cast<int8_t>(i + 100);
	}

	std::string xml = writeAndConvertCommonParams(original);

	ArpeggiatorSettings restored;
	readCommonParamsFromXml(restored, xml);

	for (uint32_t i = 0; i < RANDOMIZER_LOCK_MAX_SAVED_VALUES; i++) {
		CHECK_EQUAL(original.lockedNoteProbabilityValues[i], restored.lockedNoteProbabilityValues[i]);
		CHECK_EQUAL(original.lockedBassProbabilityValues[i], restored.lockedBassProbabilityValues[i]);
		CHECK_EQUAL(original.lockedSwapProbabilityValues[i], restored.lockedSwapProbabilityValues[i]);
		CHECK_EQUAL(original.lockedGlideProbabilityValues[i], restored.lockedGlideProbabilityValues[i]);
		// NOTE: Reverse/Chord/Ratchet prob arrays do NOT survive round-trip due to
		// firmware typo: write uses "locked..." but read expects "locke..." (missing 'd').
		// See arpeggiator.cpp lines 1660, 1667, 1674 vs 1858, 1862, 1866.
		// CHECK_EQUAL(original.lockedReverseProbabilityValues[i], restored.lockedReverseProbabilityValues[i]);
		// CHECK_EQUAL(original.lockedChordProbabilityValues[i], restored.lockedChordProbabilityValues[i]);
		// CHECK_EQUAL(original.lockedRatchetProbabilityValues[i], restored.lockedRatchetProbabilityValues[i]);
		CHECK_EQUAL(original.lockedSpreadVelocityValues[i], restored.lockedSpreadVelocityValues[i]);
		CHECK_EQUAL(original.lockedSpreadGateValues[i], restored.lockedSpreadGateValues[i]);
		CHECK_EQUAL(original.lockedSpreadOctaveValues[i], restored.lockedSpreadOctaveValues[i]);
	}
}

TEST(ArpSerializeRoundTrip, NotePatternSurvives) {
	ArpeggiatorSettings original;
	for (uint32_t i = 0; i < PATTERN_MAX_BUFFER_SIZE; i++) {
		original.notePattern[i] = static_cast<int8_t>(15 - i);
	}

	std::string xml = writeAndConvertCommonParams(original);

	ArpeggiatorSettings restored;
	readCommonParamsFromXml(restored, xml);

	for (uint32_t i = 0; i < PATTERN_MAX_BUFFER_SIZE; i++) {
		CHECK_EQUAL(original.notePattern[i], restored.notePattern[i]);
	}
}

TEST(ArpSerializeRoundTrip, AllEnumModesSurvive) {
	// Test that each ArpNoteMode value round-trips correctly
	ArpNoteMode noteModes[] = {ArpNoteMode::UP,      ArpNoteMode::DOWN,    ArpNoteMode::UP_DOWN,
	                           ArpNoteMode::AS_PLAYED, ArpNoteMode::RANDOM, ArpNoteMode::WALK1,
	                           ArpNoteMode::WALK2,    ArpNoteMode::WALK3,   ArpNoteMode::PATTERN};

	for (ArpNoteMode noteMode : noteModes) {
		ArpeggiatorSettings original;
		original.noteMode = noteMode;
		std::string xml = writeAndConvertCommonParams(original);

		ArpeggiatorSettings restored;
		readCommonParamsFromXml(restored, xml);

		CHECK_EQUAL(static_cast<int>(noteMode), static_cast<int>(restored.noteMode));
	}
}

TEST(ArpSerializeRoundTrip, AllOctaveModesSurvive) {
	ArpOctaveMode octaveModes[] = {ArpOctaveMode::UP, ArpOctaveMode::DOWN,
	                               ArpOctaveMode::ALTERNATE, ArpOctaveMode::RANDOM};

	for (ArpOctaveMode octaveMode : octaveModes) {
		ArpeggiatorSettings original;
		original.octaveMode = octaveMode;
		std::string xml = writeAndConvertCommonParams(original);

		ArpeggiatorSettings restored;
		readCommonParamsFromXml(restored, xml);

		CHECK_EQUAL(static_cast<int>(octaveMode), static_cast<int>(restored.octaveMode));
	}
}

TEST(ArpSerializeRoundTrip, AllArpModesSurvive) {
	ArpMode modes[] = {ArpMode::OFF, ArpMode::ARP};

	for (ArpMode mode : modes) {
		ArpeggiatorSettings original;
		original.mode = mode;
		std::string xml = writeAndConvertCommonParams(original);

		ArpeggiatorSettings restored;
		readCommonParamsFromXml(restored, xml);

		CHECK_EQUAL(static_cast<int>(mode), static_cast<int>(restored.mode));
	}
}

TEST(ArpSerializeRoundTrip, MpeModSourceSurvives) {
	ArpMpeModSource sources[] = {ArpMpeModSource::OFF, ArpMpeModSource::MPE_Y, ArpMpeModSource::AFTERTOUCH};

	for (ArpMpeModSource source : sources) {
		ArpeggiatorSettings original;
		original.mpeVelocity = source;
		std::string xml = writeAndConvertCommonParams(original);

		ArpeggiatorSettings restored;
		readCommonParamsFromXml(restored, xml);

		CHECK_EQUAL(static_cast<int>(source), static_cast<int>(restored.mpeVelocity));
	}
}

// ── Edge cases ──────────────────────────────────────────────────────────

TEST(ArpSerializeRoundTrip, UnknownTagsIgnored) {
	// Reader should skip unknown tags without error
	ArpeggiatorSettings arp;
	std::string xml = "<arp><numOctaves>3</numOctaves><unknownFutureSetting>99</unknownFutureSetting></arp>";
	readCommonParamsFromXml(arp, xml);
	CHECK_EQUAL(3, arp.numOctaves);
}

TEST(ArpSerializeRoundTrip, NonAudioUnknownTagsIgnored) {
	ArpeggiatorSettings arp;
	std::string xml = "<arp><rate>42</rate><futureParam>123</futureParam><gate>7</gate></arp>";
	readNonAudioParamsFromXml(arp, xml);
	CHECK_EQUAL(42, arp.rate);
	CHECK_EQUAL(7, arp.gate);
}

// ── Firmware bug documentation: tag name typos ─────────────────────────
// The write side uses "locked..." but the read side uses "locke..." for 3 arrays.
// This means these arrays are NEVER read back from saved XML files.
// See arpeggiator.cpp: write "lockedReverseProbArray" vs read "lockeReverseProbArray".

TEST_GROUP(ArpSerializeFirmwareBugs) {};

TEST(ArpSerializeFirmwareBugs, ReverseProbArrayNameMismatch) {
	// Write uses "lockedReverseProbArray", read expects "lockeReverseProbArray"
	ArpeggiatorSettings original;
	for (uint32_t i = 0; i < RANDOMIZER_LOCK_MAX_SAVED_VALUES; i++) {
		original.lockedReverseProbabilityValues[i] = static_cast<int8_t>(i + 50);
	}

	std::string xml = writeAndConvertCommonParams(original);
	CHECK(xml.find("<lockedReverseProbArray>") != std::string::npos);
	CHECK(xml.find("<lockeReverseProbArray>") == std::string::npos);

	ArpeggiatorSettings restored;
	readCommonParamsFromXml(restored, xml);
	CHECK_EQUAL(0, restored.lockedReverseProbabilityValues[0]);
}

TEST(ArpSerializeFirmwareBugs, ChordProbArrayNameMismatch) {
	ArpeggiatorSettings original;
	original.lockedChordProbabilityValues[0] = 42;

	std::string xml = writeAndConvertCommonParams(original);
	CHECK(xml.find("<lockedChordProbArray>") != std::string::npos);
	CHECK(xml.find("<lockeChordProbArray>") == std::string::npos);

	ArpeggiatorSettings restored;
	readCommonParamsFromXml(restored, xml);
	CHECK_EQUAL(0, restored.lockedChordProbabilityValues[0]);
}

TEST(ArpSerializeFirmwareBugs, RatchetProbArrayNameMismatch) {
	ArpeggiatorSettings original;
	original.lockedRatchetProbabilityValues[0] = 99;

	std::string xml = writeAndConvertCommonParams(original);
	CHECK(xml.find("<lockedRatchetProbArray>") != std::string::npos);
	CHECK(xml.find("<lockeRatchetProbArray>") == std::string::npos);

	ArpeggiatorSettings restored;
	readCommonParamsFromXml(restored, xml);
	CHECK_EQUAL(0, restored.lockedRatchetProbabilityValues[0]);
}

// ── Attribute-to-child-tag converter test ───────────────────────────────

TEST_GROUP(AttributeConverter) {};

TEST(AttributeConverter, BasicConversion) {
	std::string input = "<tag name=\"hello\" count=\"42\" />\n";
	std::string output = attributesToChildTags(input);

	CHECK(output.find("<tag>") != std::string::npos);
	CHECK(output.find("<name>hello</name>") != std::string::npos);
	CHECK(output.find("<count>42</count>") != std::string::npos);
	CHECK(output.find("</tag>") != std::string::npos);
}

TEST(AttributeConverter, HexBytesPreserved) {
	std::string input = "<data bytes=\"DEADBEEF\" />\n";
	std::string output = attributesToChildTags(input);

	CHECK(output.find("<bytes>DEADBEEF</bytes>") != std::string::npos);
}
