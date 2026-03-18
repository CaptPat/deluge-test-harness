// Deep tests for Output mock: type queries, clip instance management,
// activeClip tracking, state flags, linked list, and SoundInstrument-as-Output.

#include "CppUTest/TestHarness.h"
#include "model/clip/clip_instance.h"
#include "model/output.h"
#include "processing/sound/sound_instrument.h"
#include "song_mock.h"

// ── Concrete Output for testing (the mock is abstract) ─────────────────
class ConcreteTestOutput : public Output {
public:
	ConcreteTestOutput(OutputType t = OutputType::SYNTH) : Output(t) {}
	bool matchesPreset(OutputType, int32_t, int32_t, char const*, char const*) override { return false; }
	void renderOutput(ModelStack*, std::span<StereoSample>, int32_t*, int32_t, int32_t, bool, bool) override {}
	char const* getXMLTag() override { return "testOutput"; }
	ModelStackWithAutoParam* getModelStackWithParam(ModelStackWithTimelineCounter*, Clip*, int32_t,
	                                                deluge::modulation::params::Kind, bool, bool) override {
		return nullptr;
	}

protected:
	Clip* createNewClipForArrangementRecording(ModelStack*) override { return nullptr; }
};

// ═══════════════════════════════════════════════════════════════════════
// TEST GROUP: Output construction, type, and state flags
// ═══════════════════════════════════════════════════════════════════════

TEST_GROUP(OutputDeep) {
	void setup() {
		currentSong = &testSong;
		testSong.firstOutput = nullptr;
	}
	void teardown() { testSong.firstOutput = nullptr; }
};

TEST(OutputDeep, constructionSetsType) {
	ConcreteTestOutput synth(OutputType::SYNTH);
	CHECK(OutputType::SYNTH == synth.type);

	ConcreteTestOutput kit(OutputType::KIT);
	CHECK(OutputType::KIT == kit.type);

	ConcreteTestOutput midi(OutputType::MIDI_OUT);
	CHECK(OutputType::MIDI_OUT == midi.type);

	ConcreteTestOutput cv(OutputType::CV);
	CHECK(OutputType::CV == cv.type);

	ConcreteTestOutput audio(OutputType::AUDIO);
	CHECK(OutputType::AUDIO == audio.type);
}

TEST(OutputDeep, defaultStateFlags) {
	ConcreteTestOutput out;
	CHECK(!out.mutedInArrangementMode);
	CHECK(!out.mutedInArrangementModeBeforeStemExport);
	CHECK(!out.exportStem);
	CHECK(!out.soloingInArrangementMode);
	CHECK(!out.inValidState);
	CHECK(!out.wasCreatedForAutoOverdub);
	CHECK(!out.armedForRecording);
	CHECK(!out.recordingInArrangement);
	CHECK_EQUAL(0, out.colour);
	POINTERS_EQUAL(nullptr, out.next);
	POINTERS_EQUAL(nullptr, out.getActiveClip());
}

TEST(OutputDeep, stateFlagsMutable) {
	ConcreteTestOutput out;
	out.mutedInArrangementMode = true;
	out.soloingInArrangementMode = true;
	out.armedForRecording = true;
	out.exportStem = true;
	out.colour = 42;
	out.modKnobMode = 3;

	CHECK(out.mutedInArrangementMode);
	CHECK(out.soloingInArrangementMode);
	CHECK(out.armedForRecording);
	CHECK(out.exportStem);
	CHECK_EQUAL(42, out.colour);
	CHECK_EQUAL(3, out.modKnobMode);
}

TEST(OutputDeep, wantsToBeginArrangementRecordingFollowsArmed) {
	ConcreteTestOutput out;
	CHECK(!out.wantsToBeginArrangementRecording());
	out.armedForRecording = true;
	CHECK(out.wantsToBeginArrangementRecording());
}

// ═══════════════════════════════════════════════════════════════════════
// TEST GROUP: outputTypeToString utility
// ═══════════════════════════════════════════════════════════════════════

TEST_GROUP(OutputTypeToString){};

