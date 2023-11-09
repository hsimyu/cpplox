#include "memory.h"

#include <stdlib.h>

void* reallocate(void* ptr, size_t oldSize, size_t newSize)
{
	if (newSize == 0)
	{
		free(ptr);
		return nullptr;
	}

	void* res = realloc(ptr, newSize);
	return res;
}
