/**
 * Copyright 2022 Guillaume AUJAY. All rights reserved.
 *
 */

#ifndef LFJSON_POOLALLOCATOR_H
#define LFJSON_POOLALLOCATOR_H

#include "BaseData.h"

#include <cstdint>
#include <cmath>
#include <cstring>
#include <cassert>
#include <limits>
#include <memory>
#include <type_traits>

//#define LFJ_POOLALLOCATOR_PACK  // uncomment for packing dead cells (slower)
#ifdef LFJ_POOLALLOCATOR_SANITY
  #define LFJ_POOLALLOCATOR_SANITY_CHECK  { sanityCheck(); }
#else
  #define LFJ_POOLALLOCATOR_SANITY_CHECK
#endif
#ifdef LFJ_POOLALLOCATOR_DEBUG
  #include <vector>
  #include <utility>
  #include <algorithm>
#endif

namespace lfjson
{
//
// Slab allocator, with dead-cells management
// When using PoolPtr for StringPool (on 64-bits), enforces an alternate allocation scheme
template <uint16_t ChunkSize, class Allocator, bool ownAllocator, bool altScheme>
class PoolAllocator
{
  struct DeadCell { // 4 Bytes
    uint16_t size;
    uint16_t next;  // equals ChunkSize when none
    
    // Explicit assignments, avoid breaking strict aliasing rule
    static void setSize(unsigned char* ptr, uint16_t size)
    {
      ptr[0] = size & 0xFF;
      ptr[1] = size >> 8;
    }
    
    static void setNext(unsigned char* ptr, uint16_t next)
    {
      ptr[2] = next & 0xFF;
      ptr[3] = next >> 8;
    }
    
    static void set(unsigned char* ptr, uint16_t size, uint16_t next)
    {
      setSize(ptr, size);
      setNext(ptr, next);
    }
    
    static uint16_t getSize(const unsigned char* ptr)
    {
      return (uint16_t)ptr[0] | ((uint16_t)ptr[1] << 8);
    }
    
    static uint16_t getNext(const unsigned char* ptr)
    {
      return (uint16_t)ptr[2] | ((uint16_t)ptr[3] << 8);
    }
  };
  
  struct Chunk {  // 12/16 Bytes
    Chunk(void* ptr) : data((unsigned char*)ptr) { assert(ptr != nullptr); }
    uint16_t avail() const { return ChunkSize - firstAvail; }
    
    uint16_t firstAvail = 0;
    uint16_t firstDead  = ChunkSize;
    uint16_t totalDead  = 0;
    unsigned char* data = nullptr;
  };
  
  struct Fallback {
    Fallback(Fallback* next_, uint32_t size_)
      : next(next_)
      , size(size_)
    {}
    
    Fallback* next;
    uint32_t size;
    unsigned char data[1];  // array
  };
  
  static constexpr float ChunkVectorGrowthFactor = 1.5f;
  static constexpr uint32_t DeadCellSize = (uint32_t)sizeof(DeadCell);
  
  static_assert(ChunkSize == 0u || ChunkSize >= DeadCellSize, "[lfjson] PoolAllocator: ChunkSize must be 0 or >= DeadCellSize");
  static_assert(ChunkSize == 0u || ChunkSize >= sizeof(JBigObject), "[lfjson] PoolAllocator: ChunkSize must be 0 or >= sizeof(JBigObject)");
  static_assert(ChunkSize == 0u || ChunkSize >= sizeof(JString), "[lfjson] PoolAllocator: ChunkSize must be 0 or >= sizeof(JString)");
  static_assert(std::is_same<typename Allocator::value_type, char>::value, "[lfjson] PoolAllocator: Allocator::value_type must be 'char'");
  static_assert(alignof(JBigObject) >= DeadCellSize, "[lfjson] PoolAllocator: minimum aligned size must be >= DeadCellSize");

private:
  // Members
  uint32_t mLastChunk       = 0;
  uint32_t mTotalDead       = 0;
  uint32_t mChunksCount     = 0;
  uint32_t mChunksCapacity  = 0;
  Chunk* mChunks            = nullptr;  // vector (ordered if nominal scheme)
  Fallback* mFallbacks      = nullptr;  // forward list
#ifdef LFJ_64BIT
  uint32_t mFallbackCount   = 0;
#endif
  
  typedef typename std::conditional<ownAllocator, Allocator, Allocator&>::type BaseAllocator;
  BaseAllocator mAllocator;
  
public:
  PoolAllocator() = default;  // for owned allocator
  PoolAllocator(Allocator& allocator) : mAllocator(allocator) {}  // for borrowed allocator
  PoolAllocator(const PoolAllocator&) = delete;
  PoolAllocator& operator=(const PoolAllocator&) = delete;
  
  ~PoolAllocator()
  {
    releaseAll();
  }
  
  // Accessors
  uint32_t chunksCount() const { return mChunksCount; }
  
  uint32_t chunksCapacity() const { return mChunksCapacity; }
  
  uint32_t countFallbacks() const
  {
    uint32_t count = 0u;
    for (Fallback* it = mFallbacks; it != nullptr; it = it->next)
    {
      assert(count < std::numeric_limits<uint32_t>::max());
      ++count;
    }
    return count;
  }
  
  uint64_t countAllocated() const
  {
    uint64_t count = 0u;
    for (uint32_t i = 0u; i < mChunksCount; ++i)
      count += mChunks[i].firstAvail - mChunks[i].totalDead;
    for (Fallback* it = mFallbacks; it != nullptr; it = it->next)
      count += it->size;
    return count;
  }
  
  uint64_t countDirectAvailable() const
  {
    uint64_t count = 0u;
    for (uint32_t i = 0u; i < mChunksCount; ++i)
      count += mChunks[i].avail();
    return count;
  }
  
