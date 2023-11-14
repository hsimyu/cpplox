#include "value.h"
#include "memory.h"

#include <cstdio>

void printValue(Value val)
{
	printf("%g", val.as.number);
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
