/**
 * Copyright 2022 Guillaume AUJAY. All rights reserved.
 *
 */

#include "gtest/gtest.h"

#define LFJ_JSTRING_TEST
#define LFJ_HEAPALLOCATOR_INSTRUMENTED
#include "lfjson/lfjson.h"
#include "lfjson/StackAllocator.h"
#include "lfjson/HeapAllocator.h"

#include <cmath>
#include <array>
#include <string>
#include <memory>

using namespace lfjson;


TEST(JString, Compare)
{
  std::unique_ptr<JString> j1( JString::create("b", 1, false, false, nullptr) );
  EXPECT_EQ(j1->compare("b",  1),  0);
  EXPECT_EQ(j1->compare("a",  1),  1);
  EXPECT_EQ(j1->compare("c",  1), -1);
  EXPECT_EQ(j1->compare("bb", 2), -1);
  
  std::unique_ptr<JString> j2(JString::create("bb", 2, false, true, nullptr));
  EXPECT_EQ(j2->compare("bb", 2),  0);
  EXPECT_EQ(j2->compare("b",  1),  1);
  EXPECT_EQ(j2->compare("a",  1),  1);
  EXPECT_EQ(j2->compare("c",  1),  1);
  EXPECT_EQ(j2->compare("bc", 2), -1);
  
  std::unique_ptr<JString> j3(JString::create("b", 1, true, false, nullptr));
  EXPECT_EQ(j3->compare("b",  1),  0);
  EXPECT_EQ(j3->compare("a",  1),  1);
  EXPECT_EQ(j3->compare("c",  1), -1);
  EXPECT_EQ(j3->compare("bb", 2), -1);
  
  std::unique_ptr<JString> j4(JString::create("bbbbb", 5, true, true, nullptr));
  EXPECT_EQ(j4->compare("bbbbb",  5),  0);
  EXPECT_EQ(j4->compare("b",      1),  1);
  EXPECT_EQ(j4->compare("a",      1),  1);
  EXPECT_EQ(j4->compare("c",      1),  1);
  EXPECT_EQ(j4->compare("bbbbc",  5), -1);
}

TEST(StringPool, ProvideExtern)
{
  StringPool<> spl;
  EXPECT_EQ(spl.size(), 0u);
  EXPECT_EQ(spl.bucket_count(), 0u);
  EXPECT_EQ(spl.get("hello"), nullptr);
  
  bool found = false;
  const JString* js1 = spl.provide("hello", false, found);
  EXPECT_EQ(js1->owns(), false);
  EXPECT_EQ(js1->isKey(), false);
  EXPECT_EQ(js1->len(), 5u);
  EXPECT_NE(spl.get(js1->c_str()), nullptr);
  EXPECT_NE(spl.get("hello"), nullptr);
  
  const JString* js2 = spl.provide("hello", true, found);
  EXPECT_EQ(js2->owns(), false);
  EXPECT_EQ(js2->isKey(), true);
  EXPECT_EQ(js2->len(), 5u);
  EXPECT_EQ(js1->c_str(), js2->c_str());
  EXPECT_EQ(js1, js2);
  
  const char* world = "world!";
  const JString* js3 = spl.provide(world, true,  found);
  const JString* js4 = spl.provide(world, false, found);
  EXPECT_EQ(js3->owns(), false);
  EXPECT_EQ(js3->isKey(), true);
  EXPECT_EQ(js3->len(), 6u);
  EXPECT_STREQ(js3->c_str(), world);
  EXPECT_NE(js1, js3);
  EXPECT_EQ(js3, js4);
  EXPECT_NE(spl.get(world), nullptr);
}

TEST(StringPool, ProvideIntern)
{
  uint32_t initBucketCount = 2u;
  StringPool<> spl(initBucketCount);
  
  bool found = false;
  const JString* js1 = spl.provide((char*)"hello", true, found);
  EXPECT_EQ(js1->owns(), true);
  EXPECT_EQ(js1->isKey(), true);
  EXPECT_EQ(js1->len(), 5u);
  
  const JString* js2 = spl.provide((char*)"hello", false, found);
  EXPECT_EQ(js2->owns(), true);
  EXPECT_EQ(js2->isKey(), true);
  EXPECT_EQ(js2->len(), 5u);
  EXPECT_EQ(js1->c_str(), js2->c_str());
  EXPECT_EQ(js1, js2);
  
  const char* world = "hello world!";
  const JString* js3 = spl.provideInterned(world, false, found);
  const JString* js4 = spl.provideInterned(world, true,  found);
  EXPECT_EQ(js3->owns(), true);
  EXPECT_EQ(js3->isKey(), true);
  EXPECT_EQ(js3->len(), 12u);
  EXPECT_STREQ(js3->c_str(), world);
  EXPECT_NE(js1, js3);
  EXPECT_EQ(js3, js4);
}

TEST(StringPool, ReleaseValues)
{
  StringPool<256, StackAllocator<4096>> spl;
  
  const auto& alc = spl.callocator();
  EXPECT_EQ(alc.used(), 0u);
  
  bool found = false;
  const JString* js1 = spl.provideInterned("Hello this is a long string with many characters", true, found);
  EXPECT_EQ(js1->owns(), true);
  EXPECT_EQ(js1->isKey(), true);
  EXPECT_EQ(js1->len(), 48u);
  
  const JString* js2 = spl.provide((char*)"Hello this is a long string with many characters", false, found);
  EXPECT_EQ(js2->owns(), true);
  EXPECT_EQ(js2->isKey(), true);
  EXPECT_EQ(js2->len(), 48u);
  
  const JString* js3 = spl.provide((char*)"world!", false, found);
  EXPECT_EQ(js3->owns(), true);
  EXPECT_EQ(js3->isKey(), false);
  EXPECT_EQ(js3->len(), 6u);
  
  uint32_t size = spl.size();
  uint32_t used = alc.used();
  EXPECT_GT(used, 0u);
  
  spl.releaseValues();
  
  EXPECT_LT(spl.size(), size);
  EXPECT_LE(alc.used(), used);
}

TEST(StringPool, ProvideStress)
{
  StringPool<> spl;
  
  bool found = false;
  constexpr unsigned int size = 100;
  std::array<std::string, size> arr;
  for (unsigned int i=0; i<arr.size(); i+=2)
  {
    auto& v = arr[i];
    v = std::to_string(i);
    spl.provide(v.c_str(), false, found);
  }
  EXPECT_EQ(spl.size(), size/2);
  for (int i=(int)arr.size()-1; i>=0; i-=2)
  {
    auto& v = arr[i];
    v = std::to_string(i);
    spl.provide(v.c_str(), true, found);
  }
  EXPECT_EQ(spl.size(), size);
  for (const auto& v : arr)
  {
    auto js = spl.provide(v.c_str(), false, found);
    EXPECT_EQ(v.c_str(), js->c_str());
  }
  EXPECT_EQ(spl.size(), size);
  EXPECT_LT(spl.load_factor(), spl.max_load_factor());
  EXPECT_GT(spl.max_load_factor(), 0.f);
}

