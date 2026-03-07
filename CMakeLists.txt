cmake_minimum_required(VERSION 3.24)
project(DelugeTestHarness LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_C_STANDARD 23)

# Path to the firmware submodule
set(FIRMWARE_DIR ${CMAKE_SOURCE_DIR}/firmware)
set(FIRMWARE_SRC ${FIRMWARE_DIR}/src)

# ── Dependencies ──────────────────────────────────────────────────────────
include(FetchContent)

FetchContent_Declare(
    CppUTest
    GIT_REPOSITORY https://github.com/cpputest/cpputest.git
    GIT_TAG eccbc2190c672e598c1dd2bf5e4295f5ba27aad1
)
FetchContent_Declare(
    etl
    GIT_REPOSITORY https://github.com/ETLCPP/etl
    GIT_TAG 20.39.4
)

set(TESTS OFF CACHE BOOL "Switch off CppUTest internal tests")
FetchContent_MakeAvailable(CppUTest etl)

# ── Compile definitions ──────────────────────────────────────────────────
add_compile_definitions(
    IN_UNIT_TESTS=1
    CPPUTEST_MEM_LEAK_DETECTION_DISABLED
)
if (WIN32)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS=1)
endif ()

# ── Mock library (hardware stubs) ────────────────────────────────────────
add_library(deluge_mocks STATIC
    src/mocks/display_mock.cpp
    src/mocks/timer_mocks.cpp
    src/mocks/song_mock.cpp
    src/mocks/print_mock.cpp
    src/mocks/storage_mock.cpp
    src/mocks/audio_engine_mock.cpp
    src/mocks/midi_engine_mock.cpp
    src/mocks/button_mock.cpp
    src/mocks/matrix_driver_mock.cpp
    src/mocks/encoder_mock.cpp
    src/mocks/pad_leds_mock.cpp
    src/mocks/indicator_leds_mock.cpp
    src/mocks/memory_mock.cpp
    src/mocks/ui_mock.cpp
    src/mocks/platform_stubs.cpp
    src/mocks/flash_storage_mock.cpp
    src/mocks/general_memory_allocator_mock.cpp
    # Phase 2C stubs
    src/mocks/note_row_stubs.cpp
    src/mocks/model_stack_stubs.cpp
    # Phase 3 stubs
    src/mocks/auto_param_stubs.cpp
    src/mocks/functions_stubs.cpp
    # Phase 4 stubs
    src/mocks/playback/playback_handler.cpp
    # Phase 5 stubs
    src/mocks/cfunctions_stubs.cpp
    # Phase 14 stubs
    # clip_instance_stubs.cpp removed — replaced by real clip_instance.cpp in Phase 16
    # Phase 15 stubs
    src/mocks/storage/audio_file_stubs.cpp
)
target_include_directories(deluge_mocks PUBLIC
    src/mocks
    ${FIRMWARE_SRC}
    ${FIRMWARE_SRC}/deluge
)
target_link_libraries(deluge_mocks PUBLIC etl::etl)
target_compile_options(deluge_mocks PUBLIC
    $<$<COMPILE_LANGUAGE:CXX>:-fpermissive>
)

