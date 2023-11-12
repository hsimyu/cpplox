﻿#pragma once

#include <cstdint>
#include "value.h"

enum OpCode
{
	OP_CONSTANT,
	OP_NEGATE,
	OP_RETURN
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
	size_t capacity = 0;
	uint8_t* code = nullptr;
	int* lines = nullptr;
	ValueArray constants;
};

