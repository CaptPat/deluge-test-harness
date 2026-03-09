// Shadow header replacing firmware's model/action/action_logger.h
// param_set.cpp calls action->recordParamChangeIfNotAlreadySnapshotted() which is on Action, not ActionLogger.
// We just need the ActionLogger class + extern global for linking.

#pragma once

#include "model/action/action.h"

class ModelStackWithParamCollection;

class ActionLogger {
public:
	ActionLogger() = default;
};

extern ActionLogger actionLogger;
