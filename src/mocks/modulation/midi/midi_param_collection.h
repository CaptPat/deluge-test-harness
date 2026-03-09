// Shadow header replacing firmware's modulation/midi/midi_param_collection.h
// The real header pulls in io/midi/midi_engine.h which needs MIDIMessage and
// heavy MIDI infrastructure. We provide only the class declaration.

#pragma once

#include "modulation/midi/midi_param_vector.h"
#include "modulation/params/param_collection.h"
#include <cstdint>

class Clip;
class ModelStackWithParamCollection;
class Serializer;
class Deserializer;
class MIDISource;

class MIDIParamCollection final : public ParamCollection {
public:
	MIDIParamCollection(ParamCollectionSummary* summary);
	~MIDIParamCollection() override;

	void tickTicks(int32_t numSamples, ModelStackWithParamCollection* modelStack) override;
	void tickSamples(int32_t numSamples, ModelStackWithParamCollection* modelStack) override {};
	void setPlayPos(uint32_t pos, ModelStackWithParamCollection* modelStack, bool reversed) override;
	void playbackHasEnded(ModelStackWithParamCollection* modelStack) override {}
	void generateRepeats(ModelStackWithParamCollection* modelStack, uint32_t oldLength, uint32_t newLength,
	                     bool shouldPingpong) override;
	void appendParamCollection(ModelStackWithParamCollection* modelStack,
	                           ModelStackWithParamCollection* otherModelStack, int32_t oldLength,
	                           int32_t reverseThisRepeatWithLength, bool pingpongingGenerally) override;
	void trimToLength(uint32_t newLength, ModelStackWithParamCollection* modelStack, Action* action,
	                  bool maySetupPatching) override;
	void shiftHorizontally(ModelStackWithParamCollection* modelStack, int32_t amount, int32_t effectiveLength) override;
	void processCurrentPos(ModelStackWithParamCollection* modelStack, int32_t ticksSkipped, bool reversed,
	                       bool didPingpong, bool mayInterpolate) override;
	void remotelySwapParamState(AutoParamState* state, ModelStackWithParamId* modelStack) override;
	void deleteAllAutomation(Action* action, ModelStackWithParamCollection* modelStack) override;
	void grabValuesFromPos(uint32_t pos, ModelStackWithParamCollection* modelStack) override;
	void nudgeNonInterpolatingNodesAtPos(int32_t pos, int32_t offset, int32_t lengthBeforeLoop, Action* action,
	                                     ModelStackWithParamCollection* modelStack) override;
	ModelStackWithAutoParam* getAutoParamFromId(ModelStackWithParamId* modelStack, bool allowCreation = true) override;

	void cloneFrom(ParamCollection* otherParamSet, bool copyAutomation);
	void beenCloned(bool copyAutomation, int32_t reverseDirectionWithLength) override;
	void notifyParamModifiedInSomeWay(ModelStackWithAutoParam const* modelStack, int32_t oldValue,
	                                  bool automationChanged, bool automatedBefore, bool automatedNow) override;
	bool mayParamInterpolate(int32_t paramId) override;
	int32_t knobPosToParamValue(int32_t knobPos, ModelStackWithAutoParam* modelStack) override;
	void notifyPingpongOccurred(ModelStackWithParamCollection* modelStack) override;

	deluge::modulation::params::Kind getParamKind() override {
		return deluge::modulation::params::Kind::MIDI;
	}

	MIDIParamVector params;
};
