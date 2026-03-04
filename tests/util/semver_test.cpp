#include "CppUTest/TestHarness.h"
#include "util/semver.h"

TEST_GROUP(SemVerTest) {};

// --- Parsing ---

TEST(SemVerTest, parseSimple) {
	auto result = SemVer::parse("1.2.3");
	CHECK(result.has_value());
	CHECK_EQUAL(1, result->major);
	CHECK_EQUAL(2, result->minor);
	CHECK_EQUAL(3, result->patch);
	CHECK(result->pre_release.empty());
}

TEST(SemVerTest, parseWithPreRelease) {
	auto result = SemVer::parse("1.2.3-beta");
	CHECK(result.has_value());
	CHECK_EQUAL(1, result->major);
	CHECK_EQUAL(2, result->minor);
	CHECK_EQUAL(3, result->patch);
	STRCMP_EQUAL("beta", std::string(result->pre_release).c_str());
}

TEST(SemVerTest, parseWithDottedPreRelease) {
	auto result = SemVer::parse("1.2.3-alpha.1");
	CHECK(result.has_value());
	STRCMP_EQUAL("alpha.1", std::string(result->pre_release).c_str());
}

TEST(SemVerTest, parseLargeVersions) {
	auto result = SemVer::parse("255.255.255");
	CHECK(result.has_value());
	CHECK_EQUAL(255, result->major);
	CHECK_EQUAL(255, result->minor);
	CHECK_EQUAL(255, result->patch);
}

TEST(SemVerTest, parseOverflow) {
	auto result = SemVer::parse("256.0.0");
	CHECK(!result.has_value());
	CHECK_EQUAL((int)SemVer::Parser::Error::INVALID_NUMBER, (int)result.error());
}

TEST(SemVerTest, parseMissingDot) {
	auto result = SemVer::parse("1.2");
	CHECK(!result.has_value());
	CHECK_EQUAL((int)SemVer::Parser::Error::END_OF_STREAM, (int)result.error());
}

TEST(SemVerTest, parseEmpty) {
	auto result = SemVer::parse("");
	CHECK(!result.has_value());
	CHECK_EQUAL((int)SemVer::Parser::Error::INVALID_NUMBER, (int)result.error());
}

TEST(SemVerTest, parseGarbage) {
	auto result = SemVer::parse("abc");
	CHECK(!result.has_value());
	CHECK_EQUAL((int)SemVer::Parser::Error::INVALID_NUMBER, (int)result.error());
}

TEST(SemVerTest, parseZeroVersion) {
	auto result = SemVer::parse("0.0.0");
	CHECK(result.has_value());
	CHECK_EQUAL(0, result->major);
	CHECK_EQUAL(0, result->minor);
	CHECK_EQUAL(0, result->patch);
}

// --- Comparison ---

TEST(SemVerTest, compareEqual) {
	SemVer a{1, 2, 3};
	SemVer b{1, 2, 3};
	CHECK(a == b);
	CHECK((a <=> b) == std::strong_ordering::equal);
}

TEST(SemVerTest, compareLessPatch) {
	SemVer a{1, 2, 3};
	SemVer b{1, 2, 4};
	CHECK(a < b);
}

TEST(SemVerTest, compareLessMinor) {
	SemVer a{1, 2, 3};
	SemVer b{1, 3, 0};
	CHECK(a < b);
}

TEST(SemVerTest, compareLessMajor) {
	SemVer a{1, 2, 3};
	SemVer b{2, 0, 0};
	CHECK(a < b);
}

TEST(SemVerTest, preReleaseIsLessThanRelease) {
	SemVer release{1, 2, 3};
	SemVer preRelease{1, 2, 3, "beta"};
	CHECK(preRelease < release);
}

TEST(SemVerTest, preReleaseCompareLexicographic) {
	SemVer alpha{1, 2, 3, "alpha"};
	SemVer beta{1, 2, 3, "beta"};
	CHECK(alpha < beta);
}

TEST(SemVerTest, bothEmptyPreReleaseEqual) {
	SemVer a{1, 0, 0};
	SemVer b{1, 0, 0};
	CHECK((a <=> b) == std::strong_ordering::equal);
}
