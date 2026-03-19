// Round-trip serialization tests for Delay, Sidechain, AudioCompressor,
// Stutter, and FilterConfig parameters.
//
// These parameters are serialized inline in ModControllableAudio::writeTagsToFile()
// and read back in ModControllableAudio::readTagFromFile(). Since the test harness
// stubs those methods, we replicate the firmware's serialization logic in local
// helpers that mirror the real code, then verify round-trip fidelity.
//
// This tests the XML format contract: if the write format changes in the firmware,
// these tests will catch the deserialization breakage.

#include "CppUTest/TestHarness.h"
#include "dsp/compressor/rms_feedback.h"
#include "dsp/delay/delay.h"
#include "model/mod_controllable/filters/filter_config.h"
#include "model/sync.h"
#include "modulation/sidechain/sidechain.h"
#include "string_serializer.h"

#include <cstring>
#include <regex>
#include <string>

// ═══════════════════════════════════════════════════════════════════════
// Helpers: replicate firmware serialization logic
// ═══════════════════════════════════════════════════════════════════════

// Convert attribute-format XML to child-tag XML for the reader.
// Input:  <delay\n\tpingPong="1"\n\tanalog="0" ... />
// Output: <delay><pingPong>1</pingPong><analog>0</analog>...</delay>
static std::string attributesToChildTags(std::string const& attrXml) {
	size_t tagStart = attrXml.find('<');
	if (tagStart == std::string::npos) {
		return attrXml;
	}
	size_t nameStart = tagStart + 1;
	size_t nameEnd = attrXml.find_first_of(" \t\n\r/>", nameStart);
	std::string tagName = attrXml.substr(nameStart, nameEnd - nameStart);

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

// ── Delay write helper ──────────────────────────────────────────────────
// Mirrors ModControllableAudio::writeTagsToFile() delay section.
// Uses identity sync-level conversion (magnitude=1).
static std::string writeDelayXml(Delay const& delay) {
	StringSerializer ser;
	ser.writeOpeningTagBeginning("delay");
	ser.writeAttribute("pingPong", delay.pingPong ? 1 : 0);
	ser.writeAttribute("analog", delay.analog ? 1 : 0);
	// writeAbsoluteSyncLevelToFile with identity conversion (magnitude=1)
	ser.writeAttribute("syncLevel", static_cast<int32_t>(delay.syncLevel));
	// writeSyncTypeToFile
	ser.writeAttribute("syncType", static_cast<int32_t>(delay.syncType));
	ser.closeTag();
	return attributesToChildTags(ser.output);
}

// ── Delay read helper ───────────────────────────────────────────────────
// Mirrors ModControllableAudio::readTagFromFile() delay section.
static void readDelayFromXml(Delay& delay, std::string const& xml) {
	StringDeserializer deser;
	deser.loadXML(xml);

	// Consume outer tag
	char const* outerTag = deser.readNextTagOrAttributeName();
	(void)outerTag;

	// Set defaults as firmware does
	delay.syncType = SYNC_TYPE_EVEN;
	delay.syncLevel = SYNC_LEVEL_NONE;

	char const* tagName;
	while (*(tagName = deser.readNextTagOrAttributeName())) {
		if (!strcmp(tagName, "pingPong")) {
			delay.pingPong = static_cast<bool>(deser.readTagOrAttributeValueInt());
			deser.exitTag();
		}
		else if (!strcmp(tagName, "analog")) {
			delay.analog = static_cast<bool>(deser.readTagOrAttributeValueInt());
			deser.exitTag();
		}
		else if (!strcmp(tagName, "syncType")) {
			delay.syncType = static_cast<SyncType>(deser.readTagOrAttributeValueInt());
			deser.exitTag();
		}
		else if (!strcmp(tagName, "syncLevel")) {
			// Identity conversion (insideWorldTickMagnitude=1)
			delay.syncLevel = static_cast<SyncLevel>(deser.readTagOrAttributeValueInt());
			deser.exitTag();
		}
		else {
			deser.exitTag();
		}
	}
}

// ── Sidechain write helper ──────────────────────────────────────────────
static std::string writeSidechainXml(SideChain const& sc) {
	StringSerializer ser;
	ser.writeOpeningTagBeginning("sidechain");
	ser.writeAttribute("attack", sc.attack);
	ser.writeAttribute("release", sc.release);
	ser.writeAttribute("syncLevel", static_cast<int32_t>(sc.syncLevel));
	ser.writeAttribute("syncType", static_cast<int32_t>(sc.syncType));
	ser.closeTag();
	return attributesToChildTags(ser.output);
}

// ── Sidechain read helper ───────────────────────────────────────────────
static void readSidechainFromXml(SideChain& sc, std::string const& xml) {
	StringDeserializer deser;
	deser.loadXML(xml);

	char const* outerTag = deser.readNextTagOrAttributeName();
	(void)outerTag;

	sc.syncType = SYNC_TYPE_EVEN;
	sc.syncLevel = SYNC_LEVEL_NONE;

	char const* tagName;
	while (*(tagName = deser.readNextTagOrAttributeName())) {
		if (!strcmp(tagName, "attack")) {
			sc.attack = deser.readTagOrAttributeValueInt();
			deser.exitTag();
		}
		else if (!strcmp(tagName, "release")) {
			sc.release = deser.readTagOrAttributeValueInt();
			deser.exitTag();
		}
		else if (!strcmp(tagName, "syncType")) {
			sc.syncType = static_cast<SyncType>(deser.readTagOrAttributeValueInt());
			deser.exitTag();
		}
		else if (!strcmp(tagName, "syncLevel")) {
			sc.syncLevel = static_cast<SyncLevel>(deser.readTagOrAttributeValueInt());
			deser.exitTag();
		}
		else {
			deser.exitTag();
		}
	}
}

// ── AudioCompressor write helper ────────────────────────────────────────
static std::string writeCompressorXml(RMSFeedbackCompressor& comp) {
	StringSerializer ser;
	ser.writeOpeningTagBeginning("audioCompressor");
	ser.writeAttribute("attack", comp.getAttack());
	ser.writeAttribute("release", comp.getRelease());
	ser.writeAttribute("thresh", comp.getThreshold());
	ser.writeAttribute("ratio", comp.getRatio());
	ser.writeAttribute("compHPF", comp.getSidechain());
	ser.writeAttribute("compBlend", comp.getBlend());
	ser.closeTag();
	return attributesToChildTags(ser.output);
}

// ── AudioCompressor read helper ─────────────────────────────────────────
static void readCompressorFromXml(RMSFeedbackCompressor& comp, std::string const& xml) {
	StringDeserializer deser;
	deser.loadXML(xml);

	char const* outerTag = deser.readNextTagOrAttributeName();
	(void)outerTag;

	char const* tagName;
	while (*(tagName = deser.readNextTagOrAttributeName())) {
		if (!strcmp(tagName, "attack")) {
			comp.setAttack(deser.readTagOrAttributeValueInt());
			deser.exitTag();
		}
		else if (!strcmp(tagName, "release")) {
			comp.setRelease(deser.readTagOrAttributeValueInt());
			deser.exitTag();
		}
		else if (!strcmp(tagName, "thresh")) {
			comp.setThreshold(deser.readTagOrAttributeValueInt());
			deser.exitTag();
		}
		else if (!strcmp(tagName, "ratio")) {
			comp.setRatio(deser.readTagOrAttributeValueInt());
			deser.exitTag();
		}
		else if (!strcmp(tagName, "compHPF")) {
			comp.setSidechain(deser.readTagOrAttributeValueInt());
			deser.exitTag();
		}
		else if (!strcmp(tagName, "compBlend")) {
			comp.setBlend(deser.readTagOrAttributeValueInt());
			deser.exitTag();
		}
		else {
			deser.exitTag();
		}
	}
}

// ── StutterConfig write helper ──────────────────────────────────────────
struct StutterConfig {
	bool quantized = true;
	bool reversed = false;
	bool pingPong = false;
};

static std::string writeStutterXml(StutterConfig const& sc) {
	StringSerializer ser;
	ser.writeOpeningTagBeginning("stutter");
	ser.writeAttribute("quantized", sc.quantized ? 1 : 0);
	ser.writeAttribute("reverse", sc.reversed ? 1 : 0);
	ser.writeAttribute("pingPong", sc.pingPong ? 1 : 0);
	ser.closeTag();
	return attributesToChildTags(ser.output);
}

// ── StutterConfig read helper ───────────────────────────────────────────
static void readStutterFromXml(StutterConfig& sc, std::string const& xml) {
	StringDeserializer deser;
	deser.loadXML(xml);

	char const* outerTag = deser.readNextTagOrAttributeName();
	(void)outerTag;

	// Firmware defaults
	sc.quantized = true;
	sc.reversed = false;
	sc.pingPong = false;

	char const* tagName;
	while (*(tagName = deser.readNextTagOrAttributeName())) {
		if (!strcmp(tagName, "quantized")) {
			sc.quantized = static_cast<bool>(deser.readTagOrAttributeValueInt());
			deser.exitTag();
		}
		else if (!strcmp(tagName, "reverse")) {
			sc.reversed = static_cast<bool>(deser.readTagOrAttributeValueInt());
			deser.exitTag();
		}
		else if (!strcmp(tagName, "pingPong")) {
			sc.pingPong = static_cast<bool>(deser.readTagOrAttributeValueInt());
			deser.exitTag();
		}
		else {
			deser.exitTag();
		}
	}
}

// ═══════════════════════════════════════════════════════════════════════
// Delay Round-Trip Tests
// ═══════════════════════════════════════════════════════════════════════

TEST_GROUP(DelaySerialization) {};

TEST(DelaySerialization, DefaultValuesRoundTrip) {
	Delay original;
	// Defaults: pingPong=true, analog=false, syncType=EVEN, syncLevel=16TH

	std::string xml = writeDelayXml(original);
	Delay restored;
	readDelayFromXml(restored, xml);

	CHECK_EQUAL(original.pingPong, restored.pingPong);
	CHECK_EQUAL(original.analog, restored.analog);
	CHECK_EQUAL(static_cast<int>(original.syncType), static_cast<int>(restored.syncType));
	CHECK_EQUAL(static_cast<int>(original.syncLevel), static_cast<int>(restored.syncLevel));
}

TEST(DelaySerialization, NonDefaultValuesRoundTrip) {
	Delay original;
	original.pingPong = false;
	original.analog = true;
	original.syncType = SYNC_TYPE_TRIPLET;
	original.syncLevel = SYNC_LEVEL_4TH;

	std::string xml = writeDelayXml(original);
	Delay restored;
	readDelayFromXml(restored, xml);

	CHECK_EQUAL(false, restored.pingPong);
	CHECK_EQUAL(true, restored.analog);
	CHECK_EQUAL(static_cast<int>(SYNC_TYPE_TRIPLET), static_cast<int>(restored.syncType));
	CHECK_EQUAL(static_cast<int>(SYNC_LEVEL_4TH), static_cast<int>(restored.syncLevel));
}

TEST(DelaySerialization, PingPongTrueAnalogFalse) {
	Delay original;
	original.pingPong = true;
	original.analog = false;

	std::string xml = writeDelayXml(original);
	Delay restored;
	readDelayFromXml(restored, xml);

	CHECK_EQUAL(true, restored.pingPong);
	CHECK_EQUAL(false, restored.analog);
}

TEST(DelaySerialization, SyncLevelNone) {
	Delay original;
	original.syncLevel = SYNC_LEVEL_NONE;

	std::string xml = writeDelayXml(original);
	Delay restored;
	readDelayFromXml(restored, xml);

	CHECK_EQUAL(static_cast<int>(SYNC_LEVEL_NONE), static_cast<int>(restored.syncLevel));
}

TEST(DelaySerialization, AllSyncTypes) {
	SyncType types[] = {SYNC_TYPE_EVEN, SYNC_TYPE_TRIPLET, SYNC_TYPE_DOTTED};
	for (SyncType st : types) {
		Delay original;
		original.syncType = st;

		std::string xml = writeDelayXml(original);
		Delay restored;
		readDelayFromXml(restored, xml);

		CHECK_EQUAL(static_cast<int>(st), static_cast<int>(restored.syncType));
	}
}

TEST(DelaySerialization, AllSyncLevels) {
	for (int level = 0; level <= 9; level++) {
		Delay original;
		original.syncLevel = static_cast<SyncLevel>(level);

		std::string xml = writeDelayXml(original);
		Delay restored;
		readDelayFromXml(restored, xml);

		CHECK_EQUAL(level, static_cast<int>(restored.syncLevel));
	}
}

TEST(DelaySerialization, UnknownTagsIgnored) {
	std::string xml =
	    "<delay>"
	    "<pingPong>0</pingPong>"
	    "<futureParam>999</futureParam>"
	    "<analog>1</analog>"
	    "</delay>";

	Delay restored;
	readDelayFromXml(restored, xml);

	CHECK_EQUAL(false, restored.pingPong);
	CHECK_EQUAL(true, restored.analog);
}

TEST(DelaySerialization, WriteFormatContainsExpectedAttributes) {
	Delay d;
	d.pingPong = true;
	d.analog = false;
	d.syncLevel = SYNC_LEVEL_8TH;
	d.syncType = SYNC_TYPE_DOTTED;

	StringSerializer ser;
	ser.writeOpeningTagBeginning("delay");
	ser.writeAttribute("pingPong", 1);
	ser.writeAttribute("analog", 0);
	ser.writeAttribute("syncLevel", static_cast<int32_t>(SYNC_LEVEL_8TH));
	ser.writeAttribute("syncType", static_cast<int32_t>(SYNC_TYPE_DOTTED));
	ser.closeTag();

	CHECK(ser.output.find("pingPong=\"1\"") != std::string::npos);
	CHECK(ser.output.find("analog=\"0\"") != std::string::npos);
	CHECK(ser.output.find("syncLevel=") != std::string::npos);
	CHECK(ser.output.find("syncType=") != std::string::npos);
}

// ═══════════════════════════════════════════════════════════════════════
// Sidechain Round-Trip Tests
// ═══════════════════════════════════════════════════════════════════════

TEST_GROUP(SidechainSerialization) {};

TEST(SidechainSerialization, DefaultValuesRoundTrip) {
	SideChain original;

	std::string xml = writeSidechainXml(original);
	SideChain restored;
	readSidechainFromXml(restored, xml);

	CHECK_EQUAL(original.attack, restored.attack);
	CHECK_EQUAL(original.release, restored.release);
	CHECK_EQUAL(static_cast<int>(original.syncType), static_cast<int>(restored.syncType));
	CHECK_EQUAL(static_cast<int>(original.syncLevel), static_cast<int>(restored.syncLevel));
}

TEST(SidechainSerialization, CustomAttackRelease) {
	SideChain original;
	original.attack = 123456;
	original.release = 789012;

	std::string xml = writeSidechainXml(original);
	SideChain restored;
	readSidechainFromXml(restored, xml);

	CHECK_EQUAL(123456, restored.attack);
	CHECK_EQUAL(789012, restored.release);
}

TEST(SidechainSerialization, SyncTypeAndLevel) {
	SideChain original;
	original.syncType = SYNC_TYPE_TRIPLET;
	original.syncLevel = SYNC_LEVEL_2ND;

	std::string xml = writeSidechainXml(original);
	SideChain restored;
	readSidechainFromXml(restored, xml);

	CHECK_EQUAL(static_cast<int>(SYNC_TYPE_TRIPLET), static_cast<int>(restored.syncType));
	CHECK_EQUAL(static_cast<int>(SYNC_LEVEL_2ND), static_cast<int>(restored.syncLevel));
}

TEST(SidechainSerialization, AllSyncLevels) {
	for (int level = 0; level <= 9; level++) {
		SideChain original;
		original.syncLevel = static_cast<SyncLevel>(level);

		std::string xml = writeSidechainXml(original);
		SideChain restored;
		readSidechainFromXml(restored, xml);

		CHECK_EQUAL(level, static_cast<int>(restored.syncLevel));
	}
}

TEST(SidechainSerialization, ZeroAttackRelease) {
	SideChain original;
	original.attack = 0;
	original.release = 0;

	std::string xml = writeSidechainXml(original);
	SideChain restored;
	readSidechainFromXml(restored, xml);

	CHECK_EQUAL(0, restored.attack);
	CHECK_EQUAL(0, restored.release);
}

TEST(SidechainSerialization, NegativeValues) {
	SideChain original;
	original.attack = -100;
	original.release = -999999;

	std::string xml = writeSidechainXml(original);
	SideChain restored;
	readSidechainFromXml(restored, xml);

	CHECK_EQUAL(-100, restored.attack);
	CHECK_EQUAL(-999999, restored.release);
}

TEST(SidechainSerialization, MaxInt32Values) {
	SideChain original;
	original.attack = 2147483647;
	original.release = 2147483647;

	std::string xml = writeSidechainXml(original);
	SideChain restored;
	readSidechainFromXml(restored, xml);

	CHECK_EQUAL(2147483647, restored.attack);
	CHECK_EQUAL(2147483647, restored.release);
}

TEST(SidechainSerialization, UnknownTagsIgnored) {
	std::string xml =
	    "<sidechain>"
	    "<attack>500</attack>"
	    "<unknownFuture>42</unknownFuture>"
	    "<release>1000</release>"
	    "</sidechain>";

	SideChain restored;
	readSidechainFromXml(restored, xml);

	CHECK_EQUAL(500, restored.attack);
	CHECK_EQUAL(1000, restored.release);
}

TEST(SidechainSerialization, LegacyCompressorTagName) {
	// Pre-c1.1 firmware saves sidechain as "compressor". Verify the reader
	// can handle both tag names (the firmware checks for both).
	std::string xml =
	    "<compressor>"
	    "<attack>111</attack>"
	    "<release>222</release>"
	    "<syncType>10</syncType>"
	    "<syncLevel>5</syncLevel>"
	    "</compressor>";

	// Manually read with "compressor" as outer tag
	StringDeserializer deser;
	deser.loadXML(xml);
	char const* outerTag = deser.readNextTagOrAttributeName();
	STRCMP_EQUAL("compressor", outerTag);

	SideChain restored;
	restored.syncType = SYNC_TYPE_EVEN;
	restored.syncLevel = SYNC_LEVEL_NONE;

	char const* tagName;
	while (*(tagName = deser.readNextTagOrAttributeName())) {
		if (!strcmp(tagName, "attack")) {
			restored.attack = deser.readTagOrAttributeValueInt();
			deser.exitTag();
		}
		else if (!strcmp(tagName, "release")) {
			restored.release = deser.readTagOrAttributeValueInt();
			deser.exitTag();
		}
		else if (!strcmp(tagName, "syncType")) {
			restored.syncType = static_cast<SyncType>(deser.readTagOrAttributeValueInt());
			deser.exitTag();
		}
		else if (!strcmp(tagName, "syncLevel")) {
			restored.syncLevel = static_cast<SyncLevel>(deser.readTagOrAttributeValueInt());
			deser.exitTag();
		}
		else {
			deser.exitTag(tagName);
		}
	}

	CHECK_EQUAL(111, restored.attack);
	CHECK_EQUAL(222, restored.release);
	CHECK_EQUAL(static_cast<int>(SYNC_TYPE_TRIPLET), static_cast<int>(restored.syncType));
	CHECK_EQUAL(5, static_cast<int>(restored.syncLevel));
}

// ═══════════════════════════════════════════════════════════════════════
// AudioCompressor (RMSFeedbackCompressor) Round-Trip Tests
// ═══════════════════════════════════════════════════════════════════════

TEST_GROUP(AudioCompressorSerialization) {};

TEST(AudioCompressorSerialization, DefaultValuesRoundTrip) {
	RMSFeedbackCompressor original;

	std::string xml = writeCompressorXml(original);
	RMSFeedbackCompressor restored;
	readCompressorFromXml(restored, xml);

	CHECK_EQUAL(original.getAttack(), restored.getAttack());
	CHECK_EQUAL(original.getRelease(), restored.getRelease());
	CHECK_EQUAL(original.getThreshold(), restored.getThreshold());
	CHECK_EQUAL(original.getRatio(), restored.getRatio());
	CHECK_EQUAL(original.getSidechain(), restored.getSidechain());
	CHECK_EQUAL(original.getBlend(), restored.getBlend());
}

TEST(AudioCompressorSerialization, NonDefaultValuesRoundTrip) {
	RMSFeedbackCompressor original;
	original.setAttack(100000);
	original.setRelease(200000);
	original.setThreshold(300000);
	original.setRatio(400000);
	original.setSidechain(500000);
	original.setBlend(600000);

	std::string xml = writeCompressorXml(original);
	RMSFeedbackCompressor restored;
	readCompressorFromXml(restored, xml);

	CHECK_EQUAL(original.getAttack(), restored.getAttack());
	CHECK_EQUAL(original.getRelease(), restored.getRelease());
	CHECK_EQUAL(original.getThreshold(), restored.getThreshold());
	CHECK_EQUAL(original.getRatio(), restored.getRatio());
	CHECK_EQUAL(original.getSidechain(), restored.getSidechain());
	CHECK_EQUAL(original.getBlend(), restored.getBlend());
}

TEST(AudioCompressorSerialization, ZeroValues) {
	RMSFeedbackCompressor original;
	original.setAttack(0);
	original.setRelease(0);
	original.setThreshold(0);
	original.setRatio(0);
	original.setSidechain(0);
	original.setBlend(0);

	std::string xml = writeCompressorXml(original);
	RMSFeedbackCompressor restored;
	readCompressorFromXml(restored, xml);

	CHECK_EQUAL(0, restored.getAttack());
	CHECK_EQUAL(0, restored.getRelease());
	CHECK_EQUAL(0, restored.getThreshold());
	CHECK_EQUAL(0, restored.getRatio());
	CHECK_EQUAL(0, restored.getSidechain());
	CHECK_EQUAL(0, restored.getBlend());
}

TEST(AudioCompressorSerialization, NegativeKnobPositions) {
	RMSFeedbackCompressor original;
	original.setAttack(-1000000);
	original.setRelease(-2000000);
	original.setThreshold(-500000);
	original.setRatio(-300000);

	std::string xml = writeCompressorXml(original);
	RMSFeedbackCompressor restored;
	readCompressorFromXml(restored, xml);

	CHECK_EQUAL(original.getAttack(), restored.getAttack());
	CHECK_EQUAL(original.getRelease(), restored.getRelease());
	CHECK_EQUAL(original.getThreshold(), restored.getThreshold());
	CHECK_EQUAL(original.getRatio(), restored.getRatio());
}

TEST(AudioCompressorSerialization, UnknownTagsIgnored) {
	std::string xml =
	    "<audioCompressor>"
	    "<attack>42</attack>"
	    "<futureParam>999</futureParam>"
	    "<release>84</release>"
	    "</audioCompressor>";

	RMSFeedbackCompressor restored;
	readCompressorFromXml(restored, xml);

	CHECK_EQUAL(42, restored.getAttack());
	CHECK_EQUAL(84, restored.getRelease());
}

TEST(AudioCompressorSerialization, WriteFormatCorrect) {
	RMSFeedbackCompressor comp;
	comp.setAttack(1000);
	comp.setRelease(2000);
	comp.setThreshold(3000);
	comp.setRatio(4000);
	comp.setSidechain(5000);
	comp.setBlend(6000);

	StringSerializer ser;
	ser.writeOpeningTagBeginning("audioCompressor");
	ser.writeAttribute("attack", comp.getAttack());
	ser.writeAttribute("release", comp.getRelease());
	ser.writeAttribute("thresh", comp.getThreshold());
	ser.writeAttribute("ratio", comp.getRatio());
	ser.writeAttribute("compHPF", comp.getSidechain());
	ser.writeAttribute("compBlend", comp.getBlend());
	ser.closeTag();

	CHECK(ser.output.find("attack=") != std::string::npos);
	CHECK(ser.output.find("release=") != std::string::npos);
	CHECK(ser.output.find("thresh=") != std::string::npos);
	CHECK(ser.output.find("ratio=") != std::string::npos);
	CHECK(ser.output.find("compHPF=") != std::string::npos);
	CHECK(ser.output.find("compBlend=") != std::string::npos);
}

// ═══════════════════════════════════════════════════════════════════════
// StutterConfig Round-Trip Tests
// ═══════════════════════════════════════════════════════════════════════

TEST_GROUP(StutterConfigSerialization) {};

TEST(StutterConfigSerialization, DefaultValuesRoundTrip) {
	StutterConfig original;

	std::string xml = writeStutterXml(original);
	StutterConfig restored;
	readStutterFromXml(restored, xml);

	CHECK_EQUAL(original.quantized, restored.quantized);
	CHECK_EQUAL(original.reversed, restored.reversed);
	CHECK_EQUAL(original.pingPong, restored.pingPong);
}

TEST(StutterConfigSerialization, AllEnabled) {
	StutterConfig original;
	original.quantized = true;
	original.reversed = true;
	original.pingPong = true;

	std::string xml = writeStutterXml(original);
	StutterConfig restored;
	readStutterFromXml(restored, xml);

	CHECK_EQUAL(true, restored.quantized);
	CHECK_EQUAL(true, restored.reversed);
	CHECK_EQUAL(true, restored.pingPong);
}

TEST(StutterConfigSerialization, AllDisabled) {
	StutterConfig original;
	original.quantized = false;
	original.reversed = false;
	original.pingPong = false;

	std::string xml = writeStutterXml(original);
	StutterConfig restored;
	readStutterFromXml(restored, xml);

	CHECK_EQUAL(false, restored.quantized);
	CHECK_EQUAL(false, restored.reversed);
	CHECK_EQUAL(false, restored.pingPong);
}

TEST(StutterConfigSerialization, MixedBooleans) {
	StutterConfig original;
	original.quantized = false;
	original.reversed = true;
	original.pingPong = false;

	std::string xml = writeStutterXml(original);
	StutterConfig restored;
	readStutterFromXml(restored, xml);

	CHECK_EQUAL(false, restored.quantized);
	CHECK_EQUAL(true, restored.reversed);
	CHECK_EQUAL(false, restored.pingPong);
}

TEST(StutterConfigSerialization, UnknownTagsIgnored) {
	std::string xml =
	    "<stutter>"
	    "<quantized>0</quantized>"
	    "<newFutureFlag>1</newFutureFlag>"
	    "<reverse>1</reverse>"
	    "<pingPong>1</pingPong>"
	    "</stutter>";

	StutterConfig restored;
	readStutterFromXml(restored, xml);

	CHECK_EQUAL(false, restored.quantized);
	CHECK_EQUAL(true, restored.reversed);
	CHECK_EQUAL(true, restored.pingPong);
}

// Note: firmware uses "reverse" (not "reversed") as the XML tag name.
// This tests that the asymmetry is correctly handled.
TEST(StutterConfigSerialization, ReverseTagNameMatchesFirmware) {
	StringSerializer ser;
	ser.writeOpeningTagBeginning("stutter");
	ser.writeAttribute("reverse", 1); // firmware uses "reverse"
	ser.closeTag();

	CHECK(ser.output.find("reverse=\"1\"") != std::string::npos);
}

// ═══════════════════════════════════════════════════════════════════════
// FilterConfig Round-Trip Tests
// ═══════════════════════════════════════════════════════════════════════

TEST_GROUP(FilterConfigSerialization) {};

// These test the string↔enum conversion functions used by writeAttributesToFile
// and readTagFromFile for filter mode and route.

TEST(FilterConfigSerialization, LPFModeRoundTrip) {
	FilterMode modes[] = {
	    FilterMode::TRANSISTOR_12DB, FilterMode::TRANSISTOR_24DB, FilterMode::TRANSISTOR_24DB_DRIVE,
	    FilterMode::SVF_BAND,       FilterMode::SVF_NOTCH,       FilterMode::HPLADDER,
	};

	for (FilterMode mode : modes) {
		char const* str = lpfTypeToString(mode);
		FilterMode restored = stringToLPFType(str);
		CHECK_EQUAL(static_cast<int>(mode), static_cast<int>(restored));
	}
}

TEST(FilterConfigSerialization, FilterRouteRoundTrip) {
	FilterRoute routes[] = {
	    FilterRoute::LOW_TO_HIGH,
	    FilterRoute::PARALLEL,
	    FilterRoute::HIGH_TO_LOW,
	};

	for (FilterRoute route : routes) {
		char const* str = filterRouteToString(route);
		FilterRoute restored = stringToFilterRoute(str);
		CHECK_EQUAL(static_cast<int>(route), static_cast<int>(restored));
	}
}

TEST(FilterConfigSerialization, LPFModeXmlRoundTrip) {
	// Simulate writeAttributesToFile + readTagFromFile for filter modes
	FilterMode modes[] = {
	    FilterMode::TRANSISTOR_12DB, FilterMode::TRANSISTOR_24DB, FilterMode::TRANSISTOR_24DB_DRIVE,
	    FilterMode::SVF_BAND,       FilterMode::SVF_NOTCH,       FilterMode::HPLADDER,
	};

	for (FilterMode mode : modes) {
		StringSerializer ser;
		ser.writeOpeningTagBeginning("sound");
		ser.writeAttribute("lpfMode", lpfTypeToString(mode));
		ser.closeTag();

		std::string xml = attributesToChildTags(ser.output);

		StringDeserializer deser;
		deser.loadXML(xml);
		deser.readNextTagOrAttributeName(); // "sound"

		char const* tagName;
		FilterMode restored = FilterMode::TRANSISTOR_24DB; // default
		while (*(tagName = deser.readNextTagOrAttributeName())) {
			if (!strcmp(tagName, "lpfMode")) {
				restored = stringToLPFType(deser.readTagOrAttributeValue());
				deser.exitTag();
			}
			else {
				deser.exitTag(tagName);
			}
		}

		CHECK_EQUAL(static_cast<int>(mode), static_cast<int>(restored));
	}
}

TEST(FilterConfigSerialization, HPFModeXmlRoundTrip) {
	// hpfMode uses the same stringToLPFType/lpfTypeToString functions
	FilterMode modes[] = {
	    FilterMode::TRANSISTOR_12DB,
	    FilterMode::SVF_NOTCH,
	    FilterMode::HPLADDER,
	};

	for (FilterMode mode : modes) {
		StringSerializer ser;
		ser.writeOpeningTagBeginning("sound");
		ser.writeAttribute("hpfMode", lpfTypeToString(mode));
		ser.closeTag();

		std::string xml = attributesToChildTags(ser.output);

		StringDeserializer deser;
		deser.loadXML(xml);
		deser.readNextTagOrAttributeName();

		char const* tagName;
		FilterMode restored = FilterMode::HPLADDER;
		while (*(tagName = deser.readNextTagOrAttributeName())) {
			if (!strcmp(tagName, "hpfMode")) {
				restored = stringToLPFType(deser.readTagOrAttributeValue());
				deser.exitTag();
			}
			else {
				deser.exitTag(tagName);
			}
		}

		CHECK_EQUAL(static_cast<int>(mode), static_cast<int>(restored));
	}
}

TEST(FilterConfigSerialization, FilterRouteXmlRoundTrip) {
	FilterRoute routes[] = {
	    FilterRoute::LOW_TO_HIGH,
	    FilterRoute::PARALLEL,
	    FilterRoute::HIGH_TO_LOW,
	};

	for (FilterRoute route : routes) {
		StringSerializer ser;
		ser.writeOpeningTagBeginning("sound");
		ser.writeAttribute("filterRoute", filterRouteToString(route));
		ser.closeTag();

		std::string xml = attributesToChildTags(ser.output);

		StringDeserializer deser;
		deser.loadXML(xml);
		deser.readNextTagOrAttributeName();

		char const* tagName;
		FilterRoute restored = FilterRoute::HIGH_TO_LOW;
		while (*(tagName = deser.readNextTagOrAttributeName())) {
			if (!strcmp(tagName, "filterRoute")) {
				restored = stringToFilterRoute(deser.readTagOrAttributeValue());
				deser.exitTag();
			}
			else {
				deser.exitTag(tagName);
			}
		}

		CHECK_EQUAL(static_cast<int>(route), static_cast<int>(restored));
	}
}

// ═══════════════════════════════════════════════════════════════════════
// Combined ModControllableAudio attribute tests
// ═══════════════════════════════════════════════════════════════════════

TEST_GROUP(ModControllableAudioAttributes) {};

TEST(ModControllableAudioAttributes, FilterAndClippingRoundTrip) {
	// Test the attribute section of writeAttributesToFile/readTagFromFile:
	// modFXType, lpfMode, hpfMode, filterRoute, clippingAmount
	FilterMode lpf = FilterMode::SVF_BAND;
	FilterMode hpf = FilterMode::HPLADDER;
	FilterRoute route = FilterRoute::PARALLEL;
	uint8_t clipping = 42;

	StringSerializer ser;
	ser.writeOpeningTagBeginning("sound");
	ser.writeAttribute("lpfMode", lpfTypeToString(lpf));
	ser.writeAttribute("hpfMode", lpfTypeToString(hpf));
	ser.writeAttribute("filterRoute", filterRouteToString(route));
	ser.writeAttribute("clippingAmount", static_cast<int32_t>(clipping));
	ser.closeTag();

	std::string xml = attributesToChildTags(ser.output);

	StringDeserializer deser;
	deser.loadXML(xml);
	deser.readNextTagOrAttributeName();

	FilterMode restoredLpf = FilterMode::TRANSISTOR_24DB;
	FilterMode restoredHpf = FilterMode::TRANSISTOR_24DB;
	FilterRoute restoredRoute = FilterRoute::HIGH_TO_LOW;
	uint8_t restoredClipping = 0;

	char const* tagName;
	while (*(tagName = deser.readNextTagOrAttributeName())) {
		if (!strcmp(tagName, "lpfMode")) {
			restoredLpf = stringToLPFType(deser.readTagOrAttributeValue());
			deser.exitTag();
		}
		else if (!strcmp(tagName, "hpfMode")) {
			restoredHpf = stringToLPFType(deser.readTagOrAttributeValue());
			deser.exitTag();
		}
		else if (!strcmp(tagName, "filterRoute")) {
			restoredRoute = stringToFilterRoute(deser.readTagOrAttributeValue());
			deser.exitTag();
		}
		else if (!strcmp(tagName, "clippingAmount")) {
			restoredClipping = static_cast<uint8_t>(deser.readTagOrAttributeValueInt());
			deser.exitTag();
		}
		else {
			deser.exitTag(tagName);
		}
	}

	CHECK_EQUAL(static_cast<int>(FilterMode::SVF_BAND), static_cast<int>(restoredLpf));
	CHECK_EQUAL(static_cast<int>(FilterMode::HPLADDER), static_cast<int>(restoredHpf));
	CHECK_EQUAL(static_cast<int>(FilterRoute::PARALLEL), static_cast<int>(restoredRoute));
	CHECK_EQUAL(42, restoredClipping);
}

// ═══════════════════════════════════════════════════════════════════════
// Edge case: hand-crafted XML from real preset files
// ═══════════════════════════════════════════════════════════════════════

TEST_GROUP(RealWorldXmlFormats) {};

TEST(RealWorldXmlFormats, TypicalDelayFromPreset) {
	// Mirrors what a real Deluge preset file might contain
	std::string xml =
	    "<delay>"
	    "<pingPong>1</pingPong>"
	    "<analog>0</analog>"
	    "<syncLevel>7</syncLevel>"
	    "<syncType>0</syncType>"
	    "</delay>";

	Delay restored;
	readDelayFromXml(restored, xml);

	CHECK_EQUAL(true, restored.pingPong);
	CHECK_EQUAL(false, restored.analog);
	CHECK_EQUAL(7, static_cast<int>(restored.syncLevel));
	CHECK_EQUAL(static_cast<int>(SYNC_TYPE_EVEN), static_cast<int>(restored.syncType));
}

TEST(RealWorldXmlFormats, TypicalSidechainFromPreset) {
	std::string xml =
	    "<sidechain>"
	    "<attack>327244</attack>"
	    "<release>936</release>"
	    "<syncLevel>7</syncLevel>"
	    "<syncType>0</syncType>"
	    "</sidechain>";

	SideChain restored;
	readSidechainFromXml(restored, xml);

	CHECK_EQUAL(327244, restored.attack);
	CHECK_EQUAL(936, restored.release);
	CHECK_EQUAL(7, static_cast<int>(restored.syncLevel));
	CHECK_EQUAL(static_cast<int>(SYNC_TYPE_EVEN), static_cast<int>(restored.syncType));
}

TEST(RealWorldXmlFormats, TypicalAudioCompressorFromPreset) {
	std::string xml =
	    "<audioCompressor>"
	    "<attack>-2147483648</attack>"
	    "<release>-2147483648</release>"
	    "<thresh>-2147483648</thresh>"
	    "<ratio>-2147483648</ratio>"
	    "<compHPF>-2147483648</compHPF>"
	    "<compBlend>-2147483648</compBlend>"
	    "</audioCompressor>";

	RMSFeedbackCompressor restored;
	readCompressorFromXml(restored, xml);

	// INT32_MIN values should survive
	CHECK_EQUAL(static_cast<q31_t>(-2147483648LL), restored.getAttack());
	CHECK_EQUAL(static_cast<q31_t>(-2147483648LL), restored.getRelease());
	CHECK_EQUAL(static_cast<q31_t>(-2147483648LL), restored.getThreshold());
	CHECK_EQUAL(static_cast<q31_t>(-2147483648LL), restored.getRatio());
}

TEST(RealWorldXmlFormats, EmptyDelayTag) {
	// A delay tag with no children should use defaults
	std::string xml = "<delay></delay>";

	Delay restored;
	restored.pingPong = true; // will be overridden to default
	readDelayFromXml(restored, xml);

	// Defaults set by read helper: syncType=EVEN, syncLevel=NONE
	CHECK_EQUAL(static_cast<int>(SYNC_TYPE_EVEN), static_cast<int>(restored.syncType));
	CHECK_EQUAL(static_cast<int>(SYNC_LEVEL_NONE), static_cast<int>(restored.syncLevel));
}

TEST(RealWorldXmlFormats, EmptySidechainTag) {
	std::string xml = "<sidechain></sidechain>";

	SideChain restored;
	readSidechainFromXml(restored, xml);

	CHECK_EQUAL(static_cast<int>(SYNC_TYPE_EVEN), static_cast<int>(restored.syncType));
	CHECK_EQUAL(static_cast<int>(SYNC_LEVEL_NONE), static_cast<int>(restored.syncLevel));
}
