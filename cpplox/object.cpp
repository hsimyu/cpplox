#include "object.h"

#include "memory.h"
#include "vm.h"

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

	// linked list として vm に登録
	auto vm = getVM();
	o->next = vm->objects;
	vm->objects = o;

	return reinterpret_cast<T*>(o);
}

ObjString* allocateString(char* chars, int length, uint32_t hash)
{
	ObjString* s = allocateObject<ObjString>(ObjType::String);
	s->length = length;
	s->chars = chars;
	s->hash = hash;
	// 文字列の intern 化
	// Value はなんでもいいので nil を入れる
	tableSet(&getVM()->strings, s, Value::toNil());
	return s;
}

uint32_t hashString(const char* key, int length)
{
	// FNV-1a hashing
	uint32_t hash = 2166136261u;
	for (int i = 0; i < length; i++)
	{
		hash ^= static_cast<uint8_t>(key[i]);
		hash *= 16777619;
	}
	return hash;
}

void printFunction(ObjFunction* function)
{
	if (function->name == nullptr)
	{
		printf("<script>");
		return;
	}
	printf("<fn %s>", function->name->chars);
}

}

ObjFunction* newFunction()
{
	ObjFunction* f = allocateObject<ObjFunction>(ObjType::Function);
	f->arity = 0;
	f->name = nullptr;
	f->chunk.Init();
	return f;
}

ObjString* takeString(char* chars, int length)
{
	auto hash = hashString(chars, length);
	ObjString* interned = tableFindString(&getVM()->strings, chars, length, hash);

	if (interned != nullptr)
	{
		// すでに vm がインターン化済みだったのでそれを返す
		// -> 所有権を譲渡された文字列が必要なくなったので、解放する
		free_array(chars, length + 1);
		return interned;
	}

	// 指定した文字列を所有するのでそのまま割り当てる
	return allocateString(chars, length, hash);
}

ObjString* copyString(const char* chars, int length)
{
	auto hash = hashString(chars, length);
	ObjString* interned = tableFindString(&getVM()->strings, chars, length, hash);
	if (interned != nullptr) return interned; // 生成済みのエントリがあったのでそれを返す

	// 指定した文字列を所有しないのでヒープ上に新しく割り当てる
	char* heapChars = allocate<char>(length + 1);
	memcpy(heapChars, chars, length);
	heapChars[length] = '\0';
	return allocateString(heapChars, length, hash);
}

void freeObject(Obj* obj)
{
	switch (obj->type)
	{

	using enum ObjType;

	case Function:
	{
		ObjFunction* f = reinterpret_cast<ObjFunction*>(obj);
		f->chunk.Free();
		free(f);
		break;
	}
	case String:
	{
		ObjString* s = reinterpret_cast<ObjString*>(obj);
		free_array(s->chars, s->length + 1);
		free(s);
		break;
	}

	}
}

void printObject(Value value)
{
	switch (OBJ_TYPE(value))
	{
	using enum ObjType;
	case Function:
		printFunction(AS_FUNCTION(value));
		break;
	case String:
		printf("%s", AS_CSTRING(value));
		break;
	}
}
