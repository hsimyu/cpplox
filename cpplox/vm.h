#pragma once

#include "chunk.h"
#include "value.h"

constexpr size_t STACK_COUNT_MAX = 1024;

struct VM
{
	Chunk* chunk = nullptr;
	uint8_t* ip = nullptr;
	Value stack[STACK_COUNT_MAX] = { 0 };
	Value* stackTop = nullptr;
};

enum class InterpretResult
{
	Ok,
	CompileError,
	RuntimeError,
};

void initVM();
void freeVM();

InterpretResult interpret(const char* source);
void push(Value value);
Value pop();
