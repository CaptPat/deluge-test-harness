// Wrapper: compile open_addressing_hash_table.cpp with x86-64 portability fix.
//
// getBucketAddress() and secondaryMemoryGetBucketAddress() cast void* to
// uint32_t for pointer arithmetic, which truncates 64-bit pointers.
// This wrapper replaces those two casts with uintptr_t.
//
// Changed lines (vs firmware original):
//   getBucketAddress:                (uint32_t)memory         → (uintptr_t)memory
//   secondaryMemoryGetBucketAddress: (uint32_t)secondaryMemory → (uintptr_t)secondaryMemory

// Include all headers the .cpp needs (pragma once makes re-inclusion a no-op).
#include "util/container/hashtable/open_addressing_hash_table.h"
#include "definitions_cxx.hpp"
#include "hid/display/display.h"
#include "io/debug/log.h"
#include "memory/general_memory_allocator.h"
#include "util/functions.h"
#include <cstdint>
#include <string.h>

// Provide the two fixed implementations before including the .cpp.
// Then use #define to rename the original definitions so they compile
// under dummy names (the originals are never called because the .cpp's
// own call sites also get renamed, so we re-provide fixed free-standing
// versions below the include as well — but actually we use a different
// strategy: we just provide the implementations here and skip those
// two functions in the .cpp by macro-renaming them).
//
// IMPORTANT: Because the #define also renames CALL SITES inside
// insert()/lookup()/remove(), we must instead take the approach of
// including the .cpp WITHOUT the two broken functions, then providing
// fixed versions.  The trick: we define the member function names to
// expand to an inline body that uses uintptr_t, effectively replacing
// the definition while keeping call sites working.
//
// Actually the simplest correct approach: since these are non-virtual
// member functions called by name, we use a sed-like preprocessor
// approach.  We cannot do that in C++.  So we take the pragmatic route:
// provide the entire .cpp content with the two lines fixed.

#define SECONDARY_MEMORY_FUNCTION_NONE 0
#define SECONDARY_MEMORY_FUNCTION_BEING_INITIALIZED 1
#define SECONDARY_MEMORY_FUNCTION_BEING_REHASHED_FROM 2

OpenAddressingHashTable::OpenAddressingHashTable() {
	memory = nullptr;
	numBuckets = 0;
	numElements = 0;

	secondaryMemory = nullptr;
	secondaryMemoryNumBuckets = 0;
	secondaryMemoryCurrentFunction = SECONDARY_MEMORY_FUNCTION_NONE;

	initialNumBuckets = 16;
}

OpenAddressingHashTable::~OpenAddressingHashTable() {
	empty(true);
}

void OpenAddressingHashTable::empty(bool destructing) {
	if (memory) {
		delugeDealloc(memory);
	}
	if (secondaryMemory) {
		delugeDealloc(secondaryMemory);
	}

	if (!destructing) {
		memory = nullptr;
		numBuckets = 0;
		numElements = 0;

		secondaryMemory = nullptr;
		secondaryMemoryNumBuckets = 0;
		secondaryMemoryCurrentFunction = SECONDARY_MEMORY_FUNCTION_NONE;
	}
}

uint32_t hash(uint32_t x) {
	x = ((x >> 16) ^ x) * 0x45d9f3b;
	x = ((x >> 16) ^ x) * 0x45d9f3b;
	x = (x >> 16) ^ x;
	return x;
}

int32_t OpenAddressingHashTable::getBucketIndex(uint32_t key) {
	return hash(key) & (numBuckets - 1);
}

// *** FIX: uintptr_t instead of uint32_t ***
void* OpenAddressingHashTable::getBucketAddress(int32_t b) {
	return (void*)((uintptr_t)memory + b * elementSize);
}

// *** FIX: uintptr_t instead of uint32_t ***
void* OpenAddressingHashTable::secondaryMemoryGetBucketAddress(int32_t b) {
	return (void*)((uintptr_t)secondaryMemory + b * elementSize);
}