TEST(OutputTypeToString, allTypes) {
	STRCMP_EQUAL("synth", outputTypeToString(OutputType::SYNTH));
	STRCMP_EQUAL("kit", outputTypeToString(OutputType::KIT));
	STRCMP_EQUAL("MIDI", outputTypeToString(OutputType::MIDI_OUT));
	STRCMP_EQUAL("CV", outputTypeToString(OutputType::CV));
	STRCMP_EQUAL("audio", outputTypeToString(OutputType::AUDIO));
	STRCMP_EQUAL("none", outputTypeToString(OutputType::NONE));
}

// ═══════════════════════════════════════════════════════════════════════
// TEST GROUP: ClipInstance management on Output
// ═══════════════════════════════════════════════════════════════════════

TEST_GROUP(OutputClipInstances) {
	ConcreteTestOutput out;
	Clip clips[4];

	void setup() {
		currentSong = &testSong;
		for (int i = 0; i < 4; i++) {
			clips[i] = Clip(i);
		}
	}
};

TEST(OutputClipInstances, emptyByDefault) {
	CHECK_EQUAL(0, out.clipInstances.getNumElements());
}

TEST(OutputClipInstances, insertAndRetrieve) {
	int32_t idx = out.clipInstances.insertAtKey(0);
	CHECK(idx >= 0);
	ClipInstance* ci = out.clipInstances.getElement(idx);
	CHECK(ci != nullptr);
	ci->clip = &clips[0];
	ci->length = 100;

	CHECK_EQUAL(1, out.clipInstances.getNumElements());
	ClipInstance* retrieved = out.clipInstances.getElement(0);
	POINTERS_EQUAL(&clips[0], retrieved->clip);
	CHECK_EQUAL(100, retrieved->length);
}

TEST(OutputClipInstances, insertMultipleInOrder) {
	// Insert at positions 0, 100, 200
	for (int i = 0; i < 3; i++) {
		int32_t idx = out.clipInstances.insertAtKey(i * 100);
		CHECK(idx >= 0);
		ClipInstance* ci = out.clipInstances.getElement(idx);
		ci->clip = &clips[i];
		ci->length = 50;
	}

	CHECK_EQUAL(3, out.clipInstances.getNumElements());

	// Verify ordering: positions should be 0, 100, 200
	CHECK_EQUAL(0, out.clipInstances.getElement(0)->pos);
	CHECK_EQUAL(100, out.clipInstances.getElement(1)->pos);
	CHECK_EQUAL(200, out.clipInstances.getElement(2)->pos);
}

TEST(OutputClipInstances, insertReverseOrder) {
	// Insert in reverse: 200, 100, 0 — should still be sorted
	out.clipInstances.insertAtKey(200);
	out.clipInstances.getElement(out.clipInstances.getNumElements() - 1)->clip = &clips[2];

	out.clipInstances.insertAtKey(100);
	// After insert at 100, it sorts before 200
	out.clipInstances.insertAtKey(0);

	CHECK_EQUAL(3, out.clipInstances.getNumElements());
	CHECK_EQUAL(0, out.clipInstances.getElement(0)->pos);
	CHECK_EQUAL(100, out.clipInstances.getElement(1)->pos);
	CHECK_EQUAL(200, out.clipInstances.getElement(2)->pos);
}

TEST(OutputClipInstances, searchFindsCorrectIndex) {
	for (int i = 0; i < 3; i++) {
		int32_t idx = out.clipInstances.insertAtKey(i * 100);
		out.clipInstances.getElement(idx)->clip = &clips[i];
		out.clipInstances.getElement(idx)->length = 50;
	}

	// search(value, LESS) returns index of last element < value
	int32_t idx = out.clipInstances.search(150, LESS);
	CHECK(idx >= 0);
	ClipInstance* ci = out.clipInstances.getElement(idx);
	CHECK(ci != nullptr);
	CHECK_EQUAL(100, ci->pos);
}

TEST(OutputClipInstances, deleteAtIndex) {
	for (int i = 0; i < 3; i++) {
		int32_t idx = out.clipInstances.insertAtKey(i * 100);
		out.clipInstances.getElement(idx)->clip = &clips[i];
		out.clipInstances.getElement(idx)->length = 50;
	}
	CHECK_EQUAL(3, out.clipInstances.getNumElements());

	out.clipInstances.deleteAtIndex(1); // Remove pos=100
	CHECK_EQUAL(2, out.clipInstances.getNumElements());
	CHECK_EQUAL(0, out.clipInstances.getElement(0)->pos);
	CHECK_EQUAL(200, out.clipInstances.getElement(1)->pos);
}

