// Clip iterator regression tests — ported from firmware/tests/unit/clip_iterator_tests.cpp

#include "CppUTest/TestHarness.h"
#include "model/song/clip_iterators.h"

#include "clip_mocks.h"
#include "song_mock.h"

const int nSessionClips = 10;
static Clip sessionClips[nSessionClips];

const int nArrangementOnlyClips = 4;
static Clip arrangementOnlyClips[nArrangementOnlyClips];

const int nInstrumentClips = 11;
const int nAudioClips = 3;

static void useSessionClips() {
	for (int i = 0; i < nSessionClips; i++) {
		sessionClips[i].id = i;
		if (i == 4 || i == 7) {
			sessionClips[i].type = ClipType::AUDIO;
		}
		else {
			sessionClips[i].type = ClipType::INSTRUMENT;
		}
		currentSong->sessionClips.push(&sessionClips[i]);
	}
}

static void useArrangementOnlyClips() {
	for (int i = 0; i < nArrangementOnlyClips; i++) {
		arrangementOnlyClips[i].id = i + nSessionClips;
		if (i == 2) {
			arrangementOnlyClips[i].type = ClipType::AUDIO;
		}
		else {
			arrangementOnlyClips[i].type = ClipType::INSTRUMENT;
		}
		currentSong->arrangementOnlyClips.push(&arrangementOnlyClips[i]);
	}
}

TEST_GROUP(ClipIteratorTests) {
	void setup() { currentSong->clear(); }
};

TEST(ClipIteratorTests, noClips) {
	for ([[maybe_unused]] Clip* clip : AllClips::everywhere(currentSong)) {
		FAIL("no 1");
	}
	for ([[maybe_unused]] Clip* clip : AllClips::inSession(currentSong)) {
		FAIL("no 2");
	}
	for ([[maybe_unused]] Clip* clip : AllClips::inArrangementOnly(currentSong)) {
		FAIL("no 3");
	}
}

TEST(ClipIteratorTests, sessionClips_everywhereIterator) {
	useSessionClips();
	int n = 0;
	for (Clip* clip : AllClips::everywhere(currentSong)) {
		CHECK_EQUAL(n, clip->id);
		if (n == 4 || n == 7) {
			CHECK(ClipType::AUDIO == clip->type);
		}
		else {
			CHECK(ClipType::INSTRUMENT == clip->type);
		}
		n++;
	}
	CHECK_EQUAL(nSessionClips, n);
}

TEST(ClipIteratorTests, sessionClips_sessionIterator) {
	useSessionClips();
	int n = 0;
	for (Clip* clip : AllClips::inSession(currentSong)) {
		CHECK_EQUAL(n, clip->id);
		n++;
	}
	CHECK_EQUAL(nSessionClips, n);
}

TEST(ClipIteratorTests, sessionClips_arrangementIterator) {
	useSessionClips();
	for ([[maybe_unused]] Clip* clip : AllClips::inArrangementOnly(currentSong)) {
		FAIL("no");
	}
}

TEST(ClipIteratorTests, arrangementClips_everywhereIterator) {
	useArrangementOnlyClips();
	int n = 0;
	for (Clip* clip : AllClips::everywhere(currentSong)) {
		CHECK_EQUAL(n + nSessionClips, clip->id);
		n++;
	}
	CHECK_EQUAL(nArrangementOnlyClips, n);
}

TEST(ClipIteratorTests, allClips_everywhereIterator) {
	useSessionClips();
	useArrangementOnlyClips();
	int n = 0;
	for (Clip* clip : AllClips::everywhere(currentSong)) {
		CHECK_EQUAL(n, clip->id);
		if (clip->id == 4 || clip->id == 7 || clip->id == 12) {
			CHECK(ClipType::AUDIO == clip->type);
		}
		else {
			CHECK(ClipType::INSTRUMENT == clip->type);
		}
		n++;
	}
	CHECK_EQUAL(nSessionClips + nArrangementOnlyClips, n);
}

TEST(ClipIteratorTests, instrumentClips) {
	useSessionClips();
	useArrangementOnlyClips();
	int n = 0;
	for (InstrumentClip* clip : InstrumentClips::everywhere(currentSong)) {
		CHECK(ClipType::INSTRUMENT == clip->type);
		n++;
	}
	CHECK_EQUAL(nInstrumentClips, n);
}

TEST(ClipIteratorTests, audioClips) {
	useSessionClips();
	useArrangementOnlyClips();
	int n = 0;
	for (AudioClip* clip : AudioClips::everywhere(currentSong)) {
		CHECK(ClipType::AUDIO == clip->type);
		n++;
	}
	CHECK_EQUAL(nAudioClips, n);
}

TEST(ClipIteratorTests, deleteClip) {
	useSessionClips();
	useArrangementOnlyClips();
	AllClips all = AllClips::everywhere(currentSong);
	int n = 0;
	for (auto it = all.begin(); it != all.end();) {
		Clip* clip = *it;
		if (clip->type == ClipType::INSTRUMENT) {
			it.deleteClip(InstrumentRemoval::NONE);
			CHECK_EQUAL(true, clip->deleted);
			n++;
		}
		else {
			it++;
		}
	}
	CHECK_EQUAL(nInstrumentClips, n);
	int k = 0;
	for (Clip* clip : all) {
		CHECK(ClipType::AUDIO == clip->type);
		k++;
	}
	CHECK_EQUAL(nAudioClips, k);
}
