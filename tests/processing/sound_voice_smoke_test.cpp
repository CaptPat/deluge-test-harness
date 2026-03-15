// Smoke tests for Sound, Voice, and SoundInstrument construction.
// Verifies that these central firmware classes can be created and destroyed
// in the test harness without crashing — a prerequisite for crash-regression tests.

#include "CppUTest/TestHarness.h"
#include "processing/sound/sound.h"
#include "processing/sound/sound_instrument.h"
#include "model/voice/voice.h"
#include "processing/engines/audio_engine.h"

// ── SoundInstrument construction ───────────────────────────────────────

TEST_GROUP(SoundVoiceSmoke){};

TEST(SoundVoiceSmoke, SoundInstrumentConstructible) {
	// SoundInstrument is the concrete subclass used for synth presets.
	// It inherits from Sound + MelodicInstrument (multiple inheritance).
	SoundInstrument si;

	// Verify it registered with AudioEngine
	CHECK(!AudioEngine::sounds.empty());
	CHECK_EQUAL(static_cast<Sound*>(&si), AudioEngine::sounds.back());

	// Verify default state
	CHECK(SynthMode::SUBTRACTIVE == si.getSynthMode());
	CHECK_EQUAL(8, si.maxVoiceCount);
	CHECK_EQUAL(1, si.numUnison);
	CHECK(!si.hasActiveVoices());
	CHECK_EQUAL(static_cast<size_t>(0), si.numActiveVoices());
}

TEST(SoundVoiceSmoke, SoundInstrumentCleansUpFromSoundsVector) {
	size_t before = AudioEngine::sounds.size();
	{
		SoundInstrument si;
		CHECK_EQUAL(before + 1, AudioEngine::sounds.size());
	}
	// Destructor should erase from AudioEngine::sounds
	CHECK_EQUAL(before, AudioEngine::sounds.size());
}

TEST(SoundVoiceSmoke, VoiceConstructibleWithSoundRef) {
	SoundInstrument si;
	Voice v(si);

	// Verify Voice has a reference back to its Sound
	CHECK_EQUAL(&static_cast<Sound&>(si), &v.sound);
	CHECK(v.justCreated);
	CHECK(Voice::PedalState::None == v.pedalState);
}

TEST(SoundVoiceSmoke, VoicePoolAcquireAndRelease) {
	// Verify the ObjectPool<Voice> works in the test harness
	SoundInstrument si;
	auto& pool = AudioEngine::VoicePool::get();

	// Acquire a voice from the pool
	auto voice = pool.acquire(si);
	CHECK(voice != nullptr);
	CHECK_EQUAL(&static_cast<Sound&>(si), &voice->sound);

	// Release happens automatically when unique_ptr goes out of scope
}

TEST(SoundVoiceSmoke, MultipleSoundsCoexist) {
	size_t before = AudioEngine::sounds.size();
	SoundInstrument si1;
	SoundInstrument si2;
	CHECK_EQUAL(before + 2, AudioEngine::sounds.size());
}

TEST(SoundVoiceSmoke, SoundHasCorrectModKnobDefaults) {
	SoundInstrument si;
	// ModKnob[0][1] should default to GLOBAL_VOLUME_POST_FX
	CHECK_EQUAL(deluge::modulation::params::GLOBAL_VOLUME_POST_FX,
	            si.modKnobs[0][1].paramDescriptor.getJustTheParam());
	// ModKnob[1][1] should default to LOCAL_LPF_FREQ
	CHECK_EQUAL(deluge::modulation::params::LOCAL_LPF_FREQ,
	            si.modKnobs[1][1].paramDescriptor.getJustTheParam());
}

TEST(SoundVoiceSmoke, SoundSourcesExist) {
	SoundInstrument si;
	// Sound should have kNumSources (2) sources
	CHECK(OscType::SQUARE == si.sources[0].oscType);
	CHECK(OscType::SQUARE == si.sources[1].oscType);
}

TEST(SoundVoiceSmoke, ArpSettingsAccessible) {
	SoundInstrument si;
	ArpeggiatorSettings* arp = si.getArpSettings();
	CHECK(arp != nullptr);
	CHECK_EQUAL(&si.defaultArpSettings, arp);
}

TEST(SoundVoiceSmoke, GetArpReturnsArpeggiator) {
	SoundInstrument si;
	ArpeggiatorBase* arp = si.getArp();
	CHECK(arp != nullptr);
}
