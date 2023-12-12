#include "value.h"
#include "memory.h"
#include "object.h"

#include <cstdio>
#include <cstring>

void printValue(Value val)
{
#if NAN_BOXING
	if (IS_BOOL(val))
	{
		printf(AS_BOOL(val) ? "true" : "false");
	}
	else if (IS_NIL(val))
	{
		printf("nil");
	}
	else if (IS_NUMBER(val))
	{
		printf("%g", AS_NUMBER(val));
	}
	else if (IS_OBJ(val))
	{
		printObject(val);
	}
#else
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
#endif
}

ObjString* toString(Value val)
{
	constexpr size_t toStringBufferSize = 64;
#if NAN_BOXING
	if (IS_BOOL(val))
	{
		if (AS_BOOL(val))
		{
			return copyString("true");
		}
		else
		{
			return copyString("false");
		}
	}
	else if (IS_NIL(val))
	{
		return copyString("nil");
	}
	else if (IS_NUMBER(val))
	{
		char buffer[toStringBufferSize];
		snprintf(buffer, toStringBufferSize, "%g", AS_NUMBER(val));
		buffer[toStringBufferSize - 1] = '\0';
		return copyString(buffer);
	}
	else if (IS_OBJ(val))
	{
		char buffer[toStringBufferSize];
		writeObjString(val, buffer, toStringBufferSize);
		buffer[toStringBufferSize - 1] = '\0';
		return copyString(buffer);
	}
	return copyString("unknown");
#else
	switch (val.type)
	{
	using enum ValueType;
	case Bool:
	{
		if (AS_BOOL(val))
		{
			return copyString("true");
		}
		else
		{
			return copyString("false");
		}
	}
	case Nil:
	{
		return copyString("nil");
	}
	case Number:
	{
		char buffer[toStringBufferSize];
		snprintf(buffer, toStringBufferSize, "%g", AS_NUMBER(val));
		buffer[toStringBufferSize - 1] = '\0';
		return copyString(buffer);
	}
	case Obj:
	{
		char buffer[toStringBufferSize];
		writeObjString(val, buffer, toStringBufferSize);
		buffer[toStringBufferSize - 1] = '\0';
		return copyString(buffer);
	}
	default:
		return copyString("unknown");
	}
#endif
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
#if NAN_BOXING
	if (IS_NUMBER(a) && IS_NUMBER(b))
	{
		return AS_NUMBER(a) == AS_NUMBER(b);
	}
	return a == b;
#else
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
#endif
}
