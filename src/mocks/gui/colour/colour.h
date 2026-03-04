// Shadow header for gui/colour/colour.h
// Passes through to the real firmware RGB headers.
// Guards against Windows RGB macro from wingdi.h.

#pragma once

#ifdef RGB
#undef RGB
#endif

// Include the real firmware colour headers
// palette.h defines `using Colour = RGB` inside namespace deluge::gui::colours
#include "gui/colour/rgb.h"
#include "gui/colour/palette.h"
