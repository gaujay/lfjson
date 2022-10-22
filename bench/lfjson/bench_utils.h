/**
 * Copyright 2022 Guillaume AUJAY. All rights reserved.
 *
 */

#ifndef LFJSON_BENCH_UTILS_H
#define LFJSON_BENCH_UTILS_H

// 3rd-parties
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"

// Src
#include "lfjson/lfjson.h"
using namespace  lfjson;

// Std
#include <cstdint>
#include <cassert>
#include <iostream>


template <uint16_t StringChunkSize = LFJ_DOCUMENT_DFLT_CHUNKSIZE,
          class Allocator = StdAllocator,
          uint16_t ObjectChunkSize = StringChunkSize>
struct RapidHandler : public rapidjson::BaseReaderHandler<rapidjson::UTF8<>, RapidHandler<StringChunkSize, Allocator, ObjectChunkSize>>
{
  typename Document<StringChunkSize, Allocator, ObjectChunkSize>::Handler& handler;
  
  RapidHandler(typename Document<StringChunkSize, Allocator, ObjectChunkSize>::Handler& handler_) : handler(handler_) {}
  
  bool Null()               { return handler.pushNull(); }
  bool Bool(bool b)         { return handler.pushBool(b); }
  bool Int(int i)           { return handler.pushInt(i); }
  bool Uint(unsigned u)     { return handler.pushUInt(u); }
  bool Int64(int64_t i64)   { return handler.pushInt64(i64); }
  bool Uint64(uint64_t u64) { return handler.pushUInt64(u64); }
  bool Double(double d)     { return handler.pushDouble(d); }
  bool String(const rapidjson::UTF8<>::Ch* str, rapidjson::SizeType length, bool copy)
  {
    assert(length <= LFJ_JSTRING_MAX_LEN);
    return handler.pushString((const char*)str, copy, (int32_t)length);
  }
  bool Key(const rapidjson::UTF8<>::Ch* str, rapidjson::SizeType length, bool copy)
  {
    assert(length <= LFJ_JSTRING_MAX_LEN);
    return handler.pushKey((const char*)str, copy, (int32_t)length);
  }
  bool StartObject()                                { return handler.startObject(); }
  bool EndObject(rapidjson::SizeType memberCount)   { return handler.endObject((uint32_t)memberCount); }
  bool StartArray()                                 { return handler.startArray(); }
  bool EndArray(rapidjson::SizeType elementCount)   { return handler.endArray((uint32_t)elementCount); }
};


class RapidWriter
{
private:
  // Writer
  static void printObject(rapidjson::Writer<rapidjson::StringBuffer>& writer, const ConstValue& val)
  {
    writer.StartObject();
    const auto end = val.objectMembers() + val.objectSize();
    for (auto it = val.objectMembers(); it < end; ++it)
    {
      writer.Key(it->key());
      printVal(writer, it->value());
    }
    writer.EndObject();
  }
  
  static void printArray(rapidjson::Writer<rapidjson::StringBuffer>& writer, const ConstValue& val)
  {
    assert(val.isArray());
    writer.StartArray();
    
    const ConstValue* array = val.arrayValues();
    const uint32_t size = val.arraySize();
    for (uint32_t i = 0; i < size; ++i)
      printVal(writer, array[i]);
    
    writer.EndArray();
  }
  
  static void printBArray(rapidjson::Writer<rapidjson::StringBuffer>& writer, const ConstValue& val)
  {
    assert(val.isBArray());
    writer.StartArray();
    
    const bool* barray = val.barrayValues();
    const uint32_t size = val.barraySize();
    for (uint32_t i = 0; i < size; ++i)
      writer.Bool(barray[i]);
    
    writer.EndArray();
  }
  
  static void printIArray(rapidjson::Writer<rapidjson::StringBuffer>& writer, const ConstValue& val)
  {
    assert(val.isIArray());
    writer.StartArray();
    
    const int64_t* iarray = val.iarrayValues();
    const uint32_t size = val.iarraySize();
    for (uint32_t i = 0; i < size; ++i)
      writer.Int64(iarray[i]);
    
    writer.EndArray();
  }
  
  static void printDArray(rapidjson::Writer<rapidjson::StringBuffer>& writer, const ConstValue& val)
  {
    assert(val.isDArray());
    writer.StartArray();
    
    const double* darray = val.darrayValues();
    const uint32_t size = val.darraySize();
    for (uint32_t i = 0; i < size; ++i)
      writer.Double(darray[i]);
    
    writer.EndArray();
  }
  
