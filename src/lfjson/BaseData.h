/**
 * Copyright 2022 Guillaume AUJAY. All rights reserved.
 *
 */

#ifndef LFJSON_BASEDATA_H
#define LFJSON_BASEDATA_H

#include "JString.h"

#include <cstdint>
#include <cstring>
#include <cassert>

namespace lfjson
{
// Forwarding
class JValue;
class JMember;
class ConstMember;
struct JBigArray;
struct JBigBArray;
struct JBigIArray;
struct JBigDArray;
struct JBigObject;

uint32_t arrCapacity(const JBigArray* ba);
JValue*  arrData(JBigArray* ba);

uint32_t arrBCapacity(const JBigBArray* bb);
bool*    arrBData(JBigBArray* bb);

uint32_t arrICapacity(const JBigIArray* bi);
int64_t* arrIData(JBigIArray* bi);

uint32_t arrDCapacity(const JBigDArray* bd);
double*  arrDData(JBigDArray* bd);

uint32_t objCapacity(const JBigObject* bo);
JMember* objData(JBigObject* bo);

const JString* memberKey(JMember& m);
JValue& memberValue(JMember& m);
void initMember(JMember* m, const JString* js);
JMember& memberAt(JMember* arr, uint32_t index);

constexpr uint32_t sizeOfJValue();
constexpr uint32_t sizeOfJMember();
constexpr uint32_t sizeOfJBigArray();
constexpr uint32_t sizeOfJBigBArray();
constexpr uint32_t sizeOfJBigIArray();
constexpr uint32_t sizeOfJBigDArray();
constexpr uint32_t sizeOfJBigObject();

// Base types
enum class JType : uint8_t {
  OBJECT  = 0,
  ARRAY   = 1,
  BARRAY  = 2,
  IARRAY  = 3,
  DARRAY  = 4,
  SSTRING = 5,
  LSTRING = 6,
  INT64   = 7,
  UINT64  = 8,
  DOUBLE  = 9,
  TRUE    = 10,
  FALSE   = 11,
  NUL     = 12
};

// Meta types
enum class JMeta : uint8_t {
  OBJECT = 0,
  ARRAY  = 1,
  STRING = 2,
  NUMBER = 3,
  BOOL   = 4,
  NUL    = 5
};

// Public const interface for value
class ConstValue  // (12/16 Bytes)
{
protected:
  static uint32_t max1(uint32_t u) { return (u == 0u) ? 1u : u; }
  
  struct Number {
    Number(int64_t  i64_) : type(JType::INT64 ), i64(i64_) {}
    Number(uint64_t u64_) : type(JType::UINT64), u64(u64_) {}
    Number(double   d_)   : type(JType::DOUBLE), d(d_)     {}
    
    JType     type;
    uint16_t  _padding;
    union {
      int64_t   i64;
      uint64_t  u64;
      double    d;
    };
  };
  
  struct ShortString {
    enum { MaxSize = sizeof(Number) - 1, MaxLen = MaxSize - 1, LenPos = MaxLen };
    
    ShortString(const char* str_, uint32_t len_) : type(JType::SSTRING)
    {
      assert(isShort(len_));
      std::memcpy(str, str_, len_);
      str[len_] = '\0';
      setLen(len_);
    }
    
    static bool isShort(uint32_t len) { return              (MaxLen >= len); }
    uint32_t len() const              { return    (uint32_t)(MaxLen -  str[LenPos]); }
    void     setLen(uint32_t len)     { str[LenPos] = (char)(MaxLen -  len); }
    
    JType     type;
    char      str[MaxSize];  // 10/14 char + '\0', last Byte used as 'max - len'
  };
  
  struct LongString {
    LongString(const JString* js_, uint32_t len_) : type(JType::LSTRING), len(len_), s(js_->c_str()) {}
    
    JType       type;
    uint16_t    _padding;
    uint32_t    len;
    const char* s;
  };
  
  // Array and Object must have same layout
  struct Array {
    Array(JType type_) : type(type_), capa(0u), size(0u) {}
    
