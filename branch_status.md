<!-- markdownlint-disable MD060 -->
# Branch Status Tracking

## Terminology

| Term                 | Definition                                                                     |
| -------------------- | ------------------------------------------------------------------------------ |
| **upstream**         | `SynthstromAudible/DelugeFirmware:main` -- the canonical firmware              |
| **branch**           | Individual fix/feature branch on `CaptPat/DelugeFirmware`, rebased on upstream |
| **baseline**         | `CaptPat/DelugeFirmware:personal/nightly` -- local nightly integration branch   |
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

## Bugfix branches (22 active, 3 obsolete)

| Branch                                         | Status     | Testability | What it fixes                                                                                                          | Harness test                                                                                | Upstream issue |
| ---------------------------------------------- | ---------- | ----------- | ---------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------- | -------------- |
| `bugfix/arrangement-record-song-cast-crash`     | active     | testable    | `ModelStack` methods cast `Song*` as `Clip*` when recording song-level params in arrangement mode — crash on deref     | none (needs source contract test)                                                           | --             |
| `bugfix/arp-ratchet-probability-cache`          | active     | has-test    | Ratchet probability not cached like other prob types — causes inconsistent arp ratcheting and stuck locked randomizer  | `tests/storage/model_serialization_test.cpp` (ratchet prob serialization)                   | #4298 (open)   |
| `bugfix/browser-number-search`                 | active     | has-test    | Off-by-one in `setFileByFullPath` (`>` to `>=`), numeric prefix matching                                               | `tests/util/browser_search_test.cpp`                                                        | #4105 (open)   |
| `bugfix/chord-keyboard-melodic-minor-crash`     | active     | has-test    | Uninitialized `qualities[]` in `KeyboardLayoutChord` causes OOB access with melodic/harmonic/hungarian minor scales    | `tests/meta/upstream_bug_repro_test.cpp`, `tests/gui/chord_keyboard_test.cpp`               | #4284 (open)   |
| `bugfix/envelope-sustain-zero-stuck`            | upstream-resolved | has-test    | Fixed upstream via #4444. Our overshoot guard was wrong (Mark: sustain is signed, can be negative). Upstream fix: OFF transition when sustain==0 prevents re-audition | `tests/modulation/envelope_test.cpp` | #4029 (closed) |
| `bugfix/grid-dim-pads-after-new-clip`           | active     | gui-only    | Missing `skipGreyoutFade()` before explode animation                                                                   | none                                                                                        | #3893 (open)   |
| `bugfix/kit-midi-learn-master-level`            | active     | has-test    | `setupKitGlobalFXMenu` not set when entering SoundEditor via shortcut pad — null `ModControllable` crash on MIDI learn | `tests/meta/missing_branch_guard_test.cpp` (source contract: affectEntire checks)           | #3640 (open)   |
| `bugfix/loop-undo-pending-overdub-crash`        | active     | has-test    | Use-after-free in `revertAction()`: `deletePendingOverdubs()` frees clips still referenced by action's consequences   | `tests/unit/action_references_clip_tests.cpp`, `tests/unit/loop_undo_guard_tests.cpp`       | #3746 (open)   |
| `bugfix/lpf-drive-label-display`                | active     | has-test    | Filter morph labels show "morph" when should show "drive"/"FM" for ladder filters                                      | `tests/modulation/filter_morph_label_test.cpp` (family classification + label logic)        | --             |
| `bugfix/macro-keyboard-visual-feedback`         | active     | gui-only    | Non-active macro pads not dimmed in keyboard session column                                                             | none                                                                                        | --             |
| `bugfix/midi-follow-double-note`                | active     | has-test    | MIDI Follow handles note then general loop visits same instrument — double `recordNoteOn()` and audible note clusters  | `tests/playback/midi_follow_dedup_test.cpp` (dedup logic)                                   | --             |
| `bugfix/midi-to-synth-crash-on-save`            | active     | has-test    | Stale `midiBank/midiSub/midiPGM` after `changeOutputType()` from MIDI_OUT causes crash during save serialization      | `tests/meta/missing_branch_guard_test.cpp` (source contract: MIDI metadata cleared)         | #4014 (open)   |
| `bugfix/mod-matrix-fast-scroll`                 | obsolete   | gui-only    | OLED menu scroll jumps (wrong scroll position calc)                                                                    | none                                                                                        | #3899 — fixed by upstream #4442 |
| `bugfix/multisample-octave-zero`                | active     | has-test    | `getComparativeNoteNumberFromChars()` rejects '0' as octave digit — filenames like "C0.wav" sort incorrectly           | `tests/meta/missing_branch_guard_test.cpp` (source contract: '0' accepted in range check)   | #3784 (open)   |
| `bugfix/notes-state-const-iterator`             | active     | has-test    | `NotesState::end()` uses `notes.end() + count` instead of `notes.begin() + count`                                     | `tests/gui/notes_state_test.cpp` (real `NotesState` class)                                  | #4284 (open)   |
| `bugfix/patch-cables-fm-subtractive-switch`     | obsolete   | has-test    | `setSynthMode()` restores filter modes AFTER `setupPatchingForAllParamManagers()` -- stale `lpfMode=OFF`               | `tests/processing/synth_mode_test.cpp` (real `setSynthMode()` with filter/knob assertions)  | #4232 — fixed by upstream #4440 |
| `bugfix/settings-exit-sd-race-v3`               | active     | has-test    | Re-entrant FatFS crash: defers settings writes via `addOnceTask(RESOURCE_SD)`                                          | `tests/meta/sd_race_guard_test.cpp` (source contract: `addOnceTask` in exitCompletely)      | #3898, #2759   |
| `bugfix/settings-menu-exit-crash`               | active     | has-test    | Stale menu pointer deref after `exitCompletely()` changes active UI                                                    | `tests/meta/sd_race_guard_test.cpp` (source contract: `getCurrentUI()` guard present)       | #3898, #2759   |
| `bugfix/song-browser-loop-fix`                  | active     | gui-only    | Song browser wrap-around broken when file list is windowed                                                              | none                                                                                        | #4125 (open)   |
| `bugfix/stereo-spread-osc-sync`                 | active     | has-test    | Stereo rendering path passes `doOscSync=false` and nullptr for sync arrays, disabling osc sync with stereo spread      | `tests/meta/missing_branch_guard_test.cpp` (source contract: sync params in stereo loop)    | #3373 (open)   |
| `bugfix/song-mode-clip-stuck-after-launch`      | active     | gui-only    | `UI_MODE_CLIP_PRESSED_IN_SONG_VIEW` conflated with `UI_MODE_HOLDING_STATUS_PAD`                                        | none                                                                                        | #4049 (open)   |
| `bugfix/unsaved-synth-to-kit-row`               | active     | has-test    | Missing `existsOnCard` check before destructive drum removal — unsaved presets cause FILE_NOT_FOUND after drum deleted | `tests/meta/missing_branch_guard_test.cpp` (source contract: existsOnCard before load)      | --             |
| `bugfix/velocity-view-multi-note-delete`        | active     | gui-only    | Velocity-head press wrongly enters multi-pad ramp mode instead of delete                                                | none                                                                                        | #3985 (open)   |
| `bugfix/wavetable-mod-knob-overwrite`           | active     | has-test    | Default mod knob mappings set on every sample swap, not just wavetable transition                                      | `tests/processing/wavetable_mod_knob_test.cpp` (real `applyWavetableModKnobDefaults()`)     | --             |
| `fix/velocity-view-quantize-freeze`             | obsolete   | has-test    | Missing `UI_MODE_QUANTIZE` handler in AutomationView causes partial freeze when quantizing in Velocity View            | `tests/meta/quantize_freeze_guard_test.cpp` (source contract: quantize handler + public visibility) | #3718 — fixed by upstream #4433 |

