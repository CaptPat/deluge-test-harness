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

**Zero.** All 2003 tests pass on both upstream and baseline.
The extra tests on baseline come from branch-only code (new features/APIs).

To verify a bugfix branch is still needed, either:

1. Write a regression test that fails on upstream (preferred)
2. Manually inspect upstream code for the specific bug pattern

---

## Bugfix branches (18)

| Branch                                         | Status     | Testability | What it fixes                                                                                                          | Harness test                                                                                | Upstream issue |
| ---------------------------------------------- | ---------- | ----------- | ---------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------- | -------------- |
| `bugfix/browser-long-press-back-loads-preset`  | active     | gui-only    | `exitUI()` calls `exitAction()` instead of `Browser::close()`, loading the previewed preset                            | none                                                                                        | --             |
| `bugfix/browser-number-search`                 | active     | has-test    | Off-by-one in `setFileByFullPath` (`>` to `>=`), numeric prefix matching                                               | `tests/util/browser_search_test.cpp`                                                        | --             |
| `bugfix/browser-text-search-last-item`         | active     | has-test    | Last item unfindable in browser text search; same off-by-one + `notFound` label placement                              | `tests/util/browser_search_test.cpp`                                                        | --             |
| `bugfix/envelope-sustain-zero-stuck`            | active     | has-test    | `smoothedSustain` goes negative when sustain=0, causing stuck notes                                                    | `tests/modulation/envelope_test.cpp` (real `Envelope::render()` with alignment-break trick) | --             |
| `bugfix/grid-dim-pads-after-new-clip`           | active     | gui-only    | Missing `skipGreyoutFade()` before explode animation                                                                   | none                                                                                        | --             |
| `bugfix/lpf-drive-label-display`                | active     | has-test    | Filter morph labels show "morph" when should show "drive"/"FM" for ladder filters                                      | `tests/modulation/filter_morph_label_test.cpp` (family classification + label logic)        | --             |
| `bugfix/macro-keyboard-visual-feedback`         | active     | gui-only    | Non-active macro pads not dimmed in keyboard session column                                                             | none                                                                                        | --             |
| `bugfix/metronome-countin-toggle`               | active     | has-test    | Count-in plays metronome clicks even when metronome is off; session sync/tempoless record use wrong comparisons        | `tests/playback/metronome_countin_test.cpp` (extracted session + playback logic)             | --             |
| `bugfix/mod-matrix-fast-scroll`                 | active     | gui-only    | OLED menu scroll jumps (wrong scroll position calc)                                                                    | none                                                                                        | --             |
| `bugfix/notes-state-const-iterator`             | active     | has-test    | `NotesState::end()` uses `notes.end() + count` instead of `notes.begin() + count`                                     | `tests/gui/notes_state_test.cpp` (real `NotesState` class)                                  | --             |
| `bugfix/patch-cables-fm-subtractive-switch`     | active     | has-test    | `setSynthMode()` restores filter modes AFTER `setupPatchingForAllParamManagers()` -- stale `lpfMode=OFF`               | `tests/processing/synth_mode_test.cpp` (real `setSynthMode()` with filter/knob assertions)  | #4232 (open)   |
| `bugfix/reverb-filter-encoder-jumps`            | active     | has-test    | Float truncation drift in reverb HPF/LPF `readCurrentValue()` (missing `std::round()`)                                | `tests/dsp/reverb_filter_roundtrip_test.cpp` (round-trip math with real `Base` subclass)    | --             |
| `bugfix/settings-exit-sd-race-v3`               | active     | has-test    | Re-entrant FatFS crash: defers settings writes via `addOnceTask(RESOURCE_SD)`                                          | `tests/meta/sd_race_guard_test.cpp` (source contract: `addOnceTask` in exitCompletely)      | #3898, #2759   |
| `bugfix/settings-menu-exit-crash`               | active     | has-test    | Stale menu pointer deref after `exitCompletely()` changes active UI                                                    | `tests/meta/sd_race_guard_test.cpp` (source contract: `getCurrentUI()` guard present)       | #3898, #2759   |
| `bugfix/song-browser-loop-fix`                  | active     | gui-only    | Song browser wrap-around broken when file list is windowed                                                              | none                                                                                        | --             |
| `bugfix/song-mode-clip-stuck-after-launch`      | active     | gui-only    | `UI_MODE_CLIP_PRESSED_IN_SONG_VIEW` conflated with `UI_MODE_HOLDING_STATUS_PAD`                                       | none                                                                                        | --             |
| `bugfix/velocity-view-multi-note-delete`        | active     | gui-only    | Velocity-head press wrongly enters multi-pad ramp mode instead of delete                                               | none                                                                                        | --             |
| `bugfix/wavetable-mod-knob-overwrite`           | active     | has-test    | Default mod knob mappings set on every sample swap, not just wavetable transition                                      | `tests/processing/wavetable_mod_knob_test.cpp` (real `applyWavetableModKnobDefaults()`)     | --             |

## Feature branches (8)

