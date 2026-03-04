#include "audio_engine_mock.h"

namespace MockAudioEngine {

void reset() {
	capturedSamplesL.clear();
	capturedSamplesR.clear();
	isPlaying = false;
}

void routine() {
	// No-op — real integration would render voices into capturedSamples
}

bool hasOutput() { return !capturedSamplesL.empty(); }

int32_t getSampleCount() { return static_cast<int32_t>(capturedSamplesL.size()); }

} // namespace MockAudioEngine
