# Deluge Test Harness - Code Coverage Report

Generated: 2026-03-18
Branch: personal/nightly
Tool: gcov (GCC 15.2.0) with --coverage instrumentation
Tests: 3395 tests (3391 ran, 4 ignored, 1 known failure)

## Overall Coverage

| Metric | Value |
|--------|-------|
| Files measured | 169 |
| Total instrumentable lines | 9,629 |
| Lines executed | 7,245 |
| Lines not executed | 2,384 |
| **Line coverage** | **75.2%** |

## Top 10 Files by Coverage (highest %)

| Coverage | Executed/Total | File |
|----------|---------------|------|
| 100.0% | 187/187 | firmware/src/deluge/dsp/filter/filter.h |
| 100.0% | 175/175 | firmware/src/deluge/dsp/dx/EngineMkI.cpp |
| 100.0% | 79/79 | firmware/src/deluge/dsp/compressor/rms_feedback.cpp |
| 100.0% | 68/68 | firmware/src/deluge/dsp/filter/svf.cpp |
| 100.0% | 65/65 | firmware/src/deluge/dsp/dx/math_lut.cpp |
| 100.0% | 63/63 | firmware/src/deluge/dsp/dx/fm_op_kernel.cpp |
| 100.0% | 59/59 | firmware/src/deluge/dsp/reverb/freeverb/freeverb.cpp |
| 100.0% | 57/57 | firmware/src/deluge/hid/encoder.cpp |
| 100.0% | 56/56 | firmware/src/deluge/model/song/clip_iterators.cpp |
| 100.0% | 52/52 | firmware/src/deluge/dsp/filter/hpladder.cpp |

## Top 10 Files by Uncovered Lines (biggest gaps)

| Uncovered | Coverage | Total | File |
|-----------|----------|-------|------|
| 416 | 29.8% | 593 | firmware/src/deluge/modulation/patch/patch_cable_set.cpp |
| 259 | 77.1% | 1130 | firmware/src/deluge/modulation/arpeggiator.cpp |
| 236 | 35.0% | 363 | firmware/src/deluge/modulation/params/param_set.cpp |
| 141 | 51.0% | 288 | firmware/src/deluge/modulation/params/param_manager.cpp |
| 100 | 6.5% | 107 | firmware/src/deluge/model/model_stack.h |
| 93 | 49.7% | 185 | firmware/src/deluge/dsp/delay/delay_buffer.h |
| 75 | 86.0% | 535 | firmware/src/deluge/util/container/array/resizeable_array.cpp |
| 66 | 81.2% | 351 | firmware/src/deluge/modulation/params/param.cpp |
| 64 | 0.0% | 64 | firmware/src/deluge/model/mod_controllable/filters/filter_config.h |
| 50 | 64.0% | 139 | firmware/src/deluge/modulation/patch/patcher.cpp |

## Files with 0% Coverage (28 files)

These files are compiled into the test harness but no code paths were exercised by tests.

| Lines | File |
|-------|------|
| 64 | firmware/src/deluge/model/mod_controllable/filters/filter_config.h |
| 44 | firmware/src/deluge/dsp/reverb/freeverb/freeverb.hpp |
| 31 | firmware/src/deluge/dsp/compressor/rms_feedback.h |
| 24 | firmware/src/deluge/model/scale/note_set.h |
| 22 | firmware/src/deluge/dsp/dx/math_lut.h |
| 18 | firmware/src/deluge/model/consequence/consequence.h |
| 17 | firmware/src/deluge/model/mod_controllable/mod_controllable.h |
| 14 | firmware/src/deluge/dsp/reverb/base.hpp |
| 12 | firmware/src/deluge/model/note/note.h |
| 10 | firmware/src/deluge/dsp/envelope_follower/absolute_value.h |
| 10 | firmware/src/deluge/util/d_string.h |
| 9 | firmware/src/deluge/dsp/filter/hpladder.h |
| 9 | firmware/src/deluge/modulation/params/param_set.h |
| 6 | firmware/src/deluge/util/d_stringbuf.h |
| 5 | firmware/src/deluge/modulation/knob.h |
| 5 | firmware/src/deluge/modulation/params/param_collection.h |
| 4 | firmware/src/deluge/dsp/dx/fm_core.h |
| 4 | firmware/src/deluge/dsp/filter/svf.h |
| 4 | firmware/src/deluge/util/const_functions.h |
| 3 | firmware/src/deluge/gui/l10n/language.h |
| 3 | firmware/src/deluge/model/timeline_counter.h |
| 3 | firmware/src/deluge/modulation/patch/patch_cable_set.h |
| 3 | firmware/src/deluge/playback/mode/playback_mode.h |
| 2 | firmware/src/deluge/storage/audio/audio_file_holder.h |
| 2 | firmware/src/deluge/util/semver.h |
| 1 | firmware/src/deluge/dsp/dx/pitchenv.h |
| 1 | firmware/src/deluge/model/fx/stutterer.h |
| 1 | firmware/src/deluge/storage/DX7Cartridge.h |

## Notes

- Coverage data includes both .cpp and .h files compiled as part of  target
- Header-only code (templates, inline functions) is counted per translation unit inclusion
- Some files appear in multiple translation units; line counts are summed across all inclusions
- gcovr could not be used due to GCC bug #68080 (negative hit counts); raw gcov was used instead
- The test harness uses mocks for hardware, storage, and UI; coverage reflects unit-testable logic only
