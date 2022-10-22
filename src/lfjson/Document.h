/**
 * Copyright 2022 Guillaume AUJAY. All rights reserved.
 *
 */

#ifndef LFJSON_DOCUMENT_H
#define LFJSON_DOCUMENT_H

#include "BaseData.h"
#include "DataHelper.h"
#include "PoolAllocator.h"
#include "StringPool.h"

#include <cstddef>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <cassert>
#include <limits>
#include <memory>
#include <stdexcept>

#define LFJ_DOCUMENT_DFLT_CHUNKSIZE   32768u
#define LFJ_MAX_INT64 ((uint64_t)std::numeric_limits<int64_t>::max())

#ifdef LFJ_HANDLER_DEBUG
  #include <iostream>
#endif

namespace lfjson
{

template <uint16_t StringChunkSize = LFJ_DOCUMENT_DFLT_CHUNKSIZE,
          class Allocator = StdAllocator,
          uint16_t ObjectChunkSize = StringChunkSize>
class Document
{
public:
  using SharedStringPool = std::shared_ptr<StringPool<StringChunkSize, Allocator>>;
  
  // Reference to a Document JMember
  class RefMember
  {
    friend class RefValue;
    friend class Document;
    
  private:
    Document& mDoc;
    JMember& mMember;

    // Constructor
    RefMember(Document& doc, JMember& member) : mDoc(doc), mMember(member) {}
    
  public:
    // Converter
    RefMember(Document& doc, ConstMember& member) : mDoc(doc), mMember((JMember&)member) {}
    
    // Getters
    const char* key()   const { return mMember.key(); }
    ConstValue& value() const { return mMember.value(); }
    
    uint32_t keyLen() const { return mMember.keyLen(); }
    bool keyOwned()   const { return mMember.keyOwned(); }  // true if copied
    
    // Setters
    void setKey(const char* key, int32_t length = -1)
    {
      bool found = false;
      const JString* jKey = mDoc.mSPA->provide(key, true, found, length);
      
      mMember.setKey(jKey);
    }
  };
  
  // Reference to a Document JValue
  class RefValue
  {
    friend class Document;
    
  private:
    Document& mDoc;
    JValue& mValue;
    
    // Constructor
    RefValue(Document& doc, JValue& value) : mDoc(doc), mValue(value) {}
    
    // Functions
    uint32_t arrayIncSize()
    {
      uint32_t last = mValue.arraySize();
      if (mValue.aFull())
        helper::arrayGrow(mValue, mDoc.mOPA);
      mValue.incASizeUninit();
    #ifndef NDEBUG
      mValue[last].forceNull(); // avoid asserts
    #endif
      return last;
    }
    
    uint32_t barrayIncSize()
    {
      uint32_t last = mValue.barraySize();
      if (mValue.baFull())
        helper::barrayGrow(mValue, mDoc.mOPA);
      mValue.incBASizeUninit();
      return last;
    }
    
    uint32_t iarrayIncSize()
    {
      uint32_t last = mValue.iarraySize();
      if (mValue.iaFull())
        helper::iarrayGrow(mValue, mDoc.mOPA);
      mValue.incIASizeUninit();
      return last;
    }
    
    uint32_t darrayIncSize()
    {
      uint32_t last = mValue.darraySize();
      if (mValue.daFull())
        helper::darrayGrow(mValue, mDoc.mOPA);
      mValue.incDASizeUninit();
      return last;
    }
    
    JValue& objectIncSize(const char* key, int32_t len)
    {
      if (mValue.oFull())
        helper::objectGrow(mValue, mDoc.mOPA);
      
      bool found = false;
      const JString* js = mDoc.mSPA->provide(key, true, found, len);
      return mValue.incOSize(js);
    }
    
    JValue& objectIncSize(char* key, int32_t len)
    {
      if (mValue.oFull())
        helper::objectGrow(mValue, mDoc.mOPA);
      
      bool found = false;
      const JString* js = mDoc.mSPA->provideInterned(key, true, found, len);
      return mValue.incOSize(js);
    }
    
    void deallocate()
    {
      switch (mValue.type())
      {
        case JType::OBJECT: { deallocateObject(mDoc, mValue); break; }
        case JType::ARRAY:  { deallocateArray( mDoc, mValue); break; }
        case JType::BARRAY: { deallocateBArray(mDoc, mValue); break; }
        case JType::IARRAY: { deallocateIArray(mDoc, mValue); break; }
        case JType::DARRAY: { deallocateDArray(mDoc, mValue); break; }
        default: break;
      }
    #ifndef NDEBUG
      mValue.forceNull(); // avoid asserts
    #endif
    }
    
    static void deallocateArrayChildren(Document& doc, JValue& value)
    {
      assert(value.isArray());
      
      uint32_t size = value.arraySize();
      for (uint32_t i = 0u; i < size; ++i)
        deallocateValue(doc, value[i]);
    }
    
    static void deallocateArray(Document& doc, JValue& value)
    {
      assert(value.isArray());
      deallocateArrayChildren(doc, value);
      
      uint32_t capacity = value.arrayCapacity();
      if (capacity < LFJ_MAX_UINT16)
      {
        if (capacity > 0u)
          doc.mOPA.deallocate(value.aA(), capacity * sizeof(JValue));
      }
      else
        doc.mOPA.deallocate(value.aBA(), sizeof(JBigArray) + (capacity - 1) * sizeof(JValue));
    }
    
    static void deallocateBArray(Document& doc, JValue& value)
    {
      assert(value.isBArray());
      uint32_t capacity = value.barrayCapacity();
      if (capacity < LFJ_MAX_UINT16)
      {
        if (capacity > 0u)
          doc.mOPA.deallocate(value.baA(), capacity * sizeof(bool));
      }
      else
        doc.mOPA.deallocate(value.baBA(), sizeof(JBigBArray) + (capacity - 1) * sizeof(bool));
    }
    
    static void deallocateIArray(Document& doc, JValue& value)
    {
      assert(value.isIArray());
      uint32_t capacity = value.iarrayCapacity();
      if (capacity < LFJ_MAX_UINT16)
      {
        if (capacity > 0u)
          doc.mOPA.deallocate(value.iaA(), capacity * sizeof(int64_t));
      }
      else
        doc.mOPA.deallocate(value.iaBA(), sizeof(JBigIArray) + (capacity - 1) * sizeof(int64_t));
    }
    
    static void deallocateDArray(Document& doc, JValue& value)
    {
      assert(value.isDArray());
      uint32_t capacity = value.darrayCapacity();
      if (capacity < LFJ_MAX_UINT16)
      {
        if (capacity > 0u)
          doc.mOPA.deallocate(value.daA(), capacity * sizeof(double));
      }
      else
        doc.mOPA.deallocate(value.daBA(), sizeof(JBigDArray) + (capacity - 1) * sizeof(double));
    }
    
    static void deallocateObjectChildren(Document& doc, JValue& value)
    {
      assert(value.isObject());
      
      uint32_t size = value.objectSize();
      for (uint32_t i = 0u; i < size; ++i)
        deallocateValue(doc, value.member(i).jvalue());
    }
    
