﻿#include "debug.h"

#include <cstdio>
#include "chunk.h"

namespace
{
	int simpleInstruction(const char* name, int offset)
	{
		printf("%s\n", name);
		return offset + 1;
	}

	int constantInstruction(const char* name, const Chunk* chunk, int offset)
	{
		const auto constantIndex = chunk->code[offset + 1];
		const auto constantValue = chunk->constants.values[constantIndex];
		printf("%-16s %4d '", name, constantIndex);
		printValue(constantValue);
		printf("'\n");
		return offset + 2;
	}
}

void disassembleChunk(const Chunk* chunk, const char* name)
{
	printf("== %s == \n", name);
	for (int offset = 0; offset < chunk->count;)
	{
		offset = disassembleInstruction(chunk, offset);
	}
}

int disassembleInstruction(const Chunk* chunk, int offset)
{
	printf("%04d ", offset);
	if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1])
	{
		printf("   | ");
	}
	else
	{
		printf("%4d ", chunk->lines[offset]);
	}

	auto instruction = chunk->code[offset];

	switch (instruction)
	{
	case OP_CONSTANT:
		return constantInstruction("OP_CONSTANT", chunk, offset);
	case OP_NIL:
		return simpleInstruction("OP_NIL", offset);
	case OP_TRUE:
		return simpleInstruction("OP_TRUE", offset);
	case OP_FALSE:
		return simpleInstruction("OP_FALSE", offset);
	case OP_EQUAL:
		return simpleInstruction("OP_EQUAL", offset);
	case OP_GREATER:
		return simpleInstruction("OP_GREATER", offset);
	case OP_LESS:
		return simpleInstruction("OP_LESS", offset);
	case OP_ADD:
		return simpleInstruction("OP_ADD", offset);
	case OP_SUBTRACT:
		return simpleInstruction("OP_SUBTRACT", offset);
	case OP_MULTIPLY:
		return simpleInstruction("OP_MULTIPLY", offset);
	case OP_DIVIDE:
		return simpleInstruction("OP_DIVIDE", offset);
	case OP_NOT:
		return simpleInstruction("OP_NOT", offset);
	case OP_NEGATE:
		return simpleInstruction("OP_NEGATE", offset);
	case OP_RETURN:
		return simpleInstruction("OP_RETURN", offset);
	default:
		printf("Unknown opcode %d\n", instruction);
		return offset + 1;
	}
}
