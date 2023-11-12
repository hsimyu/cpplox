#pragma once

#include "chunk.h"

struct VM
{
	Chunk* chunk = nullptr;
	uint8_t* ip = nullptr;
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