    static void deallocateObject(Document& doc, JValue& value)
    {
      assert(value.isObject());
      deallocateObjectChildren(doc, value);
      
      uint32_t capacity = value.objectCapacity();
      if (capacity < LFJ_MAX_UINT16)
      {
        if (capacity > 0u)
          doc.mOPA.deallocate(value.oO(), capacity * sizeof(JMember));
      }
      else
        doc.mOPA.deallocate(value.oBO(), sizeof(JBigObject) + (capacity - 1) * sizeof(JMember));
    }
    
    static void deallocateValue(Document& doc, JValue& value)
    {
      switch (value.type())
      {
        case JType::OBJECT: { deallocateObject(doc, value); break; }
        case JType::ARRAY:  { deallocateArray(doc,  value); break; }
        case JType::BARRAY: { deallocateBArray(doc, value); break; }
        case JType::IARRAY: { deallocateIArray(doc, value); break; }
        case JType::DARRAY: { deallocateDArray(doc, value); break; }
        default: break;
      }
    }
    
public:
    // Converter
    RefValue(Document& doc, ConstValue& value) : mDoc(doc), mValue((JValue&)value) {}
    
    // Getters
    JType type() const { return mValue.type(); }
    JMeta meta() const { return mValue.meta(); }
    
    bool isObject()      const { return mValue.isObject(); }
    bool isArray()       const { return mValue.isArray(); }
    bool isBArray()      const { return mValue.isBArray(); }
    bool isIArray()      const { return mValue.isIArray(); }
    bool isDArray()      const { return mValue.isDArray(); }
    bool isLongString()  const { return mValue.isLongString(); }
    bool isShortString() const { return mValue.isShortString(); }
    bool isInt64()       const { return mValue.isInt64(); }
    bool isUInt64()      const { return mValue.isUInt64(); }
    bool isDouble()      const { return mValue.isDouble(); }
    bool isTrue()        const { return mValue.isTrue(); }
    bool isFalse()       const { return mValue.isFalse(); }
    bool isNul()         const { return mValue.isNul(); }
    
    bool isMetaBool()   const { return mValue.isMetaBool(); }
    bool isMetaString() const { return mValue.isMetaString(); }
    bool isMetaNumber() const { return mValue.isMetaNumber(); }
    bool isMetaArray()  const { return mValue.isMetaArray(); }
    
    bool arrayEmpty()       const { return mValue.arrayEmpty(); }
    bool barrayEmpty()      const { return mValue.barrayEmpty(); }
    bool iarrayEmpty()      const { return mValue.iarrayEmpty(); }
    bool darrayEmpty()      const { return mValue.darrayEmpty(); }
    bool objectEmpty()      const { return mValue.objectEmpty(); }
    bool shortStringEmpty() const { return mValue.shortStringEmpty(); }
    bool longStringEmpty()  const { return mValue.longStringEmpty(); }
    
    uint32_t arraySize()       const { return mValue.arraySize(); }
    uint32_t barraySize()      const { return mValue.barraySize(); }
    uint32_t iarraySize()      const { return mValue.iarraySize(); }
    uint32_t darraySize()      const { return mValue.darraySize(); }
    uint32_t objectSize()      const { return mValue.objectSize(); }
    uint32_t shortStringSize() const { return mValue.shortStringSize(); }
    uint32_t longStringSize()  const { return mValue.longStringSize(); }
                                    
    uint32_t arrayCapacity()  const { return mValue.arrayCapacity(); }
    uint32_t barrayCapacity() const { return mValue.barrayCapacity(); }
    uint32_t iarrayCapacity() const { return mValue.iarrayCapacity(); }
    uint32_t darrayCapacity() const { return mValue.darrayCapacity(); }
    uint32_t objectCapacity() const { return mValue.objectCapacity(); }
    
    uint32_t arrayMemSize()  const { return mValue.arrayMemSize(); }
    uint32_t barrayMemSize() const { return mValue.barrayMemSize(); }
    uint32_t iarrayMemSize() const { return mValue.iarrayMemSize(); }
    uint32_t darrayMemSize() const { return mValue.darrayMemSize(); }
    uint32_t objectMemSize() const { return mValue.objectMemSize(); }
    
    uint32_t arrayMemUsed()  const { return mValue.arrayMemUsed(); }
    uint32_t barrayMemUsed() const { return mValue.barrayMemUsed(); }
    uint32_t iarrayMemUsed() const { return mValue.iarrayMemUsed(); }
    uint32_t darrayMemUsed() const { return mValue.darrayMemUsed(); }
    uint32_t objectMemUsed() const { return mValue.objectMemUsed(); }
    
    // Iterators
    ConstValueIter  arrayCBegin()  const { return mValue.arrayValues(); }
    ConstBoolIter   barrayCBegin() const { return mValue.barrayValues(); }
    ConstInt64Iter  iarrayCBegin() const { return mValue.iarrayValues(); }
    ConstDoubleIter darrayCBegin() const { return mValue.darrayValues(); }
    ConstMemberIter objectCBegin() const { return mValue.objectMembers(); }
    
    ConstValueIter  arrayCEnd()  const { return mValue.arrayValues()   + arraySize(); }
    ConstBoolIter   barrayCEnd() const { return mValue.barrayValues()  + barraySize(); }
    ConstInt64Iter  iarrayCEnd() const { return mValue.iarrayValues()  + iarraySize(); }
    ConstDoubleIter darrayCEnd() const { return mValue.darrayValues()  + darraySize(); }
    ConstMemberIter objectCEnd() const { return mValue.objectMembers() + objectSize(); }
    
    // Value
    bool     getBool()   const { return mValue.getBool(); }
    int64_t  getInt64()  const { return mValue.getInt64(); }
    uint64_t getUInt64() const { return mValue.getUInt64(); }
    double   getDouble() const { return mValue.getDouble(); }
    double   asNumber()  const { return mValue.asNumber(); }
    
    const char* getShortString() const { return mValue.getShortString(); }
    const char* getLongString()  const { return mValue.getLongString(); }
    const char* asString()       const { return mValue.asString(); }
    
    // Accessors
    RefValue arrayValue(uint32_t index) const
    {
      assert((uint32_t)index < arraySize());
      return RefValue(mDoc, mValue[index]);
    }
    
    bool& barrayValue(uint32_t index) const
    {
      assert((uint32_t)index < barraySize());
      return mValue.arrayBool(index);
    }
    
    int64_t& iarrayValue(uint32_t index) const
    {
      assert((uint32_t)index < iarraySize());
      return mValue.arrayInt64(index);
    }
    
    double& darrayValue(uint32_t index) const
    {
      assert((uint32_t)index < darraySize());
      return mValue.arrayDouble(index);
    }
    
    RefMember objectMember(uint32_t index) const
    {
      assert((uint32_t)index < objectSize());
      return RefMember(mDoc, mValue.member(index));
    }
    
    ConstValue& arrayCValue(uint32_t index) const
    {
      assert((uint32_t)index < arraySize());
      return mValue[index];
    }
    
    const bool& barrayCValue(uint32_t index) const { return barrayValue(index); }
    
