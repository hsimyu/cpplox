#pragma once

inline size_t grow_capacity(size_t capacity)
{
	return (capacity < 8) ? 8 : capacity * 2;
}

void* reallocate(void* ptr, size_t oldSize, size_t newSize);

template<typename T>
T* allocate(size_t count)
{
	return static_cast<T*>(reallocate(nullptr, 0, sizeof(T) * count));
}

template<typename T>
T* grow_array(T* ptr, size_t oldCount, size_t newCount)
{
	auto newPtr = reallocate(ptr, sizeof(T) * oldCount, sizeof(T) * newCount);
	return static_cast<T*>(newPtr);
}

template<typename T>
void free_array(T* ptr, size_t oldCount)
{
	reallocate(ptr, sizeof(T) * oldCount, 0);
}