  uint64_t countDeadCells() const
  {
    uint64_t count = 0u;
    for (uint32_t i = 0u; i < mChunksCount; ++i)
    {
      const auto& chunk = mChunks[i];
      uint16_t next = chunk.firstDead;
      while (next != ChunkSize)
      {
        unsigned char* dc = &chunk.data[next];
        uint16_t dcNext = DeadCell::getNext(dc);
        next = dcNext;
        ++count;
      }
    }
    return count;
  }
  
#ifdef LFJ_POOLALLOCATOR_DEBUG
  std::vector<std::pair<uint32_t, uint32_t>> getDeadCells() const
  {
    std::vector<std::pair<uint32_t, uint32_t>> res;
    for (uint32_t i = 0u; i < mChunksCount; ++i)
    {
      std::vector<std::pair<uint32_t, uint32_t>> deadCells;
      std::pair<uint32_t, uint32_t> deads(0,0);
      const auto& chunk = mChunks[i];
      uint16_t next = chunk.firstDead;
      while (next != ChunkSize)
      {
        unsigned char* dc = &chunk.data[next];
        uint16_t dcSize = DeadCell::getSize(dc);
        uint16_t dcNext = DeadCell::getNext(dc);
        deadCells.push_back({next, (uint32_t)next + dcSize});
        next = dcNext;
      }
      std::sort(deadCells.begin(), deadCells.end(), []
                (const std::pair<uint32_t, uint32_t>& a, const std::pair<uint32_t, uint32_t>& b)
                { return a.first < b.first; });
      if (deadCells.size() > 1)
      {
        for (auto it = deadCells.begin(); it < deadCells.end() - 1; ++it)
        {
          if (it->second == (it + 1)->first)
            ++deads.first;
          ++deads.second;
        }
      }
      res.push_back(deads);
    }
    return res;
  }
  
  std::pair<uint32_t, uint32_t> getDeadCells(Chunk* chunk) const
  {
    std::vector<std::pair<uint32_t, uint32_t>> deadCells;
    std::pair<uint32_t, uint32_t> deads(0,0);
    uint16_t next = chunk->firstDead;
    while (next != ChunkSize)
    {
      unsigned char* dc = &chunk->data[next];
      uint16_t dcSize = DeadCell::getSize(dc);
      uint16_t dcNext = DeadCell::getNext(dc);
      deadCells.push_back({next, (uint32_t)next + dcSize});
      next = dcNext;
    }
    std::sort(deadCells.begin(), deadCells.end(), []
              (const std::pair<uint32_t, uint32_t>& a, const std::pair<uint32_t, uint32_t>& b)
              { return a.first < b.first; });
    if (deadCells.size() > 1)
    {
      for (auto it = deadCells.begin(); it < deadCells.end() - 1; ++it)
      {
        if (it->second == (it + 1)->first)
          ++deads.first;
        ++deads.second;
      }
    }
    return deads;
  }
#endif  // LFJ_POOLALLOCATOR_DEBUG
  
  uint32_t totalDead() const { return mTotalDead; }
  
  // Allocator
  Allocator& allocator() { return mAllocator; }
  const Allocator& callocator() const { return mAllocator; }
  
  void* allocate(uint32_t size)
  {
  #ifdef LFJ_64BIT
    assert(!altScheme);
  #endif
    uint32_t alignedSize = alignSize(size);
    assert(alignedSize >= DeadCellSize);
    assert(alignedSize <= (uint32_t)std::numeric_limits<int32_t>::max());
    
    if (chunkable(alignedSize))
    {
      // Check empty
      if (mChunksCapacity == 0)
      {
        mChunks = (Chunk*)mAllocator.allocate(sizeof(Chunk));
        assert(mChunks != nullptr);
        mChunksCapacity = 1;
        
        new (&mChunks[0]) Chunk(mAllocator.allocate(ChunkSize));
        mChunksCount = 1;
        mLastChunk = 0;
      }
      
      // Check last chunk: available
      void* mem = nullptr;
      if (mChunks[mLastChunk].avail() >= (uint16_t)alignedSize)
      {
        mem = (void*)(mChunks[mLastChunk].data + mChunks[mLastChunk].firstAvail);
        mChunks[mLastChunk].firstAvail += (uint16_t)alignedSize;
        LFJ_POOLALLOCATOR_SANITY_CHECK
        return mem;
      }
      // Check last chunk: dead
      mem = allocateFromDead(&mChunks[mLastChunk], (uint16_t)alignedSize);
      if (mem)
        return mem;
      
      // Check others chunks: available
      for (uint32_t i = 0; i < mChunksCount; ++i)
      {
        if (i != mLastChunk && mChunks[i].avail() >= (uint16_t)alignedSize)
        {
          mLastChunk = i;
          mem = (void*)(mChunks[i].data + mChunks[i].firstAvail);
          mChunks[i].firstAvail += (uint16_t)alignedSize;
          LFJ_POOLALLOCATOR_SANITY_CHECK
          return mem;
        }
      }
      // Check others chunks: dead
      if (mTotalDead >= alignedSize)
      {
        for (uint32_t i = 0; i < mChunksCount; ++i)
        {
          if (i != mLastChunk)
          {
            mem = allocateFromDead(&mChunks[i], (uint16_t)alignedSize);
            if (mem)
            {
              // mLastChunk = i; // empirically worst
              return mem;
            }
          }
        }
      }
      
    #ifdef LFJ_POOLALLOCATOR_PACK
      // Pack dead
      packDead();
      
      // Check last then others chunks dead again
      mem = allocateFromDead(&mChunks[mLastChunk], (uint16_t)alignedSize);
      if (mem)
        return mem;
      
      for (uint32_t i = 0; i < mChunksCount; ++i)
      {
        if (i != mLastChunk)
        {
          mem = allocateFromDead(&mChunks[i], (uint16_t)alignedSize);
          if (mem)
          {
            // mLastChunk = i; // empirically worst
            return mem;
          }
        }
      }
    #endif
      
      // Create new chunk (when all else failed)
      if (mChunksCount >= mChunksCapacity) // Grow chunks vector if needed
      {
        assert(mChunksCapacity < std::numeric_limits<uint32_t>::max() / ChunkVectorGrowthFactor);
        uint32_t newCapacity = (uint32_t)std::ceil(mChunksCapacity * ChunkVectorGrowthFactor);
        
        Chunk* newChunks = (Chunk*)mAllocator.allocate(sizeof(Chunk) * newCapacity);
        assert(newChunks != nullptr);
        memcpy(newChunks, mChunks, mChunksCount * sizeof(Chunk));
        
        mAllocator.deallocate((char*)mChunks, mChunksCapacity * sizeof(Chunk));
        mChunks = newChunks;
        mChunksCapacity = newCapacity;
      }
      // Construct and sort by data address
      new (&mChunks[mChunksCount]) Chunk(mAllocator.allocate(ChunkSize));
      mLastChunk = sortNewChunk();
      ++mChunksCount;
      
      mem = (void*)(mChunks[mLastChunk].data);
      mChunks[mLastChunk].firstAvail += (uint16_t)alignedSize;
      LFJ_POOLALLOCATOR_SANITY_CHECK
      return mem;
    }
    
    // Fallback
    void* raw = mAllocator.allocate(sizeof(Fallback) - 1 + size);
    assert(raw != nullptr);
    Fallback* fallback = new (raw) Fallback(mFallbacks, size);
    mFallbacks = fallback;
    
    LFJ_POOLALLOCATOR_SANITY_CHECK
    return (void*)fallback->data;
  }
  
