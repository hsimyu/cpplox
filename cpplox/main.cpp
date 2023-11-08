#include "common.h"

int main(int argc, const char* argv[])
{
	cpplox::Chunk c;
	c.Write(cpplox::OP_RETURN);

	cpplox::disassembleChunk(&c, "test chunk");

	c.Free();

	return 0;
}