    JValue*     values()   const { return (capa < LFJ_MAX_UINT16) ? a : arrData(ba); }
    ConstValue* cvalues()  const { return (ConstValue*)values(); }
    uint32_t    capacity() const { return (capa < LFJ_MAX_UINT16) ? (uint32_t)capa : arrCapacity(ba); }
    bool        full()     const { return size == capacity(); }
    uint32_t    memSize()  const {
      return (capa < LFJ_MAX_UINT16) ? (uint32_t)capa * sizeOfJValue()
                                 : sizeOfJBigArray() + (arrCapacity(ba) - 1u) * sizeOfJValue();
    }
    uint32_t    memUsed()  const {
      return (capa < LFJ_MAX_UINT16) ? size * sizeOfJValue()
                                 : sizeOfJBigArray() + (max1(size) - 1u) * sizeOfJValue();
    }
    
    bool*       bvalues()   const { return (capa < LFJ_MAX_UINT16) ? b : arrBData(bb); }
    const bool* cbvalues()  const { return bvalues(); }
    uint32_t    bcapacity() const { return (capa < LFJ_MAX_UINT16) ? (uint32_t)capa : arrBCapacity(bb); }
    bool        bfull()     const { return size == bcapacity(); }
    uint32_t    bmemSize()  const {
      return (capa < LFJ_MAX_UINT16) ? (uint32_t)capa * sizeof(bool)
                                 : sizeOfJBigBArray() + (arrBCapacity(bb) - 1u) * sizeof(bool);
    }
    uint32_t    bmemUsed()  const {
      return (capa < LFJ_MAX_UINT16) ? size * sizeof(bool)
                                 : sizeOfJBigBArray() + (max1(size) - 1u) * sizeof(bool);
    }
    
    int64_t*       ivalues()   const { return (capa < LFJ_MAX_UINT16) ? i : arrIData(bi); }
    const int64_t* civalues()  const { return ivalues(); }
    uint32_t       icapacity() const { return (capa < LFJ_MAX_UINT16) ? (uint32_t)capa : arrICapacity(bi); }
    bool           ifull()     const { return size == icapacity(); }
    uint32_t       imemSize()  const {
      return (capa < LFJ_MAX_UINT16) ? (uint32_t)capa * sizeof(int64_t)
                                 : sizeOfJBigIArray() + (arrICapacity(bi) - 1u) * sizeof(int64_t);
    }
    uint32_t       imemUsed()  const {
      return (capa < LFJ_MAX_UINT16) ? size * sizeof(int64_t)
                                 : sizeOfJBigIArray() + (max1(size) - 1u) * sizeof(int64_t);
    }
    
    double*       dvalues()   const { return (capa < LFJ_MAX_UINT16) ? d : arrDData(bd); }
    const double* cdvalues()  const { return dvalues(); }
    uint32_t      dcapacity() const { return (capa < LFJ_MAX_UINT16) ? (uint32_t)capa : arrDCapacity(bd); }
    bool          dfull()     const { return size == dcapacity(); }
    uint32_t      dmemSize()  const {
      return (capa < LFJ_MAX_UINT16) ? (uint32_t)capa * sizeof(double)
                                 : sizeOfJBigDArray() + (arrDCapacity(bd) - 1u) * sizeof(double);
    }
    uint32_t      dmemUsed()  const {
      return (capa < LFJ_MAX_UINT16) ? size * sizeof(double)
                                 : sizeOfJBigDArray() + (max1(size) - 1u) * sizeof(double);
    }
    
    JType     type;
    uint16_t  capa;
    uint32_t  size;
    union {
      JValue*     a;
      JBigArray*  ba;
      bool*       b;
      JBigBArray* bb;
      int64_t*    i;
      JBigIArray* bi;
      double*     d;
      JBigDArray* bd;
    };
  };
  
  struct Object {
    Object(JType type_) : type(type_), capa(0u), size(0u) {}
    JMember*     members()  const { return (capa < LFJ_MAX_UINT16) ? o : objData(bo); }
    ConstMember* cmembers() const { return (ConstMember*)members(); }
    uint32_t     capacity() const { return (capa < LFJ_MAX_UINT16) ? (uint32_t)capa : objCapacity(bo); }
    bool         full()     const { return size == capacity(); }
    uint32_t     memSize()  const {
      return (capa < LFJ_MAX_UINT16) ? (uint32_t)capa * sizeOfJMember()
                                 : sizeOfJBigObject() + (objCapacity(bo) - 1u) * sizeOfJMember();
    }
    uint32_t     memUsed()  const {
      return (capa < LFJ_MAX_UINT16) ? size * sizeOfJMember()
                                 : sizeOfJBigObject() + (max1(size) - 1u) * sizeOfJMember();
    }
    