TEST(Allocators, StringPoolAllocator)
{
  StringPoolAllocator<64> spa;
  
  EXPECT_EQ(spa.chunksCount(),    0u);
  EXPECT_EQ(spa.chunksCapacity(), 0u);
  EXPECT_EQ(spa.countFallbacks(), 0u);
  
  JString* js0 = (JString*)spa.toPtr(spa.allocateAlt(32u));
  EXPECT_EQ(spa.chunksCount(),    1u);
  EXPECT_EQ(spa.chunksCapacity(), 1u);
  EXPECT_EQ(spa.countFallbacks(), 0u);
  
  JString* js1 = (JString*)spa.toPtr(spa.allocateAlt(128u));
  EXPECT_EQ(spa.chunksCount(),    1u);
  EXPECT_EQ(spa.chunksCapacity(), 1u);
  EXPECT_EQ(spa.countFallbacks(), 1u);
  EXPECT_NE(js0, js1);
  
  JString* js2 = (JString*)spa.toPtr(spa.allocateAlt(32u));
  EXPECT_EQ(spa.chunksCount(),    1u);
  EXPECT_EQ(spa.chunksCapacity(), 1u);
  EXPECT_EQ(spa.countFallbacks(), 1u);
  EXPECT_NE(js0, js2);
  EXPECT_NE(js1, js2);
  
  JString* js3 = (JString*)spa.toPtr(spa.allocateAlt(14u));
  EXPECT_EQ(spa.chunksCount(),    2u);
  EXPECT_EQ(spa.chunksCapacity(), 2u);
  EXPECT_EQ(spa.countFallbacks(), 1u);
  EXPECT_NE(js0, js3);
  
  JString* js4 = (JString*)spa.toPtr(spa.allocateAlt(50u));
  EXPECT_EQ(spa.chunksCount(),    3u);
  EXPECT_EQ(spa.chunksCapacity(), 3u);
  EXPECT_EQ(spa.countFallbacks(), 1u);
  EXPECT_NE(js3, js4);
  
  JString* js5 = (JString*)spa.toPtr(spa.allocateAlt(65u));
  EXPECT_EQ(spa.chunksCount(),    3u);
  EXPECT_EQ(spa.chunksCapacity(), 3u);
  EXPECT_EQ(spa.countFallbacks(), 2u);
  EXPECT_NE(js1, js5);
  
  JString* js6 = (JString*)spa.toPtr(spa.allocateAlt(64u));
  EXPECT_EQ(spa.chunksCount(),    4u);
  EXPECT_EQ(spa.chunksCapacity(), 5u);
  EXPECT_EQ(spa.countFallbacks(), 2u);
  EXPECT_NE(js4, js6);
  
  JString* js7 = (JString*)spa.toPtr(spa.allocateAlt(1u));
  EXPECT_EQ(spa.chunksCount(),    4u);
  EXPECT_EQ(spa.chunksCapacity(), 5u);
  EXPECT_EQ(spa.countFallbacks(), 2u);
  EXPECT_NE(js6, js7);
}

TEST(Allocators, ObjectPoolAllocator)
{
  {
    ObjectPoolAllocator<64, StackAllocator<256, 8>, true> opa;
    const auto& alc = opa.callocator();
    
    EXPECT_EQ(opa.chunksCount(),    0u);
    EXPECT_EQ(opa.chunksCapacity(), 0u);
    EXPECT_EQ(opa.countFallbacks(), 0u);
    EXPECT_EQ(alc.used(),           0u);
  }
  {
    ObjectPoolAllocator<64, StackAllocator<256, 8>, true> opa;
    const auto& alc = opa.callocator();
    
    void* m0 = opa.allocate(31u);
    EXPECT_EQ(opa.chunksCount(),    1u);
    EXPECT_EQ(opa.chunksCapacity(), 1u);
    EXPECT_EQ(alc.used(),          80u);
    
    void* m1 = opa.allocate(32u);
    EXPECT_EQ(opa.chunksCount(),  1u);
    EXPECT_EQ(alc.used(),        80u);
    EXPECT_NE(m0, m1);
    
    opa.deallocate(m0, 31u);
    EXPECT_EQ(opa.totalDead(), 32u);
    EXPECT_EQ(alc.used(),      80u);
    
    opa.deallocate(m1, 32u);
    EXPECT_EQ(opa.totalDead(),  0u);
    EXPECT_EQ(alc.used(),      80u);
  }
  {
    ObjectPoolAllocator<64, HeapAllocator, true> opa;
    const auto& alc = opa.callocator();
    
    // Depends on data size/alignment
  #ifdef LFJ_64BIT
    uint32_t expected[] = { 80u, 160u };
  #else
    uint32_t expected[] = { 76u, 152u };
  #endif
    
    void* m0 = opa.allocate(32u);
    EXPECT_EQ(opa.chunksCount(),  1u);
    EXPECT_EQ(alc.getAllocated(), expected[0]);
    
    void* m1 = opa.allocate(31u);
    EXPECT_EQ(opa.chunksCount(),  1u);
    EXPECT_EQ(alc.getAllocated(), expected[0]);
    EXPECT_NE(m0, m1);
    
    void* m2 = opa.allocate(15u);
    EXPECT_EQ(opa.chunksCount(),  2u);
    EXPECT_EQ(alc.getAllocated(), expected[1]);
    EXPECT_NE(m0, m2);
    
    opa.deallocate(m1, 31u);
    EXPECT_EQ(opa.totalDead(),    0u);
    EXPECT_EQ(alc.getAllocated(), expected[1]);
    
    opa.deallocate(m2, 15u);
    EXPECT_EQ(opa.totalDead(),    0u);
    EXPECT_EQ(alc.getAllocated(), expected[1]);
  }
  {
    ObjectPoolAllocator<64, HeapAllocator, true> opa;
    const auto& alc = opa.callocator();
    
    // Depends on data size/alignment
  #ifdef LFJ_64BIT
    uint32_t expected[] = { 80u, 160u };
  #else
    uint32_t expected[] = { 76u, 152u };
  #endif
    
    void* m0 = opa.allocate(16u);
    void* m1 = opa.allocate(15u);
    void* m2 = opa.allocate(32u);
    EXPECT_EQ(opa.chunksCount(), 1u);
    EXPECT_EQ(alc.getAllocated(), expected[0]);
    EXPECT_NE(m0, m1);
    EXPECT_NE(m1, m2);
    
    void* m3 = opa.allocate(32u);
    void* m4 = opa.allocate(30u);
    EXPECT_EQ(opa.chunksCount(), 2u);
    EXPECT_EQ(alc.getAllocated(), expected[1]);
    EXPECT_NE(m2, m3);
    EXPECT_NE(m3, m4);
    
    opa.deallocate(m1, 15u);
    opa.deallocate(m3, 32u);
    EXPECT_EQ(opa.totalDead(), 48u);
    EXPECT_EQ(alc.getAllocated(), expected[1]);
    
    void* m3_ = opa.allocate(32u);
    void* m1_ = opa.allocate(15u);
    EXPECT_EQ(opa.totalDead(), 0u);
    EXPECT_EQ(alc.getAllocated(), expected[1]);
    EXPECT_EQ(m1, m1_);
    EXPECT_EQ(m3, m3_);
  }
}

TEST(Allocators, AllocatorExtract)
{
  char stack[1024];
  const char* s7  = "hello !";
  const char* s15 = "hello this is !";
  
  ObjectPoolAllocator<64, HeapAllocator, true> opa;
  EXPECT_EQ(opa.countDirectAvailable(), 0u);
  
  // Pop
  char* val0 = (char*)opa.allocate(16);
  std::memcpy(val0, s15, 16);
  EXPECT_STREQ(val0, s15);
  EXPECT_EQ(opa.countAllocated(), 16u);
  
  char* val1 = (char*)opa.allocate(8);
  std::memcpy(val1, s7, 8);
  EXPECT_STREQ(val1, s7);
  EXPECT_EQ(opa.countAllocated(), 24u);
  
  EXPECT_TRUE(opa.memPop(val1, stack, 8, 8));
  EXPECT_STREQ(val1, s7);
  EXPECT_EQ(opa.countAllocated(), 16u);
  
  EXPECT_TRUE(opa.memPop(val0, stack + 8, 16, 16));
  EXPECT_STREQ(val0, s15);
  EXPECT_EQ(opa.countAllocated(), 0u);
  
  char* val2 = (char*)opa.allocate(100);
  EXPECT_EQ(opa.countAllocated(), 100u);
  EXPECT_FALSE(opa.memPop(val2, stack, 100, 100));
  opa.deallocate(val2, 100);
  EXPECT_EQ(opa.countAllocated(), 0u);
  
  // Push
  char* val3 = (char*)opa.memPush((void*)s15, 16);
  EXPECT_EQ(opa.countAllocated(), 16u);
  EXPECT_STREQ(val3, s15);
  
  char* val4 = (char*)opa.memPush((void*)s7, 8);
  EXPECT_EQ(opa.countAllocated(), 24u);
  EXPECT_STREQ(val4, s7);
}

