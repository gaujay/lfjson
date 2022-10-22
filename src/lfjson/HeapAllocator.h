/**
 * Copyright 2022 Guillaume AUJAY. All rights reserved.
 *
 */

#ifndef LFJSON_HEAPALLOCATOR_H
#define LFJSON_HEAPALLOCATOR_H

#include <cstddef>
#include <cstdint>
#include <memory>

#define LFJ_HEAPALLOCATOR_INSTRUMENTED  // comment for simple pass-through
#ifdef LFJ_HEAPALLOCATOR_INSTRUMENTED
  #define LFJ_HEAPALLOCATOR_STATIC
#else
  #define LFJ_HEAPALLOCATOR_STATIC static
#endif

namespace lfjson
{
// Heap allocator, using std::allocator<char>
class HeapAllocator
{
public:
  using value_type = char;
  
  LFJ_HEAPALLOCATOR_STATIC char* allocate(std::size_t size)
  {
    char* mem = mAllocator.allocate(size);
  #ifdef LFJ_HEAPALLOCATOR_INSTRUMENTED
    mAllocated += size;
    mAllocPeak = mAllocPeak < mAllocated ? mAllocated : mAllocPeak;
    ++mAllocCount;
  #endif
    return mem;
  }
  
  LFJ_HEAPALLOCATOR_STATIC void deallocate(char* ptr, std::size_t size)
  {
    mAllocator.deallocate(ptr, size);
  #ifdef LFJ_HEAPALLOCATOR_INSTRUMENTED
    mAllocated -= size;
  #endif
  }
  
#ifdef LFJ_HEAPALLOCATOR_INSTRUMENTED
  uint64_t getAllocated()  const { return mAllocated; }
  uint64_t getAllocPeak()  const { return mAllocPeak; }
  uint64_t getAllocCount() const { return mAllocCount; }
#endif
  
private:
  LFJ_HEAPALLOCATOR_STATIC std::allocator<value_type> mAllocator;
  
#ifdef LFJ_HEAPALLOCATOR_INSTRUMENTED
  uint64_t mAllocated  = 0u;
  uint64_t mAllocPeak  = 0u;
  uint64_t mAllocCount = 0u;
#endif
};

#ifndef LFJ_HEAPALLOCATOR_INSTRUMENTED
  std::allocator<HeapAllocator::value_type> HeapAllocator::mAllocator;
#endif

} // namespace lfjson

#endif // LFJSON_HEAPALLOCATOR_H
