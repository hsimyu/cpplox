#include "object.h"

#include "memory.h"
#include "vm.h"
#include "common.h"

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
	o->isMarked = false;

	// linked list として vm に登録
	auto vm = getVM();
	o->next = vm->objects;
	vm->objects = o;

#if DEBUG_LOG_GC
	printf("%p allocate %zu for %d\n", o, sizeof(T), type);
#endif

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
	push(Value::toObj(s)); // GC 回避
	tableSet(&getVM()->strings, s, Value::toNil());
	pop();
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

ObjClass* newClass(ObjString* name)
{
	ObjClass* klass = allocateObject<ObjClass>(ObjType::Class);
	klass->name = name;
	return klass;
}

ObjFunction* newFunction()
{
	ObjFunction* f = allocateObject<ObjFunction>(ObjType::Function);
	f->arity = 0;
	f->upvalueCount = 0;
	f->name = nullptr;
	f->chunk.Init();
	return f;
}

ObjNative* newNative(NativeFn function)
{
	ObjNative* native = allocateObject<ObjNative>(ObjType::Native);
	native->function = function;
	return native;
}

ObjClosure* newClosure(ObjFunction* function)
{
	// [Upvalue のポインタ] の配列を管理するメモリを割り当て
	ObjUpvalue** upvalues = allocate<ObjUpvalue*>(function->upvalueCount);
	for (int i = 0; i < function->upvalueCount; i++)
	{
		upvalues[i] = nullptr;
	}

	ObjClosure* closure = allocateObject<ObjClosure>(ObjType::Closure);
	closure->function = function;
	closure->upvalues = upvalues;
	closure->upvalueCount = function->upvalueCount;
	return closure;
}

ObjUpvalue* newUpvalue(Value* slot)
{
	ObjUpvalue* upvalue = allocateObject<ObjUpvalue>(ObjType::Upvalue);
	upvalue->location = slot;
	upvalue->closed = Value::toNil();
	upvalue->next = nullptr;
	return upvalue;
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
#if DEBUG_LOG_GC
	printf("%p free type %d\n", obj, obj->type);
#endif

	switch (obj->type)
	{

	using enum ObjType;

	case Class:
	{
		ObjClass* f = reinterpret_cast<ObjClass*>(obj);
		free(f);
		break;
	}

	case Function:
	{
		ObjFunction* f = reinterpret_cast<ObjFunction*>(obj);
		f->chunk.Free();
		free(f);
		break;
	}
	case Native:
	{
		ObjNative* f = reinterpret_cast<ObjNative*>(obj);
		free(f);
		break;
	}
	case Closure:
	{
		ObjClosure* f = reinterpret_cast<ObjClosure*>(obj);
		free_array(f->upvalues, f->upvalueCount);
		free(f);
		break;
	}
	case Upvalue:
	{
		ObjUpvalue* up = reinterpret_cast<ObjUpvalue*>(obj);
		free(up);
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
	case Class:
		printf("%s", AS_CLASS(value)->name->chars);
		break;
	case Function:
		printFunction(AS_FUNCTION(value));
		break;
	case Native:
		printf("<native fn>");
		break;
	case Closure:
		printFunction(AS_CLOSURE(value)->function);
		break;
	case Upvalue:
		printf("upvalue");
		break;
	case String:
		printf("%s", AS_CSTRING(value));
		break;
	}
}
