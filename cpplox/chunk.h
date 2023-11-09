#pragma once

#include <cstdint>

enum OpCode
{
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
	void Write(uint8_t byte);

	int count = 0;
	size_t capacity = 0;
	uint8_t* code = nullptr;
};

