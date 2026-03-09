// Stubs for MIDIParamCollection — not yet compiled from firmware source.
// ParamSet, UnpatchedParamSet, PatchedParamSet, ExpressionParamSet provided by real param_set.cpp (Phase I).
// PatchCableSet provided by real patch_cable_set.cpp (Phase I2).
#include "modulation/params/param_collection_summary.h"
#include "modulation/params/param_manager.h"
#include "modulation/params/param_set.h"
#include "modulation/midi/midi_param_collection.h"
#include "modulation/patch/patch_cable_set.h"

// Phase H2: MIDIParamCollection stubs — needed by param_manager.cpp
MIDIParamCollection::MIDIParamCollection(ParamCollectionSummary* summary)
    : ParamCollection(sizeof(MIDIParamCollection), summary) {}
MIDIParamCollection::~MIDIParamCollection() {}
void MIDIParamCollection::tickTicks(int32_t, ModelStackWithParamCollection*) {}
void MIDIParamCollection::setPlayPos(uint32_t, ModelStackWithParamCollection*, bool) {}
void MIDIParamCollection::generateRepeats(ModelStackWithParamCollection*, uint32_t, uint32_t, bool) {}
void MIDIParamCollection::appendParamCollection(ModelStackWithParamCollection*, ModelStackWithParamCollection*,
                                                int32_t, int32_t, bool) {}
void MIDIParamCollection::trimToLength(uint32_t, ModelStackWithParamCollection*, Action*, bool) {}
void MIDIParamCollection::shiftHorizontally(ModelStackWithParamCollection*, int32_t, int32_t) {}
void MIDIParamCollection::processCurrentPos(ModelStackWithParamCollection*, int32_t, bool, bool, bool) {}
void MIDIParamCollection::beenCloned(bool, int32_t) {}
void MIDIParamCollection::grabValuesFromPos(uint32_t, ModelStackWithParamCollection*) {}
void MIDIParamCollection::deleteAllAutomation(Action*, ModelStackWithParamCollection*) {}
void MIDIParamCollection::nudgeNonInterpolatingNodesAtPos(int32_t, int32_t, int32_t, Action*,
                                                          ModelStackWithParamCollection*) {}
void MIDIParamCollection::remotelySwapParamState(AutoParamState*, ModelStackWithParamId*) {}
ModelStackWithAutoParam* MIDIParamCollection::getAutoParamFromId(ModelStackWithParamId*, bool) { return nullptr; }
void MIDIParamCollection::notifyParamModifiedInSomeWay(ModelStackWithAutoParam const*, int32_t, bool, bool, bool) {}
void MIDIParamCollection::notifyPingpongOccurred(ModelStackWithParamCollection*) {}
int32_t MIDIParamCollection::knobPosToParamValue(int32_t knobPos, ModelStackWithAutoParam*) { return knobPos; }
bool MIDIParamCollection::mayParamInterpolate(int32_t) { return false; }
