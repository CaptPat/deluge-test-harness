# Deluge Test Harness - Code Coverage Report

Generated: 2026-04-08
Branch: personal/nightly (34 branches merged)
Tool: gcov (GCC 15.2.0) with --coverage instrumentation
Tests: 3,761 tests (3,757 ran, 4 ignored, 6 known failures)

## Overall Coverage

| Metric | Value | Change from 2026-03-18 |
|--------|-------|----------------------|
| Files measured | 363 | +194 |
| Total instrumentable lines | 9,841 | +212 |
| Lines executed | 7,682 | +437 |
| **Line coverage** | **78.1%** | **+2.9%** |

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