  static void printVal(rapidjson::Writer<rapidjson::StringBuffer>& writer, const ConstValue& val)
  {
    switch (val.type())
    {
      case JType::OBJECT:   { printObject(writer, val); break; }
      case JType::ARRAY:    { printArray(writer,  val); break; }
      case JType::BARRAY:   { printBArray(writer, val); break; }
      case JType::IARRAY:   { printIArray(writer, val); break; }
      case JType::DARRAY:   { printDArray(writer, val); break; }
      case JType::SSTRING:  { writer.String(val.getShortString(), val.shortStringSize()); break; }
      case JType::LSTRING:  { writer.String(val.getLongString(),  val.longStringSize());  break; }
      case JType::INT64:    { writer.Int64(val.getInt64());   break; }
      case JType::UINT64:   { writer.Uint64(val.getUInt64()); break; }
      case JType::DOUBLE:   { writer.Double(val.getDouble()); break; }
      case JType::TRUE:     { writer.Bool(true);  break; }
      case JType::FALSE:    { writer.Bool(false); break; }
      case JType::NUL:      { writer.Null();  break; }
      default:
        assert(false && "[lfjson] printVal: unknown type");
    }
  }
  
  // PrettyWriter
  static void printObject(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer, const ConstValue& val)
  {
    writer.StartObject();
    const auto end = val.objectMembers() + val.objectSize();
    for (auto it = val.objectMembers(); it < end; ++it)
    {
      writer.Key(it->key());
      printVal(writer, it->value());
    }
    writer.EndObject();
  }
  
  static void printArray(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer, const ConstValue& val)
  {
    assert(val.isArray());
    writer.StartArray();
    
    const ConstValue* array = val.arrayValues();
    const uint32_t size = val.arraySize();
    for (uint32_t i = 0; i < size; ++i)
      printVal(writer, array[i]);
    
    writer.EndArray();
  }
  
  static void printBArray(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer, const ConstValue& val)
  {
    assert(val.isBArray());
    writer.StartArray();
    
    const bool* barray = val.barrayValues();
    const uint32_t size = val.barraySize();
    for (uint32_t i = 0; i < size; ++i)
      writer.Bool(barray[i]);
    
    writer.EndArray();
  }
  
  static void printIArray(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer, const ConstValue& val)
  {
    assert(val.isIArray());
    writer.StartArray();
    
    const int64_t* iarray = val.iarrayValues();
    const uint32_t size = val.iarraySize();
    for (uint32_t i = 0; i < size; ++i)
      writer.Int64(iarray[i]);
    
    writer.EndArray();
  }
  
  static void printDArray(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer, const ConstValue& val)
  {
    assert(val.isDArray());
    writer.StartArray();
    
    const double* darray = val.darrayValues();
    const uint32_t size = val.darraySize();
    for (uint32_t i = 0; i < size; ++i)
      writer.Double(darray[i]);
    
    writer.EndArray();
  }
  
  static void printVal(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer, const ConstValue& val)
  {
    switch (val.type())
    {
      case JType::OBJECT:   { printObject(writer, val); break; }
      case JType::ARRAY:    { printArray(writer,  val); break; }
      case JType::BARRAY:   { printBArray(writer, val); break; }
      case JType::IARRAY:   { printIArray(writer, val); break; }
      case JType::DARRAY:   { printDArray(writer, val); break; }
      case JType::SSTRING:  { writer.String(val.getShortString(), val.shortStringSize()); break; }
      case JType::LSTRING:  { writer.String(val.getLongString(),  val.longStringSize());  break; }
      case JType::INT64:    { writer.Int64(val.getInt64());   break; }
      case JType::UINT64:   { writer.Uint64(val.getUInt64()); break; }
      case JType::DOUBLE:   { writer.Double(val.getDouble()); break; }
      case JType::TRUE:     { writer.Bool(true);  break; }
      case JType::FALSE:    { writer.Bool(false); break; }
      case JType::NUL:      { writer.Null();  break; }
      default:
        assert(false && "[lfjson] printVal: unknown type");
    }
  }
  
public:
  static void write(rapidjson::StringBuffer& buffer, const ConstValue& root)
  {
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    
    printVal(writer, root);
  }
  
  static void prettyWrite(rapidjson::StringBuffer& buffer, const ConstValue& root, char indentChar = ' ', unsigned int indentCharCount = 2)
  {
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    writer.SetIndent(indentChar, indentCharCount);
    
    printVal(writer, root);
  }
  
  static void printDoc(const ConstValue& root, char indentChar = ' ', unsigned int indentCharCount = 2)
  {
    rapidjson::StringBuffer buffer;
    prettyWrite(buffer, root, indentChar, indentCharCount);
    
    std::cout << buffer.GetString() << std::endl;
  }
};

#endif // LFJSON_BENCH_UTILS_H
