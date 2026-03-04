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
target_link_libraries(deluge_harness PUBLIC deluge_mocks CppUTestExt etl::etl)

# ── Tests ─────────────────────────────────────────────────────────────────
enable_testing()

add_executable(HarnessTests
    tests/run_all.cpp
    tests/smoke_test.cpp
)
target_link_libraries(HarnessTests PRIVATE deluge_harness)
target_include_directories(HarnessTests PRIVATE
    src/harness
    src/mocks
    ${FIRMWARE_SRC}
    ${FIRMWARE_SRC}/deluge
)
add_test(NAME HarnessTests COMMAND HarnessTests)
