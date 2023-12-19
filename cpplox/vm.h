#pragma once

#include "chunk.h"
#include "value.h"
#include "table.h"

constexpr size_t FRAMES_MAX = 64;
constexpr size_t STACK_COUNT_MAX = 1024;

struct Obj;
struct ObjClosure;
struct ObjUpvalue;

struct CallFrame
{
	ObjClosure* closure = nullptr;
	uint8_t* ip = nullptr;
	Value* slots = nullptr;
};

struct Thread
{
	// 関数スタック
	CallFrame frames[FRAMES_MAX] = { };
	int frameCount = 0;

	Value stack[STACK_COUNT_MAX] = { };
	Value* stackTop = nullptr;
};
void initThread(Thread* thread);

struct VM
{
	Thread mainThread;
	Table globals;
	Table strings;
	ObjString* initString = nullptr;
	ObjUpvalue* openUpvalues = nullptr;

	size_t bytesAllocated = 0;
	size_t nextGC;
	Obj* objects = nullptr;

	int grayCount = 0;
	int grayCapacity = 0;
	Obj** grayStack = nullptr;
};

enum class InterpretResult
{
	Ok,
	CompileError,
	RuntimeError,
};

void initVM();
void freeVM();
VM* getVM();

InterpretResult interpret(const char* source);
void push(Value value);
Value pop();