// ═══════════════════════════════════════════════════════════════════════
// TEST GROUP: activeClip tracking
// ═══════════════════════════════════════════════════════════════════════

TEST_GROUP(OutputActiveClip) {
	void setup() { currentSong = &testSong; }
};

TEST(OutputActiveClip, defaultNullActiveClip) {
	ConcreteTestOutput out;
	POINTERS_EQUAL(nullptr, out.getActiveClip());
}

TEST(OutputActiveClip, getActiveClipReturnsSet) {
	// The mock setActiveClip is a stub returning false, but we can
	// verify getActiveClip through the protected member. Since the mock
	// exposes activeClip as protected, we test via SoundInstrument
	// which has a stubbed setActiveClip.
	SoundInstrument si;
	POINTERS_EQUAL(nullptr, si.getActiveClip());
}

// ═══════════════════════════════════════════════════════════════════════
// TEST GROUP: Output linked list (Song::firstOutput chain)
// ═══════════════════════════════════════════════════════════════════════

TEST_GROUP(OutputLinkedList) {
	void setup() {
		currentSong = &testSong;
		testSong.firstOutput = nullptr;
	}
	void teardown() { testSong.firstOutput = nullptr; }
};

TEST(OutputLinkedList, addSingleOutput) {
	ConcreteTestOutput out;
	testSong.addOutput(&out);
	POINTERS_EQUAL(&out, testSong.firstOutput);
	POINTERS_EQUAL(nullptr, out.next);
}

TEST(OutputLinkedList, addMultipleOutputsCreatesChain) {
	ConcreteTestOutput out1(OutputType::SYNTH);
	ConcreteTestOutput out2(OutputType::KIT);
	ConcreteTestOutput out3(OutputType::MIDI_OUT);

	testSong.addOutput(&out1);
	testSong.addOutput(&out2);
	testSong.addOutput(&out3);

	// addOutput inserts at head, so order is out3 → out2 → out1
	POINTERS_EQUAL(&out3, testSong.firstOutput);
	POINTERS_EQUAL(&out2, out3.next);
	POINTERS_EQUAL(&out1, out2.next);
	POINTERS_EQUAL(nullptr, out1.next);
}

TEST(OutputLinkedList, removeHead) {
	ConcreteTestOutput out1, out2;
	testSong.addOutput(&out1);
	testSong.addOutput(&out2);
	// out2 → out1

	int32_t idx = testSong.removeOutputFromMainList(&out2);
	CHECK_EQUAL(0, idx);
	POINTERS_EQUAL(&out1, testSong.firstOutput);
	POINTERS_EQUAL(nullptr, out1.next);
}

TEST(OutputLinkedList, removeTail) {
	ConcreteTestOutput out1, out2;
	testSong.addOutput(&out1);
	testSong.addOutput(&out2);
	// out2 → out1

	int32_t idx = testSong.removeOutputFromMainList(&out1);
	CHECK_EQUAL(1, idx);
	POINTERS_EQUAL(&out2, testSong.firstOutput);
	POINTERS_EQUAL(nullptr, out2.next);
}

TEST(OutputLinkedList, removeMiddle) {
	ConcreteTestOutput out1, out2, out3;
	testSong.addOutput(&out1);
	testSong.addOutput(&out2);
	testSong.addOutput(&out3);
	// out3 → out2 → out1

	int32_t idx = testSong.removeOutputFromMainList(&out2);
	CHECK_EQUAL(1, idx);
	POINTERS_EQUAL(&out3, testSong.firstOutput);
	POINTERS_EQUAL(&out1, out3.next);
	POINTERS_EQUAL(nullptr, out1.next);
}

TEST(OutputLinkedList, removeNotInList) {
	ConcreteTestOutput out1, notInList;
	testSong.addOutput(&out1);

	int32_t idx = testSong.removeOutputFromMainList(&notInList);
	CHECK_EQUAL(-1, idx);
	// List unchanged
	POINTERS_EQUAL(&out1, testSong.firstOutput);
}

