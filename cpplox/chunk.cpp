#include "chunk.h"
#include "memory.h"

void Chunk::Init()
{
	count = 0;
	capacity = 0;
	code = nullptr;
}

void Chunk::Free()
{
	free_array(code, capacity);
	Init();
}

void Chunk::Write(uint8_t byte)
{
	if (capacity < count + 1)
	{
		size_t oldCapacity = capacity;
		capacity = grow_capacity(oldCapacity);
		code = grow_array(code, oldCapacity, capacity);
	}

	code[count] = byte;
	count++;
}