    const int64_t& iarrayCValue(uint32_t index) const { return iarrayValue(index); }
    
    const double& darrayCValue(uint32_t index) const { return darrayValue(index); }
    
    ConstMember& objectCMember(uint32_t index) const
    {
      assert((uint32_t)index < objectSize());
      return mValue.member(index);
    }
    
    RefValue arrayValueAt(uint32_t index) const
    {
      if (index >= arraySize())
        throw std::out_of_range("[lfjson] RefValue: accessing array element after end");
      
      return RefValue(mDoc, mValue[index]);
    }
    
    bool& barrayValueAt(uint32_t index) const
    {
      if (index >= barraySize())
        throw std::out_of_range("[lfjson] RefValue: accessing barray element after end");
      
      return mValue.arrayBool(index);
    }
    
    int64_t& iarrayValueAt(uint32_t index) const
    {
      if (index >= iarraySize())
        throw std::out_of_range("[lfjson] RefValue: accessing iarray element after end");
      
      return mValue.arrayInt64(index);
    }
    
    double& darrayValueAt(uint32_t index) const
    {
      if (index >= darraySize())
        throw std::out_of_range("[lfjson] RefValue: accessing darray element after end");
      
      return mValue.arrayDouble(index);
    }
    
    RefMember objectMemberAt(uint32_t index) const
    {
      if (index >= objectSize())
        throw std::out_of_range("[lfjson] RefValue: accessing object element after end");
      
      return RefMember(mDoc, mValue.member(index));
    }
    
    ConstValue& arrayCValueAt(uint32_t index) const
    {
      if (index >= arraySize())
        throw std::out_of_range("[lfjson] RefValue: accessing const array element after end");
      
      return mValue[index];
    }
    
    const bool& barrayCValueAt(uint32_t index) const
    {
      if (index >= barraySize())
        throw std::out_of_range("[lfjson] RefValue: accessing const barray element after end");
      
      return mValue.arrayBool(index);
    }
    
    const int64_t& iarrayCValueAt(uint32_t index) const
    {
      if (index >= iarraySize())
        throw std::out_of_range("[lfjson] RefValue: accessing const iarray element after end");
      
      return mValue.arrayInt64(index);
    }
    
    const double& darrayCValueAt(uint32_t index) const
    {
      if (index >= darraySize())
        throw std::out_of_range("[lfjson] RefValue: accessing const darray element after end");
      
      return mValue.arrayDouble(index);
    }
    
    ConstMember& objectCMemberAt(uint32_t index) const
    {
      if (index >= objectSize())
        throw std::out_of_range("[lfjson] RefValue: accessing const object element after end");
      
      return mValue.member(index);
    }
    
    ConstMember* objectFindMember(const char* key, int32_t length = -1) const
    {
      assert(key != nullptr);
      assert(mValue.isObject());
      uint32_t size = mValue.objectSize();
      if (size == 0u)
        return nullptr;
      
      JMember* members = mValue.oMembers();
      const JString* jKey = mDoc.mSPA->get(key, length);
      if (jKey)
      {
        for (uint32_t i = 0u; i < size; ++i)
        {
          JMember& member = members[i];
          if (member.jkey() == jKey)
            return (ConstMember*)&member;
        }
      }
      return nullptr;
    }
    
    ConstValue* objectFindValue(const char* key, int32_t length = -1) const
    {
      assert(key != nullptr);
      assert(mValue.isObject());
      uint32_t size = mValue.objectSize();
      if (size == 0u)
        return nullptr;
      
      JMember* members = mValue.oMembers();
      const JString* jKey = mDoc.mSPA->get(key, length);
      if (jKey)
      {
        for (uint32_t i = 0u; i < size; ++i)
        {
          JMember& member = members[i];
          if (member.jkey() == jKey)
            return &member.value();
        }
      }
      return nullptr;
    }
    
    // Operators
    RefValue operator[](int index)
    {
      assert(index >= 0);
      if (mValue.isNul())
        mValue.set(JType::ARRAY);
      else
        assert(mValue.isArray());
      assert((uint32_t)index <= mValue.arraySize());
      
      if ((uint32_t)index == mValue.arraySize())
      {
        // New element
        if (mValue.aFull())
          helper::arrayGrow(mValue, mDoc.mOPA);
        mValue.incASize();
      }
      
      return RefValue(mDoc, mValue[index]);
    }
    
    RefValue operator[](const char* key)
    {
      assert(key != nullptr);
      if (mValue.isNul())
        mValue.set(JType::OBJECT);
      else
        assert(mValue.isObject());
      
      bool found = false;
      const JString* jKey = mDoc.mSPA->provide(key, true, found);
      JValue* val = found ? mDoc.getValue(mValue, jKey) : nullptr;
          
      if (val == nullptr)
      {
        // New element
        if (mValue.oFull())
          helper::objectGrow(mValue, mDoc.mOPA);
        val = &mValue.incOSize(jKey);
      }
      
      return RefValue(mDoc, *val);
    }
    
    RefValue operator[](char* key)
    {
      assert(key != nullptr);
      if (mValue.isNul())
        mValue.set(JType::OBJECT);
      else
        assert(mValue.isObject());
      
      bool found = false;
      const JString* jKey = mDoc.mSPA->provide(key, true, found);
      JValue* val = found ? mDoc.getValue(mValue, jKey) : nullptr;
      
      if (val == nullptr)
      {
        // New element
        if (mValue.oFull())
          helper::objectGrow(mValue, mDoc.mOPA);
        val = &mValue.incOSize(jKey);
      }
      
      return RefValue(mDoc, *val);
    }
    
    // Assign
    RefValue& operator=(bool b)
    {
      deallocate();
      mValue.set(b);
      return *this;
    }
    
    RefValue& operator=(int i) { return operator=((int64_t)i); }
    
    RefValue& operator=(int64_t i64)
    {
      deallocate();
      mValue.set(i64);
      return *this;
    }
    
    RefValue& operator=(unsigned int u) { return operator=((uint64_t)u); }
    
    RefValue& operator=(uint64_t u64)
    {
      deallocate();
      mValue.set(u64);
      return *this;
    }
    
    RefValue& operator=(double d)
    {
      deallocate();
      mValue.set(d);
      return *this;
    }
    
    RefValue& operator=(const char* s)
    {
      assert(s != nullptr);
      deallocate();
      
      // Check if short-string
      uint32_t minLen = JValue::minStringLength(s);
      if (minLen < JValue::ShortString_MaxSize)  // Short
      {
        mValue.set(s, minLen);
      }
      else  // Long
      {
        bool found = false;
        const JString* js = mDoc.mSPA->provide(s, false, found);
        mValue.set(js, js->len());
      }
      return *this;
    }
    
    RefValue& operator=(char* s)
    {
      assert(s != nullptr);
      deallocate();
      
      // Check if short-string
      uint32_t minLen = JValue::minStringLength(s);
      if (minLen < JValue::ShortString_MaxSize)  // Short
      {
        mValue.set(s, minLen);
      }
      else  // Long
      {
        bool found = false;
        const JString* js = mDoc.mSPA->provide(s, false, found);
        mValue.set(js, js->len());
      }
      return *this;
    }
    