# ── Real firmware source compiled on x86 ────────────────────────────────
add_library(deluge_firmware_src STATIC
    # Scale system
    ${FIRMWARE_SRC}/deluge/model/scale/note_set.cpp
    ${FIRMWARE_SRC}/deluge/model/scale/musical_key.cpp
    ${FIRMWARE_SRC}/deluge/model/scale/preset_scales.cpp
    ${FIRMWARE_SRC}/deluge/model/scale/utils.cpp
    ${FIRMWARE_SRC}/deluge/model/scale/scale_change.cpp
    ${FIRMWARE_SRC}/deluge/model/scale/scale_mapper.cpp
    # Clip iterators
    ${FIRMWARE_SRC}/deluge/model/song/clip_iterators.cpp
    # Sync
    ${FIRMWARE_SRC}/deluge/model/sync.cpp
    # Utility
    ${FIRMWARE_SRC}/deluge/util/d_stringbuf.cpp
    ${FIRMWARE_SRC}/deluge/util/lookuptables/lookuptables.cpp
    ${FIRMWARE_SRC}/deluge/util/waves.cpp
    # Modulation
    ${FIRMWARE_SRC}/deluge/modulation/lfo.cpp
    # GUI (leaf)
    ${FIRMWARE_SRC}/deluge/gui/menu_item/value_scaling.cpp
    ${FIRMWARE_SRC}/deluge/gui/ui/keyboard/chords.cpp
    # Containers (Phase 2B)
    ${FIRMWARE_SRC}/deluge/util/container/array/resizeable_array.cpp
    ${FIRMWARE_SRC}/deluge/util/container/array/resizeable_pointer_array.cpp
    ${FIRMWARE_SRC}/deluge/util/container/array/ordered_resizeable_array.cpp
    ${FIRMWARE_SRC}/deluge/util/container/array/ordered_resizeable_array_with_multi_word_key.cpp
    # Phase 2C: Note/Param data structures
    ${FIRMWARE_SRC}/deluge/model/note/note.cpp
    ${FIRMWARE_SRC}/deluge/model/note/note_vector.cpp
    ${FIRMWARE_SRC}/deluge/model/note/note_row_vector.cpp
    ${FIRMWARE_SRC}/deluge/model/iterance/iterance.cpp
    ${FIRMWARE_SRC}/deluge/modulation/params/param_node_vector.cpp
    # Phase 3: Modulation layer
    ${FIRMWARE_SRC}/deluge/modulation/envelope.cpp
    ${FIRMWARE_SRC}/deluge/modulation/params/param_collection.cpp
    ${FIRMWARE_SRC}/deluge/modulation/patch/patch_cable.cpp
    ${FIRMWARE_SRC}/deluge/modulation/midi/midi_param.cpp
    ${FIRMWARE_SRC}/deluge/modulation/midi/midi_param_vector.cpp
    ${FIRMWARE_SRC}/deluge/modulation/midi/midi_knob_array.cpp
    # Phase 4: Utilities, Colour, Sidechain
    ${FIRMWARE_SRC}/deluge/util/semver.cpp
    ${FIRMWARE_SRC}/deluge/util/firmware_version.cpp
    ${FIRMWARE_SRC}/deluge/util/phase_increment_fine_tuner.cpp
    ${FIRMWARE_SRC}/deluge/gui/colour/rgb.cpp
    ${FIRMWARE_SRC}/deluge/modulation/sidechain/sidechain.cpp
    # Phase 5: Freeverb, String
    ${FIRMWARE_SRC}/deluge/dsp/reverb/freeverb/freeverb.cpp
    src/mocks/d_string_wrapper.cpp
    # Phase 6: DSP Filters, MIDI Message, Envelope Follower
    ${FIRMWARE_SRC}/deluge/dsp/filter/svf.cpp
    ${FIRMWARE_SRC}/deluge/dsp/filter/lpladder.cpp
    ${FIRMWARE_SRC}/deluge/dsp/filter/hpladder.cpp
    ${FIRMWARE_SRC}/deluge/dsp/envelope_follower/absolute_value.cpp
    ${FIRMWARE_SRC}/deluge/model/midi/message.cpp
    # Phase 7: Filter completion, String, DX7, RMS Compressor
    ${FIRMWARE_SRC}/deluge/dsp/filter/filter.cpp
    ${FIRMWARE_SRC}/deluge/model/mod_controllable/filters/filter_config.cpp
    ${FIRMWARE_SRC}/deluge/dsp/filter/filter_set.cpp
    ${FIRMWARE_SRC}/deluge/util/string.cpp
    ${FIRMWARE_SRC}/deluge/dsp/dx/env.cpp
    ${FIRMWARE_SRC}/deluge/dsp/dx/pitchenv.cpp
    ${FIRMWARE_SRC}/deluge/dsp/compressor/rms_feedback.cpp
    # Phase 8A: Wavetable data tables
    ${FIRMWARE_SRC}/deluge/util/lookuptables/analog_square.cpp
    ${FIRMWARE_SRC}/deluge/util/lookuptables/saw.cpp
    ${FIRMWARE_SRC}/deluge/util/lookuptables/square.cpp
    ${FIRMWARE_SRC}/deluge/util/lookuptables/mystery_synth_a_saw.cpp
    ${FIRMWARE_SRC}/deluge/util/lookuptables/mystery_synth_b_saw.cpp
    # Phase 8B: Leaf models
    ${FIRMWARE_SRC}/deluge/model/consequence/consequence.cpp
    ${FIRMWARE_SRC}/deluge/model/timeline_counter.cpp
    ${FIRMWARE_SRC}/deluge/model/drum/drum_name.cpp
    ${FIRMWARE_SRC}/deluge/model/note/copied_note_row.cpp
    # Phase 8C: Container vectors
    ${FIRMWARE_SRC}/deluge/model/clip/clip_instance_vector.cpp
    # clip_array.cpp omitted — mock ClipArray in clip_mocks.h (used by clip_iterators) conflicts
    ${FIRMWARE_SRC}/deluge/model/sample/sample_cluster_array.cpp
    # Phase 8H: CStringArray (search + sort using strcmpspecial)
    ${FIRMWARE_SRC}/deluge/util/container/array/c_string_array.cpp
    # Phase 8D: Memory + Delay buffer
    ${FIRMWARE_SRC}/deluge/memory/memory_allocator_interface.cpp
    ${FIRMWARE_SRC}/deluge/dsp/delay/delay_buffer.cpp
    # Phase 8E: Localization
    ${FIRMWARE_SRC}/deluge/gui/l10n/g_english.cpp
    ${FIRMWARE_SRC}/deluge/gui/l10n/g_seven_segment.cpp
    ${FIRMWARE_SRC}/deluge/gui/l10n/l10n.cpp
    # Phase 9A: DX7/FM Synthesis Stack
    ${FIRMWARE_SRC}/deluge/dsp/dx/math_lut.cpp
    ${FIRMWARE_SRC}/deluge/dsp/dx/fm_op_kernel.cpp
    ${FIRMWARE_SRC}/deluge/dsp/dx/fm_core.cpp
    ${FIRMWARE_SRC}/deluge/dsp/dx/EngineMkI.cpp
    ${FIRMWARE_SRC}/deluge/dsp/dx/engine.cpp
    ${FIRMWARE_SRC}/deluge/dsp/dx/dx7note.cpp
    # Phase 9B: Container Data Structures
    ${FIRMWARE_SRC}/deluge/util/container/list/bidirectional_linked_list.cpp
    ${FIRMWARE_SRC}/deluge/util/algorithm/quick_sorter.cpp
    ${FIRMWARE_SRC}/deluge/util/container/hashtable/open_addressing_hash_table.cpp
    # Phase 9C: HID + Model Leaves
    ${FIRMWARE_SRC}/deluge/hid/button.cpp
    ${FIRMWARE_SRC}/deluge/hid/matrix/pad.cpp
    ${FIRMWARE_SRC}/deluge/model/clip/clip_minder.cpp
    ${FIRMWARE_SRC}/deluge/model/sample/sample_perc_cache_zone.cpp
    ${FIRMWARE_SRC}/deluge/model/sample/sample_controls.cpp
    # Phase 10A: Consequence subclasses
    ${FIRMWARE_SRC}/deluge/model/consequence/consequence_swing_change.cpp
    ${FIRMWARE_SRC}/deluge/model/consequence/consequence_tempo_change.cpp
    ${FIRMWARE_SRC}/deluge/model/consequence/consequence_scale_add_note.cpp
    ${FIRMWARE_SRC}/deluge/model/consequence/consequence_param_change.cpp
    ${FIRMWARE_SRC}/deluge/model/consequence/consequence_begin_playback.cpp
    # Phase 10B: ModControllable + NamedThingVector
    ${FIRMWARE_SRC}/deluge/model/mod_controllable/mod_controllable.cpp
    ${FIRMWARE_SRC}/deluge/util/container/vector/named_thing_vector.cpp
    # Phase 11A: Utility/Infrastructure
    ${FIRMWARE_SRC}/deluge/memory/operators.cpp
    ${FIRMWARE_SRC}/deluge/playback/mode/playback_mode.cpp
    # Phase 11B: More Consequence subclasses
    ${FIRMWARE_SRC}/deluge/model/consequence/consequence_clip_horizontal_shift.cpp
    ${FIRMWARE_SRC}/deluge/model/consequence/consequence_clip_length.cpp
    ${FIRMWARE_SRC}/deluge/model/consequence/consequence_note_row_length.cpp
    # Phase 12A: Leaf files (trivial deps)
    ${FIRMWARE_SRC}/deluge/hid/display/numeric_layer/numeric_layer.cpp
    ${FIRMWARE_SRC}/deluge/storage/multi_range/multi_range.cpp
    ${FIRMWARE_SRC}/deluge/storage/audio/audio_file_vector.cpp
    # Phase 12B: DSP Delay
    ${FIRMWARE_SRC}/deluge/dsp/delay/delay.cpp
    # Phase 12C: Granular DSP
    ${FIRMWARE_SRC}/deluge/dsp/granular/GranularProcessor.cpp
    # Phase 12D: HID encoder (GPIO mocked)
    ${FIRMWARE_SRC}/deluge/hid/encoder.cpp
    # Phase 12E: Modulation param
    ${FIRMWARE_SRC}/deluge/modulation/params/param.cpp
    # Phase 12F: Now unlocked by param_set.h compiling through
    ${FIRMWARE_SRC}/deluge/model/mod_controllable/ModFXProcessor.cpp
    # Phase 14A: Clip instance consequences (Output shadow unlocks instrument.h)
    ${FIRMWARE_SRC}/deluge/model/consequence/consequence_clip_instance_change.cpp
    ${FIRMWARE_SRC}/deluge/model/consequence/consequence_clip_instance_existence.cpp
    ${FIRMWARE_SRC}/deluge/model/consequence/consequence_output_existence.cpp
    # Phase 15A: Storage multi-range + wave_table chain
    ${FIRMWARE_SRC}/deluge/storage/wave_table/wave_table_holder.cpp
    ${FIRMWARE_SRC}/deluge/storage/multi_range/multi_wave_table_range.cpp
    ${FIRMWARE_SRC}/deluge/storage/multi_range/multisample_range.cpp
    ${FIRMWARE_SRC}/deluge/storage/multi_range/multi_range_array.cpp
    # Phase 15B: Note-level consequence subclasses
    ${FIRMWARE_SRC}/deluge/model/consequence/consequence_note_array_change.cpp
    ${FIRMWARE_SRC}/deluge/model/consequence/consequence_note_existence.cpp
    ${FIRMWARE_SRC}/deluge/model/consequence/consequence_note_row_horizontal_shift.cpp
    ${FIRMWARE_SRC}/deluge/model/consequence/consequence_note_row_mute.cpp
    # Phase 16: clip_instance (real), consequence_instrument_clip_multiply,
    #           consequence_performance_view_press, DX7Cartridge
    ${FIRMWARE_SRC}/deluge/model/clip/clip_instance.cpp
    ${FIRMWARE_SRC}/deluge/model/consequence/consequence_instrument_clip_multiply.cpp
    ${FIRMWARE_SRC}/deluge/model/consequence/consequence_performance_view_press.cpp
    ${FIRMWARE_SRC}/deluge/storage/DX7Cartridge.cpp
)
target_include_directories(deluge_firmware_src PUBLIC
    src/mocks
    ${FIRMWARE_SRC}
    ${FIRMWARE_SRC}/deluge
)
target_link_libraries(deluge_firmware_src PUBLIC deluge_mocks etl::etl)

