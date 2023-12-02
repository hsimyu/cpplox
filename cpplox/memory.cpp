#include "memory.h"
#include "common.h"

#include <stdlib.h>

void* reallocate(void* ptr, int oldSize, int newSize)
{
	if (newSize > oldSize)
	{
#if DEBUG_STRESS_GC
		collectGarbage();
#endif
	}

	if (newSize == 0)
	{
		free(ptr);
		return nullptr;
	}

	void* res = realloc(ptr, newSize);
	return res;
}

void collectGarbage()
{
}
