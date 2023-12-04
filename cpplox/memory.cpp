#include "memory.h"
#include "common.h"
#include "object.h"

#include <stdlib.h>

#if DEBUG_LOG_GC
#include <cstdio>
#include "debug.h"
#endif

namespace
{

void markRoots()
{
	auto vm = getVM();

	// スタック上の変数をマーク
	for (Value* slot = vm->stack; slot < vm->stackTop; slot++)
	{
		markValue(*slot);
	}

	// グローバル変数テーブルをマーク
	markTable(&vm->globals);
}

}

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

void markObject(Obj* object)
{
	if (object == nullptr) return;

#if DEBUG_LOG_GC
	printf("%p mark ", object);
	printValue(Value::toObj(object));
	printf("\n");
#endif

	object->isMarked = true;
}

void markValue(Value value)
{
	if (IS_OBJ(value)) markObject(AS_OBJ(value));
}

void collectGarbage()
{
#if DEBUG_LOG_GC
	printf("-- gc begin\n");
#endif

	markRoots();

#if DEBUG_LOG_GC
	printf("-- gc end\n");
#endif
}
