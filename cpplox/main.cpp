#include "common.h"

int main(int argc, const char* argv[])
{
	Chunk c;
	c.Write(OP_RETURN);

	disassembleChunk(&c, "test chunk");

	c.Free();

	return 0;
}
