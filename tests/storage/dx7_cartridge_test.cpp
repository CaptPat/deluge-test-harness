// Tests for DX7Cartridge: sysex checksum, pack/unpack program round-trip

#include "CppUTest/TestHarness.h"
#include "storage/DX7Cartridge.h"

// ── sysexChecksum ───────────────────────────────────────────────────────

TEST_GROUP(SysexChecksum) {};

TEST(SysexChecksum, emptySpanReturnsZero) {
	std::span<std::byte> empty;
	std::byte result = sysexChecksum(empty);
	CHECK_EQUAL(0, std::to_integer<int>(result));
}

TEST(SysexChecksum, singleByteChecksum) {
	std::byte data[] = {std::byte{0x10}};
	std::byte result = sysexChecksum(std::span(data));
	// (-0x10) & 0x7F = 0x70
	CHECK_EQUAL(0x70, std::to_integer<int>(result));
}

TEST(SysexChecksum, multiByteChecksum) {
	std::byte data[] = {std::byte{0x01}, std::byte{0x02}, std::byte{0x03}};
	std::byte result = sysexChecksum(std::span(data));
	// -(1+2+3) = -6, -6 & 0x7F = 0x7A
	CHECK_EQUAL(0x7A, std::to_integer<int>(result));
}

TEST(SysexChecksum, overflowWraps) {
	// 0x7F * 2 = 254, -(254) & 0x7F = 2
	std::byte data[] = {std::byte{0x7F}, std::byte{0x7F}};
	std::byte result = sysexChecksum(std::span(data));
	CHECK_EQUAL(2, std::to_integer<int>(result));
}

// ── DX7Cartridge ────────────────────────────────────────────────────────

TEST_GROUP(DX7Cartridge) {};

TEST(DX7Cartridge, defaultConstructorCreatesEmptyCartridge) {
	DX7Cartridge cart;
	CHECK_FALSE(cart.isCartridge());
}

TEST(DX7Cartridge, packUnpackRoundTrip) {
	DX7Cartridge cart;

	uint8_t original[155];
	memset(original, 0, sizeof(original));

	// EG rates and levels (first 10 bytes per operator are direct-mapped)
	original[0] = 90;  // OP6 EG R1
	original[1] = 80;  // OP6 EG R2
	original[5] = 70;  // OP6 EG L2
	original[134] = 5; // Algorithm
	original[135] = 3; // Feedback
	original[137] = 35; // LFO speed

	char opSwitch[7] = "111111";
	cart.packProgram(original, 0, (char*)"TESTPATCH ", opSwitch);

	// saveVoice() sets the sysex header so isCartridge() returns true
	cart.saveVoice();

	uint8_t unpacked[155];
	memset(unpacked, 0, sizeof(unpacked));
	cart.unpackProgram(std::span(unpacked), 0);

	CHECK_EQUAL(90, unpacked[0]);
	CHECK_EQUAL(80, unpacked[1]);
	CHECK_EQUAL(70, unpacked[5]);
	CHECK_EQUAL(5, unpacked[134]);
	CHECK_EQUAL(3, unpacked[135]);
	CHECK_EQUAL(35, unpacked[137]);
}

TEST(DX7Cartridge, saveVoiceMakesCartridge) {
	DX7Cartridge cart;
	CHECK_FALSE(cart.isCartridge());

	uint8_t pgm[155] = {};
	char opSwitch[7] = "111111";
	cart.packProgram(pgm, 0, (char*)"TEST      ", opSwitch);
	cart.saveVoice();

	CHECK(cart.isCartridge());
}

TEST(DX7Cartridge, packWithDisabledOperator) {
	DX7Cartridge cart;

	uint8_t original[155];
	memset(original, 0, sizeof(original));
	original[16] = 99; // OP6 output level

	char opSwitch[7] = "011111"; // Disable OP6
	cart.packProgram(original, 0, (char*)"MUTED     ", opSwitch);
	cart.saveVoice();

	uint8_t unpacked[155];
	memset(unpacked, 0, sizeof(unpacked));
	cart.unpackProgram(std::span(unpacked), 0);

	// OP6 output level should be 0 (muted by opSwitch)
	CHECK_EQUAL(0, unpacked[16]);
}

TEST(DX7Cartridge, packMultiplePrograms) {
	DX7Cartridge cart;

	uint8_t pgm0[155], pgm1[155];
	memset(pgm0, 0, sizeof(pgm0));
	memset(pgm1, 0, sizeof(pgm1));

	pgm0[134] = 10;
	pgm1[134] = 20;

	char opSwitch[7] = "111111";
	cart.packProgram(pgm0, 0, (char*)"PGM0      ", opSwitch);
	cart.packProgram(pgm1, 1, (char*)"PGM1      ", opSwitch);
	cart.saveVoice();

	uint8_t unp0[155], unp1[155];
	memset(unp0, 0, sizeof(unp0));
	memset(unp1, 0, sizeof(unp1));

	cart.unpackProgram(std::span(unp0), 0);
	cart.unpackProgram(std::span(unp1), 1);

	CHECK_EQUAL(10, unp0[134]);
	CHECK_EQUAL(20, unp1[134]);
}

TEST(DX7Cartridge, nameIsPaddedWithSpaces) {
	DX7Cartridge cart;

	uint8_t original[155];
	memset(original, 0, sizeof(original));

	char opSwitch[7] = "111111";
	cart.packProgram(original, 0, (char*)"HI\0       ", opSwitch);
	cart.saveVoice();

	uint8_t unpacked[155];
	memset(unpacked, 0, sizeof(unpacked));
	cart.unpackProgram(std::span(unpacked), 0);

	// Name in cartridge format is at offset 118 within each 128-byte packed voice,
	// but after unpack, name is at offset 145 in the 155-byte unpacked form
	CHECK_EQUAL('H', unpacked[145]);
	CHECK_EQUAL('I', unpacked[146]);
	CHECK_EQUAL(' ', unpacked[147]);
	CHECK_EQUAL(' ', unpacked[154]);
}

TEST(DX7Cartridge, normalizePgmNameStripsHighBit) {
	char buffer[11];
	const char input[11] = {(char)0xC8, (char)0xC9, 0, 0, 0, 0, 0, 0, 0, 0}; // 'H'|0x80, 'I'|0x80
	DX7Cartridge::normalizePgmName(buffer, input);
	CHECK_EQUAL('H', buffer[0]);
	CHECK_EQUAL('I', buffer[1]);
}

TEST(DX7Cartridge, normalizePgmNameTrimsTrailingSpaces) {
	char buffer[11];
	DX7Cartridge::normalizePgmName(buffer, "AB        ");
	CHECK_EQUAL('A', buffer[0]);
	CHECK_EQUAL('B', buffer[1]);
	CHECK_EQUAL(0, buffer[2]); // trailing spaces trimmed
}

TEST(DX7Cartridge, checksumComplementsSum) {
	// The DX7 checksum is: (-sum) & 0x7F
	// For known sysex data this ensures the checksum validates
	std::byte data[] = {std::byte{0x01}, std::byte{0x02}, std::byte{0x03}};
	std::byte checksum = sysexChecksum(std::span(data));

	// Adding checksum to the sum should give 0 mod 128
	int total = 1 + 2 + 3 + std::to_integer<int>(checksum);
	CHECK_EQUAL(0, total & 0x7F);
}
