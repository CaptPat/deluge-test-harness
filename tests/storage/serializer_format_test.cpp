// Tests for XML serialization formatting layer.
// Uses a StringSerializer that reimplements XMLSerializer's formatting logic
// but writes to a std::string instead of FatFS, enabling isolated testing.

#include "CppUTest/TestHarness.h"
#include "string_serializer.h"
#include "util/d_stringbuf.h"

// ── Test group ──────────────────────────────────────────────────────────

TEST_GROUP(SerializerFormat) {
	StringSerializer s;

	void setup() { s.reset(); }
};

// ── writeTag ────────────────────────────────────────────────────────────

TEST(SerializerFormat, WriteTagString) {
	s.writeTag("name", "hello");
	STRCMP_EQUAL("<name>hello</name>\n", s.output.c_str());
}

TEST(SerializerFormat, WriteTagInt) {
	s.writeTag("volume", 42);
	STRCMP_EQUAL("<volume>42</volume>\n", s.output.c_str());
}

TEST(SerializerFormat, WriteTagNegativeInt) {
	s.writeTag("pan", -7);
	STRCMP_EQUAL("<pan>-7</pan>\n", s.output.c_str());
}

TEST(SerializerFormat, WriteTagZero) {
	s.writeTag("offset", 0);
	STRCMP_EQUAL("<offset>0</offset>\n", s.output.c_str());
}

// ── writeAttribute ──────────────────────────────────────────────────────

TEST(SerializerFormat, WriteAttributeStringOnNewLine) {
	s.writeAttribute("path", "/SAMPLES/kick.wav");
	STRCMP_EQUAL("\npath=\"/SAMPLES/kick.wav\"", s.output.c_str());
}

TEST(SerializerFormat, WriteAttributeStringInline) {
	s.writeAttribute("path", "/SAMPLES/kick.wav", false);
	STRCMP_EQUAL(" path=\"/SAMPLES/kick.wav\"", s.output.c_str());
}

TEST(SerializerFormat, WriteAttributeInt) {
	s.writeAttribute("volume", 100);
	STRCMP_EQUAL("\nvolume=\"100\"", s.output.c_str());
}

TEST(SerializerFormat, WriteAttributeIntInline) {
	s.writeAttribute("volume", 100, false);
	STRCMP_EQUAL(" volume=\"100\"", s.output.c_str());
}

TEST(SerializerFormat, WriteAttributeNegativeInt) {
	s.writeAttribute("pan", -50);
	STRCMP_EQUAL("\npan=\"-50\"", s.output.c_str());
}

// ── writeAttributeHex ───────────────────────────────────────────────────

TEST(SerializerFormat, WriteAttributeHex4Chars) {
	s.writeAttributeHex("color", 0x00FF, 4);
	STRCMP_EQUAL("\ncolor=\"0x00FF\"", s.output.c_str());
}

TEST(SerializerFormat, WriteAttributeHex8Chars) {
	s.writeAttributeHex("addr", 0xDEADBEEF, 8);
	STRCMP_EQUAL("\naddr=\"0xDEADBEEF\"", s.output.c_str());
}

TEST(SerializerFormat, WriteAttributeHex2Chars) {
	s.writeAttributeHex("byte", 0x0A, 2);
	STRCMP_EQUAL("\nbyte=\"0x0A\"", s.output.c_str());
}

TEST(SerializerFormat, WriteAttributeHexInline) {
	s.writeAttributeHex("val", 0xFF, 2, false);
	STRCMP_EQUAL(" val=\"0xFF\"", s.output.c_str());
}

// ── writeAttributeHexBytes ──────────────────────────────────────────────

TEST(SerializerFormat, WriteAttributeHexBytesOnNewLine) {
	uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
	s.writeAttributeHexBytes("data", data, 4);
	STRCMP_EQUAL("\ndata=\"DEADBEEF\"", s.output.c_str());
}

TEST(SerializerFormat, WriteAttributeHexBytesInline) {
	uint8_t data[] = {0x01, 0x02};
	s.writeAttributeHexBytes("bytes", data, 2, false);
	STRCMP_EQUAL(" bytes=\"0102\"", s.output.c_str());
}

