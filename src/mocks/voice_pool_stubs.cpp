// VoicePool stub implementation.
// Defined here (not in the header) so that Voice is fully defined.

#include "processing/engines/audio_engine.h"
#include "model/voice/voice.h"
#include "processing/sound/sound_instrument.h"
#include "memory/general_memory_allocator.h"
#include <new>

namespace AudioEngine {

void VoicePool::recycle(Voice* obj) {
	if (obj) {
		obj->~Voice();
		GeneralMemoryAllocator::get().dealloc(obj);
	}
}

VoicePool& VoicePool::get() {
	static thread_local VoicePool instance;
	return instance;
}

template <typename... Args>
VoicePool::pointer_type VoicePool::acquire(Args&&... args) {
	void* mem = GeneralMemoryAllocator::get().alloc(sizeof(Voice), false, false, nullptr);
	if (!mem) {
		throw std::bad_alloc();
	}
	return pointer_type(new (mem) Voice(std::forward<Args>(args)...), &recycle);
}

// Explicit instantiations for constructor signatures used in tests
template VoicePool::pointer_type VoicePool::acquire<Sound&>(Sound&);
template VoicePool::pointer_type VoicePool::acquire<SoundInstrument&>(SoundInstrument&);

void VoicePool::clear() {}
bool VoicePool::empty() const { return true; }

} // namespace AudioEngine
