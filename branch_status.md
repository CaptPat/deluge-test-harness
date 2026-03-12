<!-- markdownlint-disable MD060 -->
# Branch Status Tracking

## Terminology

| Term                 | Definition                                                                     |
| -------------------- | ------------------------------------------------------------------------------ |
| **upstream**         | `SynthstromAudible/DelugeFirmware:main` -- the canonical firmware              |
| **branch**           | Individual fix/feature branch on `CaptPat/DelugeFirmware`, rebased on upstream |
| **baseline**         | `CaptPat/DelugeFirmware:harness/baseline` -- upstream + all active branches    |
| **regression delta** | Tests that FAIL on upstream but PASS on baseline (proves fix still needed)     |

## Status legend

- **active** -- fix/feature not in upstream, still needed
- **no-op** -- branch has no net code change (should be dropped)
- **pr-submitted** -- PR open on upstream
- **merged** -- upstream accepted our fix (or equivalent)
- **obsolete** -- upstream fixed independently or refactored away the problem

## Testability legend

- **has-test** -- harness or upstream unit test exercises this fix
- **testable** -- core logic change; a regression test could be written
- **gui-only** -- changes only affect UI rendering/interaction; not testable in x86 harness

## Current regression delta

**Zero.** All 1602 tests that run on upstream also pass on upstream.
The 23 extra tests on baseline come from branch-only code (new features/APIs).

To verify a bugfix branch is still needed, either:

1. Write a regression test that fails on upstream (preferred)
2. Manually inspect upstream code for the specific bug pattern

---

## Bugfix branches (18)

| Branch                                         | Status     | Testability | What it fixes                                                                                                          | Harness test                                                                        | Upstream issue |
| ---------------------------------------------- | ---------- | ----------- | ---------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------- | -------------- |
| `bugfix/browser-long-press-back-loads-preset`  | active     | gui-only    | `exitUI()` calls `exitAction()` instead of `Browser::close()`, loading the previewed preset                            | none                                                                                | --             |
| `bugfix/browser-number-search`                 | active     | has-test    | Off-by-one in `setFileByFullPath` (`>` to `>=`), numeric prefix matching                                               | `tests/util/browser_search_test.cpp`                                                | --             |
| `bugfix/browser-text-search-last-item`         | active     | has-test    | Last item unfindable in browser text search; same off-by-one + `notFound` label placement                              | `tests/util/browser_search_test.cpp`                                                | --             |
| `bugfix/dx7-shift-name-shortcut`               | **no-op**  | --          | Fix was reverted in same branch (net zero diff)                                                                        | --                                                                                  | --             |
| `bugfix/envelope-sustain-zero-stuck`            | active     | has-test    | `smoothedSustain` goes negative when sustain=0, causing stuck notes                                                    | `tests/modulation/envelope_test.cpp`                                                | --             |
| `bugfix/grid-dim-pads-after-new-clip`           | active     | gui-only    | Missing `skipGreyoutFade()` before explode animation                                                                   | none                                                                                | --             |
| `bugfix/lpf-drive-label-display`                | active     | testable    | Filter morph labels show "morph" when should show "drive"/"FM" for ladder filters                                      | `tests/modulation/filter_morph_label_test.cpp` (partial -- family classification)   | --             |
| `bugfix/macro-keyboard-visual-feedback`         | active     | gui-only    | Non-active macro pads not dimmed in keyboard session column                                                             | none                                                                                | --             |
| `bugfix/metronome-countin-toggle`               | active     | testable    | Count-in plays metronome clicks even when metronome is off (missing guard)                                             | none                                                                                | --             |
| `bugfix/mod-matrix-fast-scroll`                 | active     | gui-only    | OLED menu scroll jumps (wrong scroll position calc)                                                                    | none                                                                                | --             |
| `bugfix/notes-state-const-iterator`             | active     | has-test    | `NotesState::end()` uses `notes.end() + count` instead of `notes.begin() + count`                                     | upstream unit test in branch                                                        | --             |
| `bugfix/patch-cables-fm-subtractive-switch`     | active     | testable    | `setSynthMode()` restores filter modes AFTER `setupPatchingForAllParamManagers()` -- stale `lpfMode=OFF`               | none (needs Sound mock)                                                             | #4232 (open)   |
| `bugfix/reverb-filter-encoder-jumps`            | active     | has-test    | Float truncation drift in reverb HPF/LPF `readCurrentValue()` (missing `std::round()`)                                | `tests/dsp/reverb_filter_roundtrip_test.cpp`                                        | --             |
| `bugfix/settings-menu-exit-crash`               | active     | gui-only    | Stale menu pointer deref after `exitCompletely()` changes active UI                                                    | none                                                                                | #3898, #2759   |
| `bugfix/song-browser-loop-fix`                  | active     | gui-only    | Song browser wrap-around broken when file list is windowed                                                              | none                                                                                | --             |
| `bugfix/song-mode-clip-stuck-after-launch`      | active     | gui-only    | `UI_MODE_CLIP_PRESSED_IN_SONG_VIEW` conflated with `UI_MODE_HOLDING_STATUS_PAD`                                       | none                                                                                | --             |
| `bugfix/velocity-view-multi-note-delete`        | active     | gui-only    | Velocity-head press wrongly enters multi-pad ramp mode instead of delete                                               | none                                                                                | --             |
| `bugfix/wavetable-mod-knob-overwrite`           | active     | gui-only    | Default mod knob mappings set on every sample swap, not just wavetable transition                                      | none                                                                                | --             |

