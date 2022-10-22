/**
 * Copyright 2022 Guillaume AUJAY. All rights reserved.
 *
 */

#ifndef LFJSON_STRINGPOOL_H
#define LFJSON_STRINGPOOL_H

#include "JString.h"
#include "PoolAllocator.h"
#include "Utils.h"

//#define LFJ_NO_XXHASH // uncomment to fallback to FNV-1a (slower)
#ifndef LFJ_NO_XXHASH
  #define XXH_INLINE_ALL
  #include "xxhash.h"
#endif

#include <cstddef>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <cassert>
#include <limits>

#define LFJ_STRINGPOOL_DFLT_CHUNKSIZE   32768u

#ifdef LFJ_STRINGPOOL_DEBUG
  #include <string>
  #include <unordered_set>
  #define LFJ_STRINGPOOL_SANITY_CHECK  { sanityCheck(); }
#else
  #define LFJ_STRINGPOOL_SANITY_CHECK
#endif
#ifdef LFJ_STRINGPOOL_INSTRUMENTED
  #define LFJ_STRINGPOOL_UPDATE_INSTRU(key, len) { updateOnFound(key, len); }
#else
  #define LFJ_STRINGPOOL_UPDATE_INSTRU(key, len)
#endif

namespace lfjson
{
//
// Hash table using separate chaining, owns a StringPoolAllocator
// With intrusive JString items and PoolPtr on 64-bits (sparing 4 Bytes per pointer)
template <uint16_t ChunkSize = LFJ_STRINGPOOL_DFLT_CHUNKSIZE,
          class Allocator = StdAllocator>
class StringPool // (4 * bucketCount + 12/16 * ItemCount + sizeof(StringPool) Bytes)
{
  static constexpr uint32_t StartingBucketCount = 16;   // growing from 0, must be > 1
  static constexpr float GrowthFactor = 2.f;            // must be > 1.f
  static constexpr float DefaultMaxLoadFactor = 1.5f;   // must be > 0.f (up to 1.5f has limited speed impact)
  
  static_assert(StartingBucketCount > 1u, "[lfjson] StringPool: StartingBucketCount must be > 1");
  static_assert(GrowthFactor > 1.f, "[lfjson] StringPool: GrowthFactor must be > 1.f");
  static_assert(DefaultMaxLoadFactor > 0.f, "[lfjson] StringPool: DefaultMaxLoadFactor must be > 0.f");
  
#ifdef LFJ_STRINGPOOL_INSTRUMENTED
public:
  void updateOnFound(bool key, uint32_t len)
  {
    if (key) 
    {
      if (len < JValue::ShortString_MaxSize)
        ++dedupShortKeys;
      else
        ++dedupLongKeys;
      dedupKeyLen += len;
    }
    else
    {
      ++dedupLongVals;
      dedupValLen += len;
    }
  }
  
  uint64_t dedupShortKeys = 0u;
  uint64_t dedupLongKeys  = 0u;
  uint64_t dedupLongVals  = 0u;
  uint64_t dedupKeyLen    = 0u;
  uint64_t dedupValLen    = 0u;
#endif
  
private:
  StringPoolAllocator<ChunkSize, Allocator> mAllocator;
  float     mMaxLoadFactor = DefaultMaxLoadFactor;
  uint32_t  mItemCount;    // held items
  uint32_t  mBucketCount;  // total buckets
  PoolPtr*  mBuckets;      // array
  PoolPtr   mBucketsPtr;   // alt for mBuckets
  
public:
  StringPool()
    : mItemCount(0)
    , mBucketCount(0)
    , mBuckets(nullptr)
    , mBucketsPtr(nullptr)
  {
  }
  
