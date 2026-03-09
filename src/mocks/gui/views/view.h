// Shadow header replacing firmware's gui/views/view.h
// The real header pulls in clip_navigation_timeline_view.h and heavy GUI deps.
// We provide only the View stub needed by param_manager.cpp and patch_cable_set.cpp.

#pragma once
#include <cstdint>

class ParamManager;

class View {
public:
	void notifyParamAutomationOccurred(ParamManager* /*paramManager*/, bool /*updateModLevels*/ = true) {}
};

extern View view;
