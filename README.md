# Deluge Test Harness

High-level integration test harness for [Synthstrom Deluge firmware](https://github.com/SynthstromAudible/DelugeFirmware). Tests firmware logic by mocking hardware boundaries (display, buttons, pads, encoders, MIDI, audio DMA) and providing a simple DSL for writing regression tests.

## Architecture

```
tests/                    Test files (CppUTest)
src/harness/              TestFixture — high-level test DSL
src/mocks/                Hardware mock layer
firmware/                 Git submodule → SynthstromAudible/DelugeFirmware
```

The harness compiles firmware source files on x86 with `IN_UNIT_TESTS=1`, replacing hardware drivers with mock implementations. Tests interact through `TestFixture` which provides methods like `pressButton()`, `pressPad()`, `turnEncoder()`, `sendMIDINoteOn()`, and query methods like `getDisplayText()`, `hasMIDIOutput()`.

## Building

```bash
# Prerequisites: CMake 3.24+, Ninja, C++23 compiler (GCC 13+ or Clang 17+)
cmake -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

## Writing Tests

```cpp
#include "CppUTest/TestHarness.h"
#include "test_fixture.h"

TEST_GROUP(MyFeature) {};

TEST(MyFeature, ButtonStartsPlayback) {
    TestFixture f;
    f.pressButton(PLAY);
    f.advance(0.1);
    CHECK(f.isPlaying());
}

TEST(MyFeature, MIDINoteTriggersVoice) {
    TestFixture f;
    f.sendMIDINoteOn(0, 60, 127);
    f.advance(0.01);
    CHECK(f.hasMIDIOutput());
}
```

## Status

**Phase 1 — Scaffolding** (current): Mock layer + test fixture + smoke tests that verify the harness infrastructure itself works. No firmware source files are compiled yet.

**Phase 2 — Model integration**: Compile real firmware model layer (Song, Clip, NoteRow) against mocks. Test state management and clip operations.

**Phase 3 — Input routing**: Wire `TestFixture::pressButton()` through real `Buttons::buttonAction()` and view dispatch. Test UI navigation and button handling.

**Phase 4 — MIDI pipeline**: Wire `TestFixture::sendMIDINoteOn()` through real `MidiEngine` → `PlaybackHandler` → `Instrument`. Test MIDI routing, CC learning, sustain pedal.

**Phase 5 — Audio verification**: Capture rendered audio buffers. Verify voices allocate/release correctly, envelopes behave as expected.

## License

Test harness code is MIT. The firmware submodule is GPL v3 (see firmware/LICENSE).
