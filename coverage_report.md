# Deluge Test Harness - Code Coverage Report

Generated: 2026-04-08
Branch: personal/nightly (34 branches merged)
Tool: gcov (GCC 15.2.0) with --coverage instrumentation
Tests: 3,761 tests (3,757 ran, 4 ignored, 6 known failures)

## Firmware Size (pygount, code lines only — excludes comments/blanks)

| Layer | Code Lines | Testable on x86? |
|-------|-----------|------------------|
| Deluge core (src/deluge/) | 131,168 | Partially — requires mocks |
| RZA1 HAL/drivers (src/RZA1/) | 64,796 | No — ARM hardware |
| FatFS (src/fatfs/) | 20,698 | No — hardware DMA |
| **Total firmware** | **216,662** | |

## Test Harness Coverage

| Metric | Value |
|--------|-------|
| x86 tests | 3,761 |
| Files compiled into harness | 363 |
| Instrumentable lines (harness) | 9,841 |
| Lines executed | 7,682 |
| Harness coverage (of compiled code) | 78.1% |
| **Effective coverage (of Deluge core)** | **5.9%** (7,682 / 131,168) |
| **Effective coverage (of total firmware)** | **3.5%** (7,682 / 216,662) |

### Why effective coverage is low

The test harness can only compile firmware code that doesn't directly call hardware.
Every new firmware module pulled into the harness requires mocking its hardware
dependencies (DMA, GPIO, SSI, PIC, SDHI, USB). Each mock is hand-written.
Current mocks cover: audio engine (partial), display, memory allocator, MIDI engine,
timer, print/debug. Major unmocked areas: SD/FatFS, USB stack, SSI audio DMA,
PIC protocol, interrupt handlers, boot sequence.

## Testing Modes

The Deluge firmware is now tested across five complementary modes:

### 1. x86 Test Harness (CppUTest)
- **3,761 tests** covering DSP, model, storage, modulation, playback, UI logic
- Runs on host (x86) with mocked hardware
- Coverage: 78.1% of instrumented firmware source lines
- CI: runs on every push via GitHub Actions

### 2. On-Device Self-Test (RECORD + boot)
- **27 tests**: 16 POST hardware validation + 11 regression tests
- Tests: battery, SDRAM, SD card, display, audio codec, headphones, line I/O, mic, speaker, USB MIDI, firmware version, CPU speed, RAM usage
- Regression: patch cables, envelope sustain, arp probability, string overflow, notes iterator, multisample octave, reverb filter, song cast guard, L2 cache, SD DMA, MIDI DMA
- Pad LED grid: green/red per test, OLED status, audio feedback tones

### 3. On-Device Demo Mode (PLAY + boot)
- Autonomous song cycling (90-120s per song) with section switching (15-20s)
- Exercises: audio engine, sample streaming, SD I/O, song swap, voice allocation
- Used for burn-in and stability testing under realistic load

### 4. MIDI Stress Testing (USB + DIN)
- Python scripts send CC, note, sustain/sostenuto/soft pedal messages via USB
- DIN MIDI monitored via Cable Matters USB-to-DIN adapter for thru validation
- Verified: 184K messages over 5 minutes, zero errors, zero stuck notes
- Demo mode + MIDI stress: tests L2 cache coherency under simultaneous audio + MIDI + SD load

### 5. Settings Menu Benchmark (L2 Cache Bench)
- SETTINGS > L2 Cache Bench: A/B comparison of L2 OFF vs ON
- SDRAM read throughput: 512KB x 50 iterations
- Results: 712ms to 285ms (+59%), 36 to 90 MB/s

## Hardware Test Equipment

| Equipment | Purpose |
|-----------|---------|
| Synthstrom Deluge (OLED, RZ/A1L) | Target device |
| Cable Matters USB-to-DIN MIDI adapter | DIN MIDI capture/send |
| USB cable to PC | USB MIDI + firmware loading |
| SD card (Class 10) | Firmware, songs, samples |

## Test Counts by Area

| Area | Tests | Notes |
|------|-------|-------|
| DSP (filters, delay, reverb, compressor, DX7) | ~800 | Deep parameter sweeps |
| Model (song, clip, note, scale, chord) | ~600 | Serialization, iterators |
| Modulation (envelope, LFO, arpeggiator, params) | ~500 | Patch cables, param managers |
| Playback (transport, clock, MIDI follow) | ~200 | Clock scheduling, dedup |
| Storage (XML, learned MIDI, file paths) | ~300 | Round-trip serialization |
| GUI/HID (keyboard, velocity, menus) | ~200 | Layout, community menu consistency |
| Meta (source contracts, upstream bug guards) | ~400 | Structural verification |
| On-device (POST + regression) | 27 | Hardware validation |
| Stress (MIDI, demo mode) | per-session | Manual/scripted |