    RefValue& operator=(std::nullptr_t)
    {
      deallocate();
      mValue.forceNull();
      return *this;
    }
    
    // Modifiers
    RefValue& toNull()
    {
      deallocate();
      mValue.forceNull();
      return *this;
    }
    
    RefValue& toArray()
    {
      deallocate();
      mValue.set(JType::ARRAY);
      return *this;
    }
    
    RefValue& toBArray()
    {
      deallocate();
      mValue.set(JType::BARRAY);
      return *this;
    }
    
    RefValue& toIArray()
    {
      deallocate();
      mValue.set(JType::IARRAY);
      return *this;
    }
    
    RefValue& toDArray()
    {
      deallocate();
      mValue.set(JType::DARRAY);
      return *this;
    }
    
    RefValue& toObject()
    {
      deallocate();
      mValue.set(JType::OBJECT);
      return *this;
    }
    
    void arrayClear()
    {
      deallocateArrayChildren(mDoc, mValue);
      mValue.setASize(0u);
    }
    
    void barrayClear()
    {
      mValue.setBASize(0u);
    }
    
    void iarrayClear()
    {
      mValue.setIASize(0u);
    }
    
    void darrayClear()
    {
      mValue.setDASize(0u);
    }
    
    void objectClear()
    {
      deallocateObjectChildren(mDoc, mValue);
      mValue.setOSize(0u);
    }
    
    void arrayReserve(uint32_t new_cap)
    {
      helper::arrayReserve(mValue, new_cap, mDoc.mOPA);
    }
    
    void barrayReserve(uint32_t new_cap)
    {
      helper::barrayReserve(mValue, new_cap, mDoc.mOPA);
    }
    
    void iarrayReserve(uint32_t new_cap)
    {
      helper::iarrayReserve(mValue, new_cap, mDoc.mOPA);
    }
    
    void darrayReserve(uint32_t new_cap)
    {
      helper::darrayReserve(mValue, new_cap, mDoc.mOPA);
    }
    
    void objectReserve(uint32_t new_cap)
    {
      helper::objectReserve(mValue, new_cap, mDoc.mOPA);
    }
    
    void arrayShrink()
    {
      helper::arrayShrink(mValue, mDoc.mOPA);
    }
    
    void barrayShrink()
    {
      helper::barrayShrink(mValue, mDoc.mOPA);
    }
    
    void iarrayShrink()
    {
      helper::iarrayShrink(mValue, mDoc.mOPA);
    }
    
    void darrayShrink()
    {
      helper::darrayShrink(mValue, mDoc.mOPA);
    }
    
    void objectShrink()
    {
      helper::objectShrink(mValue, mDoc.mOPA);
    }
    
    // Array PushBack
    void arrayPushBack(bool b)
    {
      uint32_t last = arrayIncSize();
      mValue[last].set(b);
    }
    
    void arrayPushBack(int i) { arrayPushBack((int64_t)i); }
    
    void arrayPushBack(int64_t i64)
    {
      uint32_t last = arrayIncSize();
      mValue[last].set(i64);
    }
    
    void arrayPushBack(unsigned int u) { arrayPushBack((uint64_t)u); }
    
    void arrayPushBack(uint64_t u64)
    {
      uint32_t last = arrayIncSize();
      mValue[last].set(u64);
    }
    
    void arrayPushBack(double d)
    {
      uint32_t last = arrayIncSize();
      mValue[last].set(d);
    }
    
    void arrayPushBack(const char* s, int32_t length = -1)
    {
      uint32_t last = arrayIncSize();
      // Check if short-string
      uint32_t minLen = length >= 0 ? (uint32_t)length : JValue::minStringLength(s);
      if (minLen < JValue::ShortString_MaxSize)  // Short
      {
        mValue[last].set(s, minLen);
      }
      else  // Long
      {
        bool found = false;
        const JString* js = mDoc.mSPA->provide(s, false, found, length);
        mValue[last].set(js, js->len());
      }
    }
    
    void arrayPushBack(char* s, int32_t length = -1)
    {
      uint32_t last = arrayIncSize();
      // Check if short-string
      uint32_t minLen = length >= 0 ? (uint32_t)length : JValue::minStringLength(s);
      if (minLen < JValue::ShortString_MaxSize)  // Short
      {
        mValue[last].set(s, minLen);
      }
      else  // Long
      {
        bool found = false;
        const JString* js = mDoc.mSPA->provide(s, false, found, length);
        mValue[last].set(js, js->len());
      }
    }
    
    void arrayPushBack(std::nullptr_t)
    {
      uint32_t last = arrayIncSize();
      mValue[last].setNull();
    }
    
    void barrayPushBack(bool b)
    {
      uint32_t last = barrayIncSize();
      mValue.arrayBool(last) = b;
    }
    
    void iarrayPushBack(int64_t i64)
    {
      uint32_t last = iarrayIncSize();
      mValue.arrayInt64(last) = i64;
    }
    
    void darrayPushBack(double d)
    {
      uint32_t last = darrayIncSize();
      mValue.arrayDouble(last) = d;
    }
    
    // Object PushBack
    void objectPushBack(const char* key, bool b, int32_t keyLength = -1)
    {
      JValue& last = objectIncSize(key, keyLength);
      last.set(b);
    }
    
    void objectPushBack(char* key, bool b, int32_t keyLength = -1)
    {
      JValue& last = objectIncSize(key, keyLength);
      last.set(b);
    }
    
    void objectPushBack(const char* key, int i, int32_t keyLength = -1) { objectPushBack(key, (int64_t)i, keyLength); }
    
    void objectPushBack(char* key, int i, int32_t keyLength = -1) { objectPushBack(key, (int64_t)i, keyLength); }
    
    void objectPushBack(const char* key, int64_t i64, int32_t keyLength = -1)
    {
      JValue& last = objectIncSize(key, keyLength);
      last.set(i64);
    }
    
    void objectPushBack(char* key, int64_t i64, int32_t keyLength = -1)
    {
      JValue& last = objectIncSize(key, keyLength);
      last.set(i64);
    }
    
    void objectPushBack(const char* key, unsigned int u, int32_t keyLength = -1) { objectPushBack(key, (uint64_t)u, keyLength); }
    
    void objectPushBack(char* key, unsigned int u, int32_t keyLength = -1) { objectPushBack(key, (uint64_t)u, keyLength); }
    
    void objectPushBack(const char* key, uint64_t u64, int32_t keyLength = -1)
    {
      JValue& last = objectIncSize(key, keyLength);
      last.set(u64);
    }
    
    void objectPushBack(char* key, uint64_t u64, int32_t keyLength = -1)
    {
      JValue& last = objectIncSize(key, keyLength);
      last.set(u64);
    }
    
    void objectPushBack(const char* key, double d, int32_t keyLength = -1)
    {
      JValue& last = objectIncSize(key, keyLength);
      last.set(d);
    }
    
    void objectPushBack(char* key, double d, int32_t keyLength = -1)
    {
      JValue& last = objectIncSize(key, keyLength);
      last.set(d);
    }
    