## Feature branches (8)

| Branch                                    | Status | Testability | What it adds                                                              | Harness test                                                                      | Upstream issue |
| ----------------------------------------- | ------ | ----------- | ------------------------------------------------------------------------- | --------------------------------------------------------------------------------- | -------------- |
| `feature/midi-cc64-66-67-pedal`       | active | has-test    | CC64 sustain + CC66 sostenuto + CC67 soft pedal with `PedalState` bitfield | `tests/model/pedal_state_test.cpp`, `tests/processing/sustain_pedal_test.cpp`    | #4068 (open)   |
| `feature/midi-separate-clock-transport`   | active | has-test    | Separate MIDI clock output from transport messages                        | `tests/playback/midi_transport_toggle_test.cpp`                                   | #1399 (open)   |
| `feature/multisample-dirpath-dedup`       | active | has-test    | Shared dirPath on Source; splitPath/dirMatches utilities                   | `tests/storage/source_packed_filenames_test.cpp` (Source integration + PathUtils) | --             |
| `feature/on-device-test-framework`        | active | has-test    | RECORD+boot self-test (POST + regression), PLAY+boot demo mode            | `tests/meta/on_device_test_framework_test.cpp` (compile gate + result types)      | #112           |
| `feature/packed-filenames`                | active | has-test    | Single-alloc PackedFilenames buffer for multisample name storage          | `tests/storage/source_packed_filenames_test.cpp` (Source integration + 88-key)    | --             |
| `feature/polymeter-polyrhythm`            | active | has-test    | Per-clip tempo ratio + time signature + SYNC_SCALING+tempo knob shortcut + coarse tempo sync-scale fix | `tests/model/tempo_ratio_test.cpp`, `tests/model/time_signature_test.cpp` (54 tests) | #122 |
| `feature/swap-select-tempo-encoders`      | active | gui-only    | Runtime toggle to swap select/tempo encoder identities                    | none                                                                              | --             |
| `fix/multisample-transpose-retrigger`     | active | has-test    | Master transpose into different sample zone re-triggers voice             | `tests/processing/multisample_transpose_test.cpp` (real `retriggerVoicesForTransposeChange()`) | -- |

