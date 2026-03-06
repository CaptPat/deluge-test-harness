# Bug Reports for SynthstromAudible/DelugeFirmware

Firmware version: `nightly` (commit `5bdf3166`)

---

## Bug 1: GranularProcessor copy constructor uses uninitialized `grainBuffer` pointer

**Title:** GranularProcessor copy constructor doesn't initialize `grainBuffer` before calling `getBuffer()`, causing UB

**Description:**

The `GranularProcessor` copy constructor (`GranularProcessor.cpp:323`) calls `getBuffer()` at line 338 without first setting `grainBuffer = nullptr`. The `grainBuffer` member has no default member initializer (`GranularProcessor.h:87`), so in the copy constructor it holds an indeterminate value.

`getBuffer()` (`GranularProcessor.cpp:305`) checks `if (grainBuffer == nullptr)` to decide whether to allocate. If `grainBuffer` is non-null garbage, it falls through to the `else` branch at line 313 and executes `grainBuffer->inUse = true` — dereferencing a garbage pointer.

The default constructor correctly does `grainBuffer = nullptr` before calling `getBuffer()` (line 302), but the copy constructor omits this.

**Relevant code:**

```cpp
// GranularProcessor.cpp:323
GranularProcessor::GranularProcessor(const GranularProcessor& other) {
    wrapsToShutdown = other.wrapsToShutdown;
    bufferWriteIndex = other.bufferWriteIndex;
    // ... other members copied ...
    grainInitialized = false;
    getBuffer();  // BUG: grainBuffer is uninitialized here
}

// GranularProcessor.cpp:305
void GranularProcessor::getBuffer() {
    if (grainBuffer == nullptr) {       // may be non-null garbage
        // ... allocates new buffer ...
    } else {
        grainBuffer->inUse = true;      // dereferences garbage pointer
    }
}
```

**Expected behavior:** The copy constructor should set `grainBuffer = nullptr` before calling `getBuffer()`, matching the default constructor's pattern.

**Suggested fix:**

```cpp
GranularProcessor::GranularProcessor(const GranularProcessor& other) {
    // ... existing member copies ...
    grainInitialized = false;
    grainBuffer = nullptr;   // ADD THIS LINE
    getBuffer();
}
```

Alternatively, add a default member initializer to the header: `GrainBuffer* grainBuffer{nullptr};`

**Steps to reproduce:** Copy-construct a `GranularProcessor` from an existing instance. On x86-64, uninitialized stack memory is likely non-zero, making the crash near-certain. On ARM (Deluge hardware), stack memory patterns differ but UB remains.

**Files:** `src/deluge/dsp/granular/GranularProcessor.cpp` lines 323-339, `src/deluge/dsp/granular/GranularProcessor.h` line 87

**Existing PRs:** None found.

---

## Bug 2: `grainBufferStolen()` doesn't reset `wrapsToShutdown`, causing stale shutdown timer and potential UB

**Title:** `grainBufferStolen()` leaves `wrapsToShutdown` in stale state, allowing processing with uninitialized buffer

**Description:**

When the Stealable system steals the grain buffer, it calls `GranularProcessor::grainBufferStolen()` (`GranularProcessor.h:62`), which only sets `grainBuffer = nullptr`. It does not reset `wrapsToShutdown`, `grainInitialized`, `bufferFull`, or the grain array.

On the next call to `processGrainFX()` (`GranularProcessor.cpp:47`), the entry condition `anySoundComingIn || wrapsToShutdown >= 0` is true because `wrapsToShutdown` retains its pre-steal value. The code then:

1. Calls `getBuffer()` at line 55, which allocates a **fresh, uninitialized** buffer
2. Proceeds to `processOneGrainSample()` at line 63, where active grains (with stale `length > 0` from before the steal) read from the **uninitialized** buffer at arbitrary offsets

This produces garbage audio output and is technically undefined behavior (reading uninitialized memory).

**Relevant code:**

```cpp
// GranularProcessor.h:62
void grainBufferStolen() { grainBuffer = nullptr; }
// Does NOT reset: wrapsToShutdown, grainInitialized, bufferFull, grains[]

// GranularProcessor.cpp:47-59
void GranularProcessor::processGrainFX(...) {
    if (anySoundComingIn || wrapsToShutdown >= 0) {  // stale wrapsToShutdown passes
        if (anySoundComingIn) {
            setWrapsToShutdown();
        }
        if (grainBuffer == nullptr) {
            getBuffer();                // allocates fresh UNINITIALIZED buffer
            if (grainBuffer == nullptr) {
                return;
            }
        }
        // ... processes grains that read from uninitialized buffer ...
    }
}
```

**Expected behavior:** `grainBufferStolen()` should reset the processor to a clean state, similar to `clearGrainFXBuffer()`:

- Set `wrapsToShutdown = 0`
- Set `grainInitialized = false`
- Set `bufferFull = false`
- Zero all grain lengths (`grains[i].length = 0`)

This ensures that after a steal, re-entry to `processGrainFX` either exits early (if `wrapsToShutdown` is 0 and no sound coming in) or starts fresh (grains won't read stale positions).

**Suggested fix:**

```cpp
void grainBufferStolen() {
    grainBuffer = nullptr;
    wrapsToShutdown = 0;
    grainInitialized = false;
    bufferFull = false;
    for (auto& grain : grains) {
        grain.length = 0;
    }
}
```

**Steps to reproduce:**

1. Create a GranularProcessor and process several blocks of audio to fill the buffer and activate grains
2. Call `grainBufferStolen()` (simulating Stealable system reclaiming the 4MB buffer under memory pressure)
3. Call `processGrainFX()` again with `anySoundComingIn = true`
4. Grains read from freshly allocated, uninitialized buffer memory

**Files:** `src/deluge/dsp/granular/GranularProcessor.h` line 62, `src/deluge/dsp/granular/GranularProcessor.cpp` lines 47-87

**Existing PRs:** None found.

---

## Bug 3: `GranularProcessor` destructor uses `delete` on placement-new'd memory from custom allocator

**Title:** GranularProcessor destructor calls `delete grainBuffer` on memory allocated via `GeneralMemoryAllocator::allocStealable`

**Description:**

The `GranularProcessor` destructor (`GranularProcessor.cpp:320`) calls `delete grainBuffer`. However, `grainBuffer` is allocated via placement new on memory from `GeneralMemoryAllocator::get().allocStealable()` (`GranularProcessor.cpp:307`):

```cpp
void* grainBufferMemory = GeneralMemoryAllocator::get().allocStealable(sizeof(GrainBuffer));
if (grainBufferMemory) {
    grainBuffer = new (grainBufferMemory) GrainBuffer(this);
}
```

Using `delete` on placement-new'd memory from a custom allocator is undefined behavior. The correct approach would be to explicitly call the destructor and then free via the custom allocator:

```cpp
GranularProcessor::~GranularProcessor() {
    if (grainBuffer) {
        grainBuffer->~GrainBuffer();
        GeneralMemoryAllocator::get().dealloc(grainBuffer);
    }
}
```

On the Deluge's ARM target, this may happen to work if `GeneralMemoryAllocator` wraps the same heap as global `operator delete`, but it is not guaranteed and is formally UB.

**Expected behavior:** Memory allocated via a custom allocator should be freed via the same allocator.

**Steps to reproduce:** Construct and then destroy a `GranularProcessor`.

**Files:** `src/deluge/dsp/granular/GranularProcessor.cpp` lines 305-312 (allocation), line 321 (deallocation)

**Existing PRs:** None found.
