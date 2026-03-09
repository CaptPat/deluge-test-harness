// Shadow header replacing firmware's gui/views/automation_view.h
// param_set.cpp includes this. Only needs isUIModeActive (from gui/ui/ui.h transitively).

#pragma once
#include <cstdint>

#define UI_MODE_STUTTERING (1 << 28)

inline bool isUIModeActive(uint32_t /*uiMode*/) { return false; }