  StringPool(uint32_t initBucketCount)
    : mItemCount(0)
    , mBucketCount(initBucketCount)
  {
    if (initBucketCount > 0u)
    {
      mBucketsPtr = mAllocator.allocateAlt(sizeof(PoolPtr) * initBucketCount);
      mBuckets = (PoolPtr*)mAllocator.toPtr(mBucketsPtr);
      std::memset((void*)mBuckets, PoolPtrInit, sizeof(PoolPtr) * initBucketCount);
    }
    else
    {
      mBuckets = nullptr;
      mBucketsPtr = nullptr;
    }
  }
  
  StringPool(const StringPool&) = delete;
  StringPool& operator=(const StringPool&) = delete;
  
  ~StringPool()
  {
  #ifdef LFJ_STRINGPOOL_DEBUG
    uint32_t count = 0;
    for (uint32_t i = 0; i < mBucketCount; ++i)
    {
      PoolPtr itPtr = mBuckets[i];
      const JString* it = (JString*)mAllocator.toPtr(itPtr);
      while (it != nullptr)
      {
        ++count;
        const PoolPtr nextPtr = it->next();
        it = (JString*)mAllocator.toPtr(nextPtr);
      }
    }
    assert(count == mItemCount);
  #endif
    // Note: StringPoolAllocator destructor will deallocate all (strings and buckets)
  }
  
  // Accessors
  uint32_t size() const { return mItemCount; }
  
  uint32_t bucket_count() const { return mBucketCount; }
  
  float load_factor() const { return (mBucketCount == 0u) ? 0.f : (float)mItemCount / (float)mBucketCount; }
  
  float max_load_factor() const { return mMaxLoadFactor; }
  
  void max_load_factor(float mlf)
  {
    if (mlf > 0.f)
      mMaxLoadFactor = mlf;
    else
      assert("[lfjson] StringPool: max_load_factor must be > 0.f");
  }
  
  // Statistics
  uint64_t count_strings_length() const
  {
    uint64_t stringsLength = 0;
    for (uint32_t i = 0; i < mBucketCount; ++i)
    {
      const PoolPtr itPtr = mBuckets[i];
      const JString* it = (JString*)mAllocator.toPtr(itPtr);
      while (it != nullptr)
      {
        stringsLength += (uint64_t)it->len();
        const PoolPtr nextPtr = it->next();
        it = (JString*)mAllocator.toPtr(nextPtr);
      }
    }
    return stringsLength;
  }
  
  uint32_t count_used_buckets() const
  {
    uint32_t usedBuckets = 0;
    for (uint32_t i = 0; i < mBucketCount; ++i)
    {
      if (mAllocator.toPtr(mBuckets[i]) != nullptr)
        ++usedBuckets;
    }
    return usedBuckets;
  }
  
  uint32_t count_max_chaining() const
  {
    uint32_t maxChaining = 0;
    for (uint32_t i = 0; i < mBucketCount; ++i)
    {
      int32_t chains = -1;
      const PoolPtr itPtr = mBuckets[i];
      const JString* it = (JString*)mAllocator.toPtr(itPtr);
      while (it != nullptr)
      {
        ++chains;
        const PoolPtr nextPtr = it->next();
        it = (JString*)mAllocator.toPtr(nextPtr);
      }
      maxChaining = chains > (int32_t)maxChaining ? (uint32_t)chains : maxChaining;
    }
    return maxChaining;
  }
  
  float count_mean_chaining() const
  {
    // Only consider non-empty buckets
    uint32_t usedBuckets   = 0;
    uint32_t totalChaining = 0;
    for (uint32_t i = 0; i < mBucketCount; ++i)
    {
      int32_t chains = -1;
      const PoolPtr itPtr = mBuckets[i];
      const JString* it = (JString*)mAllocator.toPtr(itPtr);
      while (it != nullptr)
      {
        ++chains;
        const PoolPtr nextPtr = it->next();
        it = (JString*)mAllocator.toPtr(nextPtr);
      }
      if (chains >= 0)
      {
        ++usedBuckets;
        totalChaining += chains;
      }
    }
    return usedBuckets > 0 ? (float)totalChaining / usedBuckets : 0.f;
  }
  
