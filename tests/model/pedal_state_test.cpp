// PedalState bitfield enum tests for PR #4365.
// Tests the bitwise operators on the PedalState enum used by Voice
// to track sustain/sostenuto pedal state.

#include "CppUTest/TestHarness.h"
#include <cstdint>

// Copied from voice.h — PedalState is defined inside the Voice class,
// which can't be included due to deep DSP dependencies. The enum and
// operators are self-contained value types, so testing a copy is valid.
namespace {

enum class PedalState : uint8_t {
	None = 0,
	SustainDeferred = 1 << 0,  // note-off deferred by CC64 sustain
	SostenutoCapture = 1 << 1, // voice captured by CC66 pedal-down
	SostenutoDeferred = 1 << 2 // note-off deferred by CC66 sostenuto
};

constexpr PedalState operator|(PedalState a, PedalState b) {
	return static_cast<PedalState>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}
constexpr PedalState operator&(PedalState a, PedalState b) {
	return static_cast<PedalState>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}
constexpr PedalState operator~(PedalState a) {
	return static_cast<PedalState>(~static_cast<uint8_t>(a));
}

bool hasFlag(PedalState state, PedalState flag) {
	return (state & flag) != PedalState::None;
}

} // namespace

TEST_GROUP(PedalStateTest) {};

// ── Basic flag values ────────────────────────────────────────────────────────

TEST(PedalStateTest, noneIsZero) {
	CHECK_EQUAL(0, static_cast<uint8_t>(PedalState::None));
}

TEST(PedalStateTest, sustainDeferredIsBit0) {
	CHECK_EQUAL(1, static_cast<uint8_t>(PedalState::SustainDeferred));
}

TEST(PedalStateTest, sostenutoCaptureIsBit1) {
	CHECK_EQUAL(2, static_cast<uint8_t>(PedalState::SostenutoCapture));
}

TEST(PedalStateTest, sostenutoDeferredIsBit2) {
	CHECK_EQUAL(4, static_cast<uint8_t>(PedalState::SostenutoDeferred));
}

// ── OR combines flags ────────────────────────────────────────────────────────

TEST(PedalStateTest, orCombinesFlags) {
	PedalState combined = PedalState::SustainDeferred | PedalState::SostenutoCapture;
	CHECK_EQUAL(3, static_cast<uint8_t>(combined));
}

TEST(PedalStateTest, orWithNoneIsIdentity) {
	PedalState result = PedalState::SustainDeferred | PedalState::None;
	CHECK_EQUAL(static_cast<uint8_t>(PedalState::SustainDeferred), static_cast<uint8_t>(result));
}

TEST(PedalStateTest, allThreeFlags) {
	PedalState all = PedalState::SustainDeferred | PedalState::SostenutoCapture | PedalState::SostenutoDeferred;
	CHECK_EQUAL(7, static_cast<uint8_t>(all));
}

// ── AND tests flags ──────────────────────────────────────────────────────────

TEST(PedalStateTest, andTestsPresence) {
	PedalState state = PedalState::SustainDeferred | PedalState::SostenutoCapture;
	CHECK_TRUE(hasFlag(state, PedalState::SustainDeferred));
	CHECK_TRUE(hasFlag(state, PedalState::SostenutoCapture));
	CHECK_FALSE(hasFlag(state, PedalState::SostenutoDeferred));
}

TEST(PedalStateTest, andWithNoneIsNone) {
	PedalState result = PedalState::SustainDeferred & PedalState::None;
	CHECK_EQUAL(0, static_cast<uint8_t>(result));
}

TEST(PedalStateTest, independentFlagsDontOverlap) {
	PedalState result = PedalState::SustainDeferred & PedalState::SostenutoCapture;
	CHECK_EQUAL(0, static_cast<uint8_t>(result));
}

// ── NOT clears flags ─────────────────────────────────────────────────────────

TEST(PedalStateTest, clearSustainKeepsSostenuto) {
	PedalState state = PedalState::SustainDeferred | PedalState::SostenutoDeferred;
	PedalState cleared = state & ~PedalState::SustainDeferred;
	CHECK_FALSE(hasFlag(cleared, PedalState::SustainDeferred));
	CHECK_TRUE(hasFlag(cleared, PedalState::SostenutoDeferred));
}

TEST(PedalStateTest, clearSostenutoKeepsSustain) {
	PedalState state = PedalState::SustainDeferred | PedalState::SostenutoCapture;
	PedalState cleared = state & ~PedalState::SostenutoCapture;
	CHECK_TRUE(hasFlag(cleared, PedalState::SustainDeferred));
	CHECK_FALSE(hasFlag(cleared, PedalState::SostenutoCapture));
}

// ── Typical voice lifecycle scenarios ────────────────────────────────────────

TEST(PedalStateTest, noteOnResetsToNone) {
	PedalState state = PedalState::SustainDeferred | PedalState::SostenutoCapture;
	state = PedalState::None; // simulates noteOn
	CHECK_EQUAL(0, static_cast<uint8_t>(state));
}

TEST(PedalStateTest, sustainPedalDefersThenRelease) {
	PedalState state = PedalState::None;
	// noteOff while sustain held → defer
	state = state | PedalState::SustainDeferred;
	CHECK_TRUE(hasFlag(state, PedalState::SustainDeferred));
	// sustain pedal released → clear flag
	state = state & ~PedalState::SustainDeferred;
	CHECK_FALSE(hasFlag(state, PedalState::SustainDeferred));
}

TEST(PedalStateTest, sostenutoCaptureAndDeferCombine) {
	PedalState state = PedalState::None;
	// sostenuto pedal down → capture
	state = state | PedalState::SostenutoCapture;
	// noteOff while sostenuto held → also defer
	state = state | PedalState::SostenutoDeferred;
	CHECK_TRUE(hasFlag(state, PedalState::SostenutoCapture));
	CHECK_TRUE(hasFlag(state, PedalState::SostenutoDeferred));
	// sostenuto released → clear both
	state = state & ~(PedalState::SostenutoCapture | PedalState::SostenutoDeferred);
	CHECK_FALSE(hasFlag(state, PedalState::SostenutoCapture));
	CHECK_FALSE(hasFlag(state, PedalState::SostenutoDeferred));
}

TEST(PedalStateTest, sustainAndSostenutoCombine) {
	PedalState state = PedalState::None;
	// Both pedals active
	state = state | PedalState::SustainDeferred | PedalState::SostenutoCapture | PedalState::SostenutoDeferred;
	CHECK_TRUE(hasFlag(state, PedalState::SustainDeferred));
	CHECK_TRUE(hasFlag(state, PedalState::SostenutoCapture));
	CHECK_TRUE(hasFlag(state, PedalState::SostenutoDeferred));
	// Release sustain only
	state = state & ~PedalState::SustainDeferred;
	CHECK_FALSE(hasFlag(state, PedalState::SustainDeferred));
	CHECK_TRUE(hasFlag(state, PedalState::SostenutoCapture));
	CHECK_TRUE(hasFlag(state, PedalState::SostenutoDeferred));
}
