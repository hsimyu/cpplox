#pragma once

namespace cpplox
{

struct Chunk;

void disassembleChunk(const Chunk* chunk, const char* name);
int disassembleInstruction(const Chunk* chunk, int offset);

}