  // Allocators
  Allocator& allocator() { return mAllocator.allocator(); }
  const Allocator& callocator() const { return mAllocator.callocator(); }
  
  const StringPoolAllocator<ChunkSize, Allocator>& stringPoolAllocator() const { return mAllocator; }
  
  const JString* get(const char* str, int32_t length = -1) const
  {
    return get_(str, length);
  }
  
  // Provide for extern strings (i.e. not copied)
  const JString* provide(const char* str, bool key, bool& found, int32_t length = -1)
  {
    return provide(str, false, key, found, length);
  }
  
  // Provide for interned strings (i.e. copied)
  const JString* provide(char* str, bool key, bool& found, int32_t length = -1)
  {
    return provide(str, true, key, found, length);
  }
  
  const JString* provideInterned(const char* str, bool key, bool& found, int32_t length = -1)
  {
    return provide(str, true, key, found, length);
  }
  
  // Release memory of strings not used as JMember key
  void releaseValues()
  {
    for (uint32_t i = 0; i < mBucketCount; ++i)
    {
      PoolPtr itPtr = mBuckets[i];
      JString* it = (JString*)mAllocator.toPtr(itPtr);
      while (it != nullptr) // Head
      {
        if (!it->isKey())
        {
          mBuckets[i] = it->next();
          --mItemCount;
          
          uint32_t jsSize = JString::totalSize(it->owns(), it->len());
          mAllocator.deallocateAlt(itPtr, jsSize);
          
          itPtr = mBuckets[i];
          it = (JString*)mAllocator.toPtr(itPtr);
        }
        else
          break;
      }
      
      if (it != nullptr) // Remaining
      {
        PoolPtr nextPtr = it->next();
        JString* itNext = (JString*)mAllocator.toPtr(nextPtr);
        while (itNext != nullptr)
        {
          if (!itNext->isKey())
          {
            it->setNext(itNext->next());
            --mItemCount;
            
            uint32_t jsSize = JString::totalSize(itNext->owns(), itNext->len());
            mAllocator.deallocateAlt(nextPtr, jsSize);
            
            nextPtr = it->next();
            itNext = (JString*)mAllocator.toPtr(nextPtr);
          }
          else
          {
            it = itNext;
            nextPtr = itNext->next();
            itNext = (JString*)mAllocator.toPtr(nextPtr);
          }
        }
      }
    }
    LFJ_STRINGPOOL_SANITY_CHECK
  }
  
  // Release strings and buckets
  void releaseAll()
  {
    mAllocator.releaseAll();
    
    mItemCount   = 0;
    mBucketCount = 0;
    mBuckets     = nullptr;
    mBucketsPtr  = nullptr;
  }
  
  // Modifiers
  void clear()
  {
    for (uint32_t i = 0; i < mBucketCount; ++i)
    {
      PoolPtr itPtr = mBuckets[i];
      JString* it = (JString*)mAllocator.toPtr(mBuckets[i]);
      while (it != nullptr)
      {
        const PoolPtr nextPtr = it->next();
        JString* itNext = (JString*)mAllocator.toPtr(nextPtr);
        uint32_t jsSize = JString::totalSize(it->owns(), it->len());
        mAllocator.deallocateAlt(itPtr, jsSize);
        
        itPtr = nextPtr;
        it = itNext;
      }
    }
    mItemCount = 0;
    mAllocator.deallocateAlt(mBucketsPtr, sizeof(PoolPtr) * mBucketCount);
    mBucketCount = 0;
    mBuckets = nullptr;
    mBucketsPtr = nullptr;
  }
  
