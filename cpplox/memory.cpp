#include "memory.h"

#include <stdlib.h>

namespace cpplox {

void* reallocate(void* ptr, size_t oldSize, size_t newSize)
{
	// TODO: �ڐA���̂��� realloc �� free �͊O���獷�����߂�悤�ɂ���ׂ�
	if (newSize == 0) {
		free(ptr);
		return nullptr;
	}

	void* res = realloc(ptr, newSize);
	return res;
}

}