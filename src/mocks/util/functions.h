// Shadow header replacing firmware's util/functions.h
// The real header includes fatfs/ff.h, d_string.h, cfunctions.h, etc.
// We provide only the declarations needed by compiled firmware code,
// plus re-exports of utility headers that leaf code expects transitively.

#pragma once

#include "definitions_cxx.hpp"
#include "util/const_functions.h"
#include "util/fixedpoint.h"
#include "util/lookuptables/lookuptables.h"
#include "util/misc.h"
#include "util/waves.h"
#include <algorithm>
#include <cstdint>
#include <cstring>

// SSI_TX_BUFFER_NUM_SAMPLES — defined in firmware/src/definitions.h, needed by filter.h
#ifndef SSI_TX_BUFFER_NUM_SAMPLES
#define SSI_TX_BUFFER_NUM_SAMPLES 128
#endif

// Global buffer used by ordered_resizeable_array searchMultiple (real one is kFilenameBufferSize=256 bytes)
extern char miscStringBuffer[];

// Implemented in functions_stubs.cpp
int32_t interpolateTable(uint32_t input, int32_t numBitsInInput, const uint16_t* table, int32_t numBitsInTableSize = 8);
uint32_t interpolateTableInverse(int32_t tableValueBig, int32_t numBitsInLookupOutput, const uint16_t* table,
                                 int32_t numBitsInTableSize = 8);
int32_t getDecay8(uint32_t input, uint8_t numBitsInInput);
int32_t getDecay4(uint32_t input, uint8_t numBitsInInput);
int32_t getParamFromUserValue(uint8_t p, int8_t userValue);
int32_t combineHitStrengths(int32_t strength1, int32_t strength2);
int32_t instantTan(int32_t input);
int32_t quickLog(uint32_t input);

// --- Saturation / tanH inline functions needed by DSP filters ---
// The x86 fallback for signed_saturate<N> in fixedpoint.h is broken:
//   std::min(val, 1 << bits) — wrong upper bound, no lower bound clamping.
// ARM ssat clamps to [-(1<<(bits-1)), (1<<(bits-1))-1].
// We provide correct versions here for all DSP filter code.

// Correct signed saturation matching ARM ssat behavior
template <uint8_t bits>
[[gnu::always_inline]] inline int32_t ssat_correct(int32_t val) {
	constexpr int32_t max_val = (1 << (bits - 1)) - 1;
	constexpr int32_t min_val = -(1 << (bits - 1));
	if (val > max_val)
		return max_val;
	if (val < min_val)
		return min_val;
	return val;
}

[[gnu::always_inline]] inline int32_t ssat_correct_runtime(int32_t val, int32_t bits) {
	int32_t max_val = (1 << (bits - 1)) - 1;
	int32_t min_val = -(1 << (bits - 1));
	if (val > max_val)
		return max_val;
	if (val < min_val)
		return min_val;
	return val;
}

[[gnu::always_inline]] inline int32_t signed_saturate_operand_unknown(int32_t val, int32_t bits) {
	return ssat_correct_runtime(val, bits);
}

template <uint8_t lshift>
[[gnu::always_inline]] inline int32_t lshiftAndSaturate(int32_t val) {
	return ssat_correct<32 - lshift>(val) << lshift;
}

[[gnu::always_inline]] inline int32_t lshiftAndSaturateUnknown(int32_t val, uint8_t lshift) {
	return signed_saturate_operand_unknown(val, 32 - lshift) << lshift;
}

[[gnu::always_inline]] inline int32_t interpolateTableSigned2d(uint32_t inputX, uint32_t inputY,
                                                               int32_t numBitsInInputX, int32_t numBitsInInputY,
                                                               const int16_t* table, int32_t numBitsInTableSizeX,
                                                               int32_t numBitsInTableSizeY) {
	int32_t whichValue = inputY >> (numBitsInInputY - numBitsInTableSizeY);
	int32_t tableSizeOneRow = (1 << numBitsInTableSizeX) + 1;
	int32_t value1 =
	    interpolateTableSigned(inputX, numBitsInInputX, &table[whichValue * tableSizeOneRow], numBitsInTableSizeX);
	int32_t value2 = interpolateTableSigned(inputX, numBitsInInputX, &table[(whichValue + 1) * tableSizeOneRow],
	                                        numBitsInTableSizeX);
	int32_t lshiftAmount = 31 + numBitsInTableSizeY - numBitsInInputY;
	uint32_t strength2;
	if (lshiftAmount >= 0)
		strength2 = (inputY << lshiftAmount) & 2147483647;
	else
		strength2 = (inputY >> (0 - lshiftAmount)) & 2147483647;
	uint32_t strength1 = 2147483647 - strength2;
	return multiply_32x32_rshift32(value1, strength1) + multiply_32x32_rshift32(value2, strength2);
}

