// Shadow header replacing firmware's processing/engines/audio_engine.h for x86.
// Phase S: provides VoicePool with compatible pointer_type for Sound/Voice construction.
// Cannot use the real ObjectPool<Voice> here because sound.h needs VoicePool::pointer_type
// before Voice is fully defined. Instead we provide a compatible stub.

#pragma once

#include <cstdint>
#include <memory>

#include "memory/fast_allocator.h"
#include "util/containers.h"
#include "util/functions.h"

// Forward declarations matching the real audio_engine.h
class Song;
class StereoSample;
class Instrument;
class Sound;
class ParamManagerForTimeline;
class LiveInputBuffer;
class SampleRecorder;
class Voice;
class VoiceSample;
class TimeStretcher;
class String;
class SideChain;
class VoiceVector;
class SoundDrum;
class Metronome;
class RMSFeedbackCompressor;
class FilePointer;
class Output;

namespace AudioEngine {

// Lightweight VoicePool stub that provides the same pointer_type as the real
// ObjectPool<Voice, fast_allocator> without requiring Voice to be complete.
// unique_ptr<Voice, ...> only needs Voice complete when deleting, not when declared.
struct VoicePool {
	using value_type = Voice;
	static void recycle(Voice* obj);
	using pointer_type = std::unique_ptr<Voice, decltype(&recycle)>;

	static VoicePool& get();
	template <typename... Args>
	[[nodiscard]] pointer_type acquire(Args&&... args);
	void clear();
	[[nodiscard]] bool empty() const;
};

// VoiceSample pool (not needed for current tests, just forward-declare)
struct VoiceSamplePool {
	using value_type = VoiceSample;
};

// Globals used by compiled firmware code
inline bool bypassCulling = false;
inline uint32_t nextVoiceState = 0;
inline uint32_t audioSampleTimer = 0;
inline int32_t cpuDireness = 0;
inline bool renderInStereo = true;
inline bool headphonesPluggedIn = false;
inline bool micPluggedIn = false;
inline bool lineInPluggedIn = false;
inline bool audioRoutineLocked = false;
inline bool mustUpdateReverbParamsBeforeNextRender = false;

// Sound registry
inline deluge::fast_vector<Sound*> sounds;

// Function stubs
inline void routineWithClusterLoading(bool /*mayProcessUserActionsBetween*/ = false) {}
inline void logAction(char const*) {}
inline void logAction(int32_t) {}
inline void feedReverbBackdoorForGrain(int32_t l, int32_t r) { (void)l; (void)r; }
inline void registerSideChainHit(int32_t /*strength*/) {}
inline VoiceSample* solicitVoiceSample() { return nullptr; }
inline void voiceSampleUnassigned(VoiceSample* /*voiceSample*/) {}
inline TimeStretcher* solicitTimeStretcher() { return nullptr; }
inline void timeStretcherUnassigned(TimeStretcher* /*timeStretcher*/) {}

} // namespace AudioEngine