## Dropped / obsolete branches

| Branch                                    | Reason                                                     |
| ----------------------------------------- | ---------------------------------------------------------- |
| `bugfix/dx7-shift-name-shortcut`          | Reverted — still active as PR #100 on CaptPat repo        |
| `bugfix/audio-source-track-mute-cleanup`  | Already in upstream (no PR needed)                         |
| `bugfix/granular-copy-ctor-uninit-buffer` | Fixed upstream via PR #4371                                |
| `bugfix/granular-destructor-dealloc`      | Reported as #4358                                          |
| `bugfix/granular-stolen-stale-state`      | Merged upstream (#4361), reverted (#4370). Matt's header init (#4371) + getBuffer path covers most cases, but stolen→processGrainFX window remains. Defense-in-depth argument still valid — see memory. |
| `feature/midi-cc64-sustain-pedal`         | Superseded by `feature/midi-cc64-66-67-pedal`          |
| `feature/midi-cc64-sustain-v2`            | Superseded by `feature/midi-cc64-66-67-pedal`          |
| `feature/midi-cc67-soft-pedal`            | Superseded by `feature/midi-cc64-66-67-pedal`          |
| `bugfix/midi-learned-param-display`       | Obsolete                                                   |
| `bugfix/arp-locked-prob-typo`             | Upstream merged our PR #4398 — fix now in baseline                                              |
| `bugfix/browser-filepointer-cache`        | Upstream #4393 merged (different approach) — our cache approach superseded                       |
| `bugfix/metronome-countin-toggle`         | Backed out — count-in clicking with metronome off is desired behavior for live recording |
| `bugfix/song-swap-cancel-back`            | No-op after rebase — upstream #4391 landed the exitUI refactor, song swap fix was fully reverted |
| `bugfix/string-missing-return`            | Upstream merged our PR #4396 — fix now in baseline                                              |
| `bugfix/settings-exit-sd-race`            | Superseded by `bugfix/settings-exit-sd-race-v3` (PR #95)   |
| `feature/per-clip-tempo-ratio`            | Superseded by `feature/polymeter-polyrhythm` (PR #123)     |
| `feature/per-clip-time-signature`         | Superseded by `feature/polymeter-polyrhythm` (PR #123)     |
| `temp/clip-features`                      | Superseded by `feature/polymeter-polyrhythm` (PR #123)     |
| `kastenbalg/deferred-sd-operations`       | Superseded by `bugfix/settings-exit-sd-race-v3` (PR #95)   |
| `bugfix/reverb-filter-encoder-jumps`      | Upstream merged our PR #4395 — fix now in baseline                                              |
| `bugfix/browser-long-press-back-loads-preset` | Upstream merged our PR #4401 — fix now in baseline                                          |
| `bugfix/browser-text-search-last-item`    | Subsumed by `bugfix/browser-number-search` (all fixes included)                                 |
| `bugfix/mod-matrix-fast-scroll`           | Fixed by upstream #4442 (superior `std::clamp` approach)                                        |
| `bugfix/patch-cables-fm-subtractive-switch` | Fixed by upstream #4440 (same bug, cleaner minimal fix)                                       |
| `fix/velocity-view-quantize-freeze`       | Fixed by upstream #4433 (more complete implementation)                                          |

## Regression test analysis

**Current state:** 3701+ tests, all passing (except pre-existing Scheduler + ClipIterator failures). Coverage summary below.

### Test quality by branch

| Branch                                    | Test quality                                                                                                          |
| ----------------------------------------- | --------------------------------------------------------------------------------------------------------------------- |
| `bugfix/notes-state-const-iterator`       | **Strong** -- real `NotesState` class, const/mutable end() agreement verified                                         |
| `bugfix/wavetable-mod-knob-overwrite`     | **Strong** -- real `applyWavetableModKnobDefaults()` with wasAlreadyWavetable guard                                  |
| `fix/multisample-transpose-retrigger`     | **Strong** -- real `retriggerVoicesForTransposeChange()` + `Source::getRange()` with two-zone multisamples            |
| `feature/packed-filenames`                | **Strong** -- real Source integration: getFullPath, unpackFilenames, revertToFullPaths with 88-key piano scenario     |
| `feature/multisample-dirpath-dedup`       | **Strong** -- real splitPath/dirMatches + Source dirPath reconstruction across packed/unpacked/revert paths           |
| `bugfix/browser-number-search`            | **Good** -- simulation of search logic (real `Browser` class too heavy to instantiate); subsumes text-search-last-item |
| `bugfix/loop-undo-pending-overdub-crash`  | **Good** -- unit tests for `Action::referencesClip()` + source contract tests for `deletePendingOverdubs` guard      |
| `bugfix/lpf-drive-label-display`          | **Good** -- tests `SpecificFilter::getFamily()` classification that drives label selection                           |
| `feature/midi-cc64-66-67-pedal`       | **Strong** -- real `Voice::noteOff()` with pedal state machine, full lifecycle tests                                 |
| `bugfix/settings-exit-sd-race-v3`         | **Good** -- source contract test verifies `addOnceTask(RESOURCE_SD)` in exitCompletely()                             |
| `bugfix/settings-menu-exit-crash`         | **Good** -- source contract test verifies `getCurrentUI() != this` guard after goUpOneLevel()                        |
| `bugfix/arp-ratchet-probability-cache`    | **Good** -- serialization test verifies ratchet probability field persistence                                         |
| `bugfix/chord-keyboard-melodic-minor-crash` | **Strong** -- real `getChordQuality()` for all preset scales + dedicated chord keyboard test                        |
| `bugfix/midi-follow-double-note`          | **Good** -- dedup logic test verifies handled output is skipped in general loop (upstream #4225)                      |
| `bugfix/kit-midi-learn-master-level`      | **Good** -- source contract test verifies `affectEntire` checked in both `potentialShortcutPadAction` and `setup()` |
| `bugfix/midi-to-synth-crash-on-save`      | **Good** -- source contract test verifies stale MIDI metadata cleared in `changeOutputType` for MIDI_OUT |
| `bugfix/multisample-octave-zero`           | **Good** -- source contract test verifies octave '0' accepted in `getComparativeNoteNumberFromChars` range check |
| `bugfix/unsaved-synth-to-kit-row`          | **Good** -- source contract test verifies `existsOnCard` checked before destructive drum removal |
| `bugfix/stereo-spread-osc-sync`            | **Good** -- source contract test verifies sync params (`doingOscSync`, `oscSyncPos`) in stereo rendering loop |
| `feature/polymeter-polyrhythm`            | **Strong** -- 54 tests: tempo ratio Bresenham accumulator, overflow, menu round-trip, time signature metronome beat/bar detection, clip-local position tracking |
| `feature/on-device-test-framework`        | **Good** -- compile gate + result type tests; full POST/regression/demo coverage is on-device only |

### Remaining untested testable branches

None — all testable branches now have tests.
