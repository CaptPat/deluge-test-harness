// Mock for FlashStorage namespace globals referenced by firmware code.

#include "storage/flash_storage.h"
#include "modulation/patch/patch_cable.h" // for Polarity enum

namespace FlashStorage {
bool defaultUseSharps;
Polarity defaultPatchCablePolarity = Polarity::BIPOLAR;
int8_t defaultMagnitude = 0;
}