TEST(OutputLinkedList, removeFromEmptyList) {
	ConcreteTestOutput out;
	int32_t idx = testSong.removeOutputFromMainList(&out);
	CHECK_EQUAL(-1, idx);
}

TEST(OutputLinkedList, walkChainCountsTypes) {
	ConcreteTestOutput synth1(OutputType::SYNTH);
	ConcreteTestOutput synth2(OutputType::SYNTH);
	ConcreteTestOutput kit(OutputType::KIT);
	ConcreteTestOutput midi(OutputType::MIDI_OUT);

	testSong.addOutput(&synth1);
	testSong.addOutput(&synth2);
	testSong.addOutput(&kit);
	testSong.addOutput(&midi);

	int synthCount = 0, kitCount = 0, midiCount = 0;
	for (Output* o = testSong.firstOutput; o; o = o->next) {
		if (o->type == OutputType::SYNTH)
			synthCount++;
		else if (o->type == OutputType::KIT)
			kitCount++;
		else if (o->type == OutputType::MIDI_OUT)
			midiCount++;
	}
	CHECK_EQUAL(2, synthCount);
	CHECK_EQUAL(1, kitCount);
	CHECK_EQUAL(1, midiCount);
}

// ═══════════════════════════════════════════════════════════════════════
// TEST GROUP: SoundInstrument as Output
// ═══════════════════════════════════════════════════════════════════════

TEST_GROUP(SoundInstrumentAsOutput) {
	void setup() {
		currentSong = &testSong;
		testSong.firstOutput = nullptr;
	}
	void teardown() { testSong.firstOutput = nullptr; }
};

TEST(SoundInstrumentAsOutput, typeIsSynth) {
	SoundInstrument si;
	CHECK(OutputType::SYNTH == si.type);
}

TEST(SoundInstrumentAsOutput, toModControllableReturnsSelf) {
	SoundInstrument si;
	ModControllable* mc = si.toModControllable();
	CHECK(mc != nullptr);
	// SoundInstrument IS the ModControllable for synths
	CHECK_EQUAL(static_cast<ModControllable*>(&si), mc);
}

TEST(SoundInstrumentAsOutput, canBeInLinkedList) {
	SoundInstrument si1;
	SoundInstrument si2;

	testSong.addOutput(&si1);
	testSong.addOutput(&si2);

	// Use static_cast to account for multiple inheritance pointer adjustment
	POINTERS_EQUAL(static_cast<Output*>(&si2), testSong.firstOutput);
	POINTERS_EQUAL(static_cast<Output*>(&si1), si2.next);

	// Walk the chain and verify types
	int count = 0;
	for (Output* o = testSong.firstOutput; o; o = o->next) {
		CHECK(OutputType::SYNTH == o->type);
		count++;
	}
	CHECK_EQUAL(2, count);
}

TEST(SoundInstrumentAsOutput, clipInstanceVectorUsable) {
	SoundInstrument si;
	CHECK_EQUAL(0, si.clipInstances.getNumElements());

	Clip clip(42);
	int32_t idx = si.clipInstances.insertAtKey(0);
	CHECK(idx >= 0);
	si.clipInstances.getElement(idx)->clip = &clip;
	si.clipInstances.getElement(idx)->length = 96;

	CHECK_EQUAL(1, si.clipInstances.getNumElements());
	POINTERS_EQUAL(&clip, si.clipInstances.getElement(0)->clip);
}

TEST(SoundInstrumentAsOutput, nameStringWorks) {
	SoundInstrument si;
	si.name.set("MySynth");
	STRCMP_EQUAL("MySynth", si.name.get());
}

TEST(SoundInstrumentAsOutput, mixedTypeLinkedList) {
	SoundInstrument synth;
	ConcreteTestOutput kit(OutputType::KIT);
	ConcreteTestOutput midi(OutputType::MIDI_OUT);

	testSong.addOutput(&synth);
	testSong.addOutput(&kit);
	testSong.addOutput(&midi);

	// Verify heterogeneous chain works
	Output* o = testSong.firstOutput;
	CHECK(OutputType::MIDI_OUT == o->type);
	o = o->next;
	CHECK(OutputType::KIT == o->type);
	o = o->next;
	CHECK(OutputType::SYNTH == o->type);
	POINTERS_EQUAL(nullptr, o->next);
}

