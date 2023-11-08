#include "common.h"
#include "chunk.h"

int main(int argc, const char* argv[])
{
	cpplox::Chunk c;
	c.Write(cpplox::OP_RETURN);
	c.Free();

	return 0;
}