    void objectPushBack(const char* key, std::nullptr_t, int32_t keyLength = -1)
    {
      objectIncSize(key, keyLength);
    }
    
    void objectPushBack(char* key, std::nullptr_t, int32_t keyLength = -1)
    {
      objectIncSize(key, keyLength);
    }
    
    void objectPushBack(const char* key, const char* val, int32_t keyLength = -1, int32_t valLength = -1)
    {
      JValue& last = objectIncSize(key, keyLength);
      // Check if short-string
      uint32_t minLen = valLength >= 0 ? (uint32_t)valLength : JValue::minStringLength(val);
      if (minLen < JValue::ShortString_MaxSize)  // Short
      {
        last.set(val, minLen);
      }
      else  // Long
      {
        bool found = false;
        const JString* js = mDoc.mSPA->provide(val, false, found, valLength);
        last.set(js, js->len());
      }
    }
    
    void objectPushBack(const char* key, char* val, int32_t keyLength = -1, int32_t valLength = -1)
    {
      JValue& last = objectIncSize(key, keyLength);
      // Check if short-string
      uint32_t minLen = valLength >= 0 ? (uint32_t)valLength : JValue::minStringLength(val);
      if (minLen < JValue::ShortString_MaxSize)  // Short
      {
        last.set(val, minLen);
      }
      else  // Long
      {
        bool found = false;
        const JString* js = mDoc.mSPA->provide(val, false, found, valLength);
        last.set(js, js->len());
      }
    }
    
    void objectPushBack(char* key, const char* val, int32_t keyLength = -1, int32_t valLength = -1)
    {
      JValue& last = objectIncSize(key, keyLength);
      // Check if short-string
      uint32_t minLen = valLength >= 0 ? (uint32_t)valLength : JValue::minStringLength(val);
      if (minLen < JValue::ShortString_MaxSize)  // Short
      {
        last.set(val, minLen);
      }
      else  // Long
      {
        bool found = false;
        const JString* js = mDoc.mSPA->provide(val, false, found, valLength);
        last.set(js, js->len());
      }
    }
    
    void objectPushBack(char* key, char* val, int32_t keyLength = -1, int32_t valLength = -1)
    {
      JValue& last = objectIncSize(key, keyLength);
      // Check if short-string
      uint32_t minLen = valLength >= 0 ? (uint32_t)valLength : JValue::minStringLength(val);
      if (minLen < JValue::ShortString_MaxSize)  // Short
      {
        last.set(val, minLen);
      }
      else  // Long
      {
        bool found = false;
        const JString* js = mDoc.mSPA->provide(val, false, found, valLength);
        last.set(js, js->len());
      }
    }
    
    // Array Pop/Erase
    void arrayPopBack()
    {
      assert(!mValue.arrayEmpty());
      uint32_t last = mValue.arraySize() - 1u;
      deallocateValue(mDoc, mValue[last]);
      mValue.decASize();
    }
    
    void barrayPopBack()
    {
      assert(!mValue.barrayEmpty());
      mValue.decBASize();
    }
    
    void iarrayPopBack()
    {
      assert(!mValue.iarrayEmpty());
      mValue.decIASize();
    }
    
    void darrayPopBack()
    {
      assert(!mValue.darrayEmpty());
      mValue.decDASize();
    }
    
    void objectPopBack()
    {
      assert(!mValue.objectEmpty());
      uint32_t last = mValue.objectSize() - 1u;
      deallocateValue(mDoc, mValue.member(last).jvalue());
      mValue.decOSize();
    }
    
    void arrayErase(ConstValueIter it)
    {
      JValue* itValue = (JValue*)it;
      deallocateValue(mDoc, *itValue);
      helper::arrayOverwrite(mValue, itValue);
    }
    
    void barrayErase(ConstBoolIter it)
    {
      helper::barrayOverwrite(mValue, (bool*)it);
    }
    
    void iarrayErase(ConstInt64Iter it)
    {
      helper::iarrayOverwrite(mValue, (int64_t*)it);
    }
    
    void darrayErase(ConstDoubleIter it)
    {
      helper::darrayOverwrite(mValue, (double*)it);
    }
    
    void objectErase(ConstMemberIter it)
    {
      JMember* itMember = (JMember*)it;
      deallocateValue(mDoc, itMember->jvalue());
      helper::objectOverwrite(mValue, itMember);
    }
    
    // Swap (/!\ Can break tree structure if swapping parent/child)
    void swap(RefValue& other)
    {
      assert(&mDoc == &other.mDoc);
      auto temp = other.mValue;
      other.mValue = mValue;
      mValue = temp;
    }
    
    // Array Converters (new_capacity = max(capacity, size + reserveForExtra))
    void convertBArrayToArray(uint32_t reserveForExtra = 0u)
    {
      helper::convertBArrayToArray(mValue, reserveForExtra, mDoc.mOPA);
    }
    
    void convertIArrayToArray(uint32_t reserveForExtra = 0u)
    {
      helper::convertIArrayToArray(mValue, reserveForExtra, mDoc.mOPA);
    }
    
    void convertDArrayToArray(uint32_t reserveForExtra = 0u)
    {
      helper::convertDArrayToArray(mValue, reserveForExtra, mDoc.mOPA);
    }
    
    void convertIArrayToDArray(uint32_t reserveForExtra = 0u)
    {
      helper::convertIArrayToDArray(mValue, reserveForExtra, mDoc.mOPA);
    }
  };
  
  // Parsing Handler for a Document
  class Handler
  {
  private:
    struct LFStack
    {
      Allocator& allocator;
      size_t size = 0u;
      size_t capa = 0u;
      char* data = nullptr;
      static constexpr float Stack_GrowthFactor = 2.f;
      
      LFStack(Allocator& allocator_, size_t initialCapacity = 1024u)
        : allocator(allocator_)
        , capa(initialCapacity)
        , data(initialCapacity > 0u ? allocator.allocate(initialCapacity) : nullptr)
      {
      }
      
      ~LFStack()
      {
        allocator.deallocate(data, capa);
      }
      
      bool empty() const { return size == 0u; }
      
      char* end() const { return data + size; }
      
      char* lastValue() const
      {
        return data + size - sizeof(ConstValue);
      }
      
      void increment(size_t inc)
      {
        assert(size + inc <= capa);
        size += inc;
      }
      
      void decrement(size_t dec)
      {
        assert(size >= dec);
        size -= dec;
      }
      
      void reserve(size_t newCapacity)
      {
        if (capa >= newCapacity)
          return;
        
        size_t grownCapa = (capa > 0u) ? (size_t)std::ceil(capa * Stack_GrowthFactor) : 1u;
        newCapacity = (newCapacity < grownCapa) ? grownCapa : newCapacity;
        
        char* temp = allocator.allocate(newCapacity);
        assert(temp);
        std::memcpy(temp, data, size * sizeof(char));
        allocator.deallocate(data, capa);
        
        data = temp;
        capa = newCapacity;
      }
      
      void release()
      {
        allocator.deallocate(data, capa);
        size = 0u;
        capa = 0u;
        data = nullptr;
      }
    };
    
