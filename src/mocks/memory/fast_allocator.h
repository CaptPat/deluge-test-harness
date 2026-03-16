// Shadow header: replace GMA-backed fast_allocator with std::allocator.
//
// On MinGW, GeneralMemoryAllocator::get() can create duplicate static locals
// across .a library boundaries, causing memory corruption when fast_vector
// (used by Sound::voices_) allocates through one GMA instance and deallocates
// through another. Using std::allocator bypasses GMA entirely for STL
// containers, eliminating this class of bugs.

#pragma once
#include <memory>

namespace deluge::memory {

template <typename T>
using fast_allocator = std::allocator<T>;

} // namespace deluge::memory
