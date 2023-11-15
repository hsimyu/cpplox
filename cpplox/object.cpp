﻿#include "object.h"

#include "memory.h"
#include <cstring>
#include <cstdio>

namespace
{

template<typename T>
T* allocateObject(ObjType type)
{
	// TODO: T と Obj がキャスト可能であることを保証する
	Obj* o = static_cast<Obj*>(reallocate(nullptr, 0, sizeof(T)));
	o->type = type;
	return reinterpret_cast<T*>(o);
}

ObjString* allocateString(char* chars, int length)
{
	ObjString* s = allocateObject<ObjString>(ObjType::String);
	s->length = length;
	s->chars = chars;
	return s;
}

}

ObjString* takeString(char* chars, int length)
{
	// 指定した文字列を所有するのでそのまま割り当てる
	return allocateString(chars, length);
}

ObjString* copyString(const char* chars, int length)
{
	// 指定した文字列を所有しないのでヒープ上に新しく割り当てる
	char* heapChars = allocate<char>(length + 1);
	memcpy(heapChars, chars, length);
	heapChars[length] = '\0';
	return allocateString(heapChars, length);
}

void printObject(Value value)
{
	switch (OBJ_TYPE(value))
	{
	using enum ObjType;
	case String:
		printf("%s", AS_CSTRING(value));
		break;
	}
}