    JType     type;
    uint16_t  capa;
    uint32_t  size;
    union {
      JMember*      o;
      JBigObject*  bo;
    };
  };
  
  struct Type {
    Type(JType type_) : type(type_) {}
    
    JType     type;
    uint8_t   padding[sizeof(Number) - 1];
  };
  
  union {
    Object      o;
    Array       a;
    LongString  s;
    ShortString ss;
    Number      n;
    Type        t;
  };
  
  // Constructors
  ConstValue()              : t(JType::NUL) {}
  ConstValue(bool b)        : t(b ? JType::TRUE : JType::FALSE) {}
  ConstValue(int64_t  i64)  : n(i64) {}
  ConstValue(uint64_t u64)  : n(u64) {}
  ConstValue(double d)      : n(d)   {}
  ConstValue(const char* str,   uint32_t len) : ss(str, len) {}
  ConstValue(const JString* js, uint32_t len) :  s(js,  len) {}
  ConstValue(JType type)    : o(type)   // same layout as array
  {
    assert(type == JType::OBJECT || isMetaArray());
  }
  
  static JMeta meta(JType type)
  {
    static constexpr JMeta lut[] = {
      JMeta::OBJECT,  // JType::OBJECT
      JMeta::ARRAY,   // JType::ARRAY
      JMeta::ARRAY,   // JType::BARRAY
      JMeta::ARRAY,   // JType::IARRAY
      JMeta::ARRAY,   // JType::DARRAY
      JMeta::STRING,  // JType::SSTRING
      JMeta::STRING,  // JType::LSTRING
      JMeta::NUMBER,  // JType::INT64
      JMeta::NUMBER,  // JType::UINT64
      JMeta::NUMBER,  // JType::DOUBLE
      JMeta::BOOL,    // JType::TRUE
      JMeta::BOOL,    // JType::FALSE
      JMeta::NUL      // JType::NUL
    };
    
    assert(type <= JType::NUL);
    return lut[(uint8_t)type];
  }
  
public:
  // Getters
  JType type() const { return t.type; }
  JMeta meta() const { return meta(t.type); }
  
  bool isObject()      const { return t.type == JType::OBJECT; }
  bool isArray()       const { return t.type == JType::ARRAY; }
  bool isBArray()      const { return t.type == JType::BARRAY; }
  bool isIArray()      const { return t.type == JType::IARRAY; }
  bool isDArray()      const { return t.type == JType::DARRAY; }
  bool isShortString() const { return t.type == JType::SSTRING; }
  bool isLongString()  const { return t.type == JType::LSTRING; }
  bool isInt64()       const { return t.type == JType::INT64; }
  bool isUInt64()      const { return t.type == JType::UINT64; }
  bool isDouble()      const { return t.type == JType::DOUBLE; }
  bool isTrue()        const { return t.type == JType::TRUE; }
  bool isFalse()       const { return t.type == JType::FALSE; }
  bool isNul()         const { return t.type == JType::NUL; }
  
  bool isMetaBool()   const { return t.type == JType::TRUE    || t.type == JType::FALSE; }
  bool isMetaString() const { return t.type == JType::SSTRING || t.type == JType::LSTRING; }
  bool isMetaNumber() const { return meta(t.type) == JMeta::NUMBER; }
  bool isMetaArray()  const { return meta(t.type) == JMeta::ARRAY; }
  
  bool arrayEmpty()       const { return arraySize()  == 0u; }
  bool barrayEmpty()      const { return barraySize() == 0u; }
  bool iarrayEmpty()      const { return iarraySize() == 0u; }
  bool darrayEmpty()      const { return darraySize() == 0u; }
  bool objectEmpty()      const { return objectSize() == 0u; }
  bool shortStringEmpty() const { return shortStringSize() == 0u; }
  bool longStringEmpty()  const { return longStringSize()  == 0u; }
  
  uint32_t arraySize()       const { assert(a.type  == JType::ARRAY);   return a.size; }
  uint32_t barraySize()      const { assert(a.type  == JType::BARRAY);  return a.size; }
  uint32_t iarraySize()      const { assert(a.type  == JType::IARRAY);  return a.size; }
  uint32_t darraySize()      const { assert(a.type  == JType::DARRAY);  return a.size; }
  uint32_t objectSize()      const { assert(o.type  == JType::OBJECT);  return o.size; }
  uint32_t shortStringSize() const { assert(ss.type == JType::SSTRING); return ss.len(); }
  uint32_t longStringSize()  const { assert(s.type  == JType::LSTRING); return s.len; }
                                  
