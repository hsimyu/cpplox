﻿#include "common.h"

int main(int argc, const char* argv[])
{
	Chunk c;

	// 定数プールに値を格納し、そのインデックスを取得
	// それを定数命令とそのオペランドとしてチャンクに格納する
	int constant = c.AddConstant(1.2);
	c.Write(OP_CONSTANT, 123);
	c.Write(constant, 123);

	c.Write(OP_RETURN, 123);

	disassembleChunk(&c, "test chunk");

	c.Free();

	return 0;
}