void* OpenAddressingHashTable::insert(uint32_t key, bool* onlyIfNotAlreadyPresent) {

#if ALPHA_OR_BETA_VERSION
	if (doesKeyIndicateEmptyBucket(key)) {
		FREEZE_WITH_ERROR("E330");
	}
#endif

	// If no memory, get some
	if (!memory) {
		int32_t newNumBuckets = initialNumBuckets;
		memory = GeneralMemoryAllocator::get().allocMaxSpeed(newNumBuckets * elementSize);
		if (!memory) {
			return nullptr;
		}

		numBuckets = newNumBuckets;
		numElements = 0; // Should already be...

		memset(memory, 0xFF, newNumBuckets * elementSize);
	}

	// Or if reached 75% full, try getting more
	else if (numElements >= numBuckets - (numBuckets >> 2)) {
		int32_t newNumBuckets = numBuckets << 1;

		secondaryMemory = GeneralMemoryAllocator::get().allocMaxSpeed(newNumBuckets * elementSize);
		if (secondaryMemory) {

			// Initialize
			secondaryMemoryNumBuckets = newNumBuckets;
			secondaryMemoryCurrentFunction = SECONDARY_MEMORY_FUNCTION_BEING_INITIALIZED;
			memset(secondaryMemory, 0xFF, secondaryMemoryNumBuckets * elementSize);

			// Swap the memories
			void* tempMemory = memory;
			memory = secondaryMemory;
			secondaryMemory = tempMemory;

			int32_t tempNumBuckets = numBuckets;
			numBuckets = secondaryMemoryNumBuckets;
			secondaryMemoryNumBuckets = tempNumBuckets;

			// Rehash
			secondaryMemoryCurrentFunction = SECONDARY_MEMORY_FUNCTION_BEING_REHASHED_FROM;
			secondaryMemoryFunctionCurrentIteration = 0;

			while (secondaryMemoryFunctionCurrentIteration < secondaryMemoryNumBuckets) {
				void* sourceBucketAddress = secondaryMemoryGetBucketAddress(secondaryMemoryFunctionCurrentIteration);
				uint32_t keyHere = getKeyFromAddress(sourceBucketAddress);

				// If there was something in that bucket, copy it
				if (!doesKeyIndicateEmptyBucket(keyHere)) {
					int32_t destBucketIndex = getBucketIndex(keyHere);

					void* destBucketAddress;
					while (true) {
						destBucketAddress = getBucketAddress(destBucketIndex);
						uint32_t destKey = getKeyFromAddress(destBucketAddress);
						if (doesKeyIndicateEmptyBucket(destKey)) {
							break;
						}
						destBucketIndex = (destBucketIndex + 1) & (numBuckets - 1);
					}

					// Ok, we've got an empty bucket!
					memcpy(destBucketAddress, sourceBucketAddress, elementSize);
				}

				secondaryMemoryFunctionCurrentIteration++;
			}

			// Discard old stuff
			secondaryMemoryCurrentFunction = SECONDARY_MEMORY_FUNCTION_NONE;
			delugeDealloc(secondaryMemory);
			secondaryMemory = nullptr;
			secondaryMemoryNumBuckets = 0;
		}
	}

	// If still can't get new memory and table completely full...
	if (numElements == numBuckets) {
		return nullptr;
	}

	int32_t b = getBucketIndex(key);
	void* bucketAddress;
	while (true) {
		bucketAddress = getBucketAddress(b);
		uint32_t destKey = getKeyFromAddress(bucketAddress);
		if (onlyIfNotAlreadyPresent && destKey == key) {
			*onlyIfNotAlreadyPresent = true;
			goto justReturnAddress;
		}
		if (doesKeyIndicateEmptyBucket(destKey)) {
			break;
		}
		b = (b + 1) & (numBuckets - 1);
	}

	// Ok, we've got an empty bucket!
	setKeyAtAddress(key, bucketAddress);

	numElements++;

justReturnAddress:
	return bucketAddress;
}

void* OpenAddressingHashTable::lookup(uint32_t key) {

#if ALPHA_OR_BETA_VERSION
	if (doesKeyIndicateEmptyBucket(key)) {
		FREEZE_WITH_ERROR("E331");
	}
#endif

	if (!memory) {
		return nullptr;
	}

	int32_t bInitial = getBucketIndex(key);
	int32_t b = bInitial;
	while (true) {
		void* bucketAddress = getBucketAddress(b);

		uint32_t keyHere = getKeyFromAddress(bucketAddress);

		// If reached an empty bucket, there's nothing there
		if (doesKeyIndicateEmptyBucket(keyHere)) {
			break;
		}

		// Bucket's not empty. Does it hold our key?
		if (keyHere == key) {
			return bucketAddress;
		}

		b = (b + 1) & (numBuckets - 1);

		// If we've wrapped all the way around (which could only happen if table 100% full)
		if (b == bInitial) {
			break;
		}
	}

	return nullptr;
}

