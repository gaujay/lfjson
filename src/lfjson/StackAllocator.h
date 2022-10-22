/**
 * Copyright 2022 Guillaume AUJAY. All rights reserved.
 *
 */

#ifndef LFJSON_STACKALLOCATOR_H
#define LFJSON_STACKALLOCATOR_H

#include <cstddef>
#include <cstdint>
#include <cassert>
#include <new>

//#define LFJ_STACKALLOCATOR_DEBUG  // uncomment for assert on alloc fail

namespace lfjson
{
//
// General purpose stack allocator, with dead cells management
template <int32_t Capacity,
          int32_t Alignment = (int32_t)alignof(std::max_align_t)>
class StackAllocator
{
  static_assert(Capacity  >= 8, "[lfjson] StackAllocator: Capacity must be >= 8");
  static_assert(Alignment >  0, "[lfjson] StackAllocator: Alignment must be > 0");
  static_assert((Alignment & (Alignment - 1)) == 0, "[lfjson] StackAllocator: Alignment must be power of 2");
  
  struct DeadCell {
    int32_t size;
    int32_t next;
  };
  static constexpr int32_t DeadCellSize = (int32_t)sizeof(DeadCell);
  
  static int32_t alignSize(int32_t size)
  {
    int32_t floor = (size / Alignment) * Alignment;
    return size == floor ? floor : floor + Alignment;
  }

private:
  int32_t mFirstAvail =  0;
  int32_t mFirstDead  = -1;
  int32_t mTotalDead  =  0;
  int32_t _padding    =  0;
  alignas(Alignment) char mBuffer[Capacity];

public:
  using value_type = char;
  
  StackAllocator() = default;
  StackAllocator(const StackAllocator&) = delete;
  StackAllocator& operator=(const StackAllocator&) = delete;
  
  char* allocate(int32_t size)
  {
    size = alignSize(size);
    assert(size >= DeadCellSize);
    
    // Try alloc at end
    if (mFirstAvail + size <= Capacity)
    {
      int32_t avail = mFirstAvail;
      mFirstAvail += size;
      return mBuffer + avail;
    }
    
    // Try alloc from dead cells
    if (mTotalDead >= size)
    {
      int32_t curDead  = mFirstDead;
      int32_t prevDead = -1;
      while (curDead >= 0)
      {
        DeadCell* deadCell = (DeadCell*)(&mBuffer[curDead]);  // break strict aliasing rule
        // Exact size
        if (deadCell->size == size)
        {
          // Update prev
          if (prevDead < 0)
            mFirstDead = -1;
          else
          {
            DeadCell* prevDeadCell = (DeadCell*)(&mBuffer[prevDead]);
            prevDeadCell->next = deadCell->next;
          }
          
          mTotalDead -= size;
          return mBuffer + curDead;
        }
        // Large enough
        if (deadCell->size >= size + DeadCellSize)
        {
          // Update size and next
          DeadCell* newDeadCell = (DeadCell*)(&mBuffer[curDead + size]);
          newDeadCell->size = deadCell->size - size;
          newDeadCell->next = deadCell->next;
          
          // Update prev
          if (prevDead < 0)
            mFirstDead = curDead + size;
          else
          {
            DeadCell* prevDeadCell = (DeadCell*)(&mBuffer[prevDead]);
            prevDeadCell->next = curDead + size;
          }
          
          mTotalDead -= size;
          return mBuffer + curDead;
        }
        // Iterate
        prevDead = curDead;
        curDead = deadCell->next;
      }
    }
    
    // Failed to alloc
  #ifdef LFJ_STACKALLOCATOR_DEBUG
    assert(false && "[lfjson] StackAllocator: not enough space left to allocate");
  #endif
    throw std::bad_alloc();
  }
  
  void deallocate(char* ptr, int32_t size)
  {
    if (ptr == nullptr)
      return;
      
    size = alignSize(size);
    assert(size >= DeadCellSize);
    
    int32_t pos = (int32_t)((char*)ptr - mBuffer);
    if (pos + size == mFirstAvail)  // restore to avail
    {
      if (pos == mTotalDead)  // empty
      {
        mFirstAvail = 0;
        mFirstDead  = -1;
        mTotalDead  = 0;
      }
      else
        mFirstAvail = pos;
    }
    else  // add to dead
    {
      DeadCell* ptr_ = (DeadCell*)ptr;
      ptr_->size = size;
      ptr_->next = mFirstDead;
      
      mFirstDead = pos;
      mTotalDead += size;
    }
  }
  
  uint32_t used()       const { return (uint32_t)(mFirstAvail - mTotalDead); }
  uint32_t available()  const { return (uint32_t)(Capacity - used()); }
    
  int32_t first_avail() const { return mFirstAvail; }
  int32_t first_dead()  const { return mFirstDead; }
  int32_t total_dead()  const { return mTotalDead; }
};

} // namespace lfjson

#endif // LFJSON_STACKALLOCATOR_H
