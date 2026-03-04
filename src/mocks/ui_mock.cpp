#include "ui_mock.h"

namespace MockUI {

void reset() { currentView = ViewType::NONE; }

void setView(ViewType v) { currentView = v; }

ViewType getView() { return currentView; }

} // namespace MockUI
