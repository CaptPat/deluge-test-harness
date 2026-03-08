// Minimal stubs for UnpatchedParamSet + ParamSet — just enough to construct
// and call getValue() for ModFX render tests.
#include "modulation/params/param_collection_summary.h"
#include "modulation/params/param_set.h"

// ParamSet constructor
ParamSet::ParamSet(int32_t newObjectSize, ParamCollectionSummary* summary)
    : ParamCollection(newObjectSize, summary), numParams_(0), params(nullptr), topUintToRepParams(1) {
}

// UnpatchedParamSet constructor
UnpatchedParamSet::UnpatchedParamSet(ParamCollectionSummary* summary) : ParamSet(sizeof(UnpatchedParamSet), summary) {
	params = params_.data();
	numParams_ = static_cast<int32_t>(params_.size());
	topUintToRepParams = (numParams_ - 1) >> 5;
}

// Virtual overrides — stub implementations (not called in our tests)
void UnpatchedParamSet::beenCloned(bool, int32_t) {}
bool UnpatchedParamSet::shouldParamIndicateMiddleValue(ModelStackWithParamId const*) {
	return false;
}
bool UnpatchedParamSet::doesParamIdAllowAutomation(ModelStackWithParamId const*) {
	return true;
}
int32_t UnpatchedParamSet::paramValueToKnobPos(int32_t paramValue, ModelStackWithAutoParam*) {
	return paramValue;
}
int32_t UnpatchedParamSet::knobPosToParamValue(int32_t knobPos, ModelStackWithAutoParam*) {
	return knobPos;
}

// ParamSet virtual overrides needed for vtable
void ParamSet::processCurrentPos(ModelStackWithParamCollection*, int32_t, bool, bool, bool) {}
void ParamSet::tickSamples(int32_t, ModelStackWithParamCollection*) {}
void ParamSet::tickTicks(int32_t, ModelStackWithParamCollection*) {}
void ParamSet::playbackHasEnded(ModelStackWithParamCollection*) {}
void ParamSet::grabValuesFromPos(uint32_t, ModelStackWithParamCollection*) {}
void ParamSet::generateRepeats(ModelStackWithParamCollection*, uint32_t, uint32_t, bool) {}
void ParamSet::appendParamCollection(ModelStackWithParamCollection*, ModelStackWithParamCollection*, int32_t, int32_t,
                                     bool) {}
void ParamSet::beenCloned(bool, int32_t) {}
void ParamSet::trimToLength(uint32_t, ModelStackWithParamCollection*, Action*, bool) {}
void ParamSet::shiftHorizontally(ModelStackWithParamCollection*, int32_t, int32_t) {}
void ParamSet::remotelySwapParamState(AutoParamState*, ModelStackWithParamId*) {}
void ParamSet::deleteAllAutomation(Action*, ModelStackWithParamCollection*) {}
void ParamSet::nudgeNonInterpolatingNodesAtPos(int32_t, int32_t, int32_t, Action*, ModelStackWithParamCollection*) {}
void ParamSet::notifyParamModifiedInSomeWay(ModelStackWithAutoParam const*, int32_t, bool, bool, bool) {}
ModelStackWithAutoParam* ParamSet::getAutoParamFromId(ModelStackWithParamId*, bool) {
	return nullptr;
}
void ParamSet::notifyPingpongOccurred(ModelStackWithParamCollection*) {}
void ParamSet::setPlayPos(uint32_t, ModelStackWithParamCollection*, bool) {}