  uint32_t arrayCapacity()  const { assert(a.type == JType::ARRAY);  return a.capacity(); }
  uint32_t barrayCapacity() const { assert(a.type == JType::BARRAY); return a.bcapacity(); }
  uint32_t iarrayCapacity() const { assert(a.type == JType::IARRAY); return a.icapacity(); }
  uint32_t darrayCapacity() const { assert(a.type == JType::DARRAY); return a.dcapacity(); }
  uint32_t objectCapacity() const { assert(o.type == JType::OBJECT); return o.capacity(); }
  
  uint32_t arrayMemSize()  const { assert(a.type == JType::ARRAY);  return a.memSize(); }
  uint32_t barrayMemSize() const { assert(a.type == JType::BARRAY); return a.bmemSize(); }
  uint32_t iarrayMemSize() const { assert(a.type == JType::IARRAY); return a.imemSize(); }
  uint32_t darrayMemSize() const { assert(a.type == JType::DARRAY); return a.dmemSize(); }
  uint32_t objectMemSize() const { assert(a.type == JType::OBJECT); return o.memSize(); }
  
  uint32_t arrayMemUsed()  const { assert(a.type == JType::ARRAY);  return a.memUsed(); }
  uint32_t barrayMemUsed() const { assert(a.type == JType::BARRAY); return a.bmemUsed(); }
  uint32_t iarrayMemUsed() const { assert(a.type == JType::IARRAY); return a.imemUsed(); }
  uint32_t darrayMemUsed() const { assert(a.type == JType::DARRAY); return a.dmemUsed(); }
  uint32_t objectMemUsed() const { assert(a.type == JType::OBJECT); return o.memUsed(); }
  
  ConstValue*    arrayValues()   const { assert(a.type == JType::ARRAY);  return a.cvalues(); }
  const bool*    barrayValues()  const { assert(a.type == JType::BARRAY); return a.cbvalues(); }
  const int64_t* iarrayValues()  const { assert(a.type == JType::IARRAY); return a.civalues(); }
  const double*  darrayValues()  const { assert(a.type == JType::DARRAY); return a.cdvalues(); }
  ConstMember*   objectMembers() const { assert(o.type == JType::OBJECT); return o.cmembers(); }
  
  // Accessors
  bool     getBool()   const { assert(t.type == JType::TRUE || t.type == JType::FALSE); return t.type == JType::TRUE; }
  int64_t  getInt64()  const { assert(n.type == JType::INT64);   return n.i64; }
  uint64_t getUInt64() const { assert(n.type == JType::UINT64);  return n.u64; }
  double   getDouble() const { assert(n.type == JType::DOUBLE);  return n.d; }
  double   asNumber()  const
  {
    assert(meta(n.type) == JMeta::NUMBER);
    switch (n.type)
    {
      case JType::INT64:  return (double)n.i64;
      case JType::UINT64: return (double)n.u64;
      case JType::DOUBLE: return n.d;
      default:
        assert(false && "[lfjson] JValue: not a number");
    }
    return 0.;
  }
  const char* getShortString() const { assert(ss.type == JType::SSTRING); return ss.str; }
  const char* getLongString()  const { assert(s.type  == JType::LSTRING); return s.s; }
  const char* asString()       const
  {
    assert(meta(t.type) == JMeta::STRING);
    switch(t.type)
    {
      case JType::SSTRING:  return ss.str;
      case JType::LSTRING:  return s.s;
      default:
        assert(false && "[lfjson] JValue: not a string");
    }
    return "";
  }
};

typedef ConstValue*    ConstValueIter;
typedef const bool*    ConstBoolIter;
typedef const int64_t* ConstInt64Iter;
typedef const double*  ConstDoubleIter;

// Public editable interface over ConstValue
class JValue : public ConstValue // (12/16 Bytes) (inheritance without virtual = no overhead)
{
public:
  enum { ShortString_MaxSize = ConstValue::ShortString::MaxSize };
  static constexpr float Object_GrowthFactor = 1.5f;
  static constexpr float Array_GrowthFactor  = 1.5f;
  
  static uint32_t minStringLength(const char* s)
  {
    // Check for short-string
    uint32_t minLen = 0;
    for (; minLen < JValue::ShortString_MaxSize; ++minLen)
    {
      if (s[minLen] == '\0')
        break;
    }
    return minLen;
  }
  
