// Filter morph label tests for PR #4335.
// Tests that SpecificFilter::getFamily() correctly identifies ladder vs SVF modes,
// which determines whether morph params display as "drive"/"FM" or "morph".

#include "CppUTest/TestHarness.h"
#include "definitions_cxx.hpp"
#include "model/mod_controllable/filters/filter_config.h"

using deluge::dsp::filter::FilterFamily;
using deluge::dsp::filter::SpecificFilter;

#define CHECK_FAMILY(expected, mode) \
	CHECK_EQUAL(static_cast<int>(expected), static_cast<int>(SpecificFilter(mode).getFamily()))

TEST_GROUP(FilterMorphLabel) {};

// ── LPF modes: ladder family → "drive" label ────────────────────────────────

TEST(FilterMorphLabel, lpf24dbIsLadder) {
	CHECK_FAMILY(FilterFamily::LP_LADDER, FilterMode::TRANSISTOR_24DB);
}

TEST(FilterMorphLabel, lpf12dbIsLadder) {
	CHECK_FAMILY(FilterFamily::LP_LADDER, FilterMode::TRANSISTOR_12DB);
}

TEST(FilterMorphLabel, lpfDriveIsLadder) {
	CHECK_FAMILY(FilterFamily::LP_LADDER, FilterMode::TRANSISTOR_24DB_DRIVE);
}

// ── LPF modes: SVF family → standard "morph" label ──────────────────────────

TEST(FilterMorphLabel, lpfSvfBandIsSvf) {
	CHECK_FAMILY(FilterFamily::SVF, FilterMode::SVF_BAND);
}

TEST(FilterMorphLabel, lpfSvfNotchIsSvf) {
	CHECK_FAMILY(FilterFamily::SVF, FilterMode::SVF_NOTCH);
}

// ── HPF modes: ladder family → "FM" label ────────────────────────────────────

TEST(FilterMorphLabel, hpfLadderIsHpLadder) {
	CHECK_FAMILY(FilterFamily::HP_LADDER, FilterMode::HPLADDER);
}

// ── HPF modes: SVF family → standard "morph" label ──────────────────────────

TEST(FilterMorphLabel, hpfSvfBandIsSvf) {
	// SVF_BAND is used as both LPF and HPF mode
	CHECK_FAMILY(FilterFamily::SVF, FilterMode::SVF_BAND);
}

// ── Label selection logic ────────────────────────────────────────────────────
// The actual label resolution in param.cpp is:
//   if paramID == LOCAL_LPF_MORPH && family == LP_LADDER → "LPF drive"
//   if paramID == LOCAL_HPF_MORPH && family == HP_LADDER → "HPF FM"
//   otherwise → standard morph name
//
// We can't call the overloaded getParamDisplayName(Kind, int32_t, ModControllableAudio*)
// because ModControllableAudio requires too many dependencies to construct.
// Instead we verify the family classification that drives the label decision.

TEST(FilterMorphLabel, ladderModesProduceDriveLabel) {
	// All LP_LADDER modes should trigger the "drive" label path
	FilterMode ladderModes[] = {FilterMode::TRANSISTOR_24DB, FilterMode::TRANSISTOR_12DB,
	                            FilterMode::TRANSISTOR_24DB_DRIVE};
	for (auto mode : ladderModes) {
		CHECK_EQUAL(static_cast<int>(FilterFamily::LP_LADDER),
		            static_cast<int>(SpecificFilter(mode).getFamily()));
	}
}

TEST(FilterMorphLabel, svfModesDoNotTriggerDriveLabel) {
	// SVF modes should NOT trigger the "drive" label path
	FilterMode svfModes[] = {FilterMode::SVF_BAND, FilterMode::SVF_NOTCH};
	for (auto mode : svfModes) {
		CHECK(static_cast<int>(SpecificFilter(mode).getFamily()) != static_cast<int>(FilterFamily::LP_LADDER));
	}
}

TEST(FilterMorphLabel, hpLadderTriggersHpfFmLabel) {
	CHECK_FAMILY(FilterFamily::HP_LADDER, FilterMode::HPLADDER);
}

TEST(FilterMorphLabel, svfBandDoesNotTriggerHpfFmLabel) {
	CHECK(static_cast<int>(SpecificFilter(FilterMode::SVF_BAND).getFamily()) != static_cast<int>(FilterFamily::HP_LADDER));
}
