// Stubs for util/functions.h — functions needed by harness code.
// The real functions.cpp has deep deps (fatfs, d_string, etc.).
// These implementations are ported directly from firmware/src/deluge/util/functions.cpp.

#include "util/lookuptables/lookuptables.h"
#include "modulation/params/param.h"
#include <algorithm>
#include <cstdint>

int32_t interpolateTable(uint32_t input, int32_t numBitsInInput, const uint16_t* table, int32_t numBitsInTableSize) {
	int32_t whichValue = input >> (numBitsInInput - numBitsInTableSize);
	int32_t value1 = table[whichValue];
	int32_t value2 = table[whichValue + 1];

	int32_t rshiftAmount = numBitsInInput - 15 - numBitsInTableSize;
	uint32_t rshifted;
	if (rshiftAmount >= 0) {
		rshifted = input >> rshiftAmount;
	}
	else {
		rshifted = input << (-rshiftAmount);
	}

	int32_t strength2 = rshifted & 32767;
	int32_t strength1 = 32768 - strength2;
	return value1 * strength1 + value2 * strength2;
}

uint32_t interpolateTableInverse(int32_t tableValueBig, int32_t numBitsInLookupOutput, const uint16_t* table,
                                 int32_t numBitsInTableSize) {
	int32_t tableValue = tableValueBig >> 15;
	int32_t tableSize = 1 << numBitsInTableSize;

	int32_t tableDirection = (table[0] < table[tableSize]) ? 1 : -1;

	if ((tableValue - table[0]) * tableDirection <= 0) {
		return 0;
	}
	if ((tableValue - table[tableSize]) * tableDirection >= 0) {
		return (1 << numBitsInLookupOutput) - 1;
	}

	int32_t rangeStart = 0;
	int32_t rangeEnd = tableSize;

	while (rangeStart + 1 < rangeEnd) {
		int32_t examinePos = (rangeStart + rangeEnd) >> 1;
		if ((tableValue - table[examinePos]) * tableDirection >= 0) {
			rangeStart = examinePos;
		}
		else {
			rangeEnd = examinePos;
		}
	}

	uint32_t output = rangeStart << (numBitsInLookupOutput - numBitsInTableSize);
	output += (uint64_t)(tableValueBig - ((uint16_t)table[rangeStart] << 15))
	          * (1 << (numBitsInLookupOutput - numBitsInTableSize))
	          / (((uint16_t)table[rangeStart + 1] - (uint16_t)table[rangeStart]) << 15);

	return output;
}

int32_t getDecay8(uint32_t input, uint8_t numBitsInInput) {
	return interpolateTable(input, numBitsInInput, decayTableSmall8, 8);
}

int32_t getDecay4(uint32_t input, uint8_t numBitsInInput) {
	return interpolateTable(input, numBitsInInput, decayTableSmall4, 8);
}

// Phase 4: sidechain needs these
int32_t getParamFromUserValue(uint8_t p, int8_t userValue) {
	namespace params = deluge::modulation::params;
	switch (p) {
	case util::to_underlying(params::STATIC_SIDECHAIN_ATTACK):
		return attackRateTable[userValue] * 4;
	case util::to_underlying(params::STATIC_SIDECHAIN_RELEASE):
		return releaseRateTable[userValue] * 8;
	default:
		return 0;
	}
}

int32_t combineHitStrengths(int32_t strength1, int32_t strength2) {
	uint32_t sum = (uint32_t)strength1 + (uint32_t)strength2;
	sum = std::min(sum, (uint32_t)2147483647);
	int32_t maxOne = std::max(strength1, strength2);
	return (maxOne >> 1) + (sum >> 1);
}

// Phase 6: blendBuffer is now provided by the real filter.cpp (Phase 7)
#include "util/functions.h"

// Phase 6: instantTan — ported from firmware/src/deluge/util/functions.cpp
// Uses tanTable[] from lookuptables.cpp (already compiled).
int32_t instantTan(int32_t input) {
	int32_t whichValue = input >> 25;
	int32_t howMuchFurther = (input << 6) & 2147483647;
	int32_t value1 = tanTable[whichValue];
	int32_t value2 = tanTable[whichValue + 1];
	return (multiply_32x32_rshift32(value2, howMuchFurther)
	        + multiply_32x32_rshift32(value1, 2147483647 - howMuchFurther))
	       << 1;
}

// Phase 6: quickLog — approximate log2 in 8.24 fixed point
int32_t quickLog(uint32_t input) {
	if (input == 0)
		return 0;
	int32_t bits = 31 - __builtin_clz(input);
	return bits << 24;
}

