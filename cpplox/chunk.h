#pragma once

#include "common.h"

namespace cpplox
{

enum OpCode
{
	OP_RETURN
};

struct Chunk
{
	// 要素数
	int count = 0;

	// 容量
	size_t capacity = 0;

	// バイトコードの配列
	uint8_t* code = nullptr;

	void Init();
	void Write(uint8_t byte);
};

} // namespace cpplox