TEST(Allocators, StaticAllocator)
{
  StackAllocator<64, 8> sa;
  
  EXPECT_EQ(sa.total_dead(),   0);
  EXPECT_EQ(sa.first_avail(),  0);
  EXPECT_EQ(sa.used(),        0u);
  EXPECT_EQ(sa.available(),  64u);
  
  char* p0 = sa.allocate(10);
  EXPECT_EQ(sa.total_dead(),   0);
  EXPECT_EQ(sa.first_avail(), 16);
  EXPECT_EQ(sa.used(),       16u);
  
  char* p1 = sa.allocate(20);
  EXPECT_EQ(sa.total_dead(),   0);
  EXPECT_EQ(sa.first_avail(), 40);
  EXPECT_EQ(sa.used(),       40u);
  
  sa.deallocate(p0, 10);
  EXPECT_EQ(sa.total_dead(),  16);
  EXPECT_EQ(sa.first_avail(), 40);
  EXPECT_EQ(sa.used(),       24u);
  
  char* p2 = sa.allocate(10);
  EXPECT_EQ(sa.total_dead(),  16);
  EXPECT_EQ(sa.first_avail(), 56);
  EXPECT_EQ(sa.used(),       40u);
  EXPECT_NE(p2, p0);
  
  EXPECT_THROW(sa.allocate(20), std::bad_alloc);
  
  char* p3 = sa.allocate(10);
  EXPECT_EQ(sa.total_dead(),   0);
  EXPECT_EQ(sa.first_avail(), 56);
  EXPECT_EQ(sa.used(),       56u);
  EXPECT_EQ(p3, p0);
  
  sa.deallocate(p2, 10);
  EXPECT_EQ(sa.total_dead(),  0);
  EXPECT_EQ(sa.first_avail(), 40);
  EXPECT_EQ(sa.used(),       40u);
  
  sa.deallocate(p3, 10);
  EXPECT_EQ(sa.total_dead(),  16);
  EXPECT_EQ(sa.first_avail(), 40);
  EXPECT_EQ(sa.used(),       24u);
  
  sa.deallocate(p1, 20);
  EXPECT_EQ(sa.total_dead(),   0);
  EXPECT_EQ(sa.first_avail(),  0);
  EXPECT_EQ(sa.used(),        0u);
  EXPECT_EQ(sa.available(),  64u);
  
  char* p4 = sa.allocate(64);
  EXPECT_EQ(sa.total_dead(),   0);
  EXPECT_EQ(sa.first_avail(), 64);
  EXPECT_EQ(sa.used(),       64u);
  EXPECT_EQ(sa.available(),   0u);
  EXPECT_EQ(p4, p0);
}

TEST(Allocators, StaticStringPool)
{
  {
    StringPoolAllocator<64, StackAllocator<128>> spa;
    const auto& alc = spa.callocator();
    
    EXPECT_EQ(alc.used(),         0u);
    EXPECT_EQ(alc.available(),  128u);
  }
  {
    StringPoolAllocator<32, StackAllocator<256, 8>> spa;
    const auto& alc = spa.callocator();
    
    JString* js0 = (JString*)spa.toPtr(spa.allocateAlt(8u));
    EXPECT_EQ(alc.used(),        48u);  // First chunk (item + data)
    EXPECT_EQ(alc.available(),  208u);
    
    JString* js1 = (JString*)spa.toPtr(spa.allocateAlt(16u));
    EXPECT_EQ(alc.used(),        48u);  // Same chunk
    EXPECT_NE(js1, js0);
    
    // Depends on data size/alignment
  #ifdef LFJ_64BIT
    uint32_t expected[] = { 96u, 168u,  88u};
  #else
    uint32_t expected[] = { 88u, 152u, 104u};
  #endif
    
    JString* js2 = (JString*)spa.toPtr(spa.allocateAlt(32u));
    EXPECT_EQ(alc.used(),      expected[0]);  // New chunk
    EXPECT_NE(js2, js1);
    
    JString* js3 = (JString*)spa.toPtr(spa.allocateAlt(50u));
    EXPECT_EQ(alc.used(),      expected[1]);  // New fallback
    EXPECT_EQ(alc.available(), expected[2]);
    EXPECT_NE(js3, js2);
  }
  {
    constexpr uint16_t ChunkSize   = 64;
    constexpr uint32_t ChunkSizeof = sizeof(char*) + 4 * sizeof(uint16_t);
    
    StringPool<ChunkSize, HeapAllocator> spl(4);
    const auto& alc = spl.callocator();
    
    uint32_t size = ChunkSizeof + ChunkSize;  // First chunk (item + data)
    EXPECT_EQ(alc.getAllocated(), size);
    
    bool found = false;
    const JString* js0 = spl.provide("hello", true, found);  // extern
    EXPECT_EQ(js0->owns(), false);
    EXPECT_EQ(js0->len(), 5u);
    EXPECT_EQ(alc.getAllocated(), size);  // Same chunk
    
    const JString* js1 = spl.provide((char*)"hello", false, found); // interned
    EXPECT_EQ(js1->owns(), false);
    EXPECT_EQ(js1->len(), 5u);
    EXPECT_EQ(js1, js0);
    EXPECT_EQ(alc.getAllocated(), size);  // Same string
    
    const JString* js2 = spl.provide((char*)"This string is long enough to need another chunk!", true, found);  // interned
    EXPECT_EQ(js2->owns(), true);
    EXPECT_EQ(js2->len(), 49u);
    EXPECT_NE(js2, js0);
    size += ChunkSizeof + ChunkSize;
    EXPECT_EQ(alc.getAllocated(), size);
  }
}

TEST(Document, Construct)
{
  {
    DynamicDocument doc;
  }
  {
    CustomDocument<StdAllocator> doc;
  }
  {
    CustomDocument<HeapAllocator> doc;
  }
  {
    constexpr uint16_t ChunkSize = 256;
    typedef StackAllocator<1024, 8> Allocator;
    
    auto ssp = Document<ChunkSize, Allocator>::makeSharedStringPool();
    Document<ChunkSize, Allocator> doc(ssp);
  }
  {
    constexpr uint16_t StringChunkSize = 512;
    constexpr uint16_t ObjectChunkSize = 256;
    typedef StackAllocator<4096, 8> Allocator;
    
    auto ssp = std::make_shared< StringPool<StringChunkSize, Allocator> >();
    Document<StringChunkSize, Allocator, ObjectChunkSize> doc(ssp);
  }
}

