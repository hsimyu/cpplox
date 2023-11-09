#include "debug.h"

#include <cstdio>
#include "chunk.h"

namespace
{
	int simpleInstruction(const char* name, int offset)
	{
		printf("%s\n", name);
		return offset + 1;
	}
}

void disassembleChunk(const Chunk* chunk, const char* name)
{
	// TODO: ÉçÉOèoóÕÇ injection
	printf("== %s == \n", name);
	for (int offset = 0; offset < chunk->count;)
	{
		offset = disassembleInstruction(chunk, offset);
	}
}

int disassembleInstruction(const Chunk* chunk, int offset)
{
	printf("%04d ", offset);
	auto instruction = chunk->code[offset];

	switch (instruction)
	{
	case OP_RETURN:
		return simpleInstruction("OP_RETURN", offset);
	default:
		printf("Unknown opcode %d\n", instruction);
		return offset + 1;
	}
}
