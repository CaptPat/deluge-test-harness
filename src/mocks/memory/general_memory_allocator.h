// Shadow header for memory/general_memory_allocator.h
// Makes get() non-inline to ensure a single static instance across all TUs.
// Without this, MinGW creates duplicate static locals in each static library,
// causing heap corruption when one is destroyed before the other.
#pragma once

#include "definitions_cxx.hpp"
#include "memory/memory_region.h"

#define MEMORY_REGION_STEALABLE 0
#define MEMORY_REGION_INTERNAL 1
#define MEMORY_REGION_EXTERNAL 2
#define MEMORY_REGION_EXTERNAL_SMALL 3
#define MEMORY_REGION_INTERNAL_SMALL 4
#define NUM_MEMORY_REGIONS 5
constexpr uint32_t RESERVED_EXTERNAL_ALLOCATOR = 0x00200000;
constexpr uint32_t RESERVED_EXTERNAL_SMALL_ALLOCATOR = 0x00020000;
constexpr uint32_t RESERVED_INTERNAL_SMALL = 0x00010000;
class Stealable;

class GeneralMemoryAllocator {
public:
	GeneralMemoryAllocator();
	[[gnu::always_inline]] void* allocMaxSpeed(uint32_t requiredSize, void* thingNotToStealFrom = nullptr) {
		return alloc(requiredSize, true, false, thingNotToStealFrom);
	}
	[[gnu::always_inline]] void* allocLowSpeed(uint32_t requiredSize, void* thingNotToStealFrom = nullptr) {
		return alloc(requiredSize, false, false, thingNotToStealFrom);
	}
	[[gnu::always_inline]] void* allocStealable(uint32_t requiredSize, void* thingNotToStealFrom = nullptr) {
		return alloc(requiredSize, false, true, thingNotToStealFrom);
	}

	void* alloc(uint32_t requiredSize, bool mayUseOnChipRam, bool makeStealable, void* thingNotToStealFrom);
	void dealloc(void* address);
	void* allocExternal(uint32_t requiredSize);
	void* allocInternal(uint32_t requiredSize);
	void deallocExternal(void* address);
	uint32_t shortenRight(void* address, uint32_t newSize);
	uint32_t shortenLeft(void* address, uint32_t amountToShorten, uint32_t numBytesToMoveRightIfSuccessful = 0);
	void extend(void* address, uint32_t minAmountToExtend, uint32_t idealAmountToExtend,
	            uint32_t* getAmountExtendedLeft, uint32_t* getAmountExtendedRight, void* thingNotToStealFrom = nullptr);
	uint32_t extendRightAsMuchAsEasilyPossible(void* address);
	void test();
	uint32_t getAllocatedSize(void* address);
	void checkStack(char const* caller);
	void testShorten(int32_t i);
	int32_t getRegion(void* address);
	void testMemoryDeallocated(void* address);
	void putStealableInQueue(Stealable* stealable, StealableQueue q);
	void putStealableInAppropriateQueue(Stealable* stealable);

	MemoryRegion regions[NUM_MEMORY_REGIONS];
	CacheManager cacheManager;
	bool lock;

	// Out-of-line — defined in general_memory_allocator_mock.cpp
	static GeneralMemoryAllocator& get();

private:
	void checkEverythingOk(char const* errorString);
};

extern "C" {
void* delugeAlloc(unsigned int requiredSize, bool mayUseOnChipRam = true);
void delugeDealloc(void* address);
}