  void deallocate(void* ptr, uint32_t size)
  {
  #ifdef LFJ_64BIT
    assert(!altScheme);
  #endif
    if (ptr == nullptr)
      return;
    
    // Find if chunk
    uint32_t alignedSize = alignSize(size);
    bool isChunk = chunkable(alignedSize);
    if (isChunk)
    {
      uint32_t ptrIdx;
      isChunk = findChunk((unsigned char*)ptr, ptrIdx);
      
      assert(isChunk);
      Chunk* chunk = &mChunks[ptrIdx];
      assert(alignedSize >= DeadCellSize);
      
      uint32_t pos = (uint32_t)((unsigned char*)ptr - chunk->data);
      if (chunk->totalDead + alignedSize == chunk->firstAvail) // empty
      {
        mTotalDead -= chunk->totalDead;
        
        chunk->firstAvail = 0;
        chunk->firstDead  = ChunkSize;
        chunk->totalDead  = 0;
        // try finding another chunk not full
        if (mLastChunk == ptrIdx && mChunksCount > 1)
        {
          uint32_t prevIdx = (uint32_t)(((int32_t)(mLastChunk) - 1) % mChunksCount);
          if (mChunks[prevIdx].firstAvail < ChunkSize)
            mLastChunk = prevIdx;
          else
          {
            uint32_t nextIdx = (uint32_t)(((int32_t)(mLastChunk) + 1) % mChunksCount);
            if (mChunks[nextIdx].firstAvail < ChunkSize)
              mLastChunk = nextIdx;
          }
        }
      }
      else if (pos + alignedSize == chunk->firstAvail)  // restore to avail
      {
          chunk->firstAvail = (uint16_t)pos;
      }
      else  // add to dead
      {
        DeadCell::setSize((unsigned char*)ptr, (uint16_t)alignedSize);
        DeadCell::setNext((unsigned char*)ptr, chunk->firstDead);
        
        mTotalDead += alignedSize;
        
        chunk->firstDead = (uint16_t)pos;
        chunk->totalDead += (uint16_t)alignedSize;
      }
      LFJ_POOLALLOCATOR_SANITY_CHECK
    }
    else  // Fallback
    {
      // Find if fallback
      if (!mFallbacks)
      {
        assert(false && "[lfjson] PoolAllocator: pointer to deallocate doesn't belong");
        return;
      }
      
      Fallback* fallback = mFallbacks;
      if (fallback->data == (unsigned char*)ptr)
      {
        assert(fallback->size == size);
        mFallbacks = fallback->next;
        mAllocator.deallocate((char*)fallback, sizeof(Fallback) - 1 + fallback->size);
      }
      else
      {
        while (fallback->next)
        {
          if (fallback->next->data == (unsigned char*)ptr)
          {
            Fallback* found = fallback->next;
            assert(found->size == size);
            fallback->next = fallback->next->next;
            mAllocator.deallocate((char*)found, sizeof(Fallback) - 1 + found->size);
            return;
          }
          fallback = fallback->next;
        }
        // Not found
        assert(false && "[lfjson] PoolAllocator: pointer to deallocate doesn't belong");
      }
      LFJ_POOLALLOCATOR_SANITY_CHECK
    }
  }
  
#ifdef LFJ_64BIT
  // Alternative allocation scheme (keep chunk/fallback indexes stable)
  // /!\ Do not mix schemes (nominal for objects, alt for strings)
  PoolPtr allocateAlt(uint32_t size)
  {
    assert(altScheme);
    uint32_t alignedSize = alignSize(size);
    assert(alignedSize >= DeadCellSize);
    assert(alignedSize <= (uint32_t)std::numeric_limits<int32_t>::max());
    
    if (chunkable(alignedSize))
    {
      // Check empty
      if (mChunksCapacity == 0)
      {
        mChunks = (Chunk*)mAllocator.allocate(sizeof(Chunk));
        assert(mChunks != nullptr);
        mChunksCapacity = 1;
        
        new (&mChunks[0]) Chunk(mAllocator.allocate(ChunkSize));
        mChunksCount = 1;
        mLastChunk = 0;
      }
      
      // Check last chunk: available
      unsigned char* mem = nullptr;
      if (mChunks[mLastChunk].avail() >= (uint16_t)alignedSize)
      {
        uint16_t pos = mChunks[mLastChunk].firstAvail;
        mem = (unsigned char*)(mChunks[mLastChunk].data + pos);
        mChunks[mLastChunk].firstAvail += (uint16_t)alignedSize;
        LFJ_POOLALLOCATOR_SANITY_CHECK
        return {(uint16_t)mLastChunk, pos};
      }
      // Check last chunk: dead
      mem = (unsigned char*)allocateFromDead(&mChunks[mLastChunk], (uint16_t)alignedSize);
      if (mem)
      {
        uint16_t pos = (uint16_t)(mem - mChunks[mLastChunk].data);
        return {(uint16_t)mLastChunk, pos};
      }
      
      // Check others chunks: available
      for (uint32_t i = 0; i < mChunksCount; ++i)
      {
        if (i != mLastChunk && mChunks[i].avail() >= (uint16_t)alignedSize)
        {
          mLastChunk = i;
          uint16_t pos = mChunks[i].firstAvail;
          mem = (unsigned char*)(mChunks[i].data + pos);
          mChunks[i].firstAvail += (uint16_t)alignedSize;
          LFJ_POOLALLOCATOR_SANITY_CHECK
          return {(uint16_t)mLastChunk, pos};
        }
      }
      // Check others chunks: dead
      if (mTotalDead >= alignedSize)
      {
        for (uint32_t i = 0; i < mChunksCount; ++i)
        {
          if (i != mLastChunk)
          {
            mem = (unsigned char*)allocateFromDead(&mChunks[i], (uint16_t)alignedSize);
            if (mem)
            {
              // mLastChunk = i; // empirically worst
              assert(i < LFJ_MAX_UINT16 - 1u);
              uint16_t pos = (uint16_t)(mem - mChunks[i].data);
              return {(uint16_t)i, pos};
            }
          }
        }
      }
      
    #ifdef LFJ_POOLALLOCATOR_PACK
      // Pack dead
      packDead();
      
      // Check last then others chunks dead again
      mem = (unsigned char*)allocateFromDead(&mChunks[mLastChunk], (uint16_t)alignedSize);
      if (mem)
      {
        uint16_t pos = (uint16_t)(mem - mChunks[mLastChunk].data);
        return {(uint16_t)mLastChunk, pos};
      }
      
      for (uint32_t i = 0; i < mChunksCount; ++i)
      {
        if (i != mLastChunk)
        {
          mem = (unsigned char*)allocateFromDead(&mChunks[i], (uint16_t)alignedSize);
          if (mem)
          {
            // mLastChunk = i; // empirically worst
            assert(i < (uint32_t)(LFJ_MAX_UINT16 - 1));
            uint16_t pos = (uint16_t)(mem - mChunks[i].data);
            return {(uint16_t)i, pos};
          }
        }
      }
    #endif
      
      // Create new chunk (when all else failed)
      if (mChunksCount >= mChunksCapacity) // Grow chunks vector if needed
      {
        assert(mChunksCapacity < std::numeric_limits<uint32_t>::max() / ChunkVectorGrowthFactor);
        uint32_t newCapacity = (uint32_t)std::ceil(mChunksCapacity * ChunkVectorGrowthFactor);
        
        Chunk* newChunks = (Chunk*)mAllocator.allocate(sizeof(Chunk) * newCapacity);
        assert(newChunks != nullptr);
        memcpy(newChunks, mChunks, mChunksCount * sizeof(Chunk));
        
        mAllocator.deallocate((char*)mChunks, mChunksCapacity * sizeof(Chunk));
        mChunks = newChunks;
        mChunksCapacity = newCapacity;
      }
      // Construct
      new (&mChunks[mChunksCount]) Chunk(mAllocator.allocate(ChunkSize));
      mLastChunk = mChunksCount;
      ++mChunksCount;
      
      mem = (unsigned char*)(mChunks[mLastChunk].data);
      mChunks[mLastChunk].firstAvail += (uint16_t)alignedSize;
      LFJ_POOLALLOCATOR_SANITY_CHECK
      return {(uint16_t)mLastChunk, 0};
    }
    
    // Fallback
    void* raw = mAllocator.allocate(sizeof(Fallback) - 1 + size);
    assert(raw != nullptr);
    Fallback* fallback = new (raw) Fallback(mFallbacks, size);
    mFallbacks = fallback;
    ++mFallbackCount;
    
    LFJ_POOLALLOCATOR_SANITY_CHECK
    return {LFJ_MAX_UINT16 - 1, (uint16_t)(mFallbackCount - 1u)};
  }
  
