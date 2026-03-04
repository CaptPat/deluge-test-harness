// Shadow header replacing firmware's storage/flash_storage.h
// The real header pulls in preset_scales.h, <bitset>, <span> and dozens of
// extern declarations with firmware-specific types.  We provide only the
// symbols actually referenced by Phase 3 code.

#pragma once
#include "definitions_cxx.hpp"
#include <cstdint>

enum class Polarity : uint8_t;

namespace FlashStorage {
extern bool defaultUseSharps;
extern Polarity defaultPatchCablePolarity;
extern int8_t defaultMagnitude;
} // namespace FlashStorage