// Returns whether it found the element
bool OpenAddressingHashTable::remove(uint32_t key) {

#if ALPHA_OR_BETA_VERSION
	if (doesKeyIndicateEmptyBucket(key)) {
		FREEZE_WITH_ERROR("E332");
	}
#endif

	if (!memory) {
		return false;
	}

	int32_t bInitial = getBucketIndex(key);
	int32_t b = bInitial;
	void* bucketAddress;
	while (true) {
		bucketAddress = getBucketAddress(b);

		uint32_t keyHere = getKeyFromAddress(bucketAddress);

		// If reached an empty bucket, can't find our element
		if (doesKeyIndicateEmptyBucket(keyHere)) {
			return false;
		}

		// Bucket's not empty. Does it hold our key?
		if (keyHere == key) {
			break;
		}

		b = (b + 1) & (numBuckets - 1);

		// If we've wrapped all the way around (which could only happen if table 100% full)
		if (b == bInitial) {
			return false;
		}
	}

	// We found the bucket with our element.
	numElements--;

	// If we've hit zero elements, and it's worth getting rid of the memory, just do that
	if (!numElements && numBuckets > initialNumBuckets) {
		delugeDealloc(memory);
		memory = nullptr;
		numBuckets = 0;
	}

	else {
		int32_t lastBucketIndexLeftEmpty = b;
		bInitial = b;

		while (true) {
			b = (b + 1) & (numBuckets - 1);

			// If we've wrapped all the way around (which could only happen if table 100% full)
			if (b == bInitial) {
				break;
			}

			void* newBucketAddress = getBucketAddress(b);

			uint32_t keyHere = getKeyFromAddress(newBucketAddress);

			// If reached an empty bucket, we're done
			if (doesKeyIndicateEmptyBucket(keyHere)) {
				break;
			}

			// Bucket contains an element. What bucket did this element ideally want to be in?
			int32_t idealBucket = getBucketIndex(keyHere);
			if (idealBucket != b) {
				bool shouldMove;
				if (lastBucketIndexLeftEmpty < b) {
					shouldMove = (idealBucket <= lastBucketIndexLeftEmpty) || (idealBucket > b);
				}
				else {
					shouldMove = (idealBucket <= lastBucketIndexLeftEmpty) && (idealBucket > b);
				}

				if (shouldMove) {
					memcpy(bucketAddress, newBucketAddress, elementSize);
					lastBucketIndexLeftEmpty = b;
					bucketAddress = newBucketAddress;
				}
			}
		}

		// Mark the last one as empty
		setKeyAtAddress(0xFFFFFFFF, bucketAddress);
	}
	return true;
}

// 32-bit key
OpenAddressingHashTableWith32bitKey::OpenAddressingHashTableWith32bitKey() {
	elementSize = sizeof(uint32_t);
}

uint32_t OpenAddressingHashTableWith32bitKey::getKeyFromAddress(void* address) {
	return *(uint32_t*)address;
}

void OpenAddressingHashTableWith32bitKey::setKeyAtAddress(uint32_t key, void* address) {
	*(uint32_t*)address = key;
}

bool OpenAddressingHashTableWith32bitKey::doesKeyIndicateEmptyBucket(uint32_t key) {
	return (key == (uint32_t)0xFFFFFFFF);
}

// 16-bit key
OpenAddressingHashTableWith16bitKey::OpenAddressingHashTableWith16bitKey() {
	elementSize = sizeof(uint16_t);
}

uint32_t OpenAddressingHashTableWith16bitKey::getKeyFromAddress(void* address) {
	return *(uint16_t*)address;
}

void OpenAddressingHashTableWith16bitKey::setKeyAtAddress(uint32_t key, void* address) {
	*(uint16_t*)address = key;
}

bool OpenAddressingHashTableWith16bitKey::doesKeyIndicateEmptyBucket(uint32_t key) {
	return (key == (uint32_t)0xFFFF);
}

// 8-bit key
OpenAddressingHashTableWith8bitKey::OpenAddressingHashTableWith8bitKey() {
	elementSize = sizeof(uint8_t);
}

uint32_t OpenAddressingHashTableWith8bitKey::getKeyFromAddress(void* address) {
	return *(uint8_t*)address;
}

void OpenAddressingHashTableWith8bitKey::setKeyAtAddress(uint32_t key, void* address) {
	*(uint8_t*)address = key;
}

bool OpenAddressingHashTableWith8bitKey::doesKeyIndicateEmptyBucket(uint32_t key) {
	return (key == (uint32_t)0xFF);
}
