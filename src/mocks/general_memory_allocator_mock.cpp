// Mock memory allocator for x86 test harness.
// Routes GeneralMemoryAllocator methods to malloc/free.
// Tracks allocation sizes so getAllocatedSize() returns correct values.
// Adapted from firmware/tests/32bit_unit_tests/mock_memory_manager.cpp

#include "memory/general_memory_allocator.h"
#include "util/container/list/bidirectional_linked_list.h"
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <unordered_map>

// ── Allocation size tracking ─────────────────────────────────────────────
// ResizeableArray relies on getAllocatedSize() to know how much room it has.
// With malloc we must track this ourselves.

static std::unordered_map<void*, size_t>& getAllocMap() {
	static std::unordered_map<void*, size_t> map;
	return map;
}

static void trackAlloc(void* ptr, size_t size) {
	if (ptr) {
		getAllocMap()[ptr] = size;
	}
}

static void untrackAlloc(void* ptr) {
	if (ptr) {
		getAllocMap().erase(ptr);
	}
}

static size_t getTrackedSize(void* ptr) {
	auto& map = getAllocMap();
	auto it = map.find(ptr);
	if (it != map.end()) {
		return it->second;
	}
	// Fallback: return the requested amount as unknown (should not happen)
	return 0;
}

// ── MemoryRegion / CacheManager stubs ───────────────────────────────────

MemoryRegion::MemoryRegion() = default;

// BidirectionalLinkedList — now compiled from real firmware source (Phase 9B).

// ── GeneralMemoryAllocator mock ─────────────────────────────────────────

GeneralMemoryAllocator::GeneralMemoryAllocator() : lock(false) {}

GeneralMemoryAllocator& GeneralMemoryAllocator::get() {
	static GeneralMemoryAllocator generalMemoryAllocator;
	return generalMemoryAllocator;
}

void* GeneralMemoryAllocator::alloc(uint32_t requiredSize, bool mayUseOnChipRam, bool makeStealable,
                                    void* thingNotToStealFrom) {
	void* ptr = malloc(requiredSize);
	trackAlloc(ptr, requiredSize);
	return ptr;
}

void GeneralMemoryAllocator::dealloc(void* address) {
	untrackAlloc(address);
	free(address);
}

void* GeneralMemoryAllocator::allocExternal(uint32_t requiredSize) {
	void* ptr = malloc(requiredSize);
	trackAlloc(ptr, requiredSize);
	return ptr;
}

void* GeneralMemoryAllocator::allocInternal(uint32_t requiredSize) {
	void* ptr = malloc(requiredSize);
	trackAlloc(ptr, requiredSize);
	return ptr;
}

void GeneralMemoryAllocator::deallocExternal(void* address) {
	untrackAlloc(address);
	free(address);
}

uint32_t GeneralMemoryAllocator::shortenRight(void* address, uint32_t newSize) {
	// No-op in mock — can't resize malloc'd blocks
	return 0;
}

uint32_t GeneralMemoryAllocator::shortenLeft(void* address, uint32_t amountToShorten,
                                             uint32_t numBytesToMoveRightIfSuccessful) {
	// Not supported in mock
	return 0;
}

void GeneralMemoryAllocator::extend(void* address, uint32_t minAmountToExtend, uint32_t idealAmountToExtend,
                                    uint32_t* getAmountExtendedLeft, uint32_t* getAmountExtendedRight,
                                    void* thingNotToStealFrom) {
	// Can't extend malloc'd blocks
	*getAmountExtendedLeft = 0;
	*getAmountExtendedRight = 0;
}

uint32_t GeneralMemoryAllocator::extendRightAsMuchAsEasilyPossible(void* address) {
	return 0;
}

void GeneralMemoryAllocator::test() {}

uint32_t GeneralMemoryAllocator::getAllocatedSize(void* address) {
	size_t size = getTrackedSize(address);
	return static_cast<uint32_t>(size);
}

void GeneralMemoryAllocator::checkStack(char const* caller) {}

void GeneralMemoryAllocator::testShorten(int32_t i) {}

int32_t GeneralMemoryAllocator::getRegion(void* address) {
	return 0;
}

void GeneralMemoryAllocator::testMemoryDeallocated(void* address) {}

void GeneralMemoryAllocator::putStealableInQueue(Stealable* stealable, StealableQueue q) {}

void GeneralMemoryAllocator::putStealableInAppropriateQueue(Stealable* stealable) {}

// ── CacheManager stub ───────────────────────────────────────────────────

uint32_t CacheManager::ReclaimMemory(MemoryRegion& region, int32_t totalSizeNeeded, void* thingNotToStealFrom,
                                     int32_t* __restrict__ foundSpaceSize) {
	return 0;
}

// ── C wrappers ──────────────────────────────────────────────────────────

// Free-function wrappers (from memory_allocator_interface.cpp, which we exclude
// from the build because its relative #include bypasses our shadow header).
void* allocMaxSpeed(uint32_t requiredSize, void* thingNotToStealFrom) {
	return GeneralMemoryAllocator::get().alloc(requiredSize, true, false, thingNotToStealFrom);
}

void* allocLowSpeed(uint32_t requiredSize, void* thingNotToStealFrom) {
	return GeneralMemoryAllocator::get().alloc(requiredSize, false, false, thingNotToStealFrom);
}

void* allocStealable(uint32_t requiredSize, void* thingNotToStealFrom) {
	return GeneralMemoryAllocator::get().alloc(requiredSize, false, true, thingNotToStealFrom);
}

extern "C" {

void* delugeAlloc(unsigned int requiredSize, bool mayUseOnChipRam) {
	return GeneralMemoryAllocator::get().alloc(requiredSize, mayUseOnChipRam, false, nullptr);
}

void delugeDealloc(void* address) {
	GeneralMemoryAllocator::get().dealloc(address);
}

} // extern "C"
