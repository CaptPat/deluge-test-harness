#pragma once

#include <cstdint>
#include <vector>

// Mock audio engine that captures rendered samples instead of writing to DMA.
namespace MockAudioEngine {

inline std::vector<int32_t> capturedSamplesL;
inline std::vector<int32_t> capturedSamplesR;
inline bool isPlaying = false;

void reset();
void routine(); // No-op render step
bool hasOutput();
int32_t getSampleCount();

} // namespace MockAudioEngine