TEST(Document, Serialize_Create)
{
  {
    DynamicDocument doc;
    auto rt = doc.root();
    EXPECT_TRUE(rt.isNul());
  }
  {
    DynamicDocument doc;
    auto rt = doc.root();
    
    auto a0 = rt[0];
    EXPECT_TRUE(a0.isNul());
    EXPECT_TRUE(rt.isArray());
    
    EXPECT_EQ(rt.arraySize(), 1u);
    EXPECT_EQ(rt.arrayCapacity(), 1u);
    
    auto a1 = rt[1];
    EXPECT_TRUE(a1.isNul());
    EXPECT_EQ(rt.arraySize(), 2u);
    EXPECT_EQ(rt.arrayCapacity(), 2u);
    
    auto a10 = a1["string too long for SSO"];
    EXPECT_TRUE(a1.isObject());
    EXPECT_TRUE(a10.isNul());
  }
  {
    DynamicDocument doc;
    auto rt = doc.root();
    
    auto o0 = rt["hello"];
    EXPECT_TRUE(o0.isNul());
    EXPECT_TRUE(rt.isObject());
    
    EXPECT_EQ(rt.objectSize(), 1u);
    EXPECT_EQ(rt.objectCapacity(), 1u);
    
    auto o1 = rt["world"];
    EXPECT_TRUE(o1.isNul());
    EXPECT_EQ(rt.objectSize(), 2u);
    EXPECT_EQ(rt.objectCapacity(), 2u);
    
    auto o10 = o1[0];
    EXPECT_TRUE(o1.isArray());
    EXPECT_TRUE(o10.isNul());
  }
}

TEST(Document, Serialize_Assign)
{
  {
    DynamicDocument doc;
    auto rt = doc.root();
    auto o0 = rt["hello"];
    
    o0 = true;
    EXPECT_TRUE(o0.isMetaBool());
    EXPECT_TRUE(o0.isTrue());
    
    o0 = false;
    EXPECT_TRUE(o0.isMetaBool());
    EXPECT_TRUE(o0.isFalse());
  }
  {
    DynamicDocument doc;
    auto rt = doc.root();
    auto a0 = rt[0];
    
    a0 = -1;
    EXPECT_TRUE(a0.isMetaNumber());
    EXPECT_TRUE(a0.isInt64());
    
    a0 = 1u;
    EXPECT_TRUE(a0.isMetaNumber());
    EXPECT_TRUE(a0.isUInt64());
    
    rt[1] = 0.5;
    EXPECT_TRUE(rt[1].isMetaNumber());
    EXPECT_TRUE(rt[1].isDouble());
  }
  {
    DynamicDocument doc;
    auto rt = doc.root();
    auto o0 = rt["hello"];
    
    o0 = "hi";
    EXPECT_TRUE(o0.isMetaString());
    EXPECT_TRUE(o0.isShortString());
    
    o0 = "hello everybody, how are you today?";
    EXPECT_TRUE(o0.isMetaString());
    EXPECT_TRUE(o0.isLongString());
  }
  {
    DynamicDocument doc;
    auto rt = doc.root();
    
    auto it0 = rt["hello"][0]["world"];
    EXPECT_TRUE(it0.isNul());
    
    auto it1 = rt["hello"][1][0];
    EXPECT_TRUE(it1.isNul());
    
    it0 = 10;
    EXPECT_TRUE(it0.isInt64());
    
    it1 = "hi";
    EXPECT_TRUE(it1.isShortString());
  }
}

TEST(Document, Serialize_Modify)
{
  {
    Document<0, StackAllocator<1024, 8>> doc;
    
    uint32_t used = 0u;
    auto spa = doc.stringPool();
    const auto& alc = spa->callocator();
    EXPECT_EQ(alc.used(), 0u);
    
    auto rt = doc.root();
    EXPECT_EQ(alc.used(), 0u);
    
    /*auto a0 =*/ rt[0];
    EXPECT_GT(alc.used(), used);
    used = alc.used();
    
    /*auto a1 =*/ rt[1];
    EXPECT_GT(alc.used(), used);
    
    rt.toNull();
    EXPECT_EQ(alc.used(), 0u);
    used = alc.used();
    
    rt["abc"] = true;
    auto o0 = rt["abc"];
    EXPECT_GT(alc.used(), used);
    used = alc.used();
    
    rt["abc"] = 0.5;
    EXPECT_EQ(alc.used(), used);
    
    o0 = nullptr;
    EXPECT_EQ(alc.used(), used);
    
    o0 = "xyz";  // short-string
    EXPECT_EQ(alc.used(), used);

    o0 = "This is a long string avoiding SSO"; // long-string
    EXPECT_GT(alc.used(), used);
    used = alc.used();
    
    o0 = nullptr;
    EXPECT_EQ(alc.used(), used);  // string kept in pool
  }
  {
    DynamicDocument doc;
    auto rt = doc.root();
    auto o0 = rt["abc"];
    
    auto o00 = o0[0];
    EXPECT_TRUE(o00.isNul());
    
    o00.toObject();
    EXPECT_TRUE(o00.isObject());
    EXPECT_TRUE(o00.objectEmpty());
    
    o00.toNull();
    EXPECT_TRUE(o00.isNul());
    
    o00.toArray();
    EXPECT_TRUE(o00.isArray());
    EXPECT_TRUE(o00.arrayEmpty());
  }
  {
    DynamicDocument doc;
    auto rt = doc.root();
    
    rt["abc"] = 0;
    rt["abc"] = "hi";
    EXPECT_TRUE(rt["abc"].isShortString());
    
    rt["def"] = 10;
    auto mb = rt.objectFindMember("def");
    EXPECT_NE(mb, nullptr);
    auto vl = rt.objectFindValue("def");
    EXPECT_NE(vl, nullptr);
    
    DynamicDocument::RefMember rm(doc, *mb);
    rm.setKey("fed");
    
    auto mb2 = rt.objectFindMember("def");
    EXPECT_EQ(mb2, nullptr);
    auto vl2 = rt.objectFindValue("def");
    EXPECT_EQ(vl2, nullptr);
    
    auto mb3 = rt.objectFindMember("fed");
    EXPECT_NE(mb3, nullptr);
    auto vl3 = rt.objectFindValue("fed");
    EXPECT_NE(vl3, nullptr);
    EXPECT_TRUE(vl3->isInt64());
  }
  {
    DynamicDocument doc;
    auto rt = doc.root();
    auto ar = rt.toArray();
    
    std::vector<JType> types;
    
    ar.arrayPushBack(true);
    types.push_back(JType::TRUE);
    ar.arrayPushBack(-1);
    types.push_back(JType::INT64);
    ar.arrayPushBack(2u);
    types.push_back(JType::UINT64);
    ar.arrayPushBack(0.5);
    types.push_back(JType::DOUBLE);
    ar.arrayPushBack(nullptr);
    types.push_back(JType::NUL);
    ar.arrayPushBack("short");
    types.push_back(JType::SSTRING);
    ar.arrayPushBack("long_string_is_quite_long");
    types.push_back(JType::LSTRING);
    EXPECT_EQ(ar.arraySize(), 7u);
    EXPECT_GE(ar.arrayCapacity(), 7u);
    
    int idx = 0;
    for (auto it = ar.arrayCBegin(); it < ar.arrayCEnd(); ++it)
      EXPECT_EQ(it->type(), types[idx++]);
  }
  {
    DynamicDocument doc;
    auto rt = doc.root();
    auto ob = rt.toObject();
    
    int idx = 0;
    std::vector<JType> types;
    std::vector<std::string> keys;
    keys.reserve(7u);
    
    keys.push_back("k1");
    types.push_back(JType::TRUE);
    ob.objectPushBack(keys[idx++].c_str(), true);
    
    keys.push_back("k2");
    types.push_back(JType::INT64);
    ob.objectPushBack(keys[idx++].c_str(), -1);
    
    keys.push_back("k3");
    types.push_back(JType::UINT64);
    ob.objectPushBack(keys[idx++].c_str(), 2u);
    
    keys.push_back("k4");
    types.push_back(JType::DOUBLE);
    ob.objectPushBack(keys[idx++].c_str(), 0.5);
    
    keys.push_back("k5");
    types.push_back(JType::NUL);
    ob.objectPushBack(keys[idx++].c_str(), nullptr);
    
    keys.push_back("k6");
    types.push_back(JType::SSTRING);
    ob.objectPushBack(keys[idx++].c_str(), "short");
    
    keys.push_back("k7");
    types.push_back(JType::LSTRING);
    ob.objectPushBack(keys[idx++].c_str(), "long_string_is_quite_long");
    
    EXPECT_EQ(ob.objectSize(), 7u);
    EXPECT_GE(ob.objectCapacity(), 7u);
    
    idx = 0;
    for (auto it = ob.objectCBegin(); it < ob.objectCEnd(); ++it)
    {
      EXPECT_EQ(it->key(), keys[idx]);
      EXPECT_EQ(it->value().type(), types[idx++]);
    }
  }
  {
    DynamicDocument doc;
    auto rt = doc.root();
    auto ar = rt.toArray();
    
    ar.arrayPushBack(true);
    ar.arrayPushBack(1);
    ar.arrayPushBack("hi");
    ar[3].toArray();
    ar[3].arrayPushBack(-1);
    ar[3].arrayPushBack(-2);
    ar[4].toObject();
    ar[4].objectPushBack("hello", "world");
    EXPECT_EQ(ar.arraySize(), 5u);
    
    auto a0 = ar.arrayValue(0u);
    auto a1 = ar.arrayValue(1u);
    auto a2 = ar.arrayValue(2u);
    auto a3 = ar.arrayValue(3u);
    auto a4 = ar.arrayValue(4u);
    EXPECT_EQ(a0.type(), JType::TRUE);
    EXPECT_EQ(a1.type(), JType::INT64);
    EXPECT_EQ(a2.type(), JType::SSTRING);
    EXPECT_EQ(a3.type(), JType::ARRAY);
    EXPECT_EQ(a4.type(), JType::OBJECT);
    
    a0.swap(a1);
    EXPECT_EQ(a0.type(), JType::INT64);
    EXPECT_EQ(a1.type(), JType::TRUE);
    EXPECT_EQ(a2.type(), JType::SSTRING);
    EXPECT_EQ(a3.type(), JType::ARRAY);
    EXPECT_EQ(a4.type(), JType::OBJECT);
    
    a3.swap(a2);
    EXPECT_EQ(a0.type(), JType::INT64);
    EXPECT_EQ(a1.type(), JType::TRUE);
    EXPECT_EQ(a2.type(), JType::ARRAY);
    EXPECT_EQ(a3.type(), JType::SSTRING);
    EXPECT_EQ(a4.type(), JType::OBJECT);
    EXPECT_EQ(a2.arraySize(), 2u);
    EXPECT_STREQ(a3.getShortString(), "hi");
    
    a2.swap(a4);
    EXPECT_EQ(a0.type(), JType::INT64);
    EXPECT_EQ(a1.type(), JType::TRUE);
    EXPECT_EQ(a2.type(), JType::OBJECT);
    EXPECT_EQ(a3.type(), JType::SSTRING);
    EXPECT_EQ(a4.type(), JType::ARRAY);
    EXPECT_EQ(a4.arraySize(), 2u);
    EXPECT_STREQ(a2.objectMember(0u).key(), "hello");
    EXPECT_STREQ(a2.objectMember(0u).value().getShortString(), "world");
  }
}

