#pragma once

#include <cstdint>

#include "chunk.h"
#include "value.h"
#include "table.h"

struct Thread;

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_CLASS(value) isObjType(value, ObjType::Class)
#define AS_CLASS(value) (reinterpret_cast<ObjClass*>(AS_OBJ(value)))

#define IS_INSTANCE(value) isObjType(value, ObjType::Instance)
#define AS_INSTANCE(value) (reinterpret_cast<ObjInstance*>(AS_OBJ(value)))

#define IS_BOUND_METHOD(value) isObjType(value, ObjType::BoundMethod)
#define AS_BOUND_METHOD(value) (reinterpret_cast<ObjBoundMethod*>(AS_OBJ(value)))

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

#define IS_THREAD(value) isObjType(value, ObjType::Thread)
#define AS_THREAD(value) (reinterpret_cast<ObjThread*>(AS_OBJ(value)))

enum class ObjType
{
	Class,
	Instance,
	BoundMethod,
	Function,
	Native,
	Closure,
	Upvalue,
	String,
	Thread,
};

struct Obj
{
	ObjType type;
	bool isMarked = false;
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
	Value closed;
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
ObjString* copyString(const char* chars);

struct ObjClass
{
	Obj obj;
	ObjString* name = nullptr;
	Table methods;
};

ObjClass* newClass(ObjString* name);

struct ObjInstance
{
	Obj obj;
	ObjClass* klass = nullptr;
	Table fields;
};

ObjInstance* newInstance(ObjClass* klass);

struct ObjBoundMethod
{
	Obj obj;
	Value receiver;
	ObjClosure* method = nullptr;
};

ObjBoundMethod* newBoundMethod(Value receiver, ObjClosure* method);

struct ObjThread
{
	Obj obj;
	Thread* thread = nullptr;
};

ObjThread* newThread(Thread* thread);

void freeObject(Obj* obj);
void printObject(Value value);
void writeObjString(Value value, char* buffer, size_t bufferSize);

inline Value toObjValue(ObjString* s)
{
	return TO_OBJ(s);
}

inline bool isObjType(Value value, ObjType type)
{
	return IS_OBJ(value) && AS_OBJ(value)->type == type;
}
