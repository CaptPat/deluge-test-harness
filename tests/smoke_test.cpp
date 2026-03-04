#include "CppUTest/TestHarness.h"
#include "test_fixture.h"

extern "C" {
#include "RZA1/ostm/ostm.h"
}

// ─── Smoke tests: verify the harness infrastructure works ───────────────

TEST_GROUP(HarnessSmoke){};

TEST(HarnessSmoke, FixtureCreatesCleanState) {
	TestFixture f;

	CHECK_FALSE(f.isButtonPressed(0));
	CHECK_FALSE(f.isPadPressed(0, 0));
	CHECK_FALSE(f.hasMIDIOutput());
	CHECK_EQUAL("", f.getDisplayText());
	CHECK_EQUAL(0, f.getSong().sessionClips.getNumElements());
}

TEST(HarnessSmoke, ButtonPressAndRelease) {
	TestFixture f;

	f.pressButton(42);
	CHECK_TRUE(f.isButtonPressed(42));

	f.releaseButton(42);
	CHECK_FALSE(f.isButtonPressed(42));
}

TEST(HarnessSmoke, PadPressAndRelease) {
	TestFixture f;

	f.pressPad(3, 5, 100);
	CHECK_TRUE(f.isPadPressed(3, 5));
	CHECK_FALSE(f.isPadPressed(3, 4));

	f.releasePad(3, 5);
	CHECK_FALSE(f.isPadPressed(3, 5));
}

TEST(HarnessSmoke, MIDIInjectionAndCapture) {
	TestFixture f;

	CHECK_FALSE(f.hasMIDIOutput());

	// Simulate outbound MIDI (as if firmware sent it)
	MockMIDIEngine::sendMessage(0x90, 60, 127);

	CHECK_TRUE(f.hasMIDIOutput());
	CHECK_EQUAL(1, f.getMIDIOutputCount());

	auto msg = f.getLastMIDIOutput();
	CHECK_EQUAL(0x90, msg.status);
	CHECK_EQUAL(60, msg.data1);
	CHECK_EQUAL(127, msg.data2);
}

TEST(HarnessSmoke, TimeAdvances) {
	TestFixture f;

	uint32_t t0 = getTimerValue(0);
	f.advance(1.0); // 1 second
	uint32_t t1 = getTimerValue(0);

	CHECK(t1 > t0);
}

TEST(HarnessSmoke, SongClipManagement) {
	TestFixture f;

	Clip c1(1);
	Clip c2(2);
	f.getSong().sessionClips.push(&c1);
	f.getSong().sessionClips.push(&c2);

	CHECK_EQUAL(2, f.getSong().sessionClips.getNumElements());
	CHECK_EQUAL(1, f.getSong().sessionClips.getClipAtIndex(0)->id);
	CHECK_EQUAL(2, f.getSong().sessionClips.getClipAtIndex(1)->id);
}

TEST(HarnessSmoke, EncoderEventLogging) {
	TestFixture f;

	f.turnEncoder(0, 3);
	f.turnEncoder(1, -2);

	CHECK_EQUAL(2, (int)MockEncoders::eventLog.size());
	CHECK_EQUAL(0, MockEncoders::eventLog[0].encoderIndex);
	CHECK_EQUAL(3, MockEncoders::eventLog[0].delta);
	CHECK_EQUAL(1, MockEncoders::eventLog[1].encoderIndex);
	CHECK_EQUAL(-2, MockEncoders::eventLog[1].delta);
}