TEST(Document, Serialize_Storage)
{
  {
    DynamicDocument doc;
    auto rt = doc.root();
    auto ar = rt.toArray();
    
    EXPECT_EQ(ar.arraySize(), 0u);
    EXPECT_EQ(ar.arrayCapacity(), 0u);
    
    for (int i = 0; i < 5; ++i)
      ar.arrayPushBack(i);
    
    EXPECT_EQ(ar.arraySize(), 5u);
    uint32_t capa = ar.arrayCapacity();
    EXPECT_GE(capa, 5u);
    
    ar.arrayPopBack();
    EXPECT_EQ(ar.arraySize(), 4u);
    EXPECT_EQ(ar.arrayCapacity(), capa);
    
    ar.arrayClear();
    EXPECT_TRUE(ar.arrayEmpty());
    capa = ar.arrayCapacity();
    EXPECT_GT(capa, 0u);
    
    ar.arrayReserve(capa + 3u);
    EXPECT_EQ(ar.arrayCapacity(), capa + 3u);
    
    for (int i = 0; i < 5; ++i)
      ar.arrayPushBack(i);
    
    EXPECT_EQ(ar.arraySize(), 5u);
    EXPECT_GT(ar.arrayCapacity(), 5u);
    ar.arrayShrink();
    EXPECT_EQ(ar.arraySize(), ar.arrayCapacity());
    
    ar.arrayClear();
    ar.arrayShrink();
    EXPECT_EQ(ar.arrayCapacity(), 0u);
    
    // medium array (too large for chunk)
    uint32_t medium = 5000u;
    ar.arrayReserve(medium + 10u);
    for (uint32_t i = 0; i < medium; ++i)
      ar.arrayPushBack(i);
    
    EXPECT_EQ(ar.arraySize(), medium);
    EXPECT_EQ(ar.arrayCapacity(), medium + 10u);
    
    ar.arrayShrink();
    EXPECT_EQ(ar.arraySize(), medium);
    EXPECT_EQ(ar.arrayCapacity(), medium);
    
    // big array
    uint32_t big = 70000u;
    ar.arrayClear();
    ar.arrayReserve(big + 10u);
    for (uint32_t i = 0; i < big; ++i)
      ar.arrayPushBack(i);
    
    EXPECT_EQ(ar.arraySize(), big);
    EXPECT_EQ(ar.arrayCapacity(), big + 10u);
    
    ar.arrayShrink();
    EXPECT_EQ(ar.arraySize(), big);
    EXPECT_EQ(ar.arrayCapacity(), big);
    
    ar.arrayClear();
    ar.arrayPushBack(1);
    ar.arrayShrink();
    EXPECT_EQ(ar.arraySize(), 1u);
    EXPECT_EQ(ar.arrayCapacity(), 1u);
    
    ar[1]["0"] = -1;
    ar.arrayPushBack(3);
    ar.arrayPushBack(4);
    EXPECT_EQ(ar.arraySize(), 4u);
    EXPECT_EQ(ar[1]["0"].getInt64(), -1);
    ar.arrayErase(ar.arrayCBegin() + 1);
    EXPECT_EQ(ar.arraySize(), 3u);
    EXPECT_EQ(ar.arrayValueAt(0).getInt64(), 1);
    EXPECT_EQ(ar.arrayValueAt(1).getInt64(), 3);
    EXPECT_EQ(ar.arrayValueAt(2).getInt64(), 4);
  }
  {
    DynamicDocument doc;
    auto rt = doc.root();
    auto ob = rt.toObject();
    
    EXPECT_EQ(ob.objectSize(), 0u);
    EXPECT_EQ(ob.objectCapacity(), 0u);
    
    for (int i = 0; i < 5; ++i)
      ob.objectPushBack((char*)std::to_string(i).c_str(), i);
    
    EXPECT_EQ(ob.objectSize(), 5u);
    uint32_t capa = ob.objectCapacity();
    EXPECT_GE(ob.objectCapacity(), 5u);
    
    ob.objectPopBack();
    EXPECT_EQ(ob.objectSize(), 4u);
    EXPECT_EQ(ob.objectCapacity(), capa);
    
    ob.objectClear();
    EXPECT_TRUE(ob.objectEmpty());
    capa = ob.objectCapacity();
    EXPECT_GT(capa, 0u);
    
    ob.objectReserve(capa + 3u);
    EXPECT_EQ(ob.objectCapacity(), capa + 3u);
    
    for (int i = 0; i < 5; ++i)
      ob.objectPushBack((char*)std::to_string(i).c_str(), i);
    
    EXPECT_EQ(ob.objectSize(), 5u);
    EXPECT_GT(ob.objectCapacity(), 5u);
    ob.objectShrink();
    EXPECT_EQ(ob.objectSize(), ob.objectCapacity());
    
    ob.objectClear();
    ob.objectShrink();
    EXPECT_EQ(ob.objectCapacity(), 0u);
    
    // medium object (too large for chunk)
    uint32_t medium = 5000u;
    ob.objectReserve(medium + 10u);
    for (uint32_t i = 0; i < medium; ++i)
      ob.objectPushBack((char*)std::to_string(i).c_str(), i);
    
    EXPECT_EQ(ob.objectSize(), medium);
    EXPECT_EQ(ob.objectCapacity(), medium + 10u);
    
    ob.objectShrink();
    EXPECT_EQ(ob.objectSize(), medium);
    EXPECT_EQ(ob.objectCapacity(), medium);
    
    // big object
    uint32_t big = 70000u;
    ob.objectClear();
    ob.objectReserve(big + 10u);
    for (uint32_t i = 0; i < big; ++i)
      ob.objectPushBack((char*)std::to_string(i).c_str(), i);
    
    EXPECT_EQ(ob.objectSize(), big);
    EXPECT_EQ(ob.objectCapacity(), big + 10u);
    
    ob.objectShrink();
    EXPECT_EQ(ob.objectSize(), big);
    EXPECT_EQ(ob.objectCapacity(), big);
    
    ob.objectClear();
    ob.objectPushBack("hi", 1);
    ob.objectShrink();
    EXPECT_EQ(ob.objectSize(), 1u);
    EXPECT_EQ(ob.objectCapacity(), 1u);
    
    ob["1"]["0"] = -1;
    ob.objectPushBack("2", 3);
    ob.objectPushBack("3", 4);
    EXPECT_EQ(ob.objectSize(), 4u);
    EXPECT_EQ(ob["1"]["0"].getInt64(), -1);
    ob.objectErase(ob.objectCBegin() + 1);
    EXPECT_EQ(ob.objectSize(), 3u);
    EXPECT_EQ(ob.objectMemberAt(0).value().getInt64(), 1);
    EXPECT_EQ(ob.objectMemberAt(1).value().getInt64(), 3);
    EXPECT_EQ(ob.objectMemberAt(2).value().getInt64(), 4);
  }
}

