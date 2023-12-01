#pragma once

#include <cstdint>

#include "chunk.h"
#include "value.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_FUNCTION(value) isObjType(value, ObjType::Function)
#define AS_FUNCTION(value) (reinterpret_cast<ObjFunction*>(AS_OBJ(value)))

#define IS_NATIVE(value) isObjType(value, ObjType::Native)
#define AS_NATIVE(value) (reinterpret_cast<ObjNative*>(AS_OBJ(value)))

#define IS_CLOSURE(value) isObjType(value, ObjType::Closure)
#define AS_CLOSURE(value) (reinterpret_cast<ObjClosure*>(AS_OBJ(value)))

#define IS_UPVALUE(value) isObjType(value, ObjType::Upvalue)
#define AS_UPVALUE(value) (reinterpret_cast<ObjUpvalue*>(AS_OBJ(value)))

#define IS_STRING(value) isObjType(value, ObjType::String)
#define AS_STRING(value) (reinterpret_cast<ObjString*>(AS_OBJ(value)))
#define AS_CSTRING(value) (reinterpret_cast<ObjString*>(AS_OBJ(value))->chars)

enum class ObjType
{
	Function,
	Native,
	Closure,
	Upvalue,
	String,
};

struct Obj
{
	ObjType type;
	Obj* next = nullptr;
};

struct ObjFunction
{
	Obj obj;
	int arity = 0;
	int upvalueCount = 0;
	Chunk chunk;
	ObjString* name = nullptr;
};

ObjFunction* newFunction();

using NativeFn = Value(*)(int argCount, Value* args);
struct ObjNative
{
	Obj obj;
	NativeFn function = nullptr;
};

ObjNative* newNative(NativeFn function);

struct ObjClosure
{
	Obj obj;
	ObjFunction* function = nullptr;
	ObjUpvalue** upvalues = nullptr; // Upvalue のポインタの配列
	int upvalueCount = 0;
};

ObjClosure* newClosure(ObjFunction* function);

struct ObjUpvalue
{
	Obj obj;
	Value* location = nullptr;
	ObjUpvalue* next = nullptr; // for IntrusiveList
};

ObjUpvalue* newUpvalue(Value* slot);

struct ObjString
{
	Obj obj;
	int length = 0;
	char* chars = nullptr;
	uint32_t hash = 0;
};

ObjString* takeString(char* chars, int length);
ObjString* copyString(const char* chars, int length);

void freeObject(Obj* obj);
void printObject(Value value);

inline Value toObjValue(ObjString* s)
{
	return Value::toObj(reinterpret_cast<Obj*>(s));
}

inline bool isObjType(Value value, ObjType type)
{
	return IS_OBJ(value) && AS_OBJ(value)->type == type;
}
