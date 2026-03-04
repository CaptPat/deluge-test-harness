#include "test_fixture.h"

TestFixture::TestFixture() { resetAll(); }

void TestFixture::resetAll() {
	MockButtons::reset();
	MockMatrixDriver::reset();
	MockEncoders::reset();
	MockMIDIEngine::reset();
	MockAudioEngine::reset();
	MockUI::reset();
	testSong.clear();
	testDisplay.lastText.clear();
}

// ── Time ────────────────────────────────────────────────────────────────
void TestFixture::advance(double seconds) { passMockTime(seconds); }

// ── Button input ────────────────────────────────────────────────────────
void TestFixture::pressButton(uint8_t button) { MockButtons::press(button); }

void TestFixture::releaseButton(uint8_t button) { MockButtons::release(button); }

void TestFixture::tapButton(uint8_t button) {
	pressButton(button);
	advance(0.05); // 50ms press duration
	releaseButton(button);
}

bool TestFixture::isButtonPressed(uint8_t button) const { return MockButtons::isPressed(button); }

// ── Pad input ───────────────────────────────────────────────────────────
void TestFixture::pressPad(int32_t x, int32_t y, int32_t velocity) {
	MockMatrixDriver::pressPad(x, y, velocity);
}

void TestFixture::releasePad(int32_t x, int32_t y) { MockMatrixDriver::releasePad(x, y); }

bool TestFixture::isPadPressed(int32_t x, int32_t y) const { return MockMatrixDriver::isPadPressed(x, y); }

// ── Encoder input ───────────────────────────────────────────────────────
void TestFixture::turnEncoder(int32_t encoderIndex, int32_t delta) {
	MockEncoders::turn(encoderIndex, delta);
}

// ── MIDI input ──────────────────────────────────────────────────────────
void TestFixture::sendMIDINoteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
	MockMIDIEngine::injectInbound(0x90 | (channel & 0x0F), note, velocity);
}

void TestFixture::sendMIDINoteOff(uint8_t channel, uint8_t note) {
	MockMIDIEngine::injectInbound(0x80 | (channel & 0x0F), note, 0);
}

void TestFixture::sendMIDICC(uint8_t channel, uint8_t cc, uint8_t value) {
	MockMIDIEngine::injectInbound(0xB0 | (channel & 0x0F), cc, value);
}

// ── State queries ───────────────────────────────────────────────────────
std::string TestFixture::getDisplayText() const { return testDisplay.lastText; }

bool TestFixture::hasMIDIOutput() const { return MockMIDIEngine::hasSentMessages(); }

MockMIDIMessage TestFixture::getLastMIDIOutput() const { return MockMIDIEngine::getLastSent(); }

int32_t TestFixture::getMIDIOutputCount() const {
	return static_cast<int32_t>(MockMIDIEngine::sentMessages.size());
}

MockUI::ViewType TestFixture::getCurrentView() const { return MockUI::getView(); }

// ── Song state ──────────────────────────────────────────────────────────
Song& TestFixture::getSong() { return testSong; }