  JValue()              : ConstValue()    {}
  JValue(bool b)        : ConstValue(b)   {}
  JValue(int64_t  i64)  : ConstValue(i64) {}
  JValue(uint64_t u64)  : ConstValue(u64) {}
  JValue(double d)      : ConstValue(d)   {}
  JValue(const char* str,   uint32_t len) : ConstValue(str, len) {}
  JValue(const JString* js, uint32_t len) : ConstValue(js,  len) {}
  JValue(JType type)    : ConstValue(type) {}
  
  // Getters
#ifndef NDEBUG
  bool empty() const { return size() == 0u; }
  uint32_t size() const
  {
    switch (t.type)
    {
      case JType::OBJECT:
        return o.size;
      case JType::ARRAY:
      case JType::BARRAY:
      case JType::IARRAY:
      case JType::DARRAY:
        return a.size;
      case JType::LSTRING:
        return s.len;
      case JType::SSTRING:
        return ss.len();
      default:
      {
        assert(false && "[lfjson] JValue: this type has no size");
        return 0u;
      }
    }
  }
#endif  // NDEBUG
  
  bool aFull()        const { assert(a.type == JType::ARRAY);  return a.full(); }
  bool baFull()       const { assert(a.type == JType::BARRAY); return a.bfull(); }
  bool iaFull()       const { assert(a.type == JType::IARRAY); return a.ifull(); }
  bool daFull()       const { assert(a.type == JType::DARRAY); return a.dfull(); }
  bool oFull()        const { assert(o.type == JType::OBJECT); return o.full(); }
                             
  JValue*  aValues()  const { assert(a.type == JType::ARRAY);  return a.values(); }
  bool*    baValues() const { assert(a.type == JType::BARRAY); return a.bvalues(); }
  int64_t* iaValues() const { assert(a.type == JType::IARRAY); return a.ivalues(); }
  double*  daValues() const { assert(a.type == JType::DARRAY); return a.dvalues(); }
  JMember* oMembers() const { assert(o.type == JType::OBJECT); return o.members(); }
  
  double*  force_daValues() const { return a.dvalues(); }
                             
  JValue*    aA()     const { assert(a.type == JType::ARRAY);  return a.a; }
  bool*      baA()    const { assert(a.type == JType::BARRAY); return a.b; }
  int64_t*   iaA()    const { assert(a.type == JType::IARRAY); return a.i; }
  double*    daA()    const { assert(a.type == JType::DARRAY); return a.d; }
  
  JBigArray*  aBA()   const { assert(a.type == JType::ARRAY);  return a.ba; }
  JBigBArray* baBA()  const { assert(a.type == JType::BARRAY); return a.bb; }
  JBigIArray* iaBA()  const { assert(a.type == JType::IARRAY); return a.bi; }
  JBigDArray* daBA()  const { assert(a.type == JType::DARRAY); return a.bd; }
                             
  JMember*    oO()    const { assert(o.type == JType::OBJECT); return o.o; }
  JBigObject* oBO()   const { assert(o.type == JType::OBJECT); return o.bo; }
  
  uint32_t aCapa()    const { assert(a.type == JType::ARRAY);  return a.capa; };
  uint32_t baCapa()   const { assert(a.type == JType::BARRAY); return a.capa; };
  uint32_t iaCapa()   const { assert(a.type == JType::IARRAY); return a.capa; };
  uint32_t daCapa()   const { assert(a.type == JType::DARRAY); return a.capa; };
  
  // Setters
  void setAA(JValue* aa)       { assert(a.type == JType::ARRAY);  a.a  = aa; }
  void setAB(bool* ab)         { assert(a.type == JType::BARRAY); a.b  = ab; }
  void setAI(int64_t* ai)      { assert(a.type == JType::IARRAY); a.i  = ai; }
  void setAD(double* ad)       { assert(a.type == JType::DARRAY); a.d  = ad; }
  
  void setABA(JBigArray* aba)  { assert(a.type == JType::ARRAY);  a.ba = aba; }
  void setABB(JBigBArray* abb) { assert(a.type == JType::BARRAY); a.bb = abb; }
  void setABI(JBigIArray* abi) { assert(a.type == JType::IARRAY); a.bi = abi; }
  void setABD(JBigDArray* abd) { assert(a.type == JType::DARRAY); a.bd = abd; }
  
