#pragma once

#include "common.h"

struct Obj;
struct ObjClass;
struct ObjInstance;
struct ObjBoundMethod;
struct ObjFunction;
struct ObjNative;
struct ObjClosure;
struct ObjUpvalue;
struct ObjString;

#if NAN_BOXING

#include <cstring>

#define SIGN_BIT (static_cast<uint64_t>(0x8000'0000'0000'0000))
#define QNAN (static_cast<uint64_t>(0x7ffc'0000'0000'0000))

#define TAG_NIL 1
#define TAG_FALSE 2
#define TAG_TRUE 3

using Value = uint64_t;

#define IS_NUMBER(value) (((value) & QNAN) != QNAN)
#define AS_NUMBER(value) valueToNum(value)
#define TO_NUMBER(value) numToValue(value)

#define NIL_VAL (static_cast<Value>(QNAN | TAG_NIL))
#define FALSE_VAL (static_cast<Value>(QNAN | TAG_FALSE))
#define TRUE_VAL (static_cast<Value>(QNAN | TAG_TRUE))

#define TO_NIL() NIL_VAL
#define IS_NIL(value) ((value) == NIL_VAL)

#define IS_BOOL(value) (((value) | 1) == TRUE_VAL)
#define AS_BOOL(value) ((value) == TRUE_VAL)
#define TO_BOOL(value) ((value) ? TRUE_VAL : FALSE_VAL)

#define IS_OBJ(value) \
	(((value) & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT))

#define AS_OBJ(value) \
	reinterpret_cast<Obj*>(static_cast<uintptr_t>(((value) & ~(SIGN_BIT | QNAN))))

#define TO_OBJ(value) \
	static_cast<Value>(SIGN_BIT | QNAN | static_cast<uint64_t>(reinterpret_cast<uintptr_t>(value)))

inline Value numToValue(double num)
{
	Value value;
	memcpy(&value, &num, sizeof(double));
	return value;
}

inline double valueToNum(Value value)
{
	double num;
	memcpy(&num, &value, sizeof(Value));
	return num;
}

#else

enum class ValueType
{
	Bool,
	Nil,
	Number,
	Obj,
};

struct Value
{
	ValueType type;
	union {
		bool boolean;
		double number;
		Obj* obj;
	} as;
};

#define IS_BOOL(value) ((value).type == ValueType::Bool)
#define IS_NIL(value) ((value).type == ValueType::Nil)
#define IS_NUMBER(value) ((value).type == ValueType::Number)
#define IS_OBJ(value) ((value).type == ValueType::Obj)

#define AS_BOOL(value) ((value).as.boolean)
#define AS_NUMBER(value) ((value).as.number)
#define AS_OBJ(value) ((value).as.obj)

#define TO_BOOL(value) Value{ValueType::Bool, {.boolean = value} }
#define TO_NIL() Value{ValueType::Nil, {.number = 0} }
#define TO_NUMBER(value) Value{ValueType::Number, {.number = value} }
#define TO_OBJ(value) Value{ValueType::Obj, {.obj = reinterpret_cast<Obj*>(value)} }

#endif

void printValue(Value val);
ObjString* toString(Value val);

struct ValueArray
{
	void Init();
	void Free();
	void Write(Value value);

	int capacity = 0;
	int count = 0;
	Value* values = nullptr;
};

bool valuesEqual(Value a, Value b);

