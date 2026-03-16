// Shadow header for model/output.h
// Provides enough of the Output interface for instrument.h and consequence
// files to compile. The real header pulls in sample_recorder.h → fatfs.
#pragma once

#include "definitions_cxx.hpp"
#include "model/clip/clip_instance_vector.h"
#include "modulation/params/param.h"
#include "util/d_string.h"
#include <cstdint>
#include <span>

class Clip;
class Song;
class ModelStack;
class ModelStackWithTimelineCounter;
class ModelStackWithAutoParam;
class ModelStackWithThreeMainThings;
class StereoSample;
class ParamManager;
class ParamManagerForTimeline;
class MIDICable;
class LearnedMIDI;
class ModControllable;
class GlobalEffectableForClip;
class Sound;
class SampleRecorder;
class Serializer;
class Deserializer;
class NoteRow;

inline const char* outputTypeToString(OutputType type) {
	switch (type) {
	case OutputType::SYNTH:
		return "synth";
	case OutputType::KIT:
		return "kit";
	case OutputType::MIDI_OUT:
		return "MIDI";
	case OutputType::CV:
		return "CV";
	case OutputType::AUDIO:
		return "audio";
	default:
		return "none";
	}
}

class Output {
public:
	Output(OutputType newType = OutputType::SYNTH) : type(newType) {}
	virtual ~Output() = default;

	virtual bool matchesPreset(OutputType, int32_t, int32_t, char const*, char const*) = 0;
	virtual void renderOutput(ModelStack*, std::span<StereoSample>, int32_t*, int32_t, int32_t, bool, bool) = 0;

	virtual void setupWithoutActiveClip(ModelStack*) {}
	virtual bool setActiveClip(ModelStackWithTimelineCounter*, PgmChangeSend = PgmChangeSend::ONCE) { return false; }

	virtual ModControllable* toModControllable() { return nullptr; }
	virtual bool isSkippingRendering() { return true; }
	virtual void cutAllSound() {}
	virtual void getThingWithMostReverb(Sound**, ParamManager**, GlobalEffectableForClip**, int32_t*) {}

	virtual bool offerReceivedPitchBendToLearnedParams(MIDICable&, uint8_t, uint8_t, uint8_t,
	                                                   ModelStackWithTimelineCounter*) {
		return false;
	}
	virtual void offerReceivedCCToLearnedParams(MIDICable&, uint8_t, uint8_t, uint8_t,
	                                            ModelStackWithTimelineCounter*) {}
	virtual int32_t doTickForwardForArp(ModelStack*, int32_t) { return 2147483647; }
	virtual bool wantsToBeginArrangementRecording() { return armedForRecording; }

	virtual Error readFromFile(Deserializer&, Song*, Clip*, int32_t) { return Error::NONE; }
	virtual bool readTagFromFile(Deserializer&, char const*) { return false; }
	virtual bool writeDataToFile(Serializer&, Clip*, Song*) { return false; }
	virtual Error loadAllAudioFiles(bool) { return Error::NONE; }
	virtual void loadCrucialAudioFilesOnly() {}
	virtual void resyncLFOs() {}
	virtual void sendMIDIPGM() {}
	virtual void deleteBackedUpParamManagers(Song*) {}
	virtual void prepareForHibernationOrDeletion() {}
	virtual char const* getXMLTag() = 0;
	virtual ParamManager* getParamManager(Song*) { return testParamManager_; }

	// Test hook: set this to provide a ParamManager without needing a real Clip/Song
	ParamManager* testParamManager_{nullptr};
	virtual char const* getNameXMLTag() { return "name"; }

	virtual void offerReceivedNote(ModelStackWithTimelineCounter*, MIDICable&, bool, int32_t, int32_t, int32_t, bool,
	                               bool*) {}
	virtual void offerReceivedPitchBend(ModelStackWithTimelineCounter*, MIDICable&, uint8_t, uint8_t, uint8_t,
	                                    bool*) {}
	virtual void offerReceivedCC(ModelStackWithTimelineCounter*, MIDICable&, uint8_t, uint8_t, uint8_t, bool*) {}
	virtual void offerReceivedAftertouch(ModelStackWithTimelineCounter*, MIDICable&, int32_t, int32_t, int32_t,
	                                     bool*) {}
	virtual void stopAnyAuditioning(ModelStack*) {}
	virtual void offerBendRangeUpdate(ModelStack*, MIDICable&, int32_t, int32_t, int32_t) {}

	virtual ModelStackWithAutoParam* getModelStackWithParam(ModelStackWithTimelineCounter*, Clip*, int32_t,
	                                                        deluge::modulation::params::Kind, bool, bool) = 0;
	virtual bool needsEarlyPlayback() const { return false; }

	ClipInstanceVector clipInstances;
	Clip* getActiveClip() const { return activeClip; }
	String name;
	Output* next = nullptr;
	const OutputType type;
	bool mutedInArrangementMode = false;
	bool mutedInArrangementModeBeforeStemExport = false;
	bool exportStem = false;
	bool soloingInArrangementMode = false;
	bool inValidState = false;
	bool wasCreatedForAutoOverdub = false;
	bool armedForRecording = false;
	int16_t colour{0};
	uint8_t modKnobMode = 0;
	bool alreadyGotItsNewClip = false;
	bool isGettingSoloingClip = false;
	bool nextClipFoundShouldGetArmed = false;
	bool recordingInArrangement = false;

protected:
	virtual Clip* createNewClipForArrangementRecording(ModelStack*) = 0;
	bool recorderIsEchoing{false};
	Output* outputRecordingThisOutput{nullptr};
	virtual void clearRecordingFrom() {}
	Clip* activeClip{nullptr};
	SampleRecorder* recorder{nullptr};
};
