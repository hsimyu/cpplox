#include "value.h"
#include "memory.h"
#include "object.h"

#include <cstdio>
#include <cstring>

void printValue(Value val)
{
	switch (val.type)
	{
	using enum ValueType;
	case Bool:
		printf("%s", AS_BOOL(val) ? "true" : "false");
		break;
	case Nil:
		printf("nil");
		break;
	case Number:
		printf("%g", AS_NUMBER(val));
		break;
	case Obj:
		printObject(val);
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
		auto oldCapacity = capacity;
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
		return AS_BOOL(a) == AS_BOOL(b);
	case Nil:
		return true; // nil == nil -> true とする
	case Number:
		// Value にパディング領域がある可能性があるため、memcmp はできない
		return AS_NUMBER(a) == AS_NUMBER(b);
	case Obj:
		// 文字列はインターン化されているので、すべてのオブジェクトはポインタとして比較してよい
		return AS_OBJ(a) == AS_OBJ(b);
	default:
		return false; // Unreachable
	}
}
