// Shadow header replacing firmware's model/action/action.h
// auto_param.h includes this and references ActionType::NOTE_EDIT as a
// default parameter value.

#pragma once

#include <cstdint>

class ModelStack;
class ModelStackWithAutoParam;
class ParamCollection;

enum class ActionType {
	MISC,
	NOTE_EDIT,
	NOTE_TAIL_EXTEND,
	CLIP_LENGTH_INCREASE,
	CLIP_LENGTH_DECREASE,
	RECORD,
	AUTOMATION_DELETE,
	PARAM_UNAUTOMATED_VALUE_CHANGE,
	SWING_CHANGE,
	TEMPO_CHANGE,
	CLIP_MULTIPLY,
	CLIP_CLEAR,
	CLIP_DELETE,
	NOTES_PASTE,
	PATTERN_PASTE,
	AUTOMATION_PASTE,
	CLIP_INSTANCE_EDIT,
	ARRANGEMENT_TIME_EXPAND,
	ARRANGEMENT_TIME_CONTRACT,
	ARRANGEMENT_CLEAR,
	ARRANGEMENT_RECORD,
	CLIP_HORIZONTAL_SHIFT,
	NOTE_NUDGE,
	NOTE_REPEAT_EDIT,
	EUCLIDEAN_NUM_EVENTS_EDIT,
	NOTEROW_ROTATE,
	NOTEROW_LENGTH_EDIT,
	NOTEROW_HORIZONTAL_SHIFT,
};

class Consequence;

class Action {
public:
	Action() = default;
	void addConsequence(Consequence* consequence) { (void)consequence; }
};