  void shrink(bool rehashStringPool = false)
  {
  #ifndef LFJ_STRINGPOOL_BUCKETS_POW_2
    if (rehashStringPool)
    {
      // Only if not in chunk
      if (!mAllocator.chunkable( mAllocator.alignSize(sizeof(PoolPtr) * mBucketCount) ))
      {
        uint32_t newBucketCount = (uint32_t)std::ceil(mItemCount / mMaxLoadFactor);
        rehash(newBucketCount);
      }
    }
  #endif
    mAllocator.shrinkAlt();
  }
  
private:
#ifndef LFJ_NO_XXHASH
  static uint32_t xxh3_low_len(const char* str, const int32_t len)
  {
    return (uint32_t)XXH_INLINE_XXH3_64bits(str, (size_t)len);
  }
  
  static uint32_t xxh3_low_nolen(const char* str, int32_t& len)
  {
    size_t str_len = strlen(str);
    assert(str_len <= (size_t)LFJ_JSTRING_MAX_LEN);
    len = (int32_t)str_len;
    return (uint32_t)XXH_INLINE_XXH3_64bits(str, str_len);
  }
#else
  static uint32_t fnv1a_32_len(const unsigned char* str, const int32_t len)
  {
    // FNV-1a 32-bits, with known len (public domain)
    static constexpr uint32_t FNV_PRIME    = 16777619u;
    static constexpr uint32_t OFFSET_BASIS = 2166136261u;
    
    uint32_t hash = OFFSET_BASIS;
    for (int32_t i = 0; i < len; ++i)
      hash = (hash ^ str[i]) * FNV_PRIME;
    
    return hash;
  }
  
  static uint32_t fnv1a_32_nolen(const unsigned char* str, int32_t& len)
  {
    // FNV-1a 32-bits, with unknown len (public domain)
    static constexpr uint32_t FNV_PRIME    = 16777619u;
    static constexpr uint32_t OFFSET_BASIS = 2166136261u;
    
    uint32_t hash = OFFSET_BASIS;
    const unsigned char* ch = str;
    while (*ch)
      hash = (hash ^ *ch++) * FNV_PRIME;
    
    assert((ptrdiff_t)(ch - str) < (ptrdiff_t)LFJ_JSTRING_MAX_LEN);
    len = (uint32_t)(ch - str);
    return hash;
  }
#endif
  static uint32_t computeHash_len(const char* str, const int32_t len)
  {
    assert(len >= 0);
  #ifndef LFJ_NO_XXHASH
    return xxh3_low_len(str, (size_t)len);
  #else
    return fnv1a_32_len((const unsigned char*)str, len);
  #endif
  }
  
  static uint32_t computeHash(const char* str, int32_t& len)
  {
    if (len >= 0)
    {
    #ifndef LFJ_NO_XXHASH
      return xxh3_low_len(str, len);
    #else
      return fnv1a_32_len((const unsigned char*)str, len);
    #endif
    }
    else
    {
    #ifndef LFJ_NO_XXHASH
      return xxh3_low_nolen(str, len);
    #else
      return fnv1a_32_nolen((const unsigned char*)str, len);
    #endif
    }
  }
  
  static uint32_t fastMod(const uint32_t input, const uint32_t ceil)
  {
    assert(ceil > 0u);
  #ifdef LFJ_STRINGPOOL_BUCKETS_POW_2
    return input & (ceil - 1);
  #else
    return input % ceil;
  #endif
  }
  
