// Mock for FlashStorage namespace globals referenced by firmware code.

#include "storage/flash_storage.h"
#include "modulation/patch/patch_cable.h" // for Polarity enum

namespace FlashStorage {
bool defaultUseSharps;
Polarity defaultPatchCablePolarity = Polarity::BIPOLAR;
int8_t defaultMagnitude = 0;
uint8_t defaultBendRange[2] = {12, 48}; // BEND_RANGE_MAIN=12, BEND_RANGE_FINGER_LEVEL=48
}
