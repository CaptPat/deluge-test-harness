#include "CppUTest/TestHarness.h"
#include "definitions_cxx.hpp"
#include "model/mod_controllable/filters/filter_config.h"
#include <cstring>
#include <type_traits>

// Helper to compare enum classes with CHECK_EQUAL
#define CHECK_ENUM_EQUAL(expected, actual) \
	CHECK_EQUAL(static_cast<std::underlying_type_t<decltype(expected)>>(expected), \
	            static_cast<std::underlying_type_t<decltype(actual)>>(actual))

// Declared in filter_config.cpp
extern char const* filterRouteToString(FilterRoute route);
extern FilterRoute stringToFilterRoute(char const* string);
extern FilterMode stringToLPFType(char const* string);
extern char const* lpfTypeToString(FilterMode mode);

TEST_GROUP(FilterConfigTest){};

TEST(FilterConfigTest, lpfToString24dB) {
	STRCMP_EQUAL("24dB", lpfTypeToString(FilterMode::TRANSISTOR_24DB));
}

TEST(FilterConfigTest, stringToLpf24dB) {
	CHECK_ENUM_EQUAL(FilterMode::TRANSISTOR_24DB, stringToLPFType("24dB"));
}

TEST(FilterConfigTest, lpfToString12dB) {
	STRCMP_EQUAL("12dB", lpfTypeToString(FilterMode::TRANSISTOR_12DB));
}

TEST(FilterConfigTest, stringToLpf12dB) {
	CHECK_ENUM_EQUAL(FilterMode::TRANSISTOR_12DB, stringToLPFType("12dB"));
}

TEST(FilterConfigTest, lpfToStringDrive) {
	STRCMP_EQUAL("24dBDrive", lpfTypeToString(FilterMode::TRANSISTOR_24DB_DRIVE));
}

TEST(FilterConfigTest, lpfToStringSVFBand) {
	STRCMP_EQUAL("SVF_Band", lpfTypeToString(FilterMode::SVF_BAND));
}

TEST(FilterConfigTest, lpfToStringHPLadder) {
	STRCMP_EQUAL("HPLadder", lpfTypeToString(FilterMode::HPLADDER));
}

TEST(FilterConfigTest, routeToStringH2L) {
	STRCMP_EQUAL("H2L", filterRouteToString(FilterRoute::HIGH_TO_LOW));
}

TEST(FilterConfigTest, stringToRouteH2L) {
	CHECK_ENUM_EQUAL(FilterRoute::HIGH_TO_LOW, stringToFilterRoute("H2L"));
}

TEST(FilterConfigTest, routeToStringParallel) {
	STRCMP_EQUAL("PARA", filterRouteToString(FilterRoute::PARALLEL));
}

TEST(FilterConfigTest, stringToRouteParallel) {
	CHECK_ENUM_EQUAL(FilterRoute::PARALLEL, stringToFilterRoute("PARA"));
}

TEST(FilterConfigTest, routeToStringL2H) {
	STRCMP_EQUAL("L2H", filterRouteToString(FilterRoute::LOW_TO_HIGH));
}

TEST(FilterConfigTest, stringToRouteL2H) {
	CHECK_ENUM_EQUAL(FilterRoute::LOW_TO_HIGH, stringToFilterRoute("L2H"));
}
