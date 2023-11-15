﻿#pragma once

#include "value.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)
#define IS_STRING(value) isObjType(value, ObjType::String)

#define AS_STRING(value) (reinterpret_cast<ObjString*>(AS_OBJ(value)))
#define AS_CSTRING(value) (reinterpret_cast<ObjString*>(AS_OBJ(value))->chars)

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

ObjString* takeString(char* chars, int length);
ObjString* copyString(const char* chars, int length);
void printObject(Value value);

inline Value toObjValue(ObjString* s)
{
	return Value::toObj(reinterpret_cast<Obj*>(s));
}

inline bool isObjType(Value value, ObjType type)
{
	return IS_OBJ(value) && AS_OBJ(value)->type == type;
}