// Phase 12: shouldDoPanning — used by GranularProcessor
bool shouldDoPanning(int32_t panAmount, int32_t* amplitudeL, int32_t* amplitudeR) {
	if (panAmount == 0) {
		return false;
	}
	// Simple linear panning for test purposes
	if (panAmount > 0) {
		*amplitudeR = 2147483647;
		*amplitudeL = 2147483647 - panAmount;
	}
	else {
		*amplitudeL = 2147483647;
		*amplitudeR = 2147483647 + panAmount;
	}
	return true;
}

// Phase G: paramNeutralValues, getExp, getFinalParameterValueExp, cableToExpParamShortcut
// Ported from firmware/src/deluge/util/functions.cpp — needed by stutterer.cpp
int32_t paramNeutralValues[deluge::modulation::params::GLOBAL_NONE] = {};
int32_t paramRanges[deluge::modulation::params::GLOBAL_NONE] = {};

// Initialize key neutral values at static init time
static struct ParamNeutralInit {
	ParamNeutralInit() {
		namespace params = deluge::modulation::params;
		// GLOBAL_DELAY_RATE neutral value from firmware/src/deluge/util/functions.cpp
		paramNeutralValues[params::GLOBAL_DELAY_RATE] = 536870912;
	}
} paramNeutralInit;

int32_t getExp(int32_t presetValue, int32_t adjustment) {
	int32_t magnitudeIncrease = (adjustment >> 26) + 2;
	int32_t adjustedPresetValue =
	    multiply_32x32_rshift32(presetValue, interpolateTable(adjustment & 67108863, 26, expTableSmall));
	return increaseMagnitudeAndSaturate(adjustedPresetValue, magnitudeIncrease);
}

int32_t getFinalParameterValueExp(int32_t paramNeutralValue, int32_t patchedValue) {
	return getExp(paramNeutralValue, patchedValue);
}

int32_t cableToExpParamShortcut(int32_t sourceValue) {
	return sourceValue >> 2;
}

// Phase H: getFinalParameterValue* family — needed by patcher.cpp
// Ported from firmware/src/deluge/util/functions.cpp

int32_t getFinalParameterValueHybrid(int32_t paramNeutralValue, int32_t patchedValue) {
	int32_t preLimits = (paramNeutralValue >> 2) + (patchedValue >> 1);
	return signed_saturate<32 - 3>(preLimits) << 2;
}

int32_t getFinalParameterValueVolume(int32_t paramNeutralValue, int32_t patchedValue) {
	int32_t positivePatchedValue = patchedValue + 536870912;
	positivePatchedValue = (positivePatchedValue >> 16) * (positivePatchedValue >> 15);
	return lshiftAndSaturate<5>(multiply_32x32_rshift32(positivePatchedValue, paramNeutralValue));
}

int32_t getFinalParameterValueLinear(int32_t paramNeutralValue, int32_t patchedValue) {
	int32_t positivePatchedValue = patchedValue + 536870912;
	return lshiftAndSaturate<3>(multiply_32x32_rshift32(positivePatchedValue, paramNeutralValue));
}

int32_t lookupReleaseRate(int32_t input) {
	int32_t magnitude = 24;
	int32_t whichValue = input >> magnitude;
	int32_t howMuchFurther = (input << (31 - magnitude)) & 2147483647;
	whichValue += 32;
	if (whichValue < 0) {
		return releaseRateTable64[0];
	}
	else if (whichValue >= 64) {
		return releaseRateTable64[64];
	}
	int32_t value1 = releaseRateTable64[whichValue];
	int32_t value2 = releaseRateTable64[whichValue + 1];
	return (multiply_32x32_rshift32(value2, howMuchFurther)
	        + multiply_32x32_rshift32(value1, 2147483647 - howMuchFurther))
	       << 1;
}

int32_t getFinalParameterValueExpWithDumbEnvelopeHack(int32_t paramNeutralValue, int32_t patchedValue, int32_t p) {
	namespace params = deluge::modulation::params;
	if (params::LOCAL_ENV_0_DECAY <= p && p <= params::LOCAL_ENV_3_RELEASE) {
		return multiply_32x32_rshift32(paramNeutralValue, lookupReleaseRate(patchedValue));
	}
	if (params::LOCAL_ENV_0_ATTACK <= p && p <= params::LOCAL_ENV_3_ATTACK) {
		patchedValue = -patchedValue;
	}
	return getFinalParameterValueExp(paramNeutralValue, patchedValue);
}

int32_t cableToLinearParamShortcut(int32_t sourceValue) {
	return sourceValue >> 2;
}