  void setOO(JMember* oo)      { assert(o.type == JType::OBJECT); o.o  = oo; }
  void setOBO(JBigObject* obo) { assert(o.type == JType::OBJECT); o.bo = obo; }
  
  void setASize(uint32_t size)  { assert(a.type == JType::ARRAY);  a.size = size; }
  void setBASize(uint32_t size) { assert(a.type == JType::BARRAY); a.size = size; }
  void setIASize(uint32_t size) { assert(a.type == JType::IARRAY); a.size = size; }
  void setDASize(uint32_t size) { assert(a.type == JType::DARRAY); a.size = size; }
  void setOSize(uint32_t size)  { assert(o.type == JType::OBJECT); o.size = size; }
  
  void setACapa(uint16_t capa)  { assert(a.type == JType::ARRAY);  a.capa = capa; }
  void setBACapa(uint16_t capa) { assert(a.type == JType::BARRAY); a.capa = capa; }
  void setIACapa(uint16_t capa) { assert(a.type == JType::IARRAY); a.capa = capa; }
  void setDACapa(uint16_t capa) { assert(a.type == JType::DARRAY); a.capa = capa; }
  void setOCapa(uint16_t capa)  { assert(o.type == JType::OBJECT); o.capa = capa; }
  
  // Operators
  JValue& operator[](uint32_t index) const
  {
    assert(a.type == JType::ARRAY);
    assert(index < a.size);
    
    return a.values()[index];
  }
  
  // Accessors
  bool& arrayBool(uint32_t pos) const
  {
    assert(a.type == JType::BARRAY);
    assert(pos < a.size);
    
    return a.bvalues()[pos];
  }
  
  int64_t& arrayInt64(uint32_t pos) const
  {
    assert(a.type == JType::IARRAY);
    assert(pos < a.size);
    
    return a.ivalues()[pos];
  }
  
  double& arrayDouble(uint32_t pos) const
  {
    assert(a.type == JType::DARRAY);
    assert(pos < a.size);
    
    return a.dvalues()[pos];
  }
  
  JMember& member(uint32_t pos) const
  {
    assert(o.type == JType::OBJECT);
    assert(pos < o.size);
    
    JMember* members = o.members();
    return memberAt(members, pos);
  }
  
  // Modifiers
  void setNull()
  {
    assert((t.type != JType::OBJECT && !isMetaArray()) || empty());
    t.type = JType::NUL;
  }
  
  void set(bool b)
  {
    assert((t.type != JType::OBJECT && !isMetaArray()) || empty());
    t.type = b ? JType::TRUE : JType::FALSE;
  }
  
  void set(int64_t i64)
  {
    assert((t.type != JType::OBJECT && !isMetaArray()) || empty());
    n.type = JType::INT64;
    n.i64 = i64;
  }
  
  void set(uint64_t u64)
  {
    assert((t.type != JType::OBJECT && !isMetaArray()) || empty());
    n.type = JType::UINT64;
    n.u64 = u64;
  }
  
  void set(double d)
  {
    assert((t.type != JType::OBJECT && !isMetaArray()) || empty());
    n.type = JType::DOUBLE;
    n.d = d;
  }
  
  void set(const char* str, uint32_t len)
  {
    assert(ShortString::isShort(len));
    assert((t.type != JType::OBJECT && !isMetaArray()) || empty());
    
    ss.type = JType::SSTRING;
    std::memcpy(ss.str, str, len);
    ss.str[len] = '\0';
    ss.setLen(len);
  }
  
  void set(const JString* js, uint32_t len)
  {
    assert(!ShortString::isShort(len));
    assert((t.type != JType::OBJECT && !isMetaArray()) || empty());
    
    s.type = JType::LSTRING;
    s.len = len;
    s.s = js->c_str();
  }
  
  void set(JType type_)
  {
    assert(type_ == JType::OBJECT || type_ == JType::ARRAY || type_ == JType::BARRAY || type_ == JType::IARRAY || type_ == JType::DARRAY);
    assert((t.type != JType::OBJECT && !isMetaArray()) || empty());
    
    o.type = type_; // same layout as array
    o.size = 0u;
    o.capa = 0u;
  }
  
  // Force modifiers
  void forceNull()
  {
    t.type = JType::NUL;
  }
  
  void force(bool b)
  {
    n.type = b ? JType::TRUE : JType::FALSE;
  }
  