  void deallocateAlt(PoolPtr sp, uint32_t size)
  {
    assert(altScheme);
    if (sp.chunk >= LFJ_MAX_UINT16)
      return;
      
    if (sp.chunk < mChunksCount)  // Chunk
    {
      uint32_t alignedSize = alignSize(size);
      assert(chunkable(alignedSize));
      assert(alignedSize >= DeadCellSize);
      
      Chunk* chunk = &mChunks[sp.chunk];
      if (chunk->totalDead + alignedSize == chunk->firstAvail) // empty
      {
        mTotalDead -= chunk->totalDead;
        
        chunk->firstAvail = 0;
        chunk->firstDead  = ChunkSize;
        chunk->totalDead  = 0;
        // Try finding another chunk not full
        if (mLastChunk == sp.chunk && mChunksCount > 1)
        {
          uint32_t prevIdx = (uint32_t)(((int32_t)(mLastChunk) - 1) % mChunksCount);
          if (mChunks[prevIdx].firstAvail < ChunkSize)
            mLastChunk = prevIdx;
          else
          {
            uint32_t nextIdx = (uint32_t)((mLastChunk + 1) % mChunksCount);
            if (mChunks[nextIdx].firstAvail < ChunkSize)
              mLastChunk = nextIdx;
          }
        }
      }
      else if (sp.pos + alignedSize == chunk->firstAvail)  // restore to avail
      {
          chunk->firstAvail = sp.pos;
      }
      else  // add to dead
      {
        unsigned char* ptr = (chunk->data + sp.pos);
        DeadCell::setSize(ptr, (uint16_t)alignedSize);
        DeadCell::setNext(ptr, chunk->firstDead);
        
        mTotalDead += alignedSize;
        
        chunk->firstDead = sp.pos;
        chunk->totalDead += (uint16_t)alignedSize;
      }
      LFJ_POOLALLOCATOR_SANITY_CHECK
    }
    else  // Fallback
    {
      assert(sp.pos < mFallbackCount);
      uint32_t pos = mFallbackCount - 1u - sp.pos;
      if (pos == 0)
      {
        assert(mFallbacks->size == size);
        void* raw = mAllocator.allocate(sizeof(Fallback));
        assert(raw != nullptr);
        Fallback* fallback = new (raw) Fallback(mFallbacks->next, 1);  // replace by empty
        mAllocator.deallocate((char*)mFallbacks, sizeof(Fallback) - 1 + mFallbacks->size);
        mFallbacks = fallback;
      }
      else
      {
        Fallback* it = mFallbacks;
        Fallback* prevIt = mFallbacks;
        for (uint16_t i = 0; i < pos; ++i)
        {
          assert(it->next != nullptr);
          prevIt = it;
          it = it->next;
        }
        // Replace by empty
        assert(it->size == size);
        void* raw = mAllocator.allocate(sizeof(Fallback));
        assert(raw != nullptr);
        Fallback* fallback = new (raw) Fallback(it->next, 1);  // replace by empty
        prevIt->next = fallback;
        mAllocator.deallocate((char*)it, sizeof(Fallback) - 1 + it->size);
      }
      LFJ_POOLALLOCATOR_SANITY_CHECK
    }
  }
#else
  // Redirect to nominal functions
  PoolPtr allocateAlt(uint32_t size) { return (PoolPtr)allocate(size); }
  void deallocateAlt(PoolPtr sp, uint32_t size) { deallocate(sp, size); }
#endif // LFJ_64BIT
  
