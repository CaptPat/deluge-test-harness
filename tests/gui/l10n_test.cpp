#include "CppUTest/TestHarness.h"
#include "gui/l10n/l10n.h"
#include "gui/l10n/language.h"
#include "gui/l10n/strings.h"

// Avoid "using namespace deluge::l10n" — String conflicts with class String from d_string.h
namespace l = deluge::l10n;

TEST_GROUP(L10NTest) {
	void setup() override {
		l::chosenLanguage = &l::built_in::english;
	}
};

TEST(L10NTest, englishBasicString) {
	auto result = l::built_in::english.get(l::String::STRING_FOR_ERROR_SD_CARD);
	CHECK(result.has_value());
	STRCMP_EQUAL("SD card error", result.value().data());
}

TEST(L10NTest, englishEmptyString) {
	auto result = l::built_in::english.get(l::String::EMPTY_STRING);
	CHECK(result.has_value());
	STRCMP_EQUAL("", result.value().data());
}

TEST(L10NTest, sevenSegBasicString) {
	auto result = l::built_in::seven_segment.get(l::String::STRING_FOR_ERROR_SD_CARD);
	CHECK(result.has_value());
	STRCMP_EQUAL("CARD", result.value().data());
}

TEST(L10NTest, getViewEnglish) {
	std::string_view view = l::getView(l::built_in::english, l::String::STRING_FOR_ERROR_BUG);
	STRCMP_EQUAL("Bug encountered", view.data());
}

TEST(L10NTest, getDefaultLanguage) {
	const char* str = l::get(l::String::STRING_FOR_ERROR_FILE_NOT_FOUND);
	STRCMP_EQUAL("File not found", str);
}

extern "C" char const* l10n_get(size_t s);

TEST(L10NTest, cLinkageGet) {
	const char* str = l10n_get(static_cast<size_t>(l::String::STRING_FOR_ERROR_BUG));
	STRCMP_EQUAL("Bug encountered", str);
}

TEST(L10NTest, kNumStringsIsPositive) {
	CHECK(l::kNumStrings > 0);
	CHECK(l::kNumStrings == static_cast<size_t>(l::String::STRING_LAST));
}

TEST(L10NTest, languageName) {
	STRCMP_EQUAL("English", l::built_in::english.name().data());
	STRCMP_EQUAL("Seven Segment", l::built_in::seven_segment.name().data());
}