TEST(SerializerFormat, WriteAttributeHexBytesSingleByte) {
	uint8_t data[] = {0x7F};
	s.writeAttributeHexBytes("b", data, 1, false);
	STRCMP_EQUAL(" b=\"7F\"", s.output.c_str());
}

// ── Indentation ─────────────────────────────────────────────────────────

TEST(SerializerFormat, PrintIndentsZero) {
	s.printIndents();
	STRCMP_EQUAL("", s.output.c_str());
}

TEST(SerializerFormat, PrintIndentsOne) {
	s.indentAmount = 1;
	s.printIndents();
	STRCMP_EQUAL("\t", s.output.c_str());
}

TEST(SerializerFormat, PrintIndentsThree) {
	s.indentAmount = 3;
	s.printIndents();
	STRCMP_EQUAL("\t\t\t", s.output.c_str());
}

TEST(SerializerFormat, WriteTagWithIndentation) {
	s.indentAmount = 2;
	s.writeTag("name", "value");
	STRCMP_EQUAL("\t\t<name>value</name>\n", s.output.c_str());
}

TEST(SerializerFormat, WriteAttributeWithIndentation) {
	s.indentAmount = 1;
	s.writeAttribute("key", "val");
	STRCMP_EQUAL("\n\tkey=\"val\"", s.output.c_str());
}

// ── Opening/Closing tags ────────────────────────────────────────────────

TEST(SerializerFormat, WriteOpeningTag) {
	s.writeOpeningTag("sound");
	STRCMP_EQUAL("<sound>\n", s.output.c_str());
	CHECK_EQUAL(1, s.indentAmount);
}

TEST(SerializerFormat, WriteOpeningTagNoNewline) {
	s.writeOpeningTag("sound", false);
	STRCMP_EQUAL("<sound>", s.output.c_str());
	CHECK_EQUAL(1, s.indentAmount);
}

TEST(SerializerFormat, WriteClosingTag) {
	s.indentAmount = 1;
	s.writeClosingTag("sound");
	STRCMP_EQUAL("</sound>\n", s.output.c_str());
	CHECK_EQUAL(0, s.indentAmount);
}

TEST(SerializerFormat, WriteClosingTagNoIndent) {
	s.indentAmount = 1;
	s.writeClosingTag("sound", false);
	STRCMP_EQUAL("</sound>\n", s.output.c_str());
	CHECK_EQUAL(0, s.indentAmount);
}

TEST(SerializerFormat, CloseTag) {
	s.indentAmount = 1;
	s.closeTag();
	STRCMP_EQUAL(" />\n", s.output.c_str());
	CHECK_EQUAL(0, s.indentAmount);
}

// ── writeOpeningTagBeginning + writeOpeningTagEnd ───────────────────────

TEST(SerializerFormat, OpeningTagBeginningAndEnd) {
	s.writeOpeningTagBeginning("kit");
	CHECK_EQUAL(1, s.indentAmount);
	STRCMP_EQUAL("<kit", s.output.c_str());

	s.writeAttribute("name", "drums", false);
	STRCMP_EQUAL("<kit name=\"drums\"", s.output.c_str());

	s.writeOpeningTagEnd();
	STRCMP_EQUAL("<kit name=\"drums\">\n", s.output.c_str());
}

// ── writeTagNameAndSeparator ────────────────────────────────────────────

TEST(SerializerFormat, WriteTagNameAndSeparator) {
	s.writeTagNameAndSeperator("type");
	STRCMP_EQUAL("type=", s.output.c_str());
}

// ── Array start/end ─────────────────────────────────────────────────────

TEST(SerializerFormat, ArrayStartAndEnd) {
	s.writeArrayStart("noteRows");
	STRCMP_EQUAL("<noteRows>\n", s.output.c_str());
	CHECK_EQUAL(1, s.indentAmount);

	s.output.clear();
	s.writeArrayEnding("noteRows");
	STRCMP_EQUAL("</noteRows>\n", s.output.c_str());
	CHECK_EQUAL(0, s.indentAmount);
}

// ── Nesting: multi-level tag structure ──────────────────────────────────

