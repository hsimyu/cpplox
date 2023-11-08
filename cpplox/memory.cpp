#include "memory.h"

#include <stdlib.h>

namespace cpplox {

void* reallocate(void* ptr, size_t oldSize, size_t newSize)
{
	// TODO: ˆÚA«‚Ì‚½‚ß realloc ‚Æ free ‚ÍŠO‚©‚ç·‚µ‚ß‚é‚æ‚¤‚É‚·‚é‚×‚«
	if (newSize == 0) {
		free(ptr);
		return nullptr;
	}

	void* res = realloc(ptr, newSize);
	return res;
}

}