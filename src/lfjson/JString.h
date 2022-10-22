/**
 * Copyright 2022 Guillaume AUJAY. All rights reserved.
 *
 */

#ifndef LFJSON_JSTRING_H
#define LFJSON_JSTRING_H

#include "Utils.h"

#include <cstdint>
#include <cstring>
#include <cassert>
#include <limits>

#define LFJ_JSTRING_MAX_LEN ((uint32_t)1073741823u) // 2^30 - 1
#define LFJ_MAX_UINT16      (std::numeric_limits<uint16_t>::max())

#ifdef LFJ_JSTRING_TEST
  #include <memory>
#endif

namespace lfjson
{
#ifdef LFJ_64BIT
// Pointer as PoolAllocator indexes (4 Bytes instead of 8)
struct PoolPtr {
  PoolPtr() {}
  PoolPtr(std::nullptr_t) {}
  PoolPtr(uint16_t chunk_, uint16_t pos_) : chunk(chunk_) , pos(pos_) {}
  
  uint16_t chunk = LFJ_MAX_UINT16;
  uint16_t pos   = LFJ_MAX_UINT16;
};
enum { PoolPtrInit = 255 };
#else
class JString;
typedef JString* PoolPtr;
enum { PoolPtrInit = 0 };
#endif

// String data, immutable, interned or extern, no sso (done in JValue), element of a StringPool
class JString  // (12/16 Bytes + ~owned len)
{
private:
  struct Info { // (4 Bytes)
    Info(bool own, bool key, uint32_t len)
      : flags((uint32_t)own | ((uint32_t)key << 1) | (len << 2))
    {
    }
    
    bool own()     const { return flags & 0x01; } // if local char array, pointer to extern string otherwise
    bool key()     const { return flags & 0x02; } // if used as JMember key at least once
    uint32_t len() const { return flags >> 2; }   // string length
    
    void updateIsKey(bool key) { flags |= key << 1; }
    
    uint32_t  flags;  // len:30 | key:1 | own:1
  } mInfo;
  
  PoolPtr mNext;  // 4 Bytes
  
  union { // (4/8 Bytes) inplace or extern if const str
    char mStr[1];      // owned array, real size is len+1
    const char* mExt;
  };
  
  // Constructors
  JString(const char* str, uint32_t len, bool key, PoolPtr next) // owned
    : mInfo(true, key, len)
    , mNext(next)
  {
    std::memcpy(mStr, str, len);
    mStr[len] = '\0';
  }
  JString(const char* ext, uint32_t len, bool key, PoolPtr next, bool own) // extern
    : mInfo(own, key, len)
    , mNext(next)
    , mExt(ext)
  {
    assert(own == false);
  }
  
public:
#ifdef LFJ_JSTRING_TEST
  template <class Allocator = std::allocator<char>>
  static JString* create(const char* str, uint32_t len, bool own, bool key, PoolPtr next)
  {
    assert(str != nullptr);
    assert(len <= LFJ_JSTRING_MAX_LEN);
    
    Allocator allocator; // For testing purpose
    uint32_t overflowLen = (!own || len < sizeof(char*)) ? 0 : len + 1 - sizeof(char*);
    
    void* raw = allocator.allocate(sizeof(JString) + overflowLen);
    return construct(raw, str, len, own, key, next);
  }
#endif
  
  static JString* construct(void* raw, const char* str, uint32_t len, bool own, bool key, PoolPtr next)
  {
    assert(raw != nullptr);
    assert(len <= LFJ_JSTRING_MAX_LEN);
    
    if (!own)  // extern
      return new (raw) JString(str, len, key, next, own);
    
    return new (raw) JString(str, len, key, next);
  }
  
  static uint32_t totalSize(bool own, uint32_t len)
  {
    uint32_t overflowSize = (!own || len < sizeof(char*)) ? 0 : len + 1 - sizeof(char*);
    return sizeof(JString) + overflowSize;
  }
  
  // Accessors
  bool owns() const { return mInfo.own(); }
  
  bool isKey() const { return mInfo.key(); }
  
  void updateIsKey(bool key) { mInfo.updateIsKey(key); }

  uint32_t len() const { return mInfo.len(); }
  
  const char* c_str() const { return mInfo.own() ? mStr : mExt; }
  
  PoolPtr next() const { return mNext; }
  
  // Setters
  void setNext(const PoolPtr next) { mNext = next; }
  
  // Utils
  int compare(const char* other, uint32_t len) const
  {
    const uint32_t infoLen = mInfo.len();
    if (infoLen < len)
      return -1;
    if (infoLen > len)
      return 1;
    if (!mInfo.own() && mExt == other)
      return 0;
    
    return std::memcmp(c_str(), other, len);
  }
};

} // namespace lfjson

#endif // LFJSON_JSTRING_H