## Feature branches (6)

| Branch                                    | Status | Testability | What it adds                                                              | Harness test                                 | Upstream issue |
| ----------------------------------------- | ------ | ----------- | ------------------------------------------------------------------------- | -------------------------------------------- | -------------- |
| `feature/midi-cc66-sostenuto-pedal`       | active | has-test    | CC64 sustain + CC66 sostenuto + CC67 soft pedal with `PedalState` bitfield | `tests/model/pedal_state_test.cpp`           | --             |
| `feature/midi-separate-clock-transport`   | active | has-test    | Separate MIDI clock output from transport messages                        | `tests/playback/midi_transport_toggle_test.cpp` | --          |
| `feature/per-clip-tempo-ratio`            | active | has-test    | Per-clip tempo ratio (Bresenham accumulator)                              | `tests/model/tempo_ratio_test.cpp`           | --             |
| `feature/per-clip-time-signature`         | active | has-test    | Per-clip `TimeSignature` struct for metronome                             | `tests/model/time_signature_test.cpp`        | --             |
| `feature/swap-select-tempo-encoders`      | active | gui-only    | Runtime toggle to swap select/tempo encoder identities                    | none                                         | --             |
| `fix/multisample-transpose-retrigger`     | active | testable    | Master transpose into different sample zone re-triggers voice             | none (needs Voice/Sound mock)                | --             |

## Dropped / obsolete branches

| Branch                                    | Reason                                                     |
| ----------------------------------------- | ---------------------------------------------------------- |
| `bugfix/dx7-shift-name-shortcut`          | No-op: fix was reverted in same branch                     |
| `bugfix/audio-source-track-mute-cleanup`  | Already in upstream (no PR needed)                         |
| `bugfix/granular-copy-ctor-uninit-buffer` | Fixed upstream via PR #4371                                |
| `bugfix/granular-destructor-dealloc`      | Reported as #4358                                          |
| `bugfix/granular-stolen-stale-state`      | Revert was correct (PR #4370)                              |
| `feature/midi-cc64-sustain-pedal`         | Superseded by `feature/midi-cc66-sostenuto-pedal`          |
| `feature/midi-cc64-sustain-v2`            | Superseded by `feature/midi-cc66-sostenuto-pedal`          |
| `feature/midi-cc67-soft-pedal`            | Superseded by `feature/midi-cc66-sostenuto-pedal`          |
| `bugfix/midi-learned-param-display`       | Obsolete                                                   |

## Regression test analysis

**Current state:** All tests pass on both upstream and baseline.
No test currently proves any fix is still needed.

### Why existing tests don't catch the bugs on upstream

| Branch                                    | Test                             | Why it passes on upstream                                                                                             |
| ----------------------------------------- | -------------------------------- | --------------------------------------------------------------------------------------------------------------------- |
| `bugfix/envelope-sustain-zero-stuck`      | `envelope_test.cpp`              | Calls real `Envelope::render()` but params may not push `smoothedSustain` negative in 100 iterations                  |
| `bugfix/reverb-filter-encoder-jumps`      | `reverb_filter_roundtrip_test.cpp` | **Simulation only** -- tests `std::round` in a local struct, never calls real `readCurrentValue()`                  |
| `bugfix/notes-state-const-iterator`       | upstream unit test in branch     | Test is in `firmware/tests/unit/` -- runs only in firmware's CppUTest infra, not in our harness                       |
| `bugfix/browser-number-search`            | `browser_search_test.cpp`        | Tests exercise browser search logic but may not hit the exact off-by-one scenario                                     |

### Priority: tests that could become true regression tests

1. **`bugfix/envelope-sustain-zero-stuck`** -- tighten params to provoke negative `smoothedSustain`
2. **`bugfix/notes-state-const-iterator`** -- port upstream unit test to harness
3. **`bugfix/reverb-filter-encoder-jumps`** -- replace simulation with real `Reverb::readCurrentValue()`
4. **`bugfix/browser-number-search`** -- add edge case for exact off-by-one boundary
5. **`bugfix/lpf-drive-label-display`** -- needs l10n mock for label string lookup
6. **`bugfix/metronome-countin-toggle`** -- needs PlaybackHandler mock expansion
7. **`bugfix/patch-cables-fm-subtractive-switch`** -- needs Sound mock expansion
8. **`fix/multisample-transpose-retrigger`** -- needs Voice/Sound mock expansion