TEST(SerializerFormat, NestedTags) {
	s.writeOpeningTag("song");
	s.writeOpeningTag("instruments");
	s.writeOpeningTagBeginning("sound");
	s.writeAttribute("name", "synth1", false);
	s.writeOpeningTagEnd();
	s.writeTag("volume", 80);
	s.writeClosingTag("sound");
	s.writeClosingTag("instruments");
	s.writeClosingTag("song");

	std::string expected =
	    "<song>\n"
	    "\t<instruments>\n"
	    "\t\t<sound name=\"synth1\">\n"
	    "\t\t\t<volume>80</volume>\n"
	    "\t\t</sound>\n"
	    "\t</instruments>\n"
	    "</song>\n";

	STRCMP_EQUAL(expected.c_str(), s.output.c_str());
}

TEST(SerializerFormat, DeeplyNestedIndentation) {
	s.writeOpeningTag("a");
	s.writeOpeningTag("b");
	s.writeOpeningTag("c");
	s.writeTag("val", 1);
	s.writeClosingTag("c");
	s.writeClosingTag("b");
	s.writeClosingTag("a");

	std::string expected =
	    "<a>\n"
	    "\t<b>\n"
	    "\t\t<c>\n"
	    "\t\t\t<val>1</val>\n"
	    "\t\t</c>\n"
	    "\t</b>\n"
	    "</a>\n";

	STRCMP_EQUAL(expected.c_str(), s.output.c_str());
}

// ── Self-closing tag ────────────────────────────────────────────────────

TEST(SerializerFormat, SelfClosingTagWithAttributes) {
	s.writeOpeningTagBeginning("sample");
	s.writeAttribute("path", "kick.wav", false);
	s.writeAttribute("zone", 0, false);
	s.closeTag();

	STRCMP_EQUAL("<sample path=\"kick.wav\" zone=\"0\" />\n", s.output.c_str());
	CHECK_EQUAL(0, s.indentAmount);
}

// ── Mixed attributes and tags ───────────────────────────────────────────

TEST(SerializerFormat, OpeningTagWithMultipleAttributes) {
	s.writeOpeningTagBeginning("sound");
	s.writeAttribute("polyphonic", "choke", false);
	s.writeAttribute("voicePriority", 1, false);
	s.writeOpeningTagEnd();

	STRCMP_EQUAL("<sound polyphonic=\"choke\" voicePriority=\"1\">\n", s.output.c_str());
}

TEST(SerializerFormat, AttributesOnNewLinesWithIndent) {
	s.writeOpeningTagBeginning("osc1");
	s.writeAttribute("type", "square");
	s.writeAttribute("transpose", 0);
	s.writeOpeningTagEnd();

	std::string expected = "<osc1\n\ttype=\"square\"\n\ttranspose=\"0\">\n";
	STRCMP_EQUAL(expected.c_str(), s.output.c_str());
}

// ── Reset ───────────────────────────────────────────────────────────────

TEST(SerializerFormat, ResetClearsState) {
	s.writeOpeningTag("test");
	s.writeTag("inner", 42);
	CHECK(s.output.length() > 0);
	CHECK(s.indentAmount > 0);

	s.reset();
	CHECK_EQUAL(0u, s.output.length());
	CHECK_EQUAL(0, s.indentAmount);
}

// ── Edge cases ──────────────────────────────────────────────────────────

TEST(SerializerFormat, EmptyTagName) {
	s.writeTag("", "value");
	STRCMP_EQUAL("<>value</>\n", s.output.c_str());
}

TEST(SerializerFormat, EmptyAttributeValue) {
	s.writeAttribute("name", "", false);
	STRCMP_EQUAL(" name=\"\"", s.output.c_str());
}

TEST(SerializerFormat, LargeIntAttribute) {
	s.writeAttribute("big", 2147483647, false);
	STRCMP_EQUAL(" big=\"2147483647\"", s.output.c_str());
}

TEST(SerializerFormat, NegativeLargeIntAttribute) {
	s.writeAttribute("neg", -2147483647, false);
	STRCMP_EQUAL(" neg=\"-2147483647\"", s.output.c_str());
}

TEST(SerializerFormat, HexZero) {
	s.writeAttributeHex("val", 0, 4, false);
	STRCMP_EQUAL(" val=\"0x0000\"", s.output.c_str());
}

TEST(SerializerFormat, HexBytesEmpty) {
	uint8_t data[] = {0};
	s.writeAttributeHexBytes("empty", data, 0, false);
	STRCMP_EQUAL(" empty=\"\"", s.output.c_str());
}