TEST(Document, Deserialize_Iterate)
{
  DynamicDocument doc;
  
  auto rt = doc.root();
  /*auto a0 =*/ rt["abc"];
  auto a10 = rt["def"][0]; // invalidate a0
  
  auto am0 = rt.objectFindMember("abc");
  EXPECT_NE(am0, nullptr);
  auto a0 = rt["abc"];
  auto o00 = a0[0];
  EXPECT_TRUE(a10.isNul());
  EXPECT_TRUE(o00.isNul());
  
  for (int i = 0; i < 10; ++i)
    a0[i];
  
  for (auto it = rt.objectCBegin(); it < rt.objectCEnd(); ++it)
  {
    EXPECT_GT(it->keyLen(), 0u);
    EXPECT_FALSE(it->keyOwned());
    EXPECT_EQ(it->value().type(), JType::ARRAY);
  }
  
  for (auto it = a0.arrayCBegin(); it < a0.arrayCEnd(); ++it)
    EXPECT_TRUE(it->isNul());
  
  EXPECT_EQ(a0.arrayCBegin()->type(), JType::NUL);
  
  auto a00 = a0.arrayValueAt(0);
  for (int i = 0; i < 20; ++i)
    a00[i];
  
  EXPECT_EQ(a0.arraySize(), 10u);
    
  auto ca0 = a0.arrayCBegin();
  EXPECT_TRUE(ca0->isArray());
  EXPECT_EQ(ca0->arraySize(), 20u);
  
  auto it = ca0->arrayValues();
  auto end = it + ca0->arraySize();
  int count = 0;
  for (; it < end; ++it)
  {
    EXPECT_TRUE(it->isNul());
    ++count;
  }
  EXPECT_EQ(count, 20);
  
  auto& aa0 = a0.arrayCValueAt(0);
  auto av0 = aa0.arrayValues();
  for (uint32_t i = 0; i < a0.arraySize(); ++i)
    EXPECT_EQ(av0[i].type(), JType::NUL);
  
  uint32_t oSize = rt.objectSize();
  EXPECT_EQ(oSize, 2u);
  for (uint32_t i = 0; i < oSize; ++i)
  {
    auto& rtM = rt.objectCMemberAt(i);
    EXPECT_GT(rtM.keyLen(), 0u);
  }
  for (uint32_t i = 0; i < oSize; ++i)
  {
    auto rtM = rt.objectMemberAt(i);
    EXPECT_GT(rtM.keyLen(), 0u);
  }
}

