#include "memory.h"

#include <stdlib.h>

namespace cpplox {

void* reallocate(void* ptr, size_t oldSize, size_t newSize)
{
	// TODO: 移植性のため realloc と free は外から差し込めるようにするべき
	if (newSize == 0) {
		free(ptr);
		return nullptr;
	}

	void* res = realloc(ptr, newSize);
	return res;
}

}