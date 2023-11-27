#pragma once

#include "chunk.h"
#include "value.h"
#include "table.h"

constexpr size_t FRAMES_MAX = 64;
constexpr size_t STACK_COUNT_MAX = 1024;

struct Obj;
struct ObjFunction;

struct CallFrame
{
	ObjFunction* function = nullptr;
	uint8_t* ip = nullptr;
	Value* slots = nullptr;
};

struct VM
{
	// 関数スタック
	CallFrame frames[FRAMES_MAX] = { };
	int frameCount = 0;

	Value stack[STACK_COUNT_MAX] = { };
	Value* stackTop = nullptr;
	Table globals;
	Table strings;
	Obj* objects = nullptr;
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
