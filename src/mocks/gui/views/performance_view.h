// Shadow header replacing firmware's gui/views/performance_view.h
// The real header pulls in clip_navigation_timeline_view.h and storage_manager.h.
// We provide only the types needed by consequence_performance_view_press.

#pragma once

#include "definitions_cxx.hpp"
#include "modulation/params/param.h"
#include <cstdint>

struct FXColumnPress {
	int32_t previousKnobPosition;
	int32_t currentKnobPosition;
	int32_t yDisplay;
	uint32_t timeLastPadPress;
	bool padPressHeld;
};

class PerformanceView {
public:
	FXColumnPress fxPress[kDisplayWidth];
};

extern PerformanceView performanceView;
