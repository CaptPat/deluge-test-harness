# Bug Reports for SynthstromAudible/DelugeFirmware

Firmware version: `nightly` (commit `5bdf3166`)

---

## Bug 1: GranularProcessor copy constructor uses uninitialized `grainBuffer` pointer

**Status:** Fixed upstream via PR [#4371](https://github.com/SynthstromAudible/DelugeFirmware/pull/4371) (default member initializers)

**GitHub issue:** [#4356](https://github.com/SynthstromAudible/DelugeFirmware/issues/4356)

**Description:**

The `GranularProcessor` copy constructor (`GranularProcessor.cpp:323`) calls `getBuffer()` at line 338 without first setting `grainBuffer = nullptr`. The `grainBuffer` member has no default member initializer (`GranularProcessor.h:87`), so in the copy constructor it holds an indeterminate value.

`getBuffer()` (`GranularProcessor.cpp:305`) checks `if (grainBuffer == nullptr)` to decide whether to allocate. If `grainBuffer` is non-null garbage, it falls through to the `else` branch at line 313 and executes `grainBuffer->inUse = true` — dereferencing a garbage pointer.

The default constructor correctly does `grainBuffer = nullptr` before calling `getBuffer()` (line 302), but the copy constructor omits this.

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

**Files:** `src/deluge/dsp/granular/GranularProcessor.cpp` lines 323-339, `src/deluge/dsp/granular/GranularProcessor.h` line 87

---

## Bug 2: `grainBufferStolen()` doesn't reset processor state

**Status:** Closed as not-a-bug ([#4357](https://github.com/SynthstromAudible/DelugeFirmware/issues/4357)). Upstream determined the buffer can't be stolen while grains are active, so the extra resets are unnecessary. A revert PR ([#4370](https://github.com/SynthstromAudible/DelugeFirmware/pull/4370)) confirmed this reasoning. Regression test in `granular_smoke_test.cpp` guards against future removal of the existing safety path.

**Description:**

When the Stealable system steals the grain buffer, it calls `GranularProcessor::grainBufferStolen()` (`GranularProcessor.h:62`), which only sets `grainBuffer = nullptr`. It does not reset `wrapsToShutdown`, `grainInitialized`, `bufferFull`, or the grain array.

On the next call to `processGrainFX()` (`GranularProcessor.cpp:47`), the entry condition `anySoundComingIn || wrapsToShutdown >= 0` is true because `wrapsToShutdown` retains its pre-steal value. The code then:

1. Calls `getBuffer()` at line 55, which allocates a **fresh, uninitialized** buffer
2. Proceeds to `processOneGrainSample()` at line 63, where active grains (with stale `length > 0` from before the steal) read from the **uninitialized** buffer at arbitrary offsets

**Files:** `src/deluge/dsp/granular/GranularProcessor.h` line 62, `src/deluge/dsp/granular/GranularProcessor.cpp` lines 47-87

---

## Bug 3: `GranularProcessor` destructor uses `delete` on placement-new'd memory from custom allocator

**Status:** Reported as [#4358](https://github.com/SynthstromAudible/DelugeFirmware/issues/4358). Still open upstream.

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

**Files:** `src/deluge/dsp/granular/GranularProcessor.cpp` lines 305-312 (allocation), line 321 (deallocation)

---

## Bug 4: Missing `return` in `to_chars()` — buffer overflow falls through silently

**Status:** Reported as [#4372](https://github.com/SynthstromAudible/DelugeFirmware/issues/4372). Fixed in our harness build (commit `f7548a58`).

**Description:**

In `util/string.cpp:25`, `std::unexpected{...}` is constructed but never returned (missing `return`). The error path falls through, causing a buffer overflow that silently corrupts memory.

**Files:** `src/deluge/util/string.cpp` line 25

---

## Bug 5: `setSynthMode()` restores filter modes after `setupPatchingForAllParamManagers()`

**Status:** Reported as [#4232](https://github.com/SynthstromAudible/DelugeFirmware/issues/4232). Still open upstream.

**Description:**

When switching from FM to subtractive synth mode, `setSynthMode()` calls `setupPatchingForAllParamManagers()` before restoring `lpfMode`/`hpfMode`. This means patch cables see stale `lpfMode=OFF` during setup, potentially leaving filter routing in an incorrect state.

Fix exists on local branch `bugfix/patch-cables-fm-subtractive-switch` (commit `ec0f21d3` in firmware repo). PR [#4344](https://github.com/SynthstromAudible/DelugeFirmware/pull/4344) was closed; not resubmitting — letting upstream handle via the public issue.

**Files:** `src/deluge/model/instrument/kit.cpp`, `src/deluge/processing/sound/sound.cpp`