  bool realloc(void* ptr, uint32_t capacity, uint32_t newCapacity)
  {
    if (capacity == 0u)
      return false;
    assert(ptr != nullptr);
    
    // Find if chunk
    uint32_t alignedCapacity = alignSize(capacity);
    assert(alignedCapacity >= DeadCellSize);
    bool isChunk = chunkable(alignedCapacity);
    if (isChunk)
    {
      uint32_t ptrIdx;
      isChunk = findChunk((unsigned char*)ptr, ptrIdx);
      assert(isChunk);
      Chunk* chunk = &mChunks[ptrIdx];
      uint32_t alignedNewCapacity = alignSize(newCapacity);
      assert(alignedNewCapacity >= alignedCapacity);
      
      uint32_t pos = (uint32_t)((unsigned char*)ptr - chunk->data);
      if (pos + alignedCapacity == chunk->firstAvail
          && pos + alignedNewCapacity <= ChunkSize)
      {
        chunk->firstAvail = (uint16_t)(pos + alignedNewCapacity);
        LFJ_POOLALLOCATOR_SANITY_CHECK
        return true;
      }
    }
    return false;
  }
  
  // Raw memory
  bool memPop(void* src, void* dst, uint32_t size, uint32_t copy)
  {
    assert(src);
    assert(dst);
    
    uint32_t alignedSize = alignSize(size);
    assert(alignedSize >= DeadCellSize);
    bool isChunk = chunkable(alignedSize);
    if (isChunk)
    {
      uint32_t ptrIdx;
      isChunk = findChunk((unsigned char*)src, ptrIdx);
      assert(isChunk);
      
      Chunk* chunk = &mChunks[ptrIdx];
    #ifndef NDEBUG
      uint32_t pos = (uint32_t)((unsigned char*)src - chunk->data);
      assert(pos + alignedSize == (uint32_t)chunk->firstAvail);
    #endif
      
      std::memcpy(dst, src, copy);
      chunk->firstAvail -= (uint16_t)alignedSize;
      
      return true;
    }
    return false;
  }
  
  void* memPush(void* src, uint32_t size)
  {
    assert(src);
    assert(size > 0u);
    
    void* dst = allocate(size);
    std::memcpy(dst, src, size);
    
    return dst;
  }
  
  void* memPushBigArray(void* src, uint32_t count)
  {
    assert(src);
    assert(count > 0u);
    uint32_t realSize = sizeof(JBigArray) + (count - 1) * sizeof(JValue);
    
    void* dst = allocate(realSize);
    JBigArray jb;
    jb.capa = count;
    std::memcpy(dst, &jb, sizeof(JBigArray));
    JBigArray* dstJb = (JBigArray*)dst;
    std::memcpy(dstJb->data, src, count * sizeof(JValue));
    
    return dst;
  }
  
