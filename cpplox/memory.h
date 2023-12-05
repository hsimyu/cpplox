#pragma once

#include "value.h"

#define GC_HEAP_GROW_FACTOR 2

struct Obj;

inline int grow_capacity(int capacity)
{
	return (capacity < 8) ? 8 : capacity * 2;
}

void* reallocate(void* ptr, int oldSize, int newSize);

template<typename T>
T* allocate(int count)
{
	return static_cast<T*>(reallocate(nullptr, 0, sizeof(T) * count));
}

template<typename T>
void free(T* ptr)
{
	reallocate(ptr, sizeof(T), 0);
}

template<typename T>
T* grow_array(T* ptr, int oldCount, int newCount)
{
	auto newPtr = reallocate(ptr, sizeof(T) * oldCount, sizeof(T) * newCount);
	return static_cast<T*>(newPtr);
}

template<typename T>
void free_array(T* ptr, int oldCount)
{
	reallocate(ptr, sizeof(T) * oldCount, 0);
}

void markObject(Obj* object);
void markValue(Value value);
void collectGarbage();