    // Members
    Document& mDoc;
    LFStack mStack;
    bool mMemberVal = false;
    bool mRootInit  = false;
    
    const bool mIntToDouble = true;
    uint32_t mArraySize = 0u;
    JType mArrayType = JType::NUL;
    
  #ifdef LFJ_HANDLER_DEBUG
  public:
    bool print = true;
    uint64_t valCount = 0u;
  #endif
    
    // In place constructors
    static void inPlaceValue(void* dst, std::nullptr_t)  { new (dst) JValue(); }
    static void inPlaceValue(void* dst, bool b)          { new (dst) JValue(b); }
    static void inPlaceValue(void* dst, int64_t i64)     { new (dst) JValue(i64); }
    static void inPlaceValue(void* dst, uint64_t u64)    { new (dst) JValue(u64); }
    static void inPlaceValue(void* dst, double d)        { new (dst) JValue(d); }
    static void inPlaceValue(void* dst, JType type)      { new (dst) JValue(type); }
    void inPlaceValue(void* dst, const char* str, int32_t len)
    {
      assert(str != nullptr);
      // Check if short-string
      uint32_t minLen = len >= 0 ? (uint32_t)len : JValue::minStringLength(str);
      if (minLen < JValue::ShortString_MaxSize) // Short
      {
        new (dst) JValue(str, minLen);
      }
      else  // Long
      {
        bool found = false;
        const JString* js = mDoc.stringPool()->provide(str, false, found, len);
        new (dst) JValue(js, js->len());
      }
    }
    void inPlaceValue(void* dst, char* str, int32_t len)
    {
      assert(str != nullptr);
      // Check if short-string
      uint32_t minLen = len >= 0 ? (uint32_t)len : JValue::minStringLength(str);
      if (minLen < JValue::ShortString_MaxSize) // Short
      {
        new (dst) JValue(str, len);
      }
      else  // Long
      {
        bool found = false;
        const JString* js = mDoc.stringPool()->provide(str, false, found, len);
        new (dst) JValue(js, js->len());
      }
    }
    void inPlaceMember(void* dst, const char* key, int32_t len)
    {
      bool found = false;
      const JString* js = mDoc.stringPool()->provide(key, true, found, len);
      new (dst) JMember(js);
    }
    void inPlaceMember(void* dst, char* key, int32_t len)
    {
      bool found = false;
      const JString* js = mDoc.stringPool()->provide(key, true, found, len);
      new (dst) JMember(js);
    }
    
    // Returns 'true' if array is specialized
    bool convertedFor(const JType type)
    {
      assert(type == JType::ARRAY || type == JType::BARRAY || type == JType::IARRAY || type == JType::DARRAY);
      if (mArrayType == type || mArrayType == JType::NUL)
      {
        ++mArraySize;
        mArrayType = type;
        return true;
      }
      
      if (mIntToDouble)
      {
        if (mArrayType == JType::DARRAY && type == JType::IARRAY)
        {
          ++mArraySize;
          return true;
        }
        if (mArrayType == JType::IARRAY && type == JType::DARRAY)
        {
          // In place convert int to double
          assert(mArraySize > 0u);
          const int64_t* iValues = (int64_t*)(mStack.end() - (mArraySize * sizeof(int64_t)));
          double* dValues = (double*)(iValues); // not strictly aliased
          
          for (uint32_t i = 0; i < mArraySize; ++i)
            dValues[i] = (double)iValues[i];
          
          ++mArraySize;
          mArrayType = JType::DARRAY;
          return true;
        }
      }
      
      // In place convert to JValue
      assert(mArrayType == JType::ARRAY || mArrayType == JType::BARRAY || mArrayType == JType::IARRAY || mArrayType == JType::DARRAY);
      switch (mArrayType)
      {
        case JType::BARRAY:
        {
          const size_t addSize = (size_t)mArraySize * (sizeof(ConstValue) - sizeof(bool)) + sizeof(ConstValue); // +1
          mStack.reserve(mStack.size + addSize);
          
          bool* bValues = (bool*)(mStack.end() - (mArraySize * sizeof(bool)));
          JValue* aValues = (JValue*)bValues; // aligned
          
          for (int64_t i = (int64_t)mArraySize - 1; i >= 0; --i)
            aValues[i].force(bValues[i]);
          break;
        }
        case JType::IARRAY:
        {
          const size_t addSize = (size_t)mArraySize * (sizeof(ConstValue) - sizeof(int64_t)) + sizeof(ConstValue); // +1
          mStack.reserve(mStack.size + addSize);
          
          int64_t* iValues = (int64_t*)(mStack.end() - (mArraySize * sizeof(int64_t)));
          JValue* aValues = (JValue*)iValues; // aligned
          
          for (int64_t i = (int64_t)mArraySize - 1; i >= 0; --i)
            aValues[i].force(iValues[i]);
          break;
        }
        case JType::DARRAY:
        {
          const size_t addSize = (size_t)mArraySize * (sizeof(ConstValue) - sizeof(double)) + sizeof(ConstValue); // +1
          mStack.reserve(mStack.size + addSize);
          
          double* dValues = (double*)(mStack.end() - (mArraySize * sizeof(double)));
          JValue* aValues = (JValue*)dValues; // aligned
          
          for (int64_t i = (int64_t)mArraySize - 1; i >= 0; --i)
            aValues[i].force(dValues[i]);
          break;
        }
        default:
          break;
      }
      
      mArrayType = JType::ARRAY;
      return false;
    }
    
  public:
    Handler(Document& doc, bool allowIntToDouble = true)
      : mDoc(doc)
      , mStack(doc.baseAllocator())
      , mIntToDouble(allowIntToDouble)
    {}
    
    // Accessors
    uint64_t stackCapacity() const { return mStack.capa; }
  #ifdef LFJ_HANDLER_DEBUG
    uint64_t parsedValuesCount()  const { return valCount; }
  #endif
    
    // Modifiers
    void clear()
    {
      mStack.size = 0u;
      mMemberVal = false;
      mRootInit  = false;
      mArraySize = 0u;
      mArrayType = JType::NUL;
    }
    
    void finalize(bool shrinkDocument = true, bool rehashStringPool = false)
    {
      assert(mStack.size == 0u);
      mStack.release();
      
      mMemberVal = false;
      mRootInit  = false;
      mArraySize = 0u;
      mArrayType = JType::NUL;
      
      if (shrinkDocument)
        mDoc.shrink(rehashStringPool);
    }
    
    // Group
    bool startObject()
    {
      if (!mRootInit) // root
      {
        mDoc.root().toObject();
        mRootInit = true;
      }
      else
      {
        // push on stack
        if (mMemberVal)
        {
          assert(((JValue*)mStack.lastValue())->isNul());
          inPlaceValue(mStack.lastValue(), JType::OBJECT);
          mMemberVal = false;
        }
        else
        {
          convertedFor(JType::ARRAY);
          const uint64_t memSize = sizeof(ConstValue);
          mStack.reserve(mStack.size + memSize);
          inPlaceValue(mStack.end(), JType::OBJECT);
          mStack.increment(memSize);
        }
      }
    #ifdef LFJ_HANDLER_DEBUG
      ++valCount;
      if (print) std::cout << "StartObject()" << std::endl;
    #endif
      return true;
    }
    