  void pushNewString(PoolPtr* buckets, uint32_t index, JString* jstr, PoolPtr sptr)
  {
    // Empty
    PoolPtr ptr = buckets[index];
    JString* it = (JString*)mAllocator.toPtr(ptr);
    if (it == nullptr)
    {
      jstr->setNext(nullptr);
      buckets[index] = sptr;
      return;
    }
    
    const char* str = jstr->c_str();
    const uint32_t len = jstr->len();
    
    // Compare head
    {
      int res = it->compare(str, len);
      assert(res != 0 && "[lfjson] StringPool: string duplicate found when rehashing. Might be due to storage modification of non-owned string");
      if (res > 0)
      {
        // Insert at head
        jstr->setNext(ptr);
        buckets[index] = sptr;
        return;
      }
    }
    
    // Separate chaining
    do {
      const PoolPtr ptrNext = it->next();
      JString* itNext = (JString*)mAllocator.toPtr(ptrNext);
      if (itNext == nullptr)
      {
        // Add at end
        jstr->setNext(nullptr);
        it->setNext(sptr);
        return;
      }
      // Check order
      int res = itNext->compare(str, len);
      assert(res != 0 && "[lfjson] StringPool: string duplicate found when rehashing. Might be due to storage modification of non-owned string");
      if (res > 0)
      {
        // Insert
        jstr->setNext(ptrNext);
        it->setNext(sptr);
        return;
      }
      // Iterate
      it = itNext;
    }
    while(true);
  }
  
  void rehash(uint32_t newBucketCount)
  {
    PoolPtr newBucketsPtr = mAllocator.allocateAlt(sizeof(PoolPtr) * newBucketCount);
    PoolPtr* newBuckets = (PoolPtr*)mAllocator.toPtr(newBucketsPtr);
    assert(newBuckets != nullptr);
    std::memset((void*)newBuckets, PoolPtrInit, sizeof(PoolPtr) * newBucketCount);
    
    // Copy to new array, according to hash code
    uint32_t count = 0;
    for (uint32_t index = 0; count < mItemCount; ++index)
    {
      assert(index < mBucketCount);
      
      PoolPtr ptr = mBuckets[index];
      JString* it = (JString*)mAllocator.toPtr(ptr);
      while (it != nullptr)
      {
        const PoolPtr ptrNext = it->next();
        JString* itNext = (JString*)mAllocator.toPtr(ptrNext);
        uint32_t newHash = computeHash_len(it->c_str(), (int32_t)it->len());
        uint32_t newIndex = fastMod(newHash, newBucketCount);
        
        pushNewString(newBuckets, newIndex, it, ptr);
        ptr = ptrNext;
        it = itNext;
        ++count;
      }
    }
    // Apply
    mAllocator.deallocateAlt(mBucketsPtr, sizeof(PoolPtr) * mBucketCount);
    mBuckets = newBuckets;
    mBucketsPtr = newBucketsPtr;
    mBucketCount = newBucketCount;
    LFJ_STRINGPOOL_SANITY_CHECK
  }
  
  PoolPtr createString(const char* str, uint32_t len, bool own, bool key, PoolPtr next)
  {
    uint32_t jsSize = JString::totalSize(own, len);
    PoolPtr ptr = mAllocator.allocateAlt(jsSize);
    
    JString* raw = (JString*)mAllocator.toPtr(ptr);
    JString::construct(raw, str, len, own, key, next);
    
    return ptr;
  }
  
  const JString* get_(const char* str, int32_t len) const
  {
    assert(str != nullptr);
    assert(len <= (int32_t)LFJ_JSTRING_MAX_LEN);
    if (mItemCount == 0)
      return nullptr;
    
    // Hash
    uint32_t hash = computeHash(str, len);
    uint32_t index = fastMod(hash, mBucketCount);
    
    // Check bucket
    const JString* it = (JString*)mAllocator.toPtr(mBuckets[index]);
    while (it != nullptr)
    {
      if (it->compare(str, len) == 0)
        return it;
      
      it = (JString*)mAllocator.toPtr(it->next());
    }
    return nullptr;
  }
  
