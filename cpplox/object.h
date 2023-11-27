#pragma once

#include <cstdint>

#include "chunk.h"
#include "value.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_FUNCTION(value) isObjType(value, ObjType::Function)
#define AS_FUNCTION(value) (reinterpret_cast<ObjFunction*>(AS_OBJ(value)))

#define IS_STRING(value) isObjType(value, ObjType::String)
#define AS_STRING(value) (reinterpret_cast<ObjString*>(AS_OBJ(value)))
#define AS_CSTRING(value) (reinterpret_cast<ObjString*>(AS_OBJ(value))->chars)

enum class ObjType
{
	Function,
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
	Chunk chunk;
	ObjString* name = nullptr;
};

ObjFunction* newFunction();

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
