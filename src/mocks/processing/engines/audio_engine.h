// Shadow header replacing firmware's processing/engines/audio_engine.h for x86.
// The real header includes fatfs/ff.h, OSLikeStuff/scheduler_api.h, model/output.h, etc.
// We provide only the declarations used by compiled firmware code.

#pragma once

#include <cstdint>
#include "util/functions.h"

namespace AudioEngine {
inline bool bypassCulling = false;
inline uint32_t nextVoiceState = 0;     // Envelope state tracking
inline uint32_t audioSampleTimer = 0;   // Sample timer
inline int32_t cpuDireness = 0;         // Phase 6: used by lpladder.cpp for oversampling decisions
inline bool renderInStereo = true;      // Phase 8: used by ModFXProcessor, delay
inline bool headphonesPluggedIn = false;
inline bool micPluggedIn = false;
inline bool lineInPluggedIn = false;
inline bool audioRoutineLocked = false;
inline bool mustUpdateReverbParamsBeforeNextRender = false;
inline void routineWithClusterLoading() {} // Called by NoteRowVector destructor
inline void logAction(char const*) {}
inline void logAction(int32_t) {}

// Phase 12: GranularProcessor stubs
inline void feedReverbBackdoorForGrain(int32_t l, int32_t r) { (void)l; (void)r; }
} // namespace AudioEngine