  void* memPushBigBArray(void* src, uint32_t count)
  {
    assert(src);
    assert(count > 0u);
    uint32_t realSize = sizeof(JBigBArray) + (count - 1) * sizeof(bool);
    
    void* dst = allocate(realSize);
    JBigBArray jba;
    jba.capa = count;
    std::memcpy(dst, &jba, sizeof(JBigBArray));
    JBigBArray* dstJba = (JBigBArray*)dst;
    std::memcpy(dstJba->data, src, count * sizeof(bool));
    
    return dst;
  }
  
  void* memPushBigIArray(void* src, uint32_t count)
  {
    assert(src);
    assert(count > 0u);
    uint32_t realSize = sizeof(JBigIArray) + (count - 1) * sizeof(int64_t);
    
    void* dst = allocate(realSize);
    JBigIArray jbi;
    jbi.capa = count;
    std::memcpy(dst, &jbi, sizeof(JBigIArray));
    JBigIArray* dstJbi = (JBigIArray*)dst;
    std::memcpy(dstJbi->data, src, count * sizeof(int64_t));
    
    return dst;
  }
  
  void* memPushBigDArray(void* src, uint32_t count)
  {
    assert(src);
    assert(count > 0u);
    uint32_t realSize = sizeof(JBigDArray) + (count - 1) * sizeof(double);
    
    void* dst = allocate(realSize);
    JBigDArray jbd;
    jbd.capa = count;
    std::memcpy(dst, &jbd, sizeof(JBigDArray));
    JBigDArray* dstJbd = (JBigDArray*)dst;
    std::memcpy(dstJbd->data, src, count * sizeof(double));
    
    return dst;
  }
  
  void* memPushBigObject(void* src, uint32_t count)
  {
    assert(src);
    assert(count > 0u);
    uint32_t realSize = sizeof(JBigObject) + (count - 1) * sizeof(JMember);
    
    void* dst = allocate(realSize);
    JBigObject jb;
    jb.capa = count;
    std::memcpy(dst, &jb, sizeof(JBigObject));
    JBigObject* dstJb = (JBigObject*)dst;
    std::memcpy(dstJb->data, src, count * sizeof(JMember));
    
    return dst;
  }
  
  // Modifiers
  void releaseAll()
  {
    for (uint32_t i = 0; i < mChunksCount; ++i)
      mAllocator.deallocate((char*)mChunks[i].data, ChunkSize);
    mAllocator.deallocate((char*)mChunks, mChunksCapacity * sizeof(Chunk));
    
    mLastChunk      = 0;
    mTotalDead      = 0;
    mChunksCount    = 0;
    mChunksCapacity = 0;
    mChunks         = nullptr;
    
    Fallback* it = mFallbacks;
    while (it != nullptr)
    {
      Fallback* itNext = it->next;
      mAllocator.deallocate((char*)it, sizeof(Fallback) - 1 + it->size);
      it = itNext;
    }
    mFallbacks = nullptr;
  }
  
  void clear()
  {
    for (uint32_t i = 0; i < mChunksCount; ++i)
    {
      mChunks[i].firstAvail = 0;
      mChunks[i].firstDead  = ChunkSize;
      mChunks[i].totalDead  = 0;
    }
    mTotalDead = 0;
    
    Fallback* it = mFallbacks;
    while (it != nullptr)
    {
      Fallback* itNext = it->next;
      mAllocator.deallocate((char*)it, sizeof(Fallback) - 1 + it->size);
      it = itNext;
    }
    mFallbacks = nullptr;
  }
  
  void shrink()
  {
  #ifdef LFJ_64BIT
    assert(!altScheme);
  #endif
    uint32_t newSize = mChunksCount;
    for (uint32_t i = 0; i < mChunksCount; ++i)
    {
      if (mChunks[i].firstAvail == 0)
      {
        mAllocator.deallocate((char*)mChunks[i].data, ChunkSize);
        --newSize;
      }
    }
    if (newSize > 0)
    {
      for (uint32_t i = 0; i < mChunksCount;)
      {
        int j = 1;
        if (mChunks[i].firstAvail == 0)
        {
          while (i + j < mChunksCount && mChunks[i + j].firstAvail == 0)
            ++j;
          if (i + j == mChunksCount)
            break;
          
          std::memmove((void*)(&mChunks[i]), (void*)(&mChunks[i + j]), (mChunksCount - i - j) * sizeof(Chunk));
          i += j;
        }
        else
          ++i;
      }
    }
    else
    {
      mAllocator.deallocate((char*)mChunks, mChunksCapacity * sizeof(Chunk));
      mChunksCapacity = 0;
      mChunks = nullptr;
    }
    mChunksCount = newSize;
  }
  
#ifdef LFJ_64BIT
  // Alternative shrink scheme (keep chunk/fallback indexes stable)
  // /!\ Do not mix schemes (nominal for objects, alt for strings)
  void shrinkAlt()
  {
    assert(altScheme);
    // Release all or none
    for (uint32_t i = 0; i < mChunksCount; ++i)
    {
      if (mChunks[i].firstAvail != 0)
        return;
    }
    
    for (uint32_t i = 0; i < mChunksCount; ++i)
      mAllocator.deallocate((char*)mChunks[i].data, ChunkSize);
    
    mAllocator.deallocate((char*)mChunks, mChunksCapacity * sizeof(Chunk));
    mChunksCount = 0;
    mChunksCapacity = 0;
    mChunks = nullptr;
  }
#else
  // Redirect to nominal functions
  void shrinkAlt() { shrink(); }
#endif // LFJ_64BIT
  
