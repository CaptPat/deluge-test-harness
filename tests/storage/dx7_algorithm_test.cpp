// DX7 algorithm and operator edge case tests:
// All 32 algorithms round-trip, operator field boundaries, feedback range,
// multi-program cartridge with distinct algorithms.

#include "CppUTest/TestHarness.h"
#include "storage/DX7Cartridge.h"

// ── All algorithms round-trip ───────────────────────────────────────────────

TEST_GROUP(DX7AlgorithmSweep) {
	DX7Cartridge cart;
	uint8_t original[155];
	uint8_t unpacked[155];
	char opSwitch[7];

	void setup() {
		memset(original, 0, sizeof(original));
		memset(unpacked, 0, sizeof(unpacked));
		strcpy(opSwitch, "111111");
	}
};

TEST(DX7AlgorithmSweep, allAlgorithmsRoundTrip) {
	for (int algo = 0; algo <= 31; algo++) {
		DX7Cartridge c;
		uint8_t pgm[155], unp[155];
		memset(pgm, 0, sizeof(pgm));
		memset(unp, 0, sizeof(unp));

		pgm[134] = algo;
		char ops[7] = "111111";
		c.packProgram(pgm, 0, (char*)"ALGO      ", ops);
		c.saveVoice();
		c.unpackProgram(std::span(unp), 0);

		CHECK_EQUAL(algo, unp[134]);
	}
}

TEST(DX7AlgorithmSweep, algorithmOutOfRangeClampedByNormparm) {
	// normparm clamps values > max to max. Algorithm max = 31.
	// Pack value 63 (6 bits), unpack should clamp to 31.
	original[134] = 63;
	cart.packProgram(original, 0, (char*)"CLAMP     ", opSwitch);
	cart.saveVoice();
	cart.unpackProgram(std::span(unpacked), 0);

	// normparm(value, 31) when value>31 returns max (31)
	CHECK(unpacked[134] <= 31);
}

// ── Feedback range ──────────────────────────────────────────────────────────

TEST_GROUP(DX7FeedbackRange) {};

TEST(DX7FeedbackRange, allFeedbackValuesRoundTrip) {
	// Feedback is 3 bits (0-7), stored at offset 135
	for (int fb = 0; fb <= 7; fb++) {
		DX7Cartridge c;
		uint8_t pgm[155], unp[155];
		memset(pgm, 0, sizeof(pgm));
		memset(unp, 0, sizeof(unp));

		pgm[135] = fb;
		char ops[7] = "111111";
		c.packProgram(pgm, 0, (char*)"FB        ", ops);
		c.saveVoice();
		c.unpackProgram(std::span(unp), 0);

		CHECK_EQUAL(fb, unp[135]);
	}
}

// ── Operator parameter boundaries ───────────────────────────────────────────

TEST_GROUP(DX7OperatorBoundaries) {};

TEST(DX7OperatorBoundaries, egRatesMaxValue) {
	// EG rates (offsets 0-3 per operator) have max 99
	DX7Cartridge c;
	uint8_t pgm[155], unp[155];
	memset(pgm, 0, sizeof(pgm));
	memset(unp, 0, sizeof(unp));

	// Set all EG rates for OP6 (first operator) to max
	pgm[0] = 99; // EG R1
	pgm[1] = 99; // EG R2
	pgm[2] = 99; // EG R3
	pgm[3] = 99; // EG R4

	char ops[7] = "111111";
	c.packProgram(pgm, 0, (char*)"RATES     ", ops);
	c.saveVoice();
	c.unpackProgram(std::span(unp), 0);

	CHECK_EQUAL(99, unp[0]);
	CHECK_EQUAL(99, unp[1]);
	CHECK_EQUAL(99, unp[2]);
	CHECK_EQUAL(99, unp[3]);
}

TEST(DX7OperatorBoundaries, egLevelsMaxValue) {
	// EG levels (offsets 4-7 per operator) have max 99
	DX7Cartridge c;
	uint8_t pgm[155], unp[155];
	memset(pgm, 0, sizeof(pgm));
	memset(unp, 0, sizeof(unp));

	pgm[4] = 99; // EG L1
	pgm[5] = 99; // EG L2
	pgm[6] = 99; // EG L3
	pgm[7] = 99; // EG L4

	char ops[7] = "111111";
	c.packProgram(pgm, 0, (char*)"LEVELS    ", ops);
	c.saveVoice();
	c.unpackProgram(std::span(unp), 0);

	CHECK_EQUAL(99, unp[4]);
	CHECK_EQUAL(99, unp[5]);
	CHECK_EQUAL(99, unp[6]);
	CHECK_EQUAL(99, unp[7]);
}

