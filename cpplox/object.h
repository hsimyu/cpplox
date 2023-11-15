#pragma once

#include "value.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)
#define IS_STRING(value) isObjType(value, OBJ_STRING)

#define AS_STRING(value) (static_cast<ObjString*>(AS_OBJ(value)))
#define AS_CSTRING(value) (static_cast<ObjString*>(AS_OBJ(value))->chars)

enum class ObjType
{
	String,
};

struct Obj
{
	ObjType type;
};

struct ObjString
{
	Obj obj;
	int length = 0;
	char* chars = nullptr;
};

ObjString* copyString(const char* chars, int length);

inline Value toObj(ObjString* s)
{
	return Value::toObj(reinterpret_cast<Obj*>(s));
}

inline bool isObjType(Value value, ObjType type)
{
	return value.isObj() && AS_OBJ(value)->type == type;
}
