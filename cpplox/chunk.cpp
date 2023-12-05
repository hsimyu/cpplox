#include "chunk.h"
#include "memory.h"
#include "vm.h"

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
	// reallocate 時に Value が GC 対象にならないように、スタックに積んでおく
	push(value);

	constants.Write(value);

	// Value を取り出す
	pop();

	// 定数を置いたインデックスを返す
	return constants.count - 1;
}