TEST(Document, SpecializedArray)
{
  { // barray
    Document<0, StackAllocator<4096000, 8>> doc;
    const auto& opa = doc.objectAllocator();
    const auto& alc = opa.callocator();
    
    auto rt = doc.root();
    rt.toBArray();
    
    uint32_t used = alc.used();
    EXPECT_EQ(used, 0u);
    
    EXPECT_TRUE(rt.barrayEmpty());
    EXPECT_EQ(rt.barraySize(), 0u);
    EXPECT_EQ(rt.barrayCapacity(), 0u);
    
    rt.barrayPushBack(false);
    EXPECT_FALSE(rt.barrayEmpty());
    EXPECT_EQ(rt.barraySize(), 1u);
    EXPECT_EQ(rt.barrayCapacity(), 1u);
    EXPECT_EQ(*rt.barrayCBegin(), false);
    EXPECT_EQ(rt.barrayValue(0u), false);
    used = alc.used();
    EXPECT_GT(used, 0u);
    
    rt.barrayErase(rt.barrayCBegin());
    EXPECT_TRUE(rt.barrayEmpty());
    EXPECT_EQ(rt.barraySize(), 0u);
    EXPECT_EQ(rt.barrayCapacity(), 1u);
    
    rt.barrayShrink();
    EXPECT_TRUE(rt.barrayEmpty());
    EXPECT_EQ(rt.barraySize(), 0u);
    EXPECT_EQ(rt.barrayCapacity(), 0u);
    used = alc.used();
    EXPECT_EQ(used, 0u);
    
    rt.barrayReserve(3u);
    EXPECT_TRUE(rt.barrayEmpty());
    EXPECT_EQ(rt.barraySize(), 0u);
    EXPECT_EQ(rt.barrayCapacity(), 3u);
    EXPECT_GT(alc.used(), used);
    used = alc.used();
    
    rt.barrayPushBack(true);
    EXPECT_FALSE(rt.barrayEmpty());
    EXPECT_EQ(rt.barraySize(), 1u);
    EXPECT_EQ(rt.barrayCapacity(), 3u);
    EXPECT_EQ(rt.barrayCValue(0u), true);
    EXPECT_EQ(alc.used(), used);
    
    rt.barrayPushBack(false);
    rt.barrayValue(0u) = true;
    EXPECT_EQ(rt.barraySize(), 2u);
    EXPECT_EQ(rt.barrayCapacity(), 3u);
    EXPECT_EQ(rt.barrayCValue(0u), true);
    EXPECT_EQ(rt.barrayCValue(1u), false);
    
    rt.barrayPopBack();
    rt.barrayPopBack();
    EXPECT_TRUE(rt.barrayEmpty());
    EXPECT_EQ(rt.barraySize(), 0u);
    EXPECT_EQ(rt.barrayCapacity(), 3u);
    EXPECT_EQ(alc.used(), used);
    
    uint32_t big = 70000u;
    for (uint32_t i = 0; i < big; ++i)
      rt.barrayPushBack((bool)(i % 2));
    EXPECT_EQ(rt.barraySize(), big);
    EXPECT_GE(rt.barrayCapacity(), big);
    
    rt.barrayShrink();
    EXPECT_EQ(rt.barraySize(), rt.barrayCapacity());
    
    rt.barrayClear();
    EXPECT_EQ(rt.barraySize(), 0u);
    EXPECT_EQ(rt.barrayCapacity(), big);
    
    rt.barrayPushBack(true);
    rt.barrayShrink();
    EXPECT_EQ(rt.barrayCapacity(), 1u);
  }
  { // iarray
    Document<0, StackAllocator<4096000, 8>> doc;
    const auto& opa = doc.objectAllocator();
    const auto& alc = opa.callocator();
    
    auto rt = doc.root();
    rt.toIArray();
    
    uint32_t used = alc.used();
    EXPECT_EQ(used, 0u);
    
    EXPECT_TRUE(rt.iarrayEmpty());
    EXPECT_EQ(rt.iarraySize(), 0u);
    EXPECT_EQ(rt.iarrayCapacity(), 0u);
    
    rt.iarrayPushBack(0);
    EXPECT_FALSE(rt.iarrayEmpty());
    EXPECT_EQ(rt.iarraySize(), 1u);
    EXPECT_EQ(rt.iarrayCapacity(), 1u);
    EXPECT_EQ(*rt.iarrayCBegin(), 0);
    EXPECT_EQ(rt.iarrayValue(0u), 0);
    used = alc.used();
    EXPECT_GT(used, 0u);
    
    rt.iarrayErase(rt.iarrayCBegin());
    EXPECT_TRUE(rt.iarrayEmpty());
    EXPECT_EQ(rt.iarraySize(), 0u);
    EXPECT_EQ(rt.iarrayCapacity(), 1u);
    
    rt.iarrayShrink();
    EXPECT_TRUE(rt.iarrayEmpty());
    EXPECT_EQ(rt.iarraySize(), 0u);
    EXPECT_EQ(rt.iarrayCapacity(), 0u);
    used = alc.used();
    EXPECT_EQ(used, 0u);
    
    rt.iarrayReserve(3u);
    EXPECT_TRUE(rt.iarrayEmpty());
    EXPECT_EQ(rt.iarraySize(), 0u);
    EXPECT_EQ(rt.iarrayCapacity(), 3u);
    EXPECT_GT(alc.used(), used);
    used = alc.used();
    
    rt.iarrayPushBack(1);
    EXPECT_FALSE(rt.iarrayEmpty());
    EXPECT_EQ(rt.iarraySize(), 1u);
    EXPECT_EQ(rt.iarrayCapacity(), 3u);
    EXPECT_EQ(rt.iarrayCValue(0u), 1);
    EXPECT_EQ(alc.used(), used);
    
    rt.iarrayPushBack(2);
    rt.iarrayValue(0u) = -1;
    EXPECT_EQ(rt.iarraySize(), 2u);
    EXPECT_EQ(rt.iarrayCapacity(), 3u);
    EXPECT_EQ(rt.iarrayCValue(0u), -1);
    EXPECT_EQ(rt.iarrayCValue(1u), 2);
    
    rt.iarrayPopBack();
    rt.iarrayPopBack();
    EXPECT_TRUE(rt.iarrayEmpty());
    EXPECT_EQ(rt.iarraySize(), 0u);
    EXPECT_EQ(rt.iarrayCapacity(), 3u);
    EXPECT_EQ(alc.used(), used);
    
    uint32_t big = 70000u;
    for (uint32_t i = 0; i < big; ++i)
      rt.iarrayPushBack((int64_t)i);
    EXPECT_EQ(rt.iarraySize(), big);
    EXPECT_GE(rt.iarrayCapacity(), big);
    
    rt.iarrayShrink();
    EXPECT_EQ(rt.iarraySize(), rt.iarrayCapacity());
    
    rt.iarrayClear();
    EXPECT_EQ(rt.iarraySize(), 0u);
    EXPECT_EQ(rt.iarrayCapacity(), big);
    
    rt.iarrayPushBack(0);
    rt.iarrayShrink();
    EXPECT_EQ(rt.iarrayCapacity(), 1u);
  }
  { // darray
    Document<0, StackAllocator<4096000, 8>> doc;
    const auto& opa = doc.objectAllocator();
    const auto& alc = opa.callocator();
    
    auto rt = doc.root();
    rt.toDArray();
    
    uint32_t used = alc.used();
    EXPECT_EQ(used, 0u);
    
    EXPECT_TRUE(rt.darrayEmpty());
    EXPECT_EQ(rt.darraySize(), 0u);
    EXPECT_EQ(rt.darrayCapacity(), 0u);
    
    rt.darrayPushBack(0.);
    EXPECT_FALSE(rt.darrayEmpty());
    EXPECT_EQ(rt.darraySize(), 1u);
    EXPECT_EQ(rt.darrayCapacity(), 1u);
    EXPECT_EQ(*rt.darrayCBegin(), 0.);
    EXPECT_EQ(rt.darrayValue(0u), 0.);
    used = alc.used();
    EXPECT_GT(used, 0u);
    
    rt.darrayErase(rt.darrayCBegin());
    EXPECT_TRUE(rt.darrayEmpty());
    EXPECT_EQ(rt.darraySize(), 0u);
    EXPECT_EQ(rt.darrayCapacity(), 1u);
    
    rt.darrayShrink();
    EXPECT_TRUE(rt.darrayEmpty());
    EXPECT_EQ(rt.darraySize(), 0u);
    EXPECT_EQ(rt.darrayCapacity(), 0u);
    used = alc.used();
    EXPECT_EQ(used, 0u);
    
    rt.darrayReserve(3u);
    EXPECT_TRUE(rt.darrayEmpty());
    EXPECT_EQ(rt.darraySize(), 0u);
    EXPECT_EQ(rt.darrayCapacity(), 3u);
    EXPECT_GT(alc.used(), used);
    used = alc.used();
    
    rt.darrayPushBack(1.);
    EXPECT_FALSE(rt.darrayEmpty());
    EXPECT_EQ(rt.darraySize(), 1u);
    EXPECT_EQ(rt.darrayCapacity(), 3u);
    EXPECT_EQ(rt.darrayCValue(0u), 1.);
    EXPECT_EQ(alc.used(), used);
    
    rt.darrayPushBack(2.2);
    rt.darrayValue(0u) = -1.5;
    EXPECT_EQ(rt.darraySize(), 2u);
    EXPECT_EQ(rt.darrayCapacity(), 3u);
    EXPECT_EQ(rt.darrayCValue(0u), -1.5);
    EXPECT_EQ(rt.darrayCValue(1u), 2.2);
    
    rt.darrayPopBack();
    rt.darrayPopBack();
    EXPECT_TRUE(rt.darrayEmpty());
    EXPECT_EQ(rt.darraySize(), 0u);
    EXPECT_EQ(rt.darrayCapacity(), 3u);
    EXPECT_EQ(alc.used(), used);
    
    uint32_t big = 70000u;
    for (uint32_t i = 0; i < big; ++i)
      rt.darrayPushBack((double)i);
    EXPECT_EQ(rt.darraySize(), big);
    EXPECT_GE(rt.darrayCapacity(), big);
    
    rt.darrayShrink();
    EXPECT_EQ(rt.darraySize(), rt.darrayCapacity());
    
    rt.darrayClear();
    EXPECT_EQ(rt.darraySize(), 0u);
    EXPECT_EQ(rt.darrayCapacity(), big);
    
    rt.darrayShrink();
    EXPECT_EQ(rt.darrayCapacity(), 0u);
  }
  { // convert
    DynamicDocument doc;
    
    auto rt = doc.root();
    rt.toIArray();
    EXPECT_EQ(rt.type(), JType::IARRAY);
    uint32_t capacity = rt.iarrayCapacity();
    
    rt.convertIArrayToDArray();
    EXPECT_EQ(rt.type(), JType::DARRAY);
    EXPECT_EQ(rt.darraySize(), 0u);
    EXPECT_EQ(rt.darrayCapacity(), capacity);
    
    rt.toIArray();
    int64_t mult = 1000000000;
    uint32_t size = 10u;
    for (uint32_t i = 0; i < size; ++i)
      rt.iarrayPushBack(i * mult);
    EXPECT_EQ(rt.type(), JType::IARRAY);
    EXPECT_EQ(rt.iarraySize(), size);
    rt.iarrayShrink();
    capacity = rt.iarrayCapacity();
    
    uint32_t extra = 3u;
    rt.convertIArrayToDArray(extra);
    EXPECT_EQ(rt.type(), JType::DARRAY);
    EXPECT_EQ(rt.darraySize(), size);
    EXPECT_EQ(rt.darrayCapacity(), capacity + extra);
    for (uint32_t i = 0; i < size; ++i)
      EXPECT_EQ(rt.darrayValueAt(i), (double)(i * mult));
    
    rt.convertDArrayToArray();
    EXPECT_EQ(rt.type(), JType::ARRAY);
    EXPECT_EQ(rt.arraySize(), size);
    EXPECT_EQ(rt.arrayCapacity(), capacity + extra);
    for (uint32_t i = 0; i < size; ++i)
    {
      EXPECT_TRUE(rt.arrayCValueAt(i).isDouble());
      EXPECT_EQ(rt.arrayCValueAt(i).getDouble(), (double)(i * mult));
    }
    
    rt.toIArray();
    for (uint32_t i = 0; i < size/2; ++i)
      rt.iarrayPushBack(i * mult);
    EXPECT_EQ(rt.type(), JType::IARRAY);
    EXPECT_EQ(rt.iarraySize(), size/2);
    rt.iarrayShrink();
    capacity = rt.iarrayCapacity();
    
    extra = 1u;
    rt.convertIArrayToArray(extra);
    EXPECT_EQ(rt.type(), JType::ARRAY);
    EXPECT_EQ(rt.arraySize(), size/2);
    EXPECT_EQ(rt.arrayCapacity(), capacity + extra);
    for (uint32_t i = 0; i < size/2; ++i)
    {
      EXPECT_TRUE(rt.arrayCValueAt(i).isInt64());
      EXPECT_EQ(rt.arrayCValueAt(i).getInt64(), (int64_t)(i * mult));
    }
    
    rt.toBArray();
    for (uint32_t i = 0; i < size/2; ++i)
      rt.barrayPushBack(i % 2);
    EXPECT_EQ(rt.type(), JType::BARRAY);
    EXPECT_EQ(rt.barraySize(), size/2);
    rt.barrayShrink();
    capacity = rt.barrayCapacity();
    
    extra = 1u;
    rt.convertBArrayToArray(extra);
    EXPECT_EQ(rt.type(), JType::ARRAY);
    EXPECT_EQ(rt.arraySize(), size/2);
    EXPECT_EQ(rt.arrayCapacity(), capacity + extra);
    for (uint32_t i = 0; i < size/2; ++i)
    {
      EXPECT_TRUE(rt.arrayCValueAt(i).isMetaBool());
      EXPECT_EQ(rt.arrayCValueAt(i).getBool(), (bool)(i % 2));
    }
  }
}