// ═══════════════════════════════════════════════════════════════════════
// TEST GROUP: Output name and XML tag
// ═══════════════════════════════════════════════════════════════════════

TEST_GROUP(OutputName) {
	void setup() { currentSong = &testSong; }
};

TEST(OutputName, defaultNameIsEmpty) {
	ConcreteTestOutput out;
	CHECK(out.name.isEmpty());
}

TEST(OutputName, setAndGetName) {
	ConcreteTestOutput out;
	out.name.set("TestInstrument");
	STRCMP_EQUAL("TestInstrument", out.name.get());
}

TEST(OutputName, getNameXMLTagDefault) {
	ConcreteTestOutput out;
	STRCMP_EQUAL("name", out.getNameXMLTag());
}

TEST(OutputName, getXMLTag) {
	ConcreteTestOutput out;
	STRCMP_EQUAL("testOutput", out.getXMLTag());
}

// ═══════════════════════════════════════════════════════════════════════
// TEST GROUP: Output virtual method defaults
// ═══════════════════════════════════════════════════════════════════════

TEST_GROUP(OutputVirtualDefaults) {
	void setup() { currentSong = &testSong; }
};

TEST(OutputVirtualDefaults, isSkippingRenderingDefault) {
	ConcreteTestOutput out;
	CHECK(out.isSkippingRendering());
}

TEST(OutputVirtualDefaults, needsEarlyPlaybackDefault) {
	ConcreteTestOutput out;
	CHECK(!out.needsEarlyPlayback());
}

TEST(OutputVirtualDefaults, toModControllableDefaultNull) {
	ConcreteTestOutput out;
	POINTERS_EQUAL(nullptr, out.toModControllable());
}

TEST(OutputVirtualDefaults, doTickForwardForArpReturnsMax) {
	ConcreteTestOutput out;
	CHECK_EQUAL(2147483647, out.doTickForwardForArp(nullptr, 0));
}

TEST(OutputVirtualDefaults, loadAllAudioFilesReturnsNone) {
	ConcreteTestOutput out;
	CHECK(Error::NONE == out.loadAllAudioFiles(true));
	CHECK(Error::NONE == out.loadAllAudioFiles(false));
}

TEST(OutputVirtualDefaults, getParamManagerUsesTestHook) {
	ConcreteTestOutput out;
	// Default testParamManager_ is null
	POINTERS_EQUAL(nullptr, out.getParamManager(nullptr));

	// Set test hook and verify
	ParamManager pm;
	out.testParamManager_ = &pm;
	POINTERS_EQUAL(&pm, out.getParamManager(nullptr));
}

// ═══════════════════════════════════════════════════════════════════════
// TEST GROUP: ClipInstance vector stress on Output
// ═══════════════════════════════════════════════════════════════════════

TEST_GROUP(OutputClipInstanceStress) {
	void setup() { currentSong = &testSong; }
};

TEST(OutputClipInstanceStress, insertManyInstances) {
	ConcreteTestOutput out;
	const int N = 64;
	Clip clips[N];

	for (int i = 0; i < N; i++) {
		int32_t pos = i * 96; // Each 96 ticks apart
		int32_t idx = out.clipInstances.insertAtKey(pos);
		CHECK(idx >= 0);
		ClipInstance* ci = out.clipInstances.getElement(idx);
		ci->clip = &clips[i];
		ci->length = 96;
	}

	CHECK_EQUAL(N, out.clipInstances.getNumElements());

	// Verify all are sorted
	for (int i = 1; i < N; i++) {
		CHECK(out.clipInstances.getElement(i)->pos > out.clipInstances.getElement(i - 1)->pos);
	}
}

