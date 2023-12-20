#include "chunk.h"
#include "memory.h"
#include "vm.h"

void initChunk(Chunk* chunk)
{
	chunk->count = 0;
	chunk->capacity = 0;
	chunk->code = nullptr;
	chunk->lines = nullptr;
	initValueArray(&chunk->constants);
}

void freeChunk(Chunk* chunk)
{
	free_array(chunk->code, chunk->capacity);
	free_array(chunk->lines, chunk->capacity);
	freeValueArray(&chunk->constants);
	initChunk(chunk);
}

void writeToChunk(Chunk* chunk, uint8_t byte, int line)
{
	if (chunk->capacity < chunk->count + 1)
	{
		auto oldCapacity = chunk->capacity;
		chunk->capacity = grow_capacity(oldCapacity);
		chunk->code = grow_array(chunk->code, oldCapacity, chunk->capacity);
		chunk->lines = grow_array(chunk->lines, oldCapacity, chunk->capacity);
	}

	chunk->code[chunk->count] = byte;
	chunk->lines[chunk->count] = line;
	chunk->count++;
}

int addConstant(Chunk* chunk, Value value)
{
	// reallocate 時に Value が GC 対象にならないように、スタックに積んでおく
	push(&getVM()->mainThread, value);

	writeToValueArray(&chunk->constants, value);

	// Value を取り出す
	pop(&getVM()->mainThread);

	// 定数を置いたインデックスを返す
	return chunk->constants.count - 1;
}
