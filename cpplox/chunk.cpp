#include "chunk.h"
#include "memory.h"

void Chunk::Init()
{
	count = 0;
	capacity = 0;
	code = nullptr;
	constants.Init();
}

void Chunk::Free()
{
	free_array(code, capacity);
	constants.Free();
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

int Chunk::AddConstant(Value value)
{
	constants.Write(value);

	// 定数を置いたインデックスを返す
	return constants.count - 1;
}