  void force(int64_t i64)
  {
    n.type = JType::INT64;
    n.i64 = i64;
  }
  
  void force(double d)
  {
    n.type = JType::DOUBLE;
    n.d = d;
  }
  
  void force(JType type_)
  {
    assert(type_ == JType::ARRAY || type_ == JType::BARRAY || type_ == JType::IARRAY || type_ == JType::DARRAY);
    assert(isMetaArray());
    a.type = type_;
  }
  
  // Raw modifiers
  void setRawArray(void* ptr, uint32_t size)
  {
    assert(isArray());
    if (size < LFJ_MAX_UINT16)
    {
      setAA((JValue*)ptr);
      setACapa((uint16_t)size);
      setASize(size);
    }
    else  // big
    {
      setABA((JBigArray*)ptr);
      setACapa(LFJ_MAX_UINT16);
      setASize(size);
    }
  }
  
  void setRawBArray(void* ptr, uint32_t size)
  {
    assert(isArray());
    force(JType::BARRAY);
    if (size < LFJ_MAX_UINT16)
    {
      setAB((bool*)ptr);
      setBACapa((uint16_t)size);
      setBASize(size);
    }
    else  // big
    {
      setABB((JBigBArray*)ptr);
      setBACapa(LFJ_MAX_UINT16);
      setBASize(size);
    }
  }
  
  void setRawIArray(void* ptr, uint32_t size)
  {
    assert(isArray());
    force(JType::IARRAY);
    if (size < LFJ_MAX_UINT16)
    {
      setAI((int64_t*)ptr);
      setIACapa((uint16_t)size);
      setIASize(size);
    }
    else  // big
    {
      setABI((JBigIArray*)ptr);
      setIACapa(LFJ_MAX_UINT16);
      setIASize(size);
    }
  }
  
  void setRawDArray(void* ptr, uint32_t size)
  {
    assert(isArray());
    force(JType::DARRAY);
    if (size < LFJ_MAX_UINT16)
    {
      setAD((double*)ptr);
      setDACapa((uint16_t)size);
      setDASize(size);
    }
    else  // big
    {
      setABD((JBigDArray*)ptr);
      setDACapa(LFJ_MAX_UINT16);
      setDASize(size);
    }
  }
  
  void setRawObject(void* ptr, uint32_t size)
  {
    assert(isObject());
    if (size < LFJ_MAX_UINT16)
    {
      setOO((JMember*)ptr);
      setOCapa((uint16_t)size);
      setOSize(size);
    }
    else  // big
    {
      setOBO((JBigObject*)ptr);
      setOCapa(LFJ_MAX_UINT16);
      setOSize(size);
    }
  }
  
  // Size modifiers
  void incASize()
  {
    assert(a.type == JType::ARRAY);
    assert(a.capacity() > a.size);
    
    JValue& dest = a.values()[a.size];
    dest.t = JType::NUL;
    ++a.size;
  }
  
  void incASizeUninit()
  {
    assert(a.type == JType::ARRAY);
    assert(a.capacity() > a.size);
    
    ++a.size;
  }
  
  void incBASizeUninit()
  {
    assert(a.type == JType::BARRAY);
    assert(a.bcapacity() > a.size);
    
    ++a.size;
  }
  
  void incIASizeUninit()
  {
    assert(a.type == JType::IARRAY);
    assert(a.icapacity() > a.size);
    
    ++a.size;
  }
  
  void incDASizeUninit()
  {
    assert(a.type == JType::DARRAY);
    assert(a.dcapacity() > a.size);
    
    ++a.size;
  }
  
  void decASize()
  {
    assert(a.type == JType::ARRAY);
    assert(a.size > 0u);
    
    --a.size;
  }
  
  void decBASize()
  {
    assert(a.type == JType::BARRAY);
    assert(a.size > 0u);
    
    --a.size;
  }
  
  void decIASize()
  {
    assert(a.type == JType::IARRAY);
    assert(a.size > 0u);
    
    --a.size;
  }
  
  void decDASize()
  {
    assert(a.type == JType::DARRAY);
    assert(a.size > 0u);
    
    --a.size;
  }
  
  void decOSize()
  {
    assert(o.type == JType::OBJECT);
    assert(o.size > 0u);
    
    --o.size;
  }
  
