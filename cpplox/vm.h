#pragma once

#include "chunk.h"
#include "value.h"

constexpr size_t STACK_COUNT_MAX = 1024;

struct VM
{
	Chunk* chunk = nullptr;
	uint8_t* ip = nullptr;
	Value stack[STACK_COUNT_MAX];
	Value* stackTop = nullptr;
};

enum class InterpretResult
{
	INTERPRET_OK,
	INTERPRET_COMPILE_ERROR,
	INTERPRET_RUNTIME_ERROR,
};

void initVM();
void freeVM();

InterpretResult interpret(Chunk* chunk);
void push(Value value);
Value pop();
