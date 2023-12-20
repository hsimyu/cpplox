#pragma once

#include "chunk.h"
#include "value.h"
#include "table.h"
#include "thread.h"

struct Obj;
struct ObjClosure;

struct VM
{
	Thread mainThread;
	Table globals;
	Table strings;
	ObjString* initString = nullptr;

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
void push(Thread* thread, Value value);
Value pop(Thread* thread);
