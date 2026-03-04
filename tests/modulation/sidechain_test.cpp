#include "CppUTest/TestHarness.h"
#include "modulation/sidechain/sidechain.h"
#include "storage/flash_storage.h"

// These globals are defined in song_mock.cpp
extern Song* currentSong;
extern Song* preLoadedSong;

#define CHECK_ENUM(expected, actual) CHECK_EQUAL((int)(expected), (int)(actual))

TEST_GROUP(SideChainTest) {
	Song* savedCurrentSong;
	Song* savedPreLoadedSong;
	void setup() override {
		savedCurrentSong = currentSong;
		savedPreLoadedSong = preLoadedSong;
		// Null out song pointers so constructor falls back to FlashStorage::defaultMagnitude
		currentSong = nullptr;
		preLoadedSong = nullptr;
		FlashStorage::defaultMagnitude = 7;
	}
	void teardown() override {
		currentSong = savedCurrentSong;
		preLoadedSong = savedPreLoadedSong;
	}
};

// --- Constructor ---

TEST(SideChainTest, constructorDefaults) {
	SideChain sc;
	CHECK_ENUM(EnvelopeStage::OFF, sc.status);
	CHECK_EQUAL(2147483647, sc.lastValue);
	CHECK_EQUAL(0, (int)sc.pos);
	CHECK_EQUAL(0, sc.pendingHitStrength);
}

TEST(SideChainTest, constructorSyncLevel) {
	// With defaultMagnitude=7 and no song, syncLevel = 7 - 7 = 0 = SYNC_LEVEL_NONE
	SideChain sc;
	CHECK_EQUAL(0, (int)sc.syncLevel);
}

TEST(SideChainTest, constructorAttackRelease) {
	SideChain sc;
	// attack and release should be set to non-zero values from lookup tables
	CHECK(sc.attack != 0);
	CHECK(sc.release != 0);
}

// --- Clone ---

TEST(SideChainTest, cloneFrom) {
	SideChain src;
	src.attack = 12345;
	src.release = 67890;
	src.syncType = SYNC_TYPE_TRIPLET;
	src.syncLevel = (SyncLevel)3;

	SideChain dst;
	dst.cloneFrom(&src);
	CHECK_EQUAL(12345, dst.attack);
	CHECK_EQUAL(67890, dst.release);
	CHECK_ENUM(SYNC_TYPE_TRIPLET, dst.syncType);
	CHECK_EQUAL(3, (int)dst.syncLevel);
}

// --- Register Hit ---

TEST(SideChainTest, registerHit) {
	SideChain sc;
	CHECK_EQUAL(0, sc.pendingHitStrength);
	sc.registerHit(1000000000);
	CHECK(sc.pendingHitStrength > 0);
}

TEST(SideChainTest, registerHitCombines) {
	SideChain sc;
	sc.registerHit(500000000);
	int32_t first = sc.pendingHitStrength;
	sc.registerHit(500000000);
	int32_t combined = sc.pendingHitStrength;
	CHECK(combined > first);
}

// --- Render ---

TEST(SideChainTest, renderOffReturnsZero) {
	SideChain sc;
	// When OFF, lastValue = ONE_Q31, so render returns lastValue - ONE_Q31 = 0
	int32_t result = sc.render(1, 0);
	CHECK_EQUAL(0, result);
}

TEST(SideChainTest, renderAttackAfterHit) {
	SideChain sc;
	sc.registerHit(2000000000); // Strong hit
	sc.render(1, 0);
	// After processing the hit, should be in ATTACK stage
	CHECK_ENUM(EnvelopeStage::ATTACK, sc.status);
}

TEST(SideChainTest, renderAttackDucksSignal) {
	SideChain sc;
	sc.registerHit(2000000000);
	int32_t result = sc.render(1, 0);
	// The sidechain ducks: lastValue drops below ONE_Q31, so result < 0
	CHECK(result < 0);
}

TEST(SideChainTest, renderAttackThenRelease) {
	SideChain sc;
	sc.registerHit(2000000000);
	// Run many samples through attack phase to complete it
	for (int i = 0; i < 1000; i++) {
		sc.render(100, 0);
		if (sc.status == EnvelopeStage::RELEASE) {
			break;
		}
	}
	CHECK_ENUM(EnvelopeStage::RELEASE, sc.status);
}

TEST(SideChainTest, renderReleaseThenOff) {
	SideChain sc;
	sc.registerHit(2000000000);
	// Run through attack and release to reach OFF
	for (int i = 0; i < 10000; i++) {
		sc.render(100, 0);
		if (sc.status == EnvelopeStage::OFF) {
			break;
		}
	}
	CHECK_ENUM(EnvelopeStage::OFF, sc.status);
	// When off, lastValue should be ONE_Q31
	CHECK_EQUAL(2147483647, sc.lastValue);
}

TEST(SideChainTest, renderShapeAffectsRelease) {
	// Shape parameter affects the release curve
	SideChain sc1;
	SideChain sc2;
	sc1.registerHit(2000000000);
	sc2.registerHit(2000000000);

	// Run both into release phase
	for (int i = 0; i < 500; i++) {
		sc1.render(100, 0);
		sc2.render(100, 0);
	}

	// Now in release, render with different shapes
	if (sc1.status == EnvelopeStage::RELEASE && sc2.status == EnvelopeStage::RELEASE) {
		int32_t val1 = sc1.render(10, -2000000000); // Very curved
		int32_t val2 = sc2.render(10, 2000000000);   // Very straight
		// Different shapes should produce different results
		CHECK(val1 != val2);
	}
}

// --- Register Hit Retrospectively ---

TEST(SideChainTest, registerHitRetrospectivelyAttack) {
	SideChain sc;
	// Register a hit that happened 5 samples ago — should still be in attack
	sc.registerHitRetrospectively(2000000000, 5);
	CHECK_ENUM(EnvelopeStage::ATTACK, sc.status);
	CHECK(sc.pos > 0);
}

TEST(SideChainTest, registerHitRetrospectivelyRelease) {
	SideChain sc;
	// Register a hit that happened many samples ago — should be in release or off
	sc.registerHitRetrospectively(2000000000, 50000);
	CHECK(sc.status == EnvelopeStage::RELEASE || sc.status == EnvelopeStage::OFF);
}

TEST(SideChainTest, registerHitRetrospectivelyOff) {
	SideChain sc;
	// Register a hit from very long ago — should be off
	sc.registerHitRetrospectively(2000000000, 500000);
	CHECK_ENUM(EnvelopeStage::OFF, sc.status);
}
