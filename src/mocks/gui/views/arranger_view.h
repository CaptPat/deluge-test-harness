// Shadow header replacing firmware's gui/views/arranger_view.h
// clip_minder.cpp calls arrangerView.transitionToArrangementEditor().
// Provide a stub that always returns false (no transition).

#pragma once

class ArrangerView {
public:
	bool transitionToArrangementEditor() { return false; }
};

inline ArrangerView arrangerView;
