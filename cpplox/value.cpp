#include "value.h"
#include "memory.h"

#include <cstdio>

void printValue(Value val)
{
	switch (val.type)
	{
	using enum ValueType;
	case Bool:
		printf("%s", val.as.boolean ? "true" : "false");
		break;
	case Nil:
		printf("nil");
		break;
	case Number:
		printf("%g", val.as.number);
		break;
	}
}

void ValueArray::Init()
{
	count = 0;
	capacity = 0;
	values = nullptr;
}

void ValueArray::Free()
{
	free_array(values, capacity);
	Init();
}

void ValueArray::Write(Value val)
{
	if (capacity < count + 1)
	{
		size_t oldCapacity = capacity;
		capacity = grow_capacity(oldCapacity);
		values = grow_array(values, oldCapacity, capacity);
	}

	values[count] = val;
	count++;
}

bool valuesEqual(Value a, Value b)
{
	// 型が違ったら false
	if (a.type != b.type) return false;

	switch (a.type)
	{
	using enum ValueType;
	case Bool:
		return a.as.boolean == b.as.boolean;
	case Nil:
		return true; // nil == nil -> true とする
	case Number:
		// Value にパディング領域がある可能性があるため、memcmp はできない
		return a.as.number == b.as.number;
	default:
		return false; // Unreachable
	}
}
