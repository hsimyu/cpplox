﻿#pragma once

#include <cstdint>
#include "value.h"

enum OpCode
{
	OP_CONSTANT,
	OP_NIL,
	OP_TRUE,
	OP_FALSE,
	OP_POP,
	OP_GET_LOCAL,
	OP_SET_LOCAL,
	OP_GET_GLOBAL,
	OP_DEFINE_GLOBAL,
	OP_SET_GLOBAL,
	OP_GET_UPVALUE,
	OP_SET_UPVALUE,
	OP_GET_PROPERTY,
	OP_SET_PROPERTY,
	OP_GET_SUPER,
	OP_EQUAL,
	OP_GREATER,
	OP_LESS,
	OP_ADD,
	OP_SUBTRACT,
	OP_MULTIPLY,
	OP_DIVIDE,
	OP_NOT,
	OP_NEGATE,
	OP_PRINT,
	OP_JUMP,
	OP_JUMP_IF_FALSE,
	OP_LOOP,
	OP_CALL,
	OP_INVOKE,
	OP_CLOSURE,
	OP_CLOSE_UPVALUE,
	OP_RETURN,
	OP_CLASS,
	OP_INHERIT,
	OP_METHOD,
};

struct Chunk
{
	Chunk()
	{
		Init();
	}

	void Init();
	void Free();
	void Write(uint8_t byte, int line);
	int AddConstant(Value value);

	int count = 0;
	int capacity = 0;
	uint8_t* code = nullptr;
	int* lines = nullptr;
	ValueArray constants;
};

