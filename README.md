# Deluge Test Harness

x86 test harness for [Synthstrom Deluge firmware](https://github.com/SynthstromAudible/DelugeFirmware). Compiles real firmware source files on the host with hardware boundaries replaced by mocks.

## Scope

This harness tests **our contributed PRs** against the upstream firmware. It is not a comprehensive test suite for the entire firmware — upstream has its own CI (x86 unit tests, QEMU ARM tests, clang-tidy).

**What's tested:**
- Pure logic: tick scaling, time signatures, search algorithms, float round-trips, bitfield enums, filter classification
- Data structures: note vectors, param collections, patch cables, MIDI containers, clip iterators
- DSP smoke tests: granular processor, filters, reverb, delay (parameter sweeps, sync branches, no crash verification)
- Model layer: consequences, clip instances, scales, envelopes, ModFX, chords, presets
- Monte Carlo stress tests: real Q31 parameter values from community presets through ModFX/delay/reverb paths

**What's NOT tested (and why):**
- UI/button interactions (need full view dispatch + hardware mocks we don't have)
- Voice allocation and audio rendering (Sound/Voice classes have deep DSP dependency chains)
- SD card / file I/O (need filesystem mock)
- These are validated by hardware testers on real devices

**Maintenance expectations:**
- Pure logic tests are zero-maintenance — they only break if the algorithm changes
- The mock/shadow layer (~150 compiled firmware files) needs periodic updates when upstream refactors headers or class hierarchies. Breakage is always compile errors, never silent failures. Typical effort: a few hours per release.

## Architecture

```
tests/                    Test files (CppUTest, 1308 tests)
src/harness/              TestFixture — high-level test DSL
src/mocks/                Hardware mock layer + shadow headers
firmware/                 Git submodule → DelugeFirmware (testing/all-fixes branch)
```

The harness compiles firmware source with `IN_UNIT_TESTS=1`, replacing hardware drivers with mock implementations. Tests range from direct unit tests (construct a struct, check behavior) to integration-style tests using `TestFixture` which provides `pressButton()`, `sendMIDINoteOn()`, `advance()`, etc.

### Mock Layer

Two patterns:
- **Namespace mocks** (`MockAudioEngine`, `MockMIDIEngine`, etc.): capture output, inject input, provide `reset()`
- **Shadow headers**: minimal replacements for firmware headers that pull in too many dependencies (e.g., `Sound`, `Voice`, `Clip`)

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
#include "model/time_signature.h"

TEST_GROUP(TimeSignatureTest) {};

TEST(TimeSignatureTest, fourFourBarLength) {
    TimeSignature ts{4, 4};
    CHECK_EQUAL(96, ts.barLengthInBaseTicks());
}
```

For tests needing hardware interaction:

```cpp
#include "test_fixture.h"

TEST_GROUP(MyFeature) {};

TEST(MyFeature, ButtonStartsPlayback) {
    TestFixture f;
    f.pressButton(PLAY);
    f.advance(0.1);
    CHECK(f.isPlaying());
}
```

## PR Coverage

| PR | What's Tested |
|----|---------------|
| #4330 | MIDI clock/transport toggle logic, flash storage defaults |
| #4335 | Filter family classification for morph label selection |
| #4340 | Envelope sustain-zero clamp |
| #4341 | Browser prefix search boundary conditions |
| #4342 | Reverb filter float→int round-trip |
| #4349 | NotesState const_iterator bounds |
| #4360 | Granular copy constructor null-buffer safety |
| #4362 | Granular destructor deallocation |
| #4365 | PedalState bitfield operations and voice lifecycle |
| #4367 | Tempo ratio accumulator scaling, ceiling division |
| #4368 | Time signature bar/beat calculations |

## License

Test harness code is MIT. The firmware submodule is GPL v3 (see firmware/LICENSE).
