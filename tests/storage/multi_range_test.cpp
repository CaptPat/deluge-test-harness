// Tests for Phase 15 storage files:
// WaveTableHolder, MultiWaveTableRange, MultisampleRange, MultiRangeArray

#include "CppUTest/TestHarness.h"
#include "storage/wave_table/wave_table_holder.h"
#include "storage/multi_range/multi_wave_table_range.h"
#include "storage/multi_range/multisample_range.h"
#include "storage/multi_range/multi_range_array.h"
#include "storage/multi_range/multi_range.h"

// ── WaveTableHolder ─────────────────────────────────────────────────────

TEST_GROUP(WaveTableHolder) {};

TEST(WaveTableHolder, constructorSetsAudioFileType) {
	WaveTableHolder holder;
	CHECK_EQUAL((int)AudioFileType::WAVETABLE, (int)holder.audioFileType);
}

TEST(WaveTableHolder, defaultAudioFileIsNull) {
	WaveTableHolder holder;
	POINTERS_EQUAL(nullptr, holder.audioFile);
}

// ── MultiWaveTableRange ─────────────────────────────────────────────────

TEST_GROUP(MultiWaveTableRange) {};

TEST(MultiWaveTableRange, constructorDefaultTopNote) {
	MultiWaveTableRange range;
	CHECK_EQUAL(32767, range.topNote);
}

TEST(MultiWaveTableRange, getAudioFileHolderReturnsWaveTableHolder) {
	MultiWaveTableRange range;
	AudioFileHolder* holder = range.getAudioFileHolder();
	POINTERS_EQUAL(&range.waveTableHolder, holder);
}

TEST(MultiWaveTableRange, waveTableHolderHasCorrectType) {
	MultiWaveTableRange range;
	CHECK_EQUAL((int)AudioFileType::WAVETABLE, (int)range.waveTableHolder.audioFileType);
}

// ── MultisampleRange ────────────────────────────────────────────────────

TEST_GROUP(MultisampleRange) {};

TEST(MultisampleRange, constructorDefaultTopNote) {
	MultisampleRange range;
	CHECK_EQUAL(32767, range.topNote);
}

TEST(MultisampleRange, getAudioFileHolderReturnsSampleHolder) {
	MultisampleRange range;
	AudioFileHolder* holder = range.getAudioFileHolder();
	POINTERS_EQUAL(&range.sampleHolder, holder);
}

TEST(MultisampleRange, sampleHolderHasSampleType) {
	MultisampleRange range;
	CHECK_EQUAL((int)AudioFileType::SAMPLE, (int)range.sampleHolder.audioFileType);
}

// ── MultiRangeArray ─────────────────────────────────────────────────────

TEST_GROUP(MultiRangeArray) {};

TEST(MultiRangeArray, defaultConstructorIsEmpty) {
	MultiRangeArray arr;
	CHECK_EQUAL(0, arr.getNumElements());
}

TEST(MultiRangeArray, insertMultisampleRange) {
	MultiRangeArray arr;
	MultiRange* range = arr.insertMultiRange(0);
	CHECK(range != nullptr);
	CHECK_EQUAL(1, arr.getNumElements());
	CHECK_EQUAL(32767, range->topNote);
}

TEST(MultiRangeArray, insertMultipleRanges) {
	MultiRangeArray arr;
	MultiRange* r0 = arr.insertMultiRange(0);
	r0->topNote = 60;

	MultiRange* r1 = arr.insertMultiRange(1);
	r1->topNote = 72;

	CHECK_EQUAL(2, arr.getNumElements());

	MultiRange* got0 = arr.getElement(0);
	CHECK_EQUAL(60, got0->topNote);

	MultiRange* got1 = arr.getElement(1);
	CHECK_EQUAL(72, got1->topNote);
}

TEST(MultiRangeArray, getAudioFileHolderFromInsertedRange) {
	MultiRangeArray arr;
	MultiRange* range = arr.insertMultiRange(0);
	AudioFileHolder* holder = range->getAudioFileHolder();
	CHECK(holder != nullptr);
	CHECK_EQUAL((int)AudioFileType::SAMPLE, (int)holder->audioFileType);
}

TEST(MultiRangeArray, changeTypeToWaveTable) {
	MultiRangeArray arr;
	MultiRange* r0 = arr.insertMultiRange(0);
	r0->topNote = 64;

	Error err = arr.changeType(sizeof(MultiWaveTableRange));
	CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
	CHECK_EQUAL(1, arr.getNumElements());

	MultiRange* got = arr.getElement(0);
	CHECK_EQUAL(64, got->topNote);
	AudioFileHolder* holder = got->getAudioFileHolder();
	CHECK_EQUAL((int)AudioFileType::WAVETABLE, (int)holder->audioFileType);
}

TEST(MultiRangeArray, changeTypePreservesMultipleTopNotes) {
	MultiRangeArray arr;
	for (int i = 0; i < 3; i++) {
		MultiRange* r = arr.insertMultiRange(i);
		r->topNote = 48 + i * 12;
	}

	Error err = arr.changeType(sizeof(MultiWaveTableRange));
	CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
	CHECK_EQUAL(3, arr.getNumElements());

	CHECK_EQUAL(48, arr.getElement(0)->topNote);
	CHECK_EQUAL(60, arr.getElement(1)->topNote);
	CHECK_EQUAL(72, arr.getElement(2)->topNote);
}

TEST(MultiRangeArray, changeTypeOnEmptyArray) {
	MultiRangeArray arr;
	Error err = arr.changeType(sizeof(MultiWaveTableRange));
	CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
	CHECK_EQUAL(0, arr.getNumElements());
}

TEST(MultiRangeArray, changeTypeBackToMultisample) {
	MultiRangeArray arr;
	MultiRange* r = arr.insertMultiRange(0);
	r->topNote = 50;

	arr.changeType(sizeof(MultiWaveTableRange));
	Error err = arr.changeType(sizeof(MultisampleRange));
	CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));

	MultiRange* got = arr.getElement(0);
	CHECK_EQUAL(50, got->topNote);
	CHECK_EQUAL((int)AudioFileType::SAMPLE, (int)got->getAudioFileHolder()->audioFileType);
}