| Branch                                    | Status | Testability | What it adds                                                              | Harness test                                                                      | Upstream issue |
| ----------------------------------------- | ------ | ----------- | ------------------------------------------------------------------------- | --------------------------------------------------------------------------------- | -------------- |
| `feature/midi-cc66-sostenuto-pedal`       | active | has-test    | CC64 sustain + CC66 sostenuto + CC67 soft pedal with `PedalState` bitfield | `tests/model/pedal_state_test.cpp`, `tests/processing/sustain_pedal_test.cpp`    | --             |
| `feature/midi-separate-clock-transport`   | active | has-test    | Separate MIDI clock output from transport messages                        | `tests/playback/midi_transport_toggle_test.cpp`                                   | --             |
| `feature/multisample-dirpath-dedup`       | active | has-test    | Shared dirPath on Source; splitPath/dirMatches utilities                   | `tests/storage/source_packed_filenames_test.cpp` (Source integration + PathUtils) | --             |
| `feature/packed-filenames`                | active | has-test    | Single-alloc PackedFilenames buffer for multisample name storage          | `tests/storage/source_packed_filenames_test.cpp` (Source integration + 88-key)    | --             |
| `feature/per-clip-tempo-ratio`            | active | has-test    | Per-clip tempo ratio (Bresenham accumulator)                              | `tests/model/tempo_ratio_test.cpp`                                                | --             |
| `feature/per-clip-time-signature`         | active | has-test    | Per-clip `TimeSignature` struct for metronome                             | `tests/model/time_signature_test.cpp`                                             | --             |
| `feature/swap-select-tempo-encoders`      | active | gui-only    | Runtime toggle to swap select/tempo encoder identities                    | none                                                                              | --             |
| `fix/multisample-transpose-retrigger`     | active | has-test    | Master transpose into different sample zone re-triggers voice             | `tests/processing/multisample_transpose_test.cpp` (real `retriggerVoicesForTransposeChange()`) | -- |

## Dropped / obsolete branches

| Branch                                    | Reason                                                     |
| ----------------------------------------- | ---------------------------------------------------------- |
| `bugfix/dx7-shift-name-shortcut`          | Already fixed upstream via PR #2720 (merged 2024-09-29)    |
| `bugfix/audio-source-track-mute-cleanup`  | Already in upstream (no PR needed)                         |
| `bugfix/granular-copy-ctor-uninit-buffer` | Fixed upstream via PR #4371                                |
| `bugfix/granular-destructor-dealloc`      | Reported as #4358                                          |
| `bugfix/granular-stolen-stale-state`      | Revert was correct (PR #4370)                              |
| `feature/midi-cc64-sustain-pedal`         | Superseded by `feature/midi-cc66-sostenuto-pedal`          |
| `feature/midi-cc64-sustain-v2`            | Superseded by `feature/midi-cc66-sostenuto-pedal`          |
| `feature/midi-cc67-soft-pedal`            | Superseded by `feature/midi-cc66-sostenuto-pedal`          |
| `bugfix/midi-learned-param-display`       | Obsolete                                                   |
| `bugfix/settings-exit-sd-race`            | Superseded by `bugfix/settings-exit-sd-race-v3` (PR #95)   |
| `kastenbalg/deferred-sd-operations`       | Superseded by `bugfix/settings-exit-sd-race-v3` (PR #95)   |

## Regression test analysis

**Current state:** 2003 tests, all passing. Coverage summary below.

### Test quality by branch

| Branch                                    | Test quality                                                                                                          |
| ----------------------------------------- | --------------------------------------------------------------------------------------------------------------------- |
| `bugfix/envelope-sustain-zero-stuck`      | **Strong** -- real `Envelope::render()` with deliberate alignment-breaking to provoke negative overshoot              |
| `bugfix/notes-state-const-iterator`       | **Strong** -- real `NotesState` class, const/mutable end() agreement verified                                         |
| `bugfix/patch-cables-fm-subtractive-switch` | **Strong** -- real `setSynthMode()` with filter mode + mod knob assertions across all mode transitions              |
| `bugfix/wavetable-mod-knob-overwrite`     | **Strong** -- real `applyWavetableModKnobDefaults()` with wasAlreadyWavetable guard                                  |
| `fix/multisample-transpose-retrigger`     | **Strong** -- real `retriggerVoicesForTransposeChange()` + `Source::getRange()` with two-zone multisamples            |
| `feature/packed-filenames`                | **Strong** -- real Source integration: getFullPath, unpackFilenames, revertToFullPaths with 88-key piano scenario     |
| `feature/multisample-dirpath-dedup`       | **Strong** -- real splitPath/dirMatches + Source dirPath reconstruction across packed/unpacked/revert paths           |
| `bugfix/reverb-filter-encoder-jumps`      | **Good** -- tests float round-trip math via `Base` subclass, covers the essential truncation vs rounding bug          |
| `bugfix/browser-number-search`            | **Good** -- simulation of search logic (real `Browser` class too heavy to instantiate)                               |
| `bugfix/lpf-drive-label-display`          | **Good** -- tests `SpecificFilter::getFamily()` classification that drives label selection                           |
| `feature/midi-cc66-sostenuto-pedal`       | **Strong** -- real `Voice::noteOff()` with pedal state machine, full lifecycle tests                                 |
| `bugfix/metronome-countin-toggle`         | **Good** -- extracted count-in/playback/tempoless-record/sync-launch logic from session.cpp + playback_handler.cpp   |
| `bugfix/settings-exit-sd-race-v3`         | **Good** -- source contract test verifies `addOnceTask(RESOURCE_SD)` in exitCompletely()                             |
| `bugfix/settings-menu-exit-crash`         | **Good** -- source contract test verifies `getCurrentUI() != this` guard after goUpOneLevel()                        |

### Remaining untested testable branches

None — all testable branches now have tests.