  // Utils
  void* toPtr(const PoolPtr sp) const
  {
  #ifdef LFJ_64BIT
    if (sp.chunk == LFJ_MAX_UINT16)
      return nullptr;
    if (sp.chunk < mChunksCount)  // Chunk
      return (void*)(mChunks[sp.chunk].data + sp.pos);
    
    // Fallback
    assert(sp.chunk == LFJ_MAX_UINT16 - 1u);
    assert(sp.pos < mFallbackCount);
    uint32_t pos = mFallbackCount - 1u - sp.pos;
    Fallback* it = mFallbacks;
    for (uint16_t i = 0; i < pos; ++i)
    {
      assert(it->next != nullptr);
      it = it->next;
    }
    return (void*)it->data;
  #else
    return (void*)sp;
  #endif
  }
  
  static bool chunkable(uint32_t alignedSize)
  {
    return alignedSize <= (uint32_t)ChunkSize;
  }
  
  static uint32_t alignSize(uint32_t size)
  {
    constexpr uint32_t alignment = (uint32_t)alignof(JBigObject);
    uint32_t floor = (size / alignment) * alignment;
    return size == floor ? floor : floor + alignment;
  }
  
private:
  bool findChunk(unsigned char* ptr, uint32_t& ptrIdx)
  {
  #ifdef LFJ_64BIT
    assert(!altScheme);
  #endif
    // Binary search, outputs successor index if not found
    uint32_t beginIdx = 0;
    uint32_t endIdx = mChunksCount;
    while (beginIdx < endIdx)
    {
      uint32_t midIdx = (beginIdx + endIdx) / 2;
      unsigned char* mid = mChunks[midIdx].data;
      ptrdiff_t cmp = ptr - mid;
  
      if (cmp < 0)  // before
        endIdx = midIdx;
      else if (cmp >= (ptrdiff_t)ChunkSize)  // after
        beginIdx = midIdx + 1;
      else  // found
      {
        ptrIdx = midIdx;
        return true;
      }
    }
    ptrIdx = endIdx;
    return false;
  }
  
  uint32_t sortNewChunk()
  {
  #ifdef LFJ_64BIT
    assert(!altScheme);
  #endif
    uint32_t successorIdx = mChunksCount;
    findChunk(mChunks[mChunksCount].data, successorIdx);
    
    uint32_t diff = mChunksCount - successorIdx;
    if (diff > 0)
    {
      Chunk copy(mChunks[mChunksCount]);
      std::memmove(&mChunks[successorIdx + 1], &mChunks[successorIdx], sizeof(Chunk) * diff);
      mChunks[successorIdx] = copy;
    }
    return successorIdx;
  }
  
  void* allocateFromDead(Chunk* chunk, uint16_t size)
  {
    assert(size == alignSize(size));
    if (chunk->totalDead >= size)
    {
      uint32_t sizeOfTwo = (uint32_t)size * 2;
      uint16_t curDead  = chunk->firstDead;
      uint16_t prevDead = ChunkSize;
      uint16_t smallestDead = ChunkSize;
      uint16_t smallestSize = ChunkSize;
      while (curDead < ChunkSize)
      {
        unsigned char* deadCell = &chunk->data[curDead];
        uint16_t deadSize = DeadCell::getSize(deadCell);
        
        // Exact size
        if (deadSize == size)
        {
          // Update prev
          if (prevDead >= ChunkSize)
            chunk->firstDead = DeadCell::getNext(deadCell);
          else
          {
            unsigned char* prevDeadCell = &chunk->data[prevDead];
            DeadCell::setNext(prevDeadCell, DeadCell::getNext(deadCell));
          }
          
          mTotalDead -= size;
          chunk->totalDead -= size;
          LFJ_POOLALLOCATOR_SANITY_CHECK
          return chunk->data + curDead;
        }
        // Large enough for 2 (limit potential fragmentation)
        if ((uint32_t)deadSize >= sizeOfTwo)
        {
          assert((deadSize - size >= (uint16_t)DeadCellSize));
          
          // Update remaining size
          deadSize -= size;
          DeadCell::setSize(deadCell, deadSize);
          
          mTotalDead -= size;
          chunk->totalDead -= size;
          LFJ_POOLALLOCATOR_SANITY_CHECK
          return chunk->data + curDead + deadSize;
        }
        // Find smallest
        if (deadSize < smallestSize && deadSize > size)
        {
          assert(deadSize - size >= (uint16_t)DeadCellSize); // size is aligned, alignment multiple of DeadCellSize
          smallestDead = curDead;
          smallestSize = deadSize;
        }
        
        // Iterate
        prevDead = curDead;
        curDead = DeadCell::getNext(deadCell);
      }
      if (smallestDead < ChunkSize)
      {
        // Update remaining size
        unsigned char* newDeadCell = &chunk->data[smallestDead];
        assert(DeadCell::getSize(newDeadCell) == smallestSize);
        smallestSize -= size;
        DeadCell::setSize(newDeadCell, smallestSize);
        
        mTotalDead -= size;
        chunk->totalDead -= size;
        LFJ_POOLALLOCATOR_SANITY_CHECK
        return chunk->data + smallestDead + smallestSize;
      }
    }
    return nullptr;
  }
  
#ifdef LFJ_POOLALLOCATOR_PACK
  void packChunkDead(Chunk* chunk)
  {
    if (chunk->totalDead == 0)
      return;
    assert(chunk->totalDead != chunk->firstAvail);
    
    uint16_t curDead = chunk->firstDead;
    uint16_t minDead = ChunkSize;
    constexpr uint16_t chkSize = ChunkSize > 0 ? ChunkSize : 1;
    bool dead[chkSize] = { false };
    
    // Merge cells
    while (curDead != ChunkSize)
    {
      minDead = minDead < curDead ? minDead : curDead;
      
      unsigned char* deadCell = &chunk->data[curDead];
      uint16_t deadSize = DeadCell::getSize(deadCell);
      
      // Merge right
      if (curDead + deadSize < ChunkSize && dead[curDead + deadSize])
      {
        dead[curDead + deadSize] = false;
        unsigned char* rightCell = &chunk->data[curDead + deadSize];
        uint16_t rightSize = DeadCell::getSize(rightCell);
        
        DeadCell::setSize(deadCell, deadSize + rightSize);
        deadSize += rightSize;
      }
      else
        dead[curDead + deadSize - 1] = true;
      
      // Merge left
      if (curDead > 0 && dead[curDead - 1])
      {
        dead[curDead - 1] = false;
        uint16_t leftDead = curDead - 2;
        while (!dead[leftDead])
          --leftDead;
        
        unsigned char* leftCell = &chunk->data[leftDead];
        uint16_t leftSize = DeadCell::getSize(leftCell);
        
        DeadCell::setSize(leftCell, leftSize + deadSize);
        DeadCell::setNext(leftCell, DeadCell::getNext(deadCell));
      }
      else
        dead[curDead] = true;
      
      curDead = DeadCell::getNext(deadCell);
    }
    
    // Sort nextDeadCell
    curDead = minDead;
    unsigned char* deadCell = &chunk->data[curDead];
    uint16_t deadSize = DeadCell::getSize(deadCell);
    uint16_t prevDead = curDead;
    uint16_t prevSize = deadSize;
    uint16_t prevPrevDead = ChunkSize;
    chunk->firstDead = minDead;
    
    if (deadSize != chunk->totalDead) // not single dead
    {
      uint16_t deadCount = deadSize;
      curDead += deadSize + 1; // next always alive
      for (; curDead < chunk->firstAvail && deadCount < chunk->totalDead; ++curDead)
      {
        if (dead[curDead])
        {
          unsigned char* prevCell = &chunk->data[prevDead];
          DeadCell::setNext(prevCell, curDead);
          
          deadCell = &chunk->data[curDead];
          deadSize = DeadCell::getSize(deadCell);
          
          prevPrevDead = prevDead;
          prevDead = curDead;
          prevSize = deadSize;
  
          deadCount += deadSize;
          curDead += deadSize; // next always alive
          assert(curDead >= chunk->firstAvail || dead[curDead] == 0);
        }
      }
      assert(chunk->totalDead == deadCount);
    }
    unsigned char* prevCell = &chunk->data[prevDead];
    DeadCell::setNext(prevCell, ChunkSize);
    
    // Merge firstAvailable
    if (prevDead + prevSize == chunk->firstAvail)
    {
      chunk->firstAvail -= prevSize;
      chunk->totalDead -= prevSize;
      mTotalDead -= prevSize;
      
      assert(prevPrevDead != ChunkSize);
      curDead = prevPrevDead;
      unsigned char* curCell = &chunk->data[curDead];
      DeadCell::setNext(curCell, ChunkSize);
    }
    
    // Check
    LFJ_POOLALLOCATOR_SANITY_CHECK
  #ifdef LFJ_POOLALLOCATOR_DEBUG
    auto dc = getDeadCells(chunk);
    assert(dc.first == 0);
  #endif
  }
  