// Phase 8: strcmpspecial — natural string comparison with numeric ordering
// Ported from firmware/src/deluge/util/functions.cpp:1718-1863
// Global flags control note name interpretation (default: off)
bool shouldInterpretNoteNames = false;
bool octaveStartsFromA = false;

// Returns positive if first > second, negative if first < second, 0 if equal
int32_t strcmpspecial(char const* first, char const* second) {

	int32_t resultIfGetToEndOfBothStrings = 0;

	while (true) {
		bool firstIsFinished = (*first == 0);
		bool secondIsFinished = (*second == 0);

		if (firstIsFinished && secondIsFinished) {
			return resultIfGetToEndOfBothStrings;
		}

		if (firstIsFinished || secondIsFinished) {
			return (int32_t)*first - (int32_t)*second;
		}

		bool firstIsNumber = (*first >= '0' && *first <= '9');
		bool secondIsNumber = (*second >= '0' && *second <= '9');

		// If they're both numbers...
		if (firstIsNumber && secondIsNumber) {

			// Check for differing leading zeros
			if (!resultIfGetToEndOfBothStrings) {
				char const* firstHere = first;
				char const* secondHere = second;
				while (true) {
					char firstChar = *firstHere;
					char secondChar = *secondHere;
					bool firstDigitIsLeadingZero = (firstChar == '0');
					bool secondDigitIsLeadingZero = (secondChar == '0');

					if (firstDigitIsLeadingZero && secondDigitIsLeadingZero) {
						firstHere++;
						secondHere++;
						continue;
					}

					resultIfGetToEndOfBothStrings = (int32_t)firstChar - (int32_t)secondChar;
					break;
				}
			}

			int32_t firstNumber = *first - '0';
			int32_t secondNumber = *second - '0';
			first++;
			second++;

			while (*first >= '0' && *first <= '9') {
				firstNumber *= 10;
				firstNumber += *first - '0';
				first++;
			}

			while (*second >= '0' && *second <= '9') {
				secondNumber *= 10;
				secondNumber += *second - '0';
				second++;
			}

			int32_t difference = firstNumber - secondNumber;
			if (difference) {
				return difference;
			}
		}

		// Otherwise, if not both numbers...
		else {

			char firstChar = *first;
			char secondChar = *second;

			// Make lowercase
			if (firstChar >= 'A' && firstChar <= 'Z') {
				firstChar += 32;
			}
			if (secondChar >= 'A' && secondChar <= 'Z') {
				secondChar += 32;
			}

			// Note name interpretation is skipped in test harness (shouldInterpretNoteNames = false)

			// If they're the same, carry on
			if (firstChar == secondChar) {
				first++;
				second++;
			}

			// Otherwise...
			else {
				// Dot then underscore comes first
				if (firstChar == '.') {
					return -1;
				}
				else if (secondChar == '.') {
					return 1;
				}

				if (firstChar == '_') {
					return -1;
				}
				else if (secondChar == '_') {
					return 1;
				}

				return (int32_t)firstChar - (int32_t)secondChar;
			}
		}
	}
}

// Phase I2: PatchSource string conversions ported from firmware/src/deluge/util/functions.cpp

char const* sourceToString(PatchSource source) {
	switch (source) {
	case PatchSource::LFO_GLOBAL_1: return "lfo1";
	case PatchSource::LFO_GLOBAL_2: return "lfo3";
	case PatchSource::LFO_LOCAL_1: return "lfo2";
	case PatchSource::LFO_LOCAL_2: return "lfo4";
	case PatchSource::ENVELOPE_0: return "envelope1";
	case PatchSource::ENVELOPE_1: return "envelope2";
	case PatchSource::ENVELOPE_2: return "envelope3";
	case PatchSource::ENVELOPE_3: return "envelope4";
	case PatchSource::VELOCITY: return "velocity";
	case PatchSource::NOTE: return "note";
	case PatchSource::SIDECHAIN: return "compressor";
	case PatchSource::RANDOM: return "random";
	case PatchSource::AFTERTOUCH: return "aftertouch";
	case PatchSource::X: return "x";
	case PatchSource::Y: return "y";
	default: return "none";
	}
}

PatchSource stringToSource(char const* string) {
	for (int32_t s = 0; s < kNumPatchSources; s++) {
		auto patchSource = static_cast<PatchSource>(s);
		if (!strcmp(string, sourceToString(patchSource))) {
			return patchSource;
		}
	}
	return PatchSource::NONE;
}

char const* sourceToStringShort(PatchSource source) {
	return sourceToString(source);
}
