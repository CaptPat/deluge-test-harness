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
#include "string_serializer.h"

#include <vector>

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