  // Provide (get or create)
  const JString* provide(const char* str, bool own, bool key, bool& found, int32_t len)
  {
    assert(str != nullptr);
    assert(len <= (int32_t)LFJ_JSTRING_MAX_LEN);
    assert(mItemCount < std::numeric_limits<uint32_t>::max());
    
    // Grow (by anticipation)
    if ((mItemCount + 1) > (uint32_t)(mBucketCount * mMaxLoadFactor))
    {
      if (mBucketCount < std::numeric_limits<uint32_t>::max() / GrowthFactor)
      {
        uint32_t newBucketCount = (mBucketCount > 0u) ? (uint32_t)std::ceil(mBucketCount * GrowthFactor) : StartingBucketCount; // grow once
        rehash(newBucketCount);
      }
      else
        assert(false && "[lfjson] StringPool: can't grow buckets count anymore");
    }
    
    // Hash
    uint32_t hash = computeHash(str, len);
    uint32_t index = fastMod(hash, mBucketCount);
    
    // Check head
    found = false;
    JString* head = (JString*)mAllocator.toPtr(mBuckets[index]);
    if (head == nullptr)
    {
      // Add as head
      mBuckets[index] = createString(str, len, own, key, nullptr);
      
      ++mItemCount;
      LFJ_STRINGPOOL_SANITY_CHECK
      return (JString*)mAllocator.toPtr(mBuckets[index]);
    }
    int res = head->compare(str, len);
    if (res == 0)
    {
      // Found at head
      found = true;
      head->updateIsKey(key);
      LFJ_STRINGPOOL_UPDATE_INSTRU(key, len)
      return head;
    }
    if (res > 0)
    {
      // Insert at head
      PoolPtr newHead = createString(str, len, own, key, mBuckets[index]);
      mBuckets[index] = newHead;
      
      ++mItemCount;
      LFJ_STRINGPOOL_SANITY_CHECK
      return (JString*)mAllocator.toPtr(newHead);
    }
    
    // Check separate chaining
    JString* it = head;
    do {
      const PoolPtr ptrNext = it->next();
      JString* itNext = (JString*)mAllocator.toPtr(ptrNext);
      if (itNext == nullptr)
      {
        // Add at end
        PoolPtr newString = createString(str, len, own, key, nullptr);
        it->setNext(newString);
        
        ++mItemCount;
        LFJ_STRINGPOOL_SANITY_CHECK
        return (JString*)mAllocator.toPtr(newString);
      }
      // Check order
      res = itNext->compare(str, len);
      if (res == 0)
      {
        // Found
        found = true;
        itNext->updateIsKey(key);
        LFJ_STRINGPOOL_UPDATE_INSTRU(key, len)
        return itNext;
      }
      if (res > 0)
      {
        // Insert
        PoolPtr newString = createString(str, len, own, key, ptrNext);
        it->setNext(newString);
        
        ++mItemCount;
        LFJ_STRINGPOOL_SANITY_CHECK
        return (JString*)mAllocator.toPtr(newString);
      }
      // Iterate
      it = itNext;
    }
    while(true);
  }
  
#ifdef LFJ_STRINGPOOL_DEBUG
  void sanityCheck() const
  {
    std::unordered_set<std::string> set;
    uint32_t count = 0;
    for (uint32_t i = 0; i < mBucketCount; ++i)
    {
      PoolPtr itPtr = mBuckets[i];
      const JString* it = (JString*)mAllocator.toPtr(itPtr);
      while (it != nullptr)
      {
        const char* str = it->c_str();
        const uint32_t len = it->len();
        
        // Check order
        const PoolPtr nextPtr = it->next();
        const JString* itNext = (JString*)mAllocator.toPtr(nextPtr);
        if (itNext != nullptr)
        {
          assert(itNext->compare(str, len) != 0
              && "[lfjson] StringPool: string duplicate found during sanity check. Might be due to storage modification of non-owned string");
        }
        // Check unicity
        {
          assert(set.find(str) == set.end());
          set.insert(str);
        }
        ++count;
        it = itNext;
      }
    }
    assert(count == mItemCount);
  }
#endif  // LFJ_STRINGPOOL_DEBUG
};

} // namespace lfjson

#endif // LFJSON_STRINGPOOL_H