TEST(DX7OperatorBoundaries, outputLevelMax) {
	// Output level (offset 16 per operator), max 99
	DX7Cartridge c;
	uint8_t pgm[155], unp[155];
	memset(pgm, 0, sizeof(pgm));
	memset(unp, 0, sizeof(unp));

	pgm[16] = 99; // OP6 output level

	char ops[7] = "111111";
	c.packProgram(pgm, 0, (char*)"OL        ", ops);
	c.saveVoice();
	c.unpackProgram(std::span(unp), 0);

	CHECK_EQUAL(99, unp[16]);
}

TEST(DX7OperatorBoundaries, allSixOperatorsIndependent) {
	DX7Cartridge c;
	uint8_t pgm[155], unp[155];
	memset(pgm, 0, sizeof(pgm));
	memset(unp, 0, sizeof(unp));

	// Set distinct EG R1 for each of the 6 operators (21 bytes apart)
	for (int op = 0; op < 6; op++) {
		pgm[op * 21] = 10 * (op + 1); // 10, 20, 30, 40, 50, 60
	}

	char ops[7] = "111111";
	c.packProgram(pgm, 0, (char*)"ALLOPS    ", ops);
	c.saveVoice();
	c.unpackProgram(std::span(unp), 0);

	for (int op = 0; op < 6; op++) {
		CHECK_EQUAL(10 * (op + 1), unp[op * 21]);
	}
}

// ── Multi-program with distinct algorithms ──────────────────────────────────

TEST_GROUP(DX7MultiProgramAlgorithms) {};

TEST(DX7MultiProgramAlgorithms, fourProgramsDistinctAlgorithms) {
	DX7Cartridge c;
	char ops[7] = "111111";

	for (int pgmIdx = 0; pgmIdx < 4; pgmIdx++) {
		uint8_t pgm[155];
		memset(pgm, 0, sizeof(pgm));
		pgm[134] = pgmIdx * 8; // algorithms 0, 8, 16, 24
		char name[11];
		snprintf(name, 11, "PGM%-7d", pgmIdx);
		c.packProgram(pgm, pgmIdx, name, ops);
	}
	c.saveVoice();

	for (int pgmIdx = 0; pgmIdx < 4; pgmIdx++) {
		uint8_t unp[155];
		memset(unp, 0, sizeof(unp));
		c.unpackProgram(std::span(unp), pgmIdx);
		CHECK_EQUAL(pgmIdx * 8, unp[134]);
	}
}

TEST(DX7MultiProgramAlgorithms, all32ProgramSlots) {
	DX7Cartridge c;
	char ops[7] = "111111";

	// Fill all 32 slots with distinct algorithm values
	for (int i = 0; i < 32; i++) {
		uint8_t pgm[155];
		memset(pgm, 0, sizeof(pgm));
		pgm[134] = i;
		char name[11];
		snprintf(name, 11, "A%-9d", i);
		c.packProgram(pgm, i, name, ops);
	}
	c.saveVoice();

	for (int i = 0; i < 32; i++) {
		uint8_t unp[155];
		memset(unp, 0, sizeof(unp));
		c.unpackProgram(std::span(unp), i);
		CHECK_EQUAL(i, unp[134]);
	}
}

// ── LFO parameters round-trip ───────────────────────────────────────────────

TEST_GROUP(DX7LFOParams) {};

TEST(DX7LFOParams, lfoParamsRoundTrip) {
	DX7Cartridge c;
	uint8_t pgm[155], unp[155];
	memset(pgm, 0, sizeof(pgm));
	memset(unp, 0, sizeof(unp));

	pgm[137] = 99; // LFO speed
	pgm[138] = 50; // LFO delay
	pgm[139] = 75; // LFO PMD
	pgm[140] = 25; // LFO AMD

	char ops[7] = "111111";
	c.packProgram(pgm, 0, (char*)"LFO       ", ops);
	c.saveVoice();
	c.unpackProgram(std::span(unp), 0);

	CHECK_EQUAL(99, unp[137]);
	CHECK_EQUAL(50, unp[138]);
	CHECK_EQUAL(75, unp[139]);
	CHECK_EQUAL(25, unp[140]);
}

// ── Operator key sync ───────────────────────────────────────────────────────

TEST_GROUP(DX7OscKeySync) {};

TEST(DX7OscKeySync, oscKeySyncBit) {
	// OKS is stored at offset 136, packed with feedback at offset 111
	for (int oks = 0; oks <= 1; oks++) {
		DX7Cartridge c;
		uint8_t pgm[155], unp[155];
		memset(pgm, 0, sizeof(pgm));
		memset(unp, 0, sizeof(unp));

		pgm[136] = oks;

		char ops[7] = "111111";
		c.packProgram(pgm, 0, (char*)"OKS       ", ops);
		c.saveVoice();
		c.unpackProgram(std::span(unp), 0);

		CHECK_EQUAL(oks, unp[136]);
	}
}