  void packDead()
  {
    for (uint32_t i = 0; i < mChunksCount; ++i)
      packChunkDead(&mChunks[i]);
  }
#endif
  
#ifdef LFJ_POOLALLOCATOR_SANITY
  void sanityCheck() const
  {
    assert(mChunksCapacity >= mChunksCount);
    uint32_t totalDead = 0;
  #ifndef NDEBUG
    unsigned char* prevData = 0;
  #endif
    
    for (uint32_t i = 0; i < mChunksCount; ++i)
    {
      const auto& chunk = mChunks[i];
      assert(chunk.data != nullptr);
      assert(altScheme || chunk.data > prevData);
      assert(chunk.firstAvail > chunk.firstDead || chunk.firstDead == ChunkSize);
      if (chunk.totalDead == 0)
      {
        assert(chunk.firstAvail <= ChunkSize);
        assert(chunk.firstDead  == ChunkSize);
      }
      else
      {
        assert(chunk.firstAvail > 0);
        assert(chunk.firstDead  < ChunkSize);
        
        uint32_t chunkDead = 0;
        uint16_t next = chunk.firstDead;
        while (next != ChunkSize)
        {
          unsigned char* dc = &chunk.data[next];
          uint16_t dcSize = DeadCell::getSize(dc);
          uint16_t dcNext = DeadCell::getNext(dc);
          assert(dcSize <= chunk.totalDead);
          chunkDead += dcSize;
          next = dcNext;
        }
        assert(chunkDead == chunk.totalDead);
        totalDead += chunkDead;
      }
    #ifndef NDEBUG
      prevData = chunk.data;
    #endif
    }
    assert(totalDead == mTotalDead);
    
    Fallback* fb = mFallbacks;
    while (fb)
    {
      assert(fb->data != nullptr);
      assert(altScheme || alignSize(fb->size) > ChunkSize);
      fb = fb->next;
    }
  }
#endif  // LFJ_POOLALLOCATOR_SANITY
};

// Aliases
typedef std::allocator<char> StdAllocator;

template <uint16_t ChunkSize, class Allocator = StdAllocator>
using StringPoolAllocator = PoolAllocator<ChunkSize, Allocator, true, true>;

template <uint16_t ChunkSize, class Allocator = StdAllocator, bool own = false>
using ObjectPoolAllocator = PoolAllocator<ChunkSize, Allocator, own, false>;

} // namespace lfjson

#undef LFJ_POOLALLOCATOR_SANITY_CHECK

#endif // LFJSON_POOLALLOCATOR_H