    bool endObject(uint32_t memberCount)
    {
      assert(!mMemberVal);
      // move stack to alloc
      if (memberCount > 0u)
      {
        void* ptr = nullptr;
        auto& opa = mDoc.objectAllocator();
        const uint32_t memSize = memberCount * sizeof(ConstMember);
        if (memberCount < LFJ_MAX_UINT16)
          ptr = opa.memPush(mStack.end() - memSize, memSize);
        else  // big
          ptr = opa.memPushBigObject(mStack.end() - memSize, memberCount);
        
        mStack.decrement(memSize);
        assert(mStack.size == 0u || mStack.size >= sizeof(ConstValue));
        auto& val = mStack.size == 0u ? mDoc.root().mValue : *(JValue*)mStack.lastValue();
        assert(val.isObject());
        val.setRawObject(ptr, (uint32_t)memberCount);
      }
      mArrayType = JType::ARRAY;
      
    #ifdef LFJ_HANDLER_DEBUG
      if (print) std::cout << "EndObject(" << memberCount << ")" << std::endl;
    #endif
      return true;
    }
    
    bool startArray()
    {
      if (!mRootInit) // root
      {
        mDoc.root().toArray();
        mRootInit = true;
      }
      else
      {
        // push on stack
        if (mMemberVal)
        {
          assert(((JValue*)mStack.lastValue())->isNul());
          inPlaceValue(mStack.lastValue(), JType::ARRAY);
          mMemberVal = false;
        }
        else
        {
          convertedFor(JType::ARRAY);
          size_t memSize = sizeof(ConstValue);
          mStack.reserve(mStack.size + memSize);
          inPlaceValue(mStack.end(), JType::ARRAY);
          mStack.increment(memSize);
        }
      }
      mArraySize = 0u;
      mArrayType = JType::NUL;
      
    #ifdef LFJ_HANDLER_DEBUG
      ++valCount;
      if (print) std::cout << "StartArray()" << std::endl;
    #endif
      return true;
    }
    
    bool endArray(uint32_t elementCount)
    {
      assert(!mMemberVal);
      // move stack to alloc
      if (elementCount > 0u)
      {
        assert(mArrayType != JType::NUL);
        void* ptr = nullptr;
        uint32_t memSize = 0u;
        auto& opa = mDoc.objectAllocator();
        switch (mArrayType)
        {
          case JType::ARRAY:
          {
            memSize = elementCount * sizeof(ConstValue);
            if (elementCount < LFJ_MAX_UINT16)
              ptr = opa.memPush(mStack.end() - memSize, memSize);
            else  // big
              ptr = opa.memPushBigArray(mStack.end() - memSize, elementCount);
            
            mStack.decrement(memSize);
            assert(mStack.size == 0u || mStack.size >= sizeof(ConstValue));
            auto& val = mStack.size == 0u ? mDoc.root().mValue : *(JValue*)mStack.lastValue();
            val.setRawArray(ptr, (uint32_t)elementCount);
            break;
          }
          case JType::BARRAY:
          {
            memSize = elementCount * sizeof(bool);
            if (elementCount < LFJ_MAX_UINT16)
              ptr = opa.memPush(mStack.end() - memSize, memSize);
            else  // big
              ptr = opa.memPushBigBArray(mStack.end() - memSize, elementCount);
            
            mStack.decrement(memSize);
            assert(mStack.size == 0u || mStack.size >= sizeof(ConstValue));
            auto& val = mStack.size == 0u ? mDoc.root().mValue : *(JValue*)mStack.lastValue();
            val.setRawBArray(ptr, (uint32_t)elementCount);
            break;
          }
          case JType::IARRAY:
          {
            memSize = elementCount * sizeof(int64_t);
            if (elementCount < LFJ_MAX_UINT16)
              ptr = opa.memPush(mStack.end() - memSize, memSize);
            else  // big
              ptr = opa.memPushBigIArray(mStack.end() - memSize, elementCount);
            
            mStack.decrement(memSize);
            assert(mStack.size == 0u || mStack.size >= sizeof(ConstValue));
            auto& val = mStack.size == 0u ? mDoc.root().mValue : *(JValue*)mStack.lastValue();
            val.setRawIArray(ptr, (uint32_t)elementCount);
            break;
          }
          case JType::DARRAY:
          {
            memSize = elementCount * sizeof(double);
            if (elementCount < LFJ_MAX_UINT16)
              ptr = opa.memPush(mStack.end() - memSize, memSize);
            else  // big
              ptr = opa.memPushBigDArray(mStack.end() - memSize, elementCount);
            
            mStack.decrement(memSize);
            assert(mStack.size == 0u || mStack.size >= sizeof(ConstValue));
            auto& val = mStack.size == 0u ? mDoc.root().mValue : *(JValue*)mStack.lastValue();
            val.setRawDArray(ptr, (uint32_t)elementCount);
            break;
          }
          default:
            assert(false && "[lfjson] EndArray: unknown arrayType");
        }
      }
      mArrayType = JType::ARRAY;
      
    #ifdef LFJ_HANDLER_DEBUG
      if (print) std::cout << "EndArray(" << elementCount << ")" << std::endl;
    #endif
      return true;
    }
    
    // Scalar
    bool pushKey(const char* str, bool copy, int32_t length = -1)
    {
      assert(!mMemberVal);
      // push on stack
      const uint64_t memSize = sizeof(ConstMember);
      mStack.reserve(mStack.size + memSize);
      if (copy)
        inPlaceMember(mStack.end(), (char*)str, length);
      else
        inPlaceMember(mStack.end(), str, length);
      mStack.increment(memSize);
      
      mMemberVal = true;
      
    #ifdef LFJ_HANDLER_DEBUG
      ++valCount;
      if (print) std::cout << "Key(" << str << ", " << length << ", " << std::boolalpha << copy << ")" << std::endl;
    #endif
      return true;
    }
    
    bool pushNull()
    {
      // push on stack
      if (mMemberVal)
      {
        assert(((JValue*)mStack.lastValue())->isNul());
        mMemberVal = false;
      }
      else
      {
        convertedFor(JType::ARRAY);
        const uint64_t memSize = sizeof(ConstValue);
        mStack.reserve(mStack.size + memSize);
        inPlaceValue(mStack.end(), nullptr);
        mStack.increment(memSize);
      }
    #ifdef LFJ_HANDLER_DEBUG
      ++valCount;
      if (print) std::cout << "Null()" << std::endl;
    #endif
      return true;
    }
    
    bool pushBool(bool b)
    {
      // push on stack
      if (mMemberVal)
      {
        assert(((JValue*)mStack.lastValue())->isNul());
        inPlaceValue(mStack.lastValue(), b);
        mMemberVal = false;
      }
      else
      {
        bool specialized = convertedFor(JType::BARRAY);
        if (specialized)
        {
          const uint64_t memSize = sizeof(bool);
          mStack.reserve(mStack.size + memSize);
          bool* dst = (bool*)mStack.end();
          *dst = b;
          mStack.increment(memSize);
        }
        else
        {
          const uint64_t memSize = sizeof(ConstValue);
          mStack.reserve(mStack.size + memSize);
          inPlaceValue(mStack.end(), b);
          mStack.increment(memSize);
        }
      }
    #ifdef LFJ_HANDLER_DEBUG
      ++valCount;
      if (print) std::cout << "Bool(" << std::boolalpha << b << ")" << std::endl;
    #endif
      return true;
    }
    
