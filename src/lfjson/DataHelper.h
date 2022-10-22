/**
 * Copyright 2022 Guillaume AUJAY. All rights reserved.
 *
 */

#ifndef LFJSON_DATAHELPER_H
#define LFJSON_DATAHELPER_H

#include "BaseData.h"
#include "PoolAllocator.h"

#include <cstdint>
#include <cmath>
#include <cstring>
#include <cassert>

namespace lfjson {
namespace helper
{
template <uint16_t ChunkSize, class Allocator>
void arrayReserve(JValue& value, uint32_t newCapacity, ObjectPoolAllocator<ChunkSize, Allocator>& opa)
{
  assert(value.type() == JType::ARRAY);
  const uint32_t capacity = value.arrayCapacity();
  if (newCapacity <= capacity)
    return;
  
  const uint32_t size = value.arraySize();
  JValue* oldValues = value.aValues();
  
  if (newCapacity < LFJ_MAX_UINT16)
  {
    JValue* newValues = oldValues;
    if (!opa.realloc(oldValues, capacity * sizeof(JValue), newCapacity * sizeof(JValue)))  // try in-place
    {
      newValues = (JValue*)opa.allocate(newCapacity * sizeof(JValue));
      
      if (capacity > 0u)
      {
        std::memcpy((void*)newValues, (void*)oldValues, size * sizeof(JValue));
        opa.deallocate(oldValues, capacity * sizeof(JValue));
      }
    }
    value.setAA(newValues);
    value.setACapa((uint16_t)newCapacity);
  }
  else  // Big array
  {
    JBigArray* newBigArray = (JBigArray*)opa.allocate(sizeof(JBigArray) + (newCapacity - 1) * sizeof(JValue));
    newBigArray->capa = newCapacity;
    
    if (capacity > 0u)
    {
      std::memcpy((void*)newBigArray->data, (void*)oldValues, size * sizeof(JValue));
      if (capacity < LFJ_MAX_UINT16)
        opa.deallocate(oldValues, capacity * sizeof(JValue));
      else
        opa.deallocate(value.aBA(), sizeof(JBigArray) + (capacity - 1) * sizeof(JValue));
    }
    value.setABA(newBigArray);
    value.setACapa(LFJ_MAX_UINT16);
  }
}

template <uint16_t ChunkSize, class Allocator>
void barrayReserve(JValue& value, uint32_t newCapacity, ObjectPoolAllocator<ChunkSize, Allocator>& opa)
{
  assert(value.type() == JType::BARRAY);
  const uint32_t capacity = value.barrayCapacity();
  if (newCapacity <= capacity)
    return;
  
  const uint32_t size = value.barraySize();
  bool* oldValues = value.baValues();
  
  if (newCapacity < LFJ_MAX_UINT16)
  {
    bool* newValues = oldValues;
    if (!opa.realloc(oldValues, capacity * sizeof(bool), newCapacity * sizeof(bool)))  // try in-place
    {
      newValues = (bool*)opa.allocate(newCapacity * sizeof(bool));
      
      if (capacity > 0u)
      {
        std::memcpy((void*)newValues, (void*)oldValues, size * sizeof(bool));
        opa.deallocate(oldValues, capacity * sizeof(bool));
      }
    }
    value.setAB(newValues);
    value.setBACapa((uint16_t)newCapacity);
  }
  else  // Big bool array
  {
    JBigBArray* newBigBArray = (JBigBArray*)opa.allocate(sizeof(JBigBArray) + (newCapacity - 1) * sizeof(bool));
    newBigBArray->capa = newCapacity;
    
    if (capacity > 0u)
    {
      std::memcpy((void*)newBigBArray->data, (void*)oldValues, size * sizeof(bool));
      if (capacity < LFJ_MAX_UINT16)
        opa.deallocate(oldValues, capacity * sizeof(bool));
      else
        opa.deallocate(value.baBA(), sizeof(JBigBArray) + (capacity - 1) * sizeof(bool));
    }
    value.setABB(newBigBArray);
    value.setBACapa(LFJ_MAX_UINT16);
  }
}

template <uint16_t ChunkSize, class Allocator>
void iarrayReserve(JValue& value, uint32_t newCapacity, ObjectPoolAllocator<ChunkSize, Allocator>& opa)
{
  assert(value.type() == JType::IARRAY);
  const uint32_t capacity = value.iarrayCapacity();
  if (newCapacity <= capacity)
    return;
  
  const uint32_t size = value.iarraySize();
  int64_t* oldValues = value.iaValues();
  
  if (newCapacity < LFJ_MAX_UINT16)
  {
    int64_t* newValues = oldValues;
    if (!opa.realloc(oldValues, capacity * sizeof(int64_t), newCapacity * sizeof(int64_t)))  // try in-place
    {
      newValues = (int64_t*)opa.allocate(newCapacity * sizeof(int64_t));
      
      if (capacity > 0u)
      {
        std::memcpy((void*)newValues, (void*)oldValues, size * sizeof(int64_t));
        opa.deallocate(oldValues, capacity * sizeof(int64_t));
      }
    }
    value.setAI(newValues);
    value.setIACapa((uint16_t)newCapacity);
  }
  else  // Big int64 array
  {
    JBigIArray* newBigIArray = (JBigIArray*)opa.allocate(sizeof(JBigIArray) + (newCapacity - 1) * sizeof(int64_t));
    newBigIArray->capa = newCapacity;
    
    if (capacity > 0u)
    {
      std::memcpy((void*)newBigIArray->data, (void*)oldValues, size * sizeof(int64_t));
      if (capacity < LFJ_MAX_UINT16)
        opa.deallocate(oldValues, capacity * sizeof(int64_t));
      else
        opa.deallocate(value.iaBA(), sizeof(JBigIArray) + (capacity - 1) * sizeof(int64_t));
    }
    value.setABI(newBigIArray);
    value.setIACapa(LFJ_MAX_UINT16);
  }
}

template <uint16_t ChunkSize, class Allocator>
void darrayReserve(JValue& value, uint32_t newCapacity, ObjectPoolAllocator<ChunkSize, Allocator>& opa)
{
  assert(value.type() == JType::DARRAY);
  const uint32_t capacity = value.darrayCapacity();
  if (newCapacity <= capacity)
    return;
  
  const uint32_t size = value.darraySize();
  double* oldValues = value.daValues();
  
  if (newCapacity < LFJ_MAX_UINT16)
  {
    double* newValues = oldValues;
    if (!opa.realloc(oldValues, capacity * sizeof(double), newCapacity * sizeof(double)))  // try in-place
    {
      newValues = (double*)opa.allocate(newCapacity * sizeof(double));
      
      if (capacity > 0u)
      {
        std::memcpy((void*)newValues, (void*)oldValues, size * sizeof(double));
        opa.deallocate(oldValues, capacity * sizeof(double));
      }
    }
    value.setAD(newValues);
    value.setDACapa((uint16_t)newCapacity);
  }
  else  // Big double array
  {
    JBigDArray* newBigDArray = (JBigDArray*)opa.allocate(sizeof(JBigDArray) + (newCapacity - 1) * sizeof(double));
    newBigDArray->capa = newCapacity;
    
    if (capacity > 0u)
    {
      std::memcpy((void*)newBigDArray->data, (void*)oldValues, size * sizeof(double));
      if (capacity < LFJ_MAX_UINT16)
        opa.deallocate(oldValues, capacity * sizeof(double));
      else
        opa.deallocate(value.daBA(), sizeof(JBigDArray) + (capacity - 1) * sizeof(double));
    }
    value.setABD(newBigDArray);
    value.setDACapa(LFJ_MAX_UINT16);
  }
}

template <uint16_t ChunkSize, class Allocator>
void objectReserve(JValue& value, uint32_t newCapacity, ObjectPoolAllocator<ChunkSize, Allocator>& opa)
{
  assert(value.type() == JType::OBJECT);
  const uint32_t capacity = value.objectCapacity();
  if (newCapacity <= capacity)
    return;
  
  uint32_t size = value.objectSize();
  JMember* oldMembers = value.oMembers();
  
  if (newCapacity < LFJ_MAX_UINT16)
  {
    if (!opa.realloc(oldMembers, capacity * sizeof(JMember), newCapacity * sizeof(JMember)))  // try in-place
    {
      JMember* newMembers = (JMember*)opa.allocate(newCapacity * sizeof(JMember));
      
      if (capacity > 0u)
      {
        std::memcpy((void*)newMembers, (void*)oldMembers, size * sizeof(JMember));
        opa.deallocate(oldMembers, capacity * sizeof(JMember));
      }
      value.setOO(newMembers);
    }
    value.setOCapa((uint16_t)newCapacity);
  }
  else  // Big object
  {
    JBigObject* newBigObject = (JBigObject*)opa.allocate(sizeof(JBigObject) + (newCapacity - 1) * sizeof(JMember));
    newBigObject->capa = newCapacity;
    
    std::memcpy((void*)newBigObject->data, (void*)oldMembers, size * sizeof(JMember));
    if (capacity < LFJ_MAX_UINT16)
      opa.deallocate(oldMembers, capacity * sizeof(JMember));
    else
      opa.deallocate(value.oBO(), sizeof(JBigObject) + (capacity - 1) * sizeof(JMember));
    
    value.setOBO(newBigObject);
    value.setOCapa(LFJ_MAX_UINT16);
  }
}

template <uint16_t ChunkSize, class Allocator>
void arrayGrow(JValue& value, ObjectPoolAllocator<ChunkSize, Allocator>& opa)
{
  assert(value.type() == JType::ARRAY);
  
  const uint32_t capacity = value.arrayCapacity();
  uint32_t newCapacity = (capacity > 0u) ? (uint32_t)std::ceil(capacity * JValue::Array_GrowthFactor) : 1u;
  
  arrayReserve(value, newCapacity, opa);
}

template <uint16_t ChunkSize, class Allocator>
void barrayGrow(JValue& value, ObjectPoolAllocator<ChunkSize, Allocator>& opa)
{
  assert(value.type() == JType::BARRAY);
  
  const uint32_t capacity = value.barrayCapacity();
  uint32_t newCapacity = (capacity > 0u) ? (uint32_t)std::ceil(capacity * JValue::Array_GrowthFactor) : 1u;
  
  barrayReserve(value, newCapacity, opa);
}

template <uint16_t ChunkSize, class Allocator>
void iarrayGrow(JValue& value, ObjectPoolAllocator<ChunkSize, Allocator>& opa)
{
  assert(value.type() == JType::IARRAY);
  
  const uint32_t capacity = value.iarrayCapacity();
  uint32_t newCapacity = (capacity > 0u) ? (uint32_t)std::ceil(capacity * JValue::Array_GrowthFactor) : 1u;
  
  iarrayReserve(value, newCapacity, opa);
}

template <uint16_t ChunkSize, class Allocator>
void darrayGrow(JValue& value, ObjectPoolAllocator<ChunkSize, Allocator>& opa)
{
  assert(value.type() == JType::DARRAY);
  
  const uint32_t capacity = value.darrayCapacity();
  uint32_t newCapacity = (capacity > 0u) ? (uint32_t)std::ceil(capacity * JValue::Array_GrowthFactor) : 1u;
  
  darrayReserve(value, newCapacity, opa);
}

template <uint16_t ChunkSize, class Allocator>
void objectGrow(JValue& value, ObjectPoolAllocator<ChunkSize, Allocator>& opa)
{
  assert(value.type() == JType::OBJECT);
  
  const uint32_t capacity = value.objectCapacity();
  uint32_t newCapacity = (capacity > 0u) ? (uint32_t)std::ceil(capacity * JValue::Object_GrowthFactor) : 1u;
  
  objectReserve(value, newCapacity, opa);
}

template <uint16_t ChunkSize, class Allocator>
void arrayShrink(JValue& value, ObjectPoolAllocator<ChunkSize, Allocator>& opa)
{
  assert(value.type() == JType::ARRAY);
  const uint32_t size = value.arraySize();
  const uint32_t capacity = value.arrayCapacity();
  if (size == capacity)
    return;
  
  if (capacity < LFJ_MAX_UINT16)  // Array
  {
    JValue* oldValues = value.aA();
    if (opa.chunkable(opa.alignSize(capacity * sizeof(JValue)))) // in-place shrink
    {
      opa.deallocate(oldValues + size, (capacity - size) * sizeof(JValue));
    }
    else // re-alloc
    {
      if (size > 0u)
      {
        JValue* newValues = (JValue*)opa.allocate(size * sizeof(JValue));
        std::memcpy((void*)newValues, (void*)oldValues, size * sizeof(JValue));
        value.setAA(newValues);
      }
      opa.deallocate(oldValues, capacity * sizeof(JValue));
    }
    value.setACapa((uint16_t)size);
  }
  else if (size < LFJ_MAX_UINT16) // Big array to array
  {
    if (size > 0u)
    {
      JValue* oldValues = value.aBA()->data;
      JValue* newValues = (JValue*)opa.allocate(size * sizeof(JValue));
      
      std::memcpy((void*)newValues, (void*)oldValues, size * sizeof(JValue));
      opa.deallocate(value.aBA(), sizeof(JBigArray) + (capacity - 1) * sizeof(JValue));
      
      value.setAA(newValues);
    }
    else
    {
      opa.deallocate(value.aBA(), sizeof(JBigArray) + (capacity - 1) * sizeof(JValue));
    }
    value.setACapa((uint16_t)size);
  }
  else  // Big array
  {
    JValue* oldValues = value.aBA()->data;
    JBigArray* newBigArray = (JBigArray*)opa.allocate(sizeof(JBigArray) + (size - 1) * sizeof(JValue));
    newBigArray->capa = size;
    
    std::memcpy((void*)newBigArray->data, (void*)oldValues, size * sizeof(JValue));
    opa.deallocate(value.aBA(), sizeof(JBigArray) + (capacity - 1) * sizeof(JValue));
    
    value.setABA(newBigArray);
  }
}

template <uint16_t ChunkSize, class Allocator>
void barrayShrink(JValue& value, ObjectPoolAllocator<ChunkSize, Allocator>& opa)
{
  assert(value.type() == JType::BARRAY);
  const uint32_t size = value.barraySize();
  const uint32_t capacity = value.barrayCapacity();
  if (size == capacity)
    return;
  
  if (capacity < LFJ_MAX_UINT16)  // Array
  {
    bool* oldValues = value.baA();
    if (opa.chunkable(opa.alignSize(capacity * sizeof(bool)))) // in-place shrink
    {
      opa.deallocate(oldValues + size, (capacity - size) * sizeof(bool));
    }
    else // re-alloc
    {
      if (size > 0u)
      {
        bool* newValues = (bool*)opa.allocate(size * sizeof(bool));
        std::memcpy((void*)newValues, (void*)oldValues, size * sizeof(bool));
        value.setAB(newValues);
      }
      opa.deallocate(oldValues, capacity * sizeof(bool));
    }
    value.setBACapa((uint16_t)size);
  }
  else if (size < LFJ_MAX_UINT16) // Big array to array
  {
    if (size > 0u)
    {
      bool* oldValues = value.baBA()->data;
      bool* newValues = (bool*)opa.allocate(size * sizeof(bool));
      
      std::memcpy((void*)newValues, (void*)oldValues, size * sizeof(bool));
      opa.deallocate(value.baBA(), sizeof(JBigBArray) + (capacity - 1) * sizeof(bool));
      
      value.setAB(newValues);
    }
    else
    {
      opa.deallocate(value.baBA(), sizeof(JBigBArray) + (capacity - 1) * sizeof(bool));
    }
    value.setBACapa((uint16_t)size);
  }
  else  // Big array
  {
    bool* oldValues = value.baBA()->data;
    JBigBArray* newBigBArray = (JBigBArray*)opa.allocate(sizeof(JBigBArray) + (size - 1) * sizeof(bool));
    newBigBArray->capa = size;
    
    std::memcpy((void*)newBigBArray->data, (void*)oldValues, size * sizeof(bool));
    opa.deallocate(value.baBA(), sizeof(JBigBArray) + (capacity - 1) * sizeof(bool));
    
    value.setABB(newBigBArray);
  }
}

template <uint16_t ChunkSize, class Allocator>
void iarrayShrink(JValue& value, ObjectPoolAllocator<ChunkSize, Allocator>& opa)
{
  assert(value.type() == JType::IARRAY);
  const uint32_t size = value.iarraySize();
  const uint32_t capacity = value.iarrayCapacity();
  if (size == capacity)
    return;
  
  if (capacity < LFJ_MAX_UINT16)  // Array
  {
    int64_t* oldValues = value.iaA();
    if (opa.chunkable(opa.alignSize(capacity * sizeof(int64_t)))) // in-place shrink
    {
      opa.deallocate(oldValues + size, (capacity - size) * sizeof(int64_t));
    }
    else // re-alloc
    {
      if (size > 0u)
      {
        int64_t* newValues = (int64_t*)opa.allocate(size * sizeof(int64_t));
        std::memcpy((void*)newValues, (void*)oldValues, size * sizeof(int64_t));
        value.setAI(newValues);
      }
      opa.deallocate(oldValues, capacity * sizeof(int64_t));
    }
    value.setIACapa((uint16_t)size);
  }
  else if (size < LFJ_MAX_UINT16) // Big array to array
  {
    if (size > 0u)
    {
      int64_t* oldValues = value.iaBA()->data;
      int64_t* newValues = (int64_t*)opa.allocate(size * sizeof(int64_t));
      
      std::memcpy((void*)newValues, (void*)oldValues, size * sizeof(int64_t));
      opa.deallocate(value.iaBA(), sizeof(JBigIArray) + (capacity - 1) * sizeof(int64_t));
      
      value.setAI(newValues);
    }
    else
    {
      opa.deallocate(value.iaBA(), sizeof(JBigIArray) + (capacity - 1) * sizeof(int64_t));
    }
    value.setIACapa((uint16_t)size);
  }
  else  // Big array
  {
    int64_t* oldValues = value.iaBA()->data;
    JBigIArray* newBigIArray = (JBigIArray*)opa.allocate(sizeof(JBigIArray) + (size - 1) * sizeof(int64_t));
    newBigIArray->capa = size;
    
    std::memcpy((void*)newBigIArray->data, (void*)oldValues, size * sizeof(int64_t));
    opa.deallocate(value.iaBA(), sizeof(JBigIArray) + (capacity - 1) * sizeof(int64_t));
    
    value.setABI(newBigIArray);
  }
}

template <uint16_t ChunkSize, class Allocator>
void darrayShrink(JValue& value, ObjectPoolAllocator<ChunkSize, Allocator>& opa)
{
  assert(value.type() == JType::DARRAY);
  const uint32_t size = value.darraySize();
  const uint32_t capacity = value.darrayCapacity();
  if (size == capacity)
    return;
  
  if (capacity < LFJ_MAX_UINT16)  // Array
  {
    double* oldValues = value.daA();
    if (opa.chunkable(opa.alignSize(capacity * sizeof(double)))) // in-place shrink
    {
      opa.deallocate(oldValues + size, (capacity - size) * sizeof(double));
    }
    else // re-alloc
    {
      if (size > 0u)
      {
        double* newValues = (double*)opa.allocate(size * sizeof(double));
        std::memcpy((void*)newValues, (void*)oldValues, size * sizeof(double));
        value.setAD(newValues);
      }
      opa.deallocate(oldValues, capacity * sizeof(double));
    }
    value.setDACapa((uint16_t)size);
  }
  else if (size < LFJ_MAX_UINT16) // Big array to array
  {
    if (size > 0u)
    {
      double* oldValues = value.daBA()->data;
      double* newValues = (double*)opa.allocate(size * sizeof(double));
      
      std::memcpy((void*)newValues, (void*)oldValues, size * sizeof(double));
      opa.deallocate(value.daBA(), sizeof(JBigDArray) + (capacity - 1) * sizeof(double));
      
      value.setAD(newValues);
    }
    else
    {
      opa.deallocate(value.daBA(), sizeof(JBigDArray) + (capacity - 1) * sizeof(double));
    }
    value.setDACapa((uint16_t)size);
  }
  else  // Big array
  {
    double* oldValues = value.daBA()->data;
    JBigDArray* newBigDArray = (JBigDArray*)opa.allocate(sizeof(JBigDArray) + (size - 1) * sizeof(double));
    newBigDArray->capa = size;
    
    std::memcpy((void*)newBigDArray->data, (void*)oldValues, size * sizeof(double));
    opa.deallocate(value.daBA(), sizeof(JBigDArray) + (capacity - 1) * sizeof(double));
    
    value.setABD(newBigDArray);
  }
}

template <uint16_t ChunkSize, class Allocator>
void objectShrink(JValue& value, ObjectPoolAllocator<ChunkSize, Allocator>& opa)
{
  assert(value.type() == JType::OBJECT);
  const uint32_t size = value.objectSize();
  const uint32_t capacity = value.objectCapacity();
  if (size == capacity)
    return;
  
  if (capacity < LFJ_MAX_UINT16)  // Object
  {
    JMember* oldMembers = value.oO();
    if (opa.chunkable(opa.alignSize(capacity * sizeof(JMember)))) // in-place shrink
    {
     opa.deallocate(oldMembers + size, (capacity - size) * sizeof(JMember));
    }
    else  // re-alloc
    {
      if (size > 0u)
      {
        JMember* newMembers = (JMember*)opa.allocate(size * sizeof(JMember));
        std::memcpy((void*)newMembers, (void*)oldMembers, size * sizeof(JMember));
        value.setOO(newMembers);
      }
      opa.deallocate(oldMembers, capacity * sizeof(JMember));
    }
    value.setOCapa((uint16_t)size);
  }
  else if (size < LFJ_MAX_UINT16) // Big object to object
  {
    if (size > 0u)
    {
      JMember* oldMembers = value.oBO()->data;
      JMember* newMembers = (JMember*)opa.allocate(size * sizeof(JMember));
      
      std::memcpy((void*)newMembers, (void*)oldMembers, size * sizeof(JMember));
      opa.deallocate(value.oBO(), sizeof(JBigObject) + (capacity - 1) * sizeof(JMember));
      
      value.setOO(newMembers);
    }
    else
    {
      opa.deallocate(value.oBO(), sizeof(JBigObject) + (capacity - 1) * sizeof(JMember));
    }
    value.setOCapa((uint16_t)size);
  }
  else  // Big object
  {
    JMember* oldMembers = value.oBO()->data;
    JBigObject* newBigObject = (JBigObject*)opa.allocate(sizeof(JBigObject) + (size - 1) * sizeof(JMember));
    newBigObject->capa = size;
    
    std::memcpy((void*)newBigObject->data, (void*)oldMembers, size * sizeof(JMember));
    opa.deallocate(value.oBO(), sizeof(JBigObject) + (capacity - 1) * sizeof(JMember));
    
    value.setOBO(newBigObject);
  }
}

void arrayOverwrite(JValue& value, JValue* it)
{
  assert(value.type() == JType::ARRAY);
  JValue* end = value.aValues() + value.arraySize();
  
  std::memmove(it, it + 1, (end - 1 - it) * sizeof(JValue));
  value.decASize();
}

void barrayOverwrite(JValue& value, bool* it)
{
  assert(value.type() == JType::BARRAY);
  bool* end = value.baValues() + value.barraySize();
  
  std::memmove(it, it + 1, (end - 1 - it) * sizeof(bool));
  value.decBASize();
}

void iarrayOverwrite(JValue& value, int64_t* it)
{
  assert(value.type() == JType::IARRAY);
  int64_t* end = value.iaValues() + value.iarraySize();
  
  std::memmove(it, it + 1, (end - 1 - it) * sizeof(int64_t));
  value.decIASize();
}

void darrayOverwrite(JValue& value, double* it)
{
  assert(value.type() == JType::DARRAY);
  double* end = value.daValues() + value.darraySize();
  
  std::memmove(it, it + 1, (end - 1 - it) * sizeof(double));
  value.decDASize();
}

void objectOverwrite(JValue& value, JMember* it)
{
  assert(value.type() == JType::OBJECT);
  JMember* end = value.oMembers() + value.objectSize();
  
  std::memmove(it, it + 1, (end - 1 - it) * sizeof(JMember));
  value.decOSize();
}

// Converters
template <uint16_t ChunkSize, class Allocator>
void convertBArrayToArray(JValue& value, uint32_t reserveForExtra, ObjectPoolAllocator<ChunkSize, Allocator>& opa)
{
  assert(value.type() == JType::BARRAY);
  const uint32_t size = value.barraySize();
  const uint32_t capacity = value.barrayCapacity();
  uint32_t newCapacity = size + reserveForExtra;
  newCapacity = (newCapacity < capacity) ? capacity : newCapacity;
  if (newCapacity == 0u)
  {
    value.force(JType::ARRAY);
    return;
  }
  
  JValue* aValues = nullptr;
  JBigArray* newBigArray = nullptr;
  if (newCapacity < LFJ_MAX_UINT16)
    aValues = (JValue*)opa.allocate(newCapacity * sizeof(JValue));
  else
  {
    newBigArray = (JBigArray*)opa.allocate(sizeof(JBigArray) + (newCapacity - 1) * sizeof(JValue));
    newBigArray->capa = newCapacity;
    aValues = newBigArray->data;
  }
  
  bool* bValues = value.baValues();
  if (capacity < LFJ_MAX_UINT16)
  {
    if (capacity > 0u)
    {
      for (uint32_t i = 0; i < size; ++i)
        aValues[i].force(bValues[i]);
      
      opa.deallocate(bValues, capacity * sizeof(bool));
    }
  }
  else
  {
    for (uint32_t i = 0; i < size; ++i)
      aValues[i].force(bValues[i]);
    
    opa.deallocate(value.baBA(), sizeof(JBigBArray) + (capacity - 1) * sizeof(bool));
  }
  
  value.force(JType::ARRAY);
  if (newBigArray == nullptr)
  {
    value.setAA(aValues);
    value.setACapa((uint16_t)newCapacity);
  }
  else
  {
    value.setABA(newBigArray);
    value.setACapa(LFJ_MAX_UINT16);
  }
}

template <uint16_t ChunkSize, class Allocator>
void convertIArrayToArray(JValue& value, uint32_t reserveForExtra, ObjectPoolAllocator<ChunkSize, Allocator>& opa)
{
  assert(value.type() == JType::IARRAY);
  const uint32_t size = value.iarraySize();
  const uint32_t capacity = value.iarrayCapacity();
  uint32_t newCapacity = size + reserveForExtra;
  newCapacity = (newCapacity < capacity) ? capacity : newCapacity;
  if (newCapacity == 0u)
  {
    value.force(JType::ARRAY);
    return;
  }
  
  JValue* aValues = nullptr;
  JBigArray* newBigArray = nullptr;
  if (newCapacity < LFJ_MAX_UINT16)
    aValues = (JValue*)opa.allocate(newCapacity * sizeof(JValue));
  else
  {
    newBigArray = (JBigArray*)opa.allocate(sizeof(JBigArray) + (newCapacity - 1) * sizeof(JValue));
    newBigArray->capa = newCapacity;
    aValues = newBigArray->data;
  }
  
  int64_t* iValues = value.iaValues();
  if (capacity < LFJ_MAX_UINT16)
  {
    if (capacity > 0u)
    {
      for (uint32_t i = 0; i < size; ++i)
        aValues[i].force(iValues[i]);
      
      opa.deallocate(iValues, capacity * sizeof(int64_t));
    }
  }
  else
  {
    for (uint32_t i = 0; i < size; ++i)
      aValues[i].force(iValues[i]);
    
    opa.deallocate(value.iaBA(), sizeof(JBigIArray) + (capacity - 1) * sizeof(int64_t));
  }
  
  value.force(JType::ARRAY);
  if (newBigArray == nullptr)
  {
    value.setAA(aValues);
    value.setACapa((uint16_t)newCapacity);
  }
  else
  {
    value.setABA(newBigArray);
    value.setACapa(LFJ_MAX_UINT16);
  }
}

template <uint16_t ChunkSize, class Allocator>
void convertDArrayToArray(JValue& value, uint32_t reserveForExtra, ObjectPoolAllocator<ChunkSize, Allocator>& opa)
{
  assert(value.type() == JType::DARRAY);
  const uint32_t size = value.darraySize();
  const uint32_t capacity = value.darrayCapacity();
  uint32_t newCapacity = size + reserveForExtra;
  newCapacity = (newCapacity < capacity) ? capacity : newCapacity;
  if (newCapacity == 0u)
  {
    value.force(JType::ARRAY);
    return;
  }
  
  JValue* aValues = nullptr;
  JBigArray* newBigArray = nullptr;
  if (newCapacity < LFJ_MAX_UINT16)
    aValues = (JValue*)opa.allocate(newCapacity * sizeof(JValue));
  else
  {
    newBigArray = (JBigArray*)opa.allocate(sizeof(JBigArray) + (newCapacity - 1) * sizeof(JValue));
    newBigArray->capa = newCapacity;
    aValues = newBigArray->data;
  }
  
  double* dValues = value.daValues();
  if (capacity < LFJ_MAX_UINT16)
  {
    if (capacity > 0u)
    {
      for (uint32_t i = 0; i < size; ++i)
        aValues[i].force(dValues[i]);
      
      opa.deallocate(dValues, capacity * sizeof(double));
    }
  }
  else
  {
    for (uint32_t i = 0; i < size; ++i)
      aValues[i].force(dValues[i]);
    
    opa.deallocate(value.daBA(), sizeof(JBigIArray) + (capacity - 1) * sizeof(double));
  }
  
  value.force(JType::ARRAY);
  if (newBigArray == nullptr)
  {
    value.setAA(aValues);
    value.setACapa((uint16_t)newCapacity);
  }
  else
  {
    value.setABA(newBigArray);
    value.setACapa(LFJ_MAX_UINT16);
  }
}

template <uint16_t ChunkSize, class Allocator>
void convertIArrayToDArray(JValue& value, uint32_t reserveForExtra, ObjectPoolAllocator<ChunkSize, Allocator>& opa)
{
  assert(value.type() == JType::IARRAY);
  const uint32_t size = value.iarraySize();
  const uint32_t capacity = value.iarrayCapacity();
  uint32_t newCapacity = size + reserveForExtra;
  newCapacity = (newCapacity < capacity) ? capacity : newCapacity;
  if (newCapacity == 0u)
  {
    value.force(JType::DARRAY);
    return;
  }
  
  if (newCapacity == capacity)  // in-place
  {
    double* dValues = value.force_daValues(); // not strictly aliased
    
    const int64_t* iValues = value.iarrayValues();
    for (uint32_t i = 0; i < size; ++i)
      dValues[i] = (double)iValues[i];
    
    value.force(JType::DARRAY);
  }
  else if (newCapacity < LFJ_MAX_UINT16)
  {
    int64_t* iValues = value.iaValues();
    double* dValues = value.force_daValues(); // not strictly aliased
    if (!opa.realloc(iValues, capacity * sizeof(int64_t), newCapacity * sizeof(int64_t)))  // try in-place
    {
      dValues = (double*)opa.allocate(newCapacity * sizeof(double));
      
      for (uint32_t i = 0; i < size; ++i)
        dValues[i] = (double)iValues[i];
      
      opa.deallocate(iValues, capacity * sizeof(int64_t));
    }
    else
    {
      for (uint32_t i = 0; i < size; ++i)
        dValues[i] = (double)iValues[i];
    }
    
    value.force(JType::DARRAY);
    value.setAD(dValues);
    value.setDACapa((uint16_t)newCapacity);
    
  }
  else
  {
    JBigDArray* newBigDArray = (JBigDArray*)opa.allocate(sizeof(JBigDArray) + (newCapacity - 1) * sizeof(double));
    newBigDArray->capa = newCapacity;
    double* dValues = newBigDArray->data;
    
    int64_t* iValues = value.iaValues();
    for (uint32_t i = 0; i < size; ++i)
      dValues[i] = (double)iValues[i];
    
    if (capacity < LFJ_MAX_UINT16)
      opa.deallocate(iValues, capacity * sizeof(int64_t));
    else
      opa.deallocate(value.iaBA(), sizeof(JBigIArray) + (capacity - 1) * sizeof(int64_t));
    
    value.force(JType::DARRAY);
    value.setABD(newBigDArray);
    value.setDACapa(LFJ_MAX_UINT16);
  }
}

} // namespace helper
} // namespace lfjson

#endif // LFJSON_DATAHELPER_H