  JValue& incOSize(const JString* key)
  {
    assert(key);
    assert(o.type == JType::OBJECT);
    assert(o.capacity() > o.size);
    
    JMember* members = o.members();
    JMember& dest = memberAt(members, o.size);
    
    initMember(&dest, key);
    ++o.size;
    
    return memberValue(dest);
  }
};
static_assert(sizeof(JValue)  == sizeof(ConstValue),  "[lfjson] BaseData: JValue and ConstValue must be the same size");
static_assert(alignof(JValue) == alignof(ConstValue), "[lfjson] BaseData: JValue and ConstValue must be the same alignment");

// Public const interface for member
class ConstMember // (16/24 Bytes)
{
protected:
  const JString*  mKey;
  JValue          mValue;  // default: JType::NUL
  
  ConstMember(const JString* key) : mKey(key) {}
  
public:
  const char* key()   const { return mKey->c_str(); }
  ConstValue& value() const { return (ConstValue&)mValue; }
  
  uint32_t keyLen() const { return mKey->len(); }
  bool keyOwned()   const { return mKey->owns(); }  // true if copied
};
typedef ConstMember* ConstMemberIter;

// Public editable interface over ConstMember
class JMember : public ConstMember  // (16/24 Bytes) (inheritance without virtual = no overhead)
{
public:
  JMember() : ConstMember(nullptr) {}
  JMember(const JString* key_) : ConstMember(key_) {}
  
  const JString* jkey() const { return mKey; }
  JValue& jvalue()            { return mValue; }
  
  void setKey(const JString* key) { mKey = key; }
};
static_assert(sizeof(JMember)  == sizeof(ConstMember),  "[lfjson] BaseData: JMember and ConstMember must be the same size");
static_assert(alignof(JMember) == alignof(ConstMember), "[lfjson] BaseData: JMember and ConstMember must be the same alignment");

// Base structs
struct JBigArray {  // (12/16 * capa + 4/8 Bytes)
  uint32_t  capa;
  JValue    data[1];  // array
};

struct JBigBArray { // (1 * capa + 4 Bytes)
  uint32_t  capa;
  bool      data[1];  // array
};

struct JBigIArray { // (8 * capa + 4/8 Bytes)
  uint32_t  capa;
  int64_t   data[1];  // array
};

struct JBigDArray { // (8 * capa + 4/8 Bytes)
  uint32_t  capa;
  double    data[1];  // array
};

struct JBigObject { // (16/24 * capa + 4/8 Bytes)
  uint32_t  capa;
  JMember   data[1];  // array
};

// Forwarded
uint32_t arrCapacity(const JBigArray* ba)   { return ba->capa; }
JValue*  arrData(JBigArray* ba)             { return ba->data; }

uint32_t arrBCapacity(const JBigBArray* bb) { return bb->capa; }
bool*    arrBData(JBigBArray* bb)           { return bb->data; }

uint32_t arrICapacity(const JBigIArray* bi) { return bi->capa; }
int64_t*  arrIData(JBigIArray* bi)          { return bi->data; }

uint32_t arrDCapacity(const JBigDArray* bd) { return bd->capa; }
double*  arrDData(JBigDArray* bd)           { return bd->data; }

uint32_t objCapacity(const JBigObject* bo)  { return bo->capa; }
JMember* objData(JBigObject* bo)            { return bo->data; }

const JString* memberKey(JMember& m)            { return m.jkey(); }
JValue& memberValue(JMember& m)                 { return m.jvalue(); }
void initMember(JMember* m, const JString* js)  { new (m) JMember(js); }
JMember& memberAt(JMember* arr, uint32_t index) { return arr[index]; }

constexpr uint32_t sizeOfJValue()     { return (uint32_t)sizeof(JValue); }
constexpr uint32_t sizeOfJMember()    { return (uint32_t)sizeof(JMember); }
constexpr uint32_t sizeOfJBigArray()  { return (uint32_t)sizeof(JBigArray); }
constexpr uint32_t sizeOfJBigBArray() { return (uint32_t)sizeof(JBigBArray); }
constexpr uint32_t sizeOfJBigIArray() { return (uint32_t)sizeof(JBigIArray); }
constexpr uint32_t sizeOfJBigDArray() { return (uint32_t)sizeof(JBigDArray); }
constexpr uint32_t sizeOfJBigObject() { return (uint32_t)sizeof(JBigObject); }

} // namespace lfjson

#endif // LFJSON_BASEDATA_H
