# Deluge Test Harness

x86 regression test harness for [Synthstrom Deluge firmware](https://github.com/SynthstromAudible/DelugeFirmware). Compiles real firmware source files on the host with hardware boundaries replaced by mocks, enabling coverage-driven testing without target hardware.

## Scope

This harness provides **broad regression coverage** of Deluge firmware logic on x86. It compiles ~120 real firmware `.cpp` files against a mock hardware layer and exercises them with 1950+ CppUTest test cases. Development is guided by gcov line coverage and static analysis rather than any specific PR.

**What's tested:**

- **Model layer:** consequences, clip instances, scales, time signatures, iterance, pedal state, note vectors, param collections
- **Modulation core:** patcher (patch cable routing), param_set, param_manager, patch_cable_set, arpeggiator, envelope, LFO, sidechain
- **DSP:** granular processor, filters (SVF/LP/HP ladder), reverb (freeverb), delay, DX7/FM synthesis, ModFX, RMS compressor
- **Data structures:** resizeable arrays, ordered arrays, bidirectional linked lists, note vectors, CStringArray, named thing vectors
- **Utilities:** semver, firmware version, string operations, phase increment tuner, browser search, value scaling
- **HID:** encoder quadrature, button/pad mapping
- **Storage:** DX7 cartridge, multi-range, wave table holder
- **Monte Carlo stress tests:** real Q31 parameter values from community presets through ModFX/delay/reverb paths
- **Smoke tests:** lifecycle scenarios for delay, granular, and filter set

**What's NOT tested (and why):**

- UI/button interactions (need full view dispatch + hardware mocks we don't have)
- Voice allocation and audio rendering (Sound/Voice classes have deep DSP dependency chains)
- SD card / file I/O (need filesystem mock — FatFS dependency)
- These are validated by hardware testers on real devices

**Maintenance expectations:**

- Pure logic tests are zero-maintenance — they only break if the algorithm changes
- The mock/shadow layer (~120 compiled firmware files) needs periodic updates when upstream refactors headers or class hierarchies. Breakage is always compile errors, never silent failures.

## Architecture

```text
tests/                    Test files (CppUTest, 145+ test cases)
src/harness/              TestFixture — high-level test DSL
src/mocks/                Hardware mock layer + shadow headers
firmware/                 Git submodule → DelugeFirmware
scripts/                  Coverage reporting (gcov/gcovr)
```

The harness compiles firmware source with `IN_UNIT_TESTS=1`, replacing hardware drivers with mock implementations. Tests range from direct unit tests (construct a struct, check behavior) to integration-style tests using `TestFixture` which provides `pressButton()`, `sendMIDINoteOn()`, `advance()`, etc.

### Mock Layer

Two patterns:

- **Namespace mocks** (`MockAudioEngine`, `MockMIDIEngine`, etc.): capture output, inject input, provide `reset()`
- **Shadow headers**: minimal replacements for firmware headers that pull in too many dependencies (e.g., `Sound`, `Voice`, `Clip`)

### Coverage

```bash
# Generate HTML coverage report (requires gcovr)
bash scripts/coverage.sh

# CSV summary only
bash scripts/coverage.sh --csv
```

Coverage data (`coverage_summary.csv`) is tracked in the repo to monitor progress over time.

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

## Firmware Bugs Found

This harness has identified several bugs in the upstream firmware. See [bug_reports.md](bug_reports.md) for details and resolution status.

## Known Limitations

- **MinGW process-exit segfault**: On MinGW/GCC, the test executable segfaults during static destruction after all tests have passed. This is caused by `GeneralMemoryAllocator::get()` using an inline static local variable, which creates duplicate instances across static library boundaries (`.a` files). The shadow header (`src/mocks/memory/general_memory_allocator.h`) moves `get()` out-of-line to mitigate this during test execution, but the process-exit teardown order still triggers a crash. **All tests run and pass correctly** — the segfault is cosmetic. CTest reports it as a failure; check the test output for the actual pass/fail count.
- `strcmpspecial` mock omits note-name ordering (only affects file browser sort)
- `QuickSorter`/`OpenAddressingHashTable` use `(uint32_t)memory` pointer casts — compile-only on x86-64, no runtime tests
- `d_string.cpp` truncates 64-bit pointers — wrapped, not tested directly
- Remaining blockers for further coverage: `instrument_clip.h` (God Object), `gui/views/*`, `argon.hpp`, NE10/NEON, `fatfs/ff.h`

## License

Test harness code is MIT. The firmware submodule is GPL v3 (see firmware/LICENSE).
