#include "chunk.h"
#include "memory.h"

void Chunk::Init()
{
	count = 0;
	capacity = 0;
	code = nullptr;
	lines = nullptr;
	constants.Init();
}

void Chunk::Free()
{
	free_array(code, capacity);
	free_array(lines, capacity);
	constants.Free();
	Init();
}

void Chunk::Write(uint8_t byte, int line)
{
	if (capacity < count + 1)
	{
		auto oldCapacity = capacity;
		capacity = grow_capacity(oldCapacity);
		code = grow_array(code, oldCapacity, capacity);
		lines = grow_array(lines, oldCapacity, capacity);
	}

	code[count] = byte;
	lines[count] = line;
	count++;
}

int Chunk::AddConstant(Value value)
{
	constants.Write(value);

	// 定数を置いたインデックスを返す
	return constants.count - 1;
}