TEST(Document, Finalize)
{
  Document<512u, HeapAllocator> doc;
  const auto& alc = doc.baseAllocator();
  
  auto rt = doc.root();
  auto ar = rt.toArray();
  
  // Fill
  ar.arrayPushBack(true);
  ar.arrayPushBack(1);
  ar.arrayPushBack("hi");
  ar[3].toArray();
  ar[3].arrayPushBack(-1);
  ar[3].arrayPushBack(-2);
  ar[4].toObject();
  ar[4].objectPushBack("hello", "world");
  ar[4].objectPushBack("hi", "this is a long string for test");
  ar[4].objectPushBack("hey", "this is another long string for test");
  auto ar5 = ar[5].toArray();
  for (uint32_t i = 0; i < 20; ++i)
    ar5.arrayPushBack(i);
  EXPECT_EQ(ar.arraySize(), 6u);
  uint64_t mem = alc.getAllocated();
  EXPECT_GT(mem, 0u);
  
  // Remove some
  ar5 = nullptr;
  EXPECT_EQ(alc.getAllocated(), mem);
  
  // Shrink
  doc.shrink();
  EXPECT_LT(alc.getAllocated(), mem);
  mem = alc.getAllocated();
  
  // Clear
  doc.clear();
  EXPECT_EQ(alc.getAllocated(), mem);
  
  // Shrink
  doc.shrink();
  EXPECT_EQ(alc.getAllocated(), 0u);
  
  // Re-fill
  auto rt2 = doc.root();
  auto ar2 = rt2.toArray();
  
  ar2.arrayPushBack(true);
  ar2.arrayPushBack(1);
  ar2.arrayPushBack("hi");
  EXPECT_EQ(ar2.arraySize(), 3u);
  EXPECT_GT(alc.getAllocated(), 0u);
}

TEST(Document, ReuseStringPool)
{
  Document<512u, HeapAllocator> doc1;
  const auto& alc1 = doc1.baseAllocator();
  auto sp = doc1.stringPool();
  {
    auto rt = doc1.root();
    auto ar = rt.toArray();
    
    ar.arrayPushBack((char*)"hi");
    ar.arrayPushBack((char*)"hello");
    ar.arrayPushBack((char*)"world!");
    ar.arrayPushBack((char*)"this is a long string for test");
    ar.arrayPushBack((char*)"this is another long string for test");
    
    EXPECT_EQ(ar.arraySize(), 5u);
  }
  uint64_t mem1 = alc1.getAllocated();
  EXPECT_GT(mem1, 0u);
  uint32_t size1 = sp->size();
  EXPECT_EQ(size1, 2u); // long only
  
  Document<512u, HeapAllocator> doc2(sp);
  const auto& alc2 = doc2.baseAllocator();
  {
    auto rt = doc2.root();
    auto ar = rt.toArray();
    
    ar.arrayPushBack((char*)"hi");
    ar.arrayPushBack((char*)"hello");
    ar.arrayPushBack((char*)"world!");
    ar.arrayPushBack((char*)"this is a long string for test");
    ar.arrayPushBack((char*)"this is another long string for test");
    
    EXPECT_EQ(ar.arraySize(), 5u);
  }
  uint64_t mem2 = alc2.getAllocated();
  EXPECT_GT(mem2, 0u);
  EXPECT_LT(mem2, mem1 * 2u); // same allocator
  uint32_t size2 = sp->size();
  EXPECT_EQ(size2, size1);  // reused
}
