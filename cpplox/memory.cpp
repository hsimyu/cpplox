﻿#include "memory.h"
#include "common.h"
#include "object.h"
#include "compiler.h"

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

	// 各コールフレームのクロージャをマーク
	for (int i = 0; i < vm->frameCount; i++)
	{
		markObject(reinterpret_cast<Obj*>(vm->frames[i].closure));
	}

	// オープン上位値のリストをマーク
	for (ObjUpvalue* upvalue = vm->openUpvalues; upvalue != nullptr; upvalue = upvalue->next)
	{
		markObject(reinterpret_cast<Obj*>(upvalue));
	}

	// グローバル変数テーブルをマーク
	markTable(&vm->globals);
	markCompilerRoots();
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

	auto vm = getVM();
	if (vm->grayCapacity < vm->grayCount + 1)
	{
		vm->grayCapacity = grow_capacity(vm->grayCapacity);

		// NOTE: グレイスタックの割当てに reallocate を使わないのは、再帰的な GC のトリガーを防ぐため
		void* res = realloc(vm->grayStack, sizeof(Obj*) * vm->grayCapacity);
		if (res == nullptr) exit(1);
		vm->grayStack = static_cast<Obj**>(res);
	}

	vm->grayStack[vm->grayCount++] = object;
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