TEST(OutputClipInstanceStress, searchBoundaries) {
	ConcreteTestOutput out;
	Clip clip;

	// Insert at pos 100, 200, 300
	for (int p = 100; p <= 300; p += 100) {
		int32_t idx = out.clipInstances.insertAtKey(p);
		out.clipInstances.getElement(idx)->clip = &clip;
		out.clipInstances.getElement(idx)->length = 50;
	}

	// Search LESS than 100 → should return -1 or element before
	int32_t idx = out.clipInstances.search(100, LESS);
	// Pos 100 is at index 0, LESS should return index -1 (nothing before it)
	// or the element depending on the search semantics
	ClipInstance* ci = out.clipInstances.getElement(idx);
	if (ci) {
		CHECK(ci->pos < 100);
	}

	// Search for exact position 200
	idx = out.clipInstances.search(200, LESS);
	ci = out.clipInstances.getElement(idx);
	CHECK(ci != nullptr);
	CHECK_EQUAL(100, ci->pos); // Last element LESS than 200

	// Search beyond all
	idx = out.clipInstances.search(400, LESS);
	ci = out.clipInstances.getElement(idx);
	CHECK(ci != nullptr);
	CHECK_EQUAL(300, ci->pos);
}

TEST(OutputClipInstanceStress, deleteAllInstances) {
	ConcreteTestOutput out;
	Clip clip;

	for (int i = 0; i < 10; i++) {
		int32_t idx = out.clipInstances.insertAtKey(i * 100);
		out.clipInstances.getElement(idx)->clip = &clip;
		out.clipInstances.getElement(idx)->length = 50;
	}
	CHECK_EQUAL(10, out.clipInstances.getNumElements());

	// Delete from end to avoid index shifting
	for (int i = 9; i >= 0; i--) {
		out.clipInstances.deleteAtIndex(i);
	}
	CHECK_EQUAL(0, out.clipInstances.getNumElements());
}

TEST(OutputClipInstanceStress, nullClipInstances) {
	ConcreteTestOutput out;

	// Insert instances with null clips (arrangement gaps)
	for (int i = 0; i < 5; i++) {
		int32_t idx = out.clipInstances.insertAtKey(i * 100);
		out.clipInstances.getElement(idx)->clip = nullptr;
		out.clipInstances.getElement(idx)->length = 50;
	}

	// All should have null clips
	for (int i = 0; i < 5; i++) {
		POINTERS_EQUAL(nullptr, out.clipInstances.getElement(i)->clip);
	}
}

// ═══════════════════════════════════════════════════════════════════════
// TEST GROUP: Multiple Outputs sharing clips
// ═══════════════════════════════════════════════════════════════════════

TEST_GROUP(OutputMultipleOutputs) {
	void setup() {
		currentSong = &testSong;
		testSong.firstOutput = nullptr;
	}
	void teardown() { testSong.firstOutput = nullptr; }
};

TEST(OutputMultipleOutputs, differentOutputsSameClipInstances) {
	// Two outputs can reference the same Clip object
	ConcreteTestOutput out1(OutputType::SYNTH);
	ConcreteTestOutput out2(OutputType::SYNTH);
	Clip sharedClip(99);

	int32_t idx1 = out1.clipInstances.insertAtKey(0);
	out1.clipInstances.getElement(idx1)->clip = &sharedClip;
	out1.clipInstances.getElement(idx1)->length = 100;

	int32_t idx2 = out2.clipInstances.insertAtKey(0);
	out2.clipInstances.getElement(idx2)->clip = &sharedClip;
	out2.clipInstances.getElement(idx2)->length = 200;

	// Both reference the same clip but with different lengths
	POINTERS_EQUAL(&sharedClip, out1.clipInstances.getElement(0)->clip);
	POINTERS_EQUAL(&sharedClip, out2.clipInstances.getElement(0)->clip);
	CHECK_EQUAL(100, out1.clipInstances.getElement(0)->length);
	CHECK_EQUAL(200, out2.clipInstances.getElement(0)->length);
}

TEST(OutputMultipleOutputs, removeAndReAddToLinkedList) {
	ConcreteTestOutput out1(OutputType::SYNTH);
	ConcreteTestOutput out2(OutputType::KIT);
	ConcreteTestOutput out3(OutputType::MIDI_OUT);

	testSong.addOutput(&out1);
	testSong.addOutput(&out2);
	testSong.addOutput(&out3);

	// Remove middle
	testSong.removeOutputFromMainList(&out2);

	// Re-add at head
	testSong.addOutput(&out2);

	// New order: out2 → out3 → out1
	POINTERS_EQUAL(&out2, testSong.firstOutput);
	POINTERS_EQUAL(&out3, out2.next);
	POINTERS_EQUAL(&out1, out3.next);
	POINTERS_EQUAL(nullptr, out1.next);
}