    bool pushInt(int i) { return pushInt64((int64_t)i); }
    
    bool pushUInt(unsigned u) { return pushInt64((int64_t)u); } // preferred because of IARRAY
    
    bool pushInt64(int64_t i64)
    {
      // push on stack
      if (mMemberVal)
      {
        assert(((JValue*)mStack.lastValue())->isNul());
        inPlaceValue(mStack.lastValue(), i64);
        mMemberVal = false;
      }
      else
      {
        bool specialized = convertedFor(JType::IARRAY);
        if (specialized)
        {
          if (mArrayType == JType::IARRAY)
          {
            const uint64_t memSize = sizeof(int64_t);
            mStack.reserve(mStack.size + memSize);
            int64_t* dst = (int64_t*)mStack.end();
            *dst = i64;
            mStack.increment(memSize);
          }
          else // JType::DARRAY
          {
            assert(mArrayType == JType::DARRAY);
            const uint64_t memSize = sizeof(double);
            mStack.reserve(mStack.size + memSize);
            double* dst = (double*)mStack.end();
            *dst = (double)i64;
            mStack.increment(memSize);
          }
        }
        else
        {
          const uint64_t memSize = sizeof(ConstValue);
          mStack.reserve(mStack.size + memSize);
          inPlaceValue(mStack.end(), i64);
          mStack.increment(memSize);
        }
      }
    #ifdef LFJ_HANDLER_DEBUG
      ++valCount;
      if (print) std::cout << "Int64(" << i64 << ")" << std::endl;
    #endif
      return true;
    }
    
    bool pushUInt64(uint64_t u64)
    {
      if (u64 <= LFJ_MAX_INT64)
        return pushInt64((int64_t)u64); // preferred because of IARRAY
      
      // push on stack
      if (mMemberVal)
      {
        assert(((JValue*)mStack.lastValue())->isNul());
        inPlaceValue(mStack.lastValue(), u64);
        mMemberVal = false;
      }
      else
      {
        convertedFor(JType::ARRAY);
        const uint64_t memSize = sizeof(ConstValue);
        mStack.reserve(mStack.size + memSize);
        inPlaceValue(mStack.end(), u64);
        mStack.increment(memSize);
      }
    #ifdef LFJ_HANDLER_DEBUG
      ++valCount;
      if (print) std::cout << "Uint64(" << u64 << ")" << std::endl;
    #endif
      return true;
    }
    
    bool pushDouble(double d)
    {
      // push on stack
      if (mMemberVal)
      {
        assert(((JValue*)mStack.lastValue())->isNul());
        inPlaceValue(mStack.lastValue(), d);
        mMemberVal = false;
      }
      else
      {
        bool specialized = convertedFor(JType::DARRAY);
        if (specialized)
        {
          const uint64_t memSize = sizeof(double);
          mStack.reserve(mStack.size + memSize);
          double* dst = (double*)mStack.end();
          *dst = d;
          mStack.increment(memSize);
        }
        else
        {
          const uint64_t memSize = sizeof(ConstValue);
          mStack.reserve(mStack.size + memSize);
          inPlaceValue(mStack.end(), d);
          mStack.increment(memSize);
        }
      }
    #ifdef LFJ_HANDLER_DEBUG
      ++valCount;
      if (print) std::cout << "Double(" << d << ")" << std::endl;
    #endif
      return true;
    }
    
    bool pushString(const char* str, bool copy, int32_t length = -1)
    {
      // push on stack
      if (mMemberVal)
      {
        assert(((JValue*)mStack.lastValue())->isNul());
        if (copy)
          inPlaceValue(mStack.lastValue(), (char*)str, length);
        else
          inPlaceValue(mStack.lastValue(), str, length);
        mMemberVal = false;
      }
      else
      {
        convertedFor(JType::ARRAY);
        const uint64_t memSize = sizeof(ConstValue);
        mStack.reserve(mStack.size + memSize);
        if (copy)
          inPlaceValue(mStack.end(), (char*)str, length);
        else
          inPlaceValue(mStack.end(), str, length);
        mStack.increment(memSize);
      }
    #ifdef LFJ_HANDLER_DEBUG
      ++valCount;
      if (print) std::cout << "String(" << str << ", " << length << ", " << std::boolalpha << copy << ")" << std::endl;
    #endif
      return true;
    }
  };
  
private:
  JValue mRoot;
  SharedStringPool mSPA;
  ObjectPoolAllocator<ObjectChunkSize, Allocator> mOPA;
  
  JValue* getValue(const JValue& value, const JString* & jKey)
  {
    assert(value.type() == JType::OBJECT);
    
    JValue* res = nullptr;
    uint32_t size = value.objectSize();
    if (size == 0u)
      return res;
    
    JMember* members = value.oMembers();
    for (uint32_t i = 0u; i < size; ++i)
    {
      JMember& member = members[i];
      if (member.jkey() == jKey)
      {
        res = &member.jvalue();
        return res;
      }
    }
    return res;
  }

public:
  Document() : mSPA(std::make_shared<StringPool<StringChunkSize, Allocator>>()), mOPA(mSPA->allocator()) {}
  Document(const SharedStringPool& spa) : mSPA(spa), mOPA(mSPA->allocator()) {}
  
  Document(const Document& ot) = delete;
  Document& operator=(const Document&) = delete;
  
  // Accessors
  RefValue    root()        { return { *this, mRoot }; }
  ConstValue& croot() const { return (ConstValue&)mRoot; }
  
  Allocator& baseAllocator() { return mSPA->allocator(); }
  ObjectPoolAllocator<ObjectChunkSize, Allocator>& objectAllocator() { return mOPA; }
  const SharedStringPool& stringPool() const { return mSPA; }
  
  // Modifiers
  void clear()
  {
    clearObjects();
    clearStrings();
  }
  
  void clearObjects()
  {
    mRoot.forceNull();
    mOPA.clear();
  }
  
  void clearStrings()
  {
    mSPA->clear();
  }
  
  void shrink(bool rehashStringPool = false)
  {
    mOPA.shrink();
    mSPA->shrink(rehashStringPool);
  }
  
  // Factories
  static SharedStringPool makeSharedStringPool()
  {
    return std::make_shared<StringPool<StringChunkSize, Allocator>>();
  }
  
  Handler makeHandler(bool allowIntToDouble = true)
  {
    return Handler(*this, allowIntToDouble);
  }
};

// Helper aliases
using DynamicDocument = Document<>;

template <class Allocator, uint16_t ChunkSize = LFJ_DOCUMENT_DFLT_CHUNKSIZE>
using CustomDocument = Document<ChunkSize, Allocator>;

} // namespace lfjson

#endif // LFJSON_DOCUMENT_H
