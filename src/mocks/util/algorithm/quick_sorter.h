// Shadow header replacing firmware's util/algorithm/quick_sorter.h
// The real QuickSorter uses (uint32_t)memory pointer casts — SEGFAULTs on x86-64.
// This shadow provides an x86-safe implementation using std::sort.

#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>

class QuickSorter {
public:
	QuickSorter(int32_t newElementSize, int32_t keyNumBits, void* newMemory)
	    : elementSize(newElementSize), keyMask((1u << keyNumBits) - 1), memory(newMemory) {}

	void sort(int32_t numElements) {
		// Simple insertion sort (small N in practice, avoids complex std::sort with raw memory)
		auto getAddr = [&](int32_t i) -> uint8_t* {
			return static_cast<uint8_t*>(memory) + i * elementSize;
		};
		auto getKey = [&](int32_t i) -> uint32_t {
			uint32_t key;
			std::memcpy(&key, getAddr(i), sizeof(key));
			return key & keyMask;
		};
		uint8_t tmp[256]; // Max element size we'd expect
		for (int32_t i = 1; i < numElements; i++) {
			int32_t j = i;
			while (j > 0 && getKey(j - 1) > getKey(j)) {
				std::memcpy(tmp, getAddr(j), elementSize);
				std::memcpy(getAddr(j), getAddr(j - 1), elementSize);
				std::memcpy(getAddr(j - 1), tmp, elementSize);
				j--;
			}
		}
	}

private:
	const int32_t elementSize;
	const uint32_t keyMask;
	void* const memory;
};
