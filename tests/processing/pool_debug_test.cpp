#include "CppUTest/TestHarness.h"
#include "processing/sound/sound_instrument.h"
#include "model/voice/voice.h"
#include "model/model_stack.h"
#include "modulation/params/param_set.h"
#include "processing/engines/audio_engine.h"
#include <cstdio>

TEST_GROUP(PoolDebug){};

TEST(PoolDebug, PrintSizes) {
	fprintf(stderr, "\nsizeof(SoundInstrument) = %zu\n", sizeof(SoundInstrument));
	fprintf(stderr, "sizeof(PatchedParamSet) = %zu\n", sizeof(PatchedParamSet));
	fprintf(stderr, "sizeof(UnpatchedParamSet) = %zu\n", sizeof(UnpatchedParamSet));
	fprintf(stderr, "sizeof(ParamManagerForTimeline) = %zu\n", sizeof(ParamManagerForTimeline));
	fprintf(stderr, "sizeof(ModelStackWithSoundFlags) = %zu\n", sizeof(ModelStackWithSoundFlags));
	fprintf(stderr, "sizeof(ModelStackWithAutoParam) = %zu\n", sizeof(ModelStackWithAutoParam));
	fprintf(stderr, "MODEL_STACK_MAX_SIZE = %zu\n", (size_t)MODEL_STACK_MAX_SIZE);
	fprintf(stderr, "sizeof(Voice) = %zu\n", sizeof(Voice));
	fprintf(stderr, "sizeof(Arpeggiator) = %zu\n", sizeof(Arpeggiator));
	size_t total = sizeof(SoundInstrument) + sizeof(PatchedParamSet) + sizeof(UnpatchedParamSet)
	               + sizeof(ParamManagerForTimeline) + MODEL_STACK_MAX_SIZE;
	fprintf(stderr, "NoteTestFixture approx stack = %zu bytes\n", total);
}

TEST(PoolDebug, AcquireAndDestroy) {
	SoundInstrument si;
	{
		auto voice = AudioEngine::VoicePool::get().acquire(si);
		CHECK(voice != nullptr);
	}
}
