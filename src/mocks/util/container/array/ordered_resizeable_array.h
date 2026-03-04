// Shadow header for ordered_resizeable_array.h — fixes uint32_t pointer
// casts that truncate on x86-64.  Only the inline getKeyAtMemoryLocation /
// setKeyAtMemoryLocation helpers are changed (uint32_t → uintptr_t).

/*
 * Copyright © 2017-2023 Synthstrom Audible Limited
 *
 * This file is part of The Synthstrom Audible Deluge Firmware.
 *
 * The Synthstrom Audible Deluge Firmware is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program.
 * If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "util/container/array/resizeable_array.h"
#include <cstdint>

class OrderedResizeableArray : public ResizeableArray {
public:
	OrderedResizeableArray(int32_t newElementSize, int32_t keyNumBits, int32_t newKeyOffset = 0,
	                       int32_t newMaxNumEmptySpacesToKeep = 16, int32_t newNumExtraSpacesToAllocate = 15);
	int32_t search(int32_t key, int32_t comparison, int32_t rangeBegin, int32_t rangeEnd);
	inline int32_t search(int32_t key, int32_t comparison, int32_t rangeBegin = 0) {
		return search(key, comparison, rangeBegin, numElements);
	}

	int32_t searchExact(int32_t key);
	int32_t insertAtKey(int32_t key, bool isDefinitelyLast = false);
	void deleteAtKey(int32_t key);

#if TEST_VECTOR
	void test();
#endif
#if TEST_VECTOR_DUPLICATES
	void testDuplicates();
#endif
	void testSequentiality(char const* errorCode);

	inline int32_t getKeyAtIndex(int32_t i) { return getKeyAtMemoryLocation(getElementAddress(i)); }

	inline void setKeyAtIndex(int32_t key, int32_t i) { setKeyAtMemoryLocation(key, getElementAddress(i)); }

protected:
	// Fixed for 64-bit: use uintptr_t instead of uint32_t for pointer arithmetic
	inline int32_t getKeyAtMemoryLocation(void* address) {
		int32_t keyBig = *(uint32_t*)((uintptr_t)address + keyOffset) << keyShiftAmount;
		return keyBig >> keyShiftAmount;
	}

	inline void setKeyAtMemoryLocation(int32_t key, void* address) {
		uintptr_t offsetAddress = (uintptr_t)address + keyOffset;
		uint32_t prevContents = *(uint32_t*)offsetAddress;
		*(uint32_t*)offsetAddress = (key & keyMask) | (prevContents & ~keyMask);
	}

private:
	const uint32_t keyMask;
	const int32_t keyOffset;
	const int32_t keyShiftAmount;
};

class OrderedResizeableArrayWith32bitKey : public OrderedResizeableArray {
public:
	explicit OrderedResizeableArrayWith32bitKey(int32_t newElementSize, int32_t newMaxNumEmptySpacesToKeep = 16,
	                                            int32_t newNumExtraSpacesToAllocate = 15);
	void shiftHorizontal(int32_t amount, int32_t effectiveLength);
	void searchDual(int32_t const* __restrict__ searchTerms, int32_t* __restrict__ resultingIndexes);
	void searchMultiple(int32_t* __restrict__ searchTerms, int32_t numSearchTerms, int32_t rangeEnd = -1);
	bool generateRepeats(int32_t wrapPoint, int32_t endPos);
#if TEST_VECTOR_SEARCH_MULTIPLE
	void testSearchMultiple();
#endif

	inline int32_t getKeyAtIndex(int32_t i) { return getKeyAtMemoryLocation(getElementAddress(i)); }

	inline void setKeyAtIndex(int32_t key, int32_t i) { setKeyAtMemoryLocation(key, getElementAddress(i)); }

protected:
	inline int32_t getKeyAtMemoryLocation(void* address) { return *(int32_t*)address; }

	inline void setKeyAtMemoryLocation(int32_t key, void* address) { *(int32_t*)address = key; }
};