template <unsigned saturationAmount>
[[gnu::always_inline]] inline int32_t getTanH(int32_t input) {
	uint32_t workingValue;
	if (saturationAmount)
		workingValue = (uint32_t)lshiftAndSaturate<saturationAmount>(input) + 2147483648u;
	else
		workingValue = (uint32_t)input + 2147483648u;
	return interpolateTableSigned(workingValue, 32, tanHSmall, 8) >> (saturationAmount + 2);
}

[[gnu::always_inline]] inline int32_t getTanHUnknown(int32_t input, uint32_t saturationAmount) {
	uint32_t workingValue;
	if (saturationAmount)
		workingValue = (uint32_t)lshiftAndSaturateUnknown(input, saturationAmount) + 2147483648u;
	else
		workingValue = (uint32_t)input + 2147483648u;
	return interpolateTableSigned(workingValue, 32, tanHSmall, 8) >> (saturationAmount + 2);
}

[[gnu::always_inline]] inline int32_t getTanHAntialiased(int32_t input, uint32_t* lastWorkingValue,
                                                         uint32_t saturationAmount) {
	uint32_t workingValue = (uint32_t)lshiftAndSaturateUnknown(input, saturationAmount) + 2147483648u;
	int32_t toReturn = interpolateTableSigned2d(workingValue, *lastWorkingValue, 32, 32, &tanH2d[0][0], 7, 6)
	                   >> (saturationAmount + 1);
	*lastWorkingValue = workingValue;
	return toReturn;
}

[[gnu::always_inline]] inline int32_t getNoise() {
	return CONG;
}

// Phase 15: swapEndianness — x86 fallback (ARM uses rev/rev16 instructions)
[[gnu::always_inline]] inline uint32_t swapEndianness32(uint32_t input) {
	return ((input >> 24) & 0xFF) | ((input >> 8) & 0xFF00) | ((input << 8) & 0xFF0000) | ((input << 24) & 0xFF000000);
}

[[gnu::always_inline]] inline uint32_t swapEndianness2x16(uint32_t input) {
	return ((input >> 8) & 0x00FF00FF) | ((input << 8) & 0xFF00FF00);
}

// Phase 12: q31_mult — x86 fallback (ARM version uses smmul inline asm)
[[gnu::always_inline]] inline q31_t q31_mult(q31_t a, q31_t b) {
	return (int32_t)(((int64_t)a * b) >> 31);
}

// Phase 12: q31tRescale — x86 fallback
[[gnu::always_inline]] inline q31_t q31tRescale(q31_t a, uint32_t proportion) {
	return (int32_t)(((int64_t)a * (int64_t)proportion) >> 32);
}

// Phase 12: random/getRandom255/sampleTriangleDistribution — used by GranularProcessor
inline int32_t random(int32_t upperLimit) {
	return (upperLimit > 0) ? (std::rand() % upperLimit) : 0;
}

[[gnu::always_inline]] inline uint8_t getRandom255() {
	return static_cast<uint8_t>(std::rand() & 0xFF);
}

inline q31_t sampleTriangleDistribution() {
	int32_t a = static_cast<int32_t>(std::rand()) - (RAND_MAX / 2);
	int32_t b = static_cast<int32_t>(std::rand()) - (RAND_MAX / 2);
	return static_cast<q31_t>((static_cast<int64_t>(a) + b) / 2);
}

bool shouldDoPanning(int32_t panAmount, int32_t* amplitudeL, int32_t* amplitudeR);

// Phase 9: getMagnitudeOld — used by sample_controls.cpp
[[gnu::always_inline]] inline int32_t getMagnitudeOld(uint32_t input) {
	return 32 - clz(input);
}

[[gnu::always_inline]] inline int32_t increaseMagnitudeAndSaturate(int32_t number, int32_t magnitude) {
	if (magnitude > 0) {
		return lshiftAndSaturateUnknown(number, magnitude);
	}
	return number >> (-magnitude);
}

struct StereoFloatSample {
	float l;
	float r;
};

// Phase 8: strcmpspecial — natural string comparison with numeric ordering
extern bool shouldInterpretNoteNames;
extern bool octaveStartsFromA;
int32_t strcmpspecial(char const* first, char const* second);
