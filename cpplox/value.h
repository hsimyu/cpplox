#pragma once

using Value = double;

void printValue(Value val);

struct ValueArray
{
	void Init();
	void Free();
	void Write(Value value);

	size_t capacity = 0;
	int count = 0;
	Value* values = nullptr;
};