# ── Test harness library ─────────────────────────────────────────────────
add_library(deluge_harness STATIC
    src/harness/test_fixture.cpp
)
target_include_directories(deluge_harness PUBLIC
    src/harness
    src/mocks
    ${FIRMWARE_SRC}
    ${FIRMWARE_SRC}/deluge
)
target_link_libraries(deluge_harness PUBLIC deluge_mocks deluge_firmware_src CppUTestExt etl::etl)

# ── Tests ─────────────────────────────────────────────────────────────────
enable_testing()

add_executable(HarnessTests
    tests/run_all.cpp
    tests/smoke_test.cpp
    tests/model/scale_test.cpp
    tests/model/sync_test.cpp
    tests/model/chord_test.cpp
    tests/model/clip_iterators_test.cpp
    tests/modulation/lfo_test.cpp
    tests/model/resizeable_array_test.cpp
    # Phase 2C
    tests/modulation/param_node_vector_test.cpp
    tests/model/note_vector_test.cpp
    tests/model/model_stack_test.cpp
    # Phase 3
    tests/modulation/envelope_test.cpp
    tests/modulation/param_collection_test.cpp
    tests/modulation/patch_cable_test.cpp
    tests/modulation/auto_param_test.cpp
    tests/modulation/midi_containers_test.cpp
    # Phase 4
    tests/util/semver_test.cpp
    tests/util/firmware_version_test.cpp
    tests/util/phase_increment_fine_tuner_test.cpp
    tests/gui/colour_test.cpp
    tests/modulation/sidechain_test.cpp
    # Phase 5
    tests/dsp/freeverb_test.cpp
    tests/util/d_string_test.cpp
    tests/util/d_stringbuf_test.cpp
    # Phase 6
    tests/dsp/svf_test.cpp
    tests/dsp/lpladder_test.cpp
    tests/dsp/hpladder_test.cpp
    tests/dsp/absolute_value_test.cpp
    tests/model/midi_message_test.cpp
    # Phase 7
    tests/model/filter_config_test.cpp
    tests/dsp/filter_set_test.cpp
    tests/util/string_test.cpp
    tests/dsp/dx7_env_test.cpp
    tests/dsp/dx7_pitchenv_test.cpp
    tests/dsp/rms_feedback_test.cpp
    # Phase 8
    tests/model/leaf_models_test.cpp
    tests/model/container_vectors_test.cpp
    tests/dsp/delay_buffer_test.cpp
    tests/gui/l10n_test.cpp
    tests/util/c_string_array_test.cpp
    # Phase 9
    tests/dsp/dx7_synth_test.cpp
    tests/util/containers_test.cpp
    tests/hid/hid_test.cpp
    tests/model/sample_controls_test.cpp
    # Phase 10
    tests/model/consequence_test.cpp
    tests/model/mod_controllable_test.cpp
    tests/util/named_thing_vector_test.cpp
    # Phase 11
    tests/util/waves_test.cpp
    tests/playback/playback_mode_test.cpp
    tests/model/consequence_clip_test.cpp
    # Phase 12
    tests/dsp/delay_test.cpp
    tests/dsp/granular_test.cpp
    tests/hid/encoder_test.cpp
    tests/modulation/param_test.cpp
    tests/model/modfx_processor_test.cpp
    tests/model/leaf_phase12_test.cpp
    # Phase 14: Clip instance consequences + iterance
    tests/model/consequence_clip_instance_test.cpp
    tests/model/iterance_test.cpp
    # Phase 14: Value scaling tests
    tests/gui/value_scaling_test.cpp
    # Phase 13: Smoke tests — lifecycle & scenario coverage
    tests/dsp/delay_smoke_test.cpp
    tests/dsp/granular_smoke_test.cpp
    tests/dsp/filter_set_smoke_test.cpp
    # Phase 15: Storage multi-range + note consequences
    tests/storage/multi_range_test.cpp
    tests/model/consequence_note_test.cpp
    # Phase 16: clip_instance, clip_multiply, performance_view_press, DX7Cartridge
    tests/model/consequence_clip_multiply_test.cpp
    tests/model/consequence_perf_view_test.cpp
    tests/storage/dx7_cartridge_test.cpp
    # Phase 17: Coverage gap tests
    tests/modulation/param_roundtrip_test.cpp
    tests/model/note_edge_case_test.cpp
    tests/util/c_string_array_edge_test.cpp
    tests/dsp/filter_resonance_extreme_test.cpp
    tests/storage/dx7_algorithm_test.cpp
    # Phase 18: all-fixes branch coverage
    tests/model/time_signature_test.cpp
)
target_link_libraries(HarnessTests PRIVATE deluge_harness)
target_include_directories(HarnessTests PRIVATE
    src/harness
    src/mocks
    ${FIRMWARE_SRC}
    ${FIRMWARE_SRC}/deluge
)
add_test(NAME HarnessTests COMMAND HarnessTests)
