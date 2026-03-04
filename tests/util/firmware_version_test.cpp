#include "CppUTest/TestHarness.h"
#include "util/firmware_version.h"

TEST_GROUP(FirmwareVersionTest) {};

TEST(FirmwareVersionTest, parseCommunity) {
	auto fv = FirmwareVersion::parse("c1.2.3");
	CHECK_EQUAL((int)FirmwareVersion::Type::COMMUNITY, (int)fv.type());
	CHECK_EQUAL(1, fv.version().major);
	CHECK_EQUAL(2, fv.version().minor);
	CHECK_EQUAL(3, fv.version().patch);
}

TEST(FirmwareVersionTest, parseOfficial) {
	auto fv = FirmwareVersion::parse("4.1.0");
	CHECK_EQUAL((int)FirmwareVersion::Type::OFFICIAL, (int)fv.type());
	CHECK_EQUAL(4, fv.version().major);
	CHECK_EQUAL(1, fv.version().minor);
	CHECK_EQUAL(0, fv.version().patch);
}

TEST(FirmwareVersionTest, parseCommunityPreRelease) {
	auto fv = FirmwareVersion::parse("c1.0.0-beta");
	CHECK_EQUAL((int)FirmwareVersion::Type::COMMUNITY, (int)fv.type());
	CHECK_EQUAL(1, fv.version().major);
	STRCMP_EQUAL("beta", std::string(fv.version().pre_release).c_str());
}

TEST(FirmwareVersionTest, parseInvalid) {
	auto fv = FirmwareVersion::parse("xyz");
	CHECK_EQUAL((int)FirmwareVersion::Type::UNKNOWN, (int)fv.type());
	CHECK_EQUAL(0, fv.version().major);
	CHECK_EQUAL(0, fv.version().minor);
	CHECK_EQUAL(0, fv.version().patch);
}

TEST(FirmwareVersionTest, communityGreaterThanOfficial) {
	auto community = FirmwareVersion::community({1, 0, 0});
	auto official = FirmwareVersion::official({1, 0, 0});
	CHECK(community > official);
}

TEST(FirmwareVersionTest, officialFactory) {
	auto fv = FirmwareVersion::official({1, 2, 3});
	CHECK_EQUAL((int)FirmwareVersion::Type::OFFICIAL, (int)fv.type());
	CHECK_EQUAL(1, fv.version().major);
	CHECK_EQUAL(2, fv.version().minor);
	CHECK_EQUAL(3, fv.version().patch);
}

TEST(FirmwareVersionTest, communityFactory) {
	auto fv = FirmwareVersion::community({1, 2, 3});
	CHECK_EQUAL((int)FirmwareVersion::Type::COMMUNITY, (int)fv.type());
	CHECK_EQUAL(1, fv.version().major);
}

TEST(FirmwareVersionTest, currentVersion) {
	auto fv = FirmwareVersion::current();
	CHECK_EQUAL((int)FirmwareVersion::Type::COMMUNITY, (int)fv.type());
	CHECK_EQUAL(0, fv.version().major);
	CHECK_EQUAL(0, fv.version().minor);
	CHECK_EQUAL(0, fv.version().patch);
}
