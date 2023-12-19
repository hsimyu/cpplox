#include "memory.h"
#include "common.h"
#include "object.h"
#include "compiler.h"
#include "vm.h"

#include <stdlib.h>

#if DEBUG_LOG_GC
#include <cstdio>
#include "debug.h"
#endif

namespace
{

void markThread(Thread* thread)
{
	// スタック上の変数をマーク
	for (Value* slot = thread->stack; slot < thread->stackTop; slot++)
	{
		markValue(*slot);
	}

	// 各コールフレームのクロージャをマーク
	for (int i = 0; i < thread->frameCount; i++)
	{
		markObject(reinterpret_cast<Obj*>(thread->frames[i].closure));
	}

	// オープン上位値のリストをマーク
	for (ObjUpvalue* upvalue = thread->openUpvalues; upvalue != nullptr; upvalue = upvalue->next)
	{
		markObject(reinterpret_cast<Obj*>(upvalue));
	}
}

void markRoots()
{
	auto vm = getVM();
	markThread(&vm->mainThread);

	// グローバル変数テーブルをマーク
	markTable(&vm->globals);
	markCompilerRoots();

	markObject(reinterpret_cast<Obj*>(vm->initString));
}

void markArray(ValueArray* array)
{
	for (int i = 0; i < array->count; i++)
	{
		markValue(array->values[i]);
	}
}

void blackenObject(Obj* obj)
{
#if DEBUG_LOG_GC
	printf("%p blacken ", obj);
	printValue(Value::toObj(obj));
	printf("\n");
#endif

	switch (obj->type)
	{
	case ObjType::Class:
	{
		ObjClass* klass = reinterpret_cast<ObjClass*>(obj);
		markObject(reinterpret_cast<Obj*>(klass->name));
		markTable(&klass->methods);
		break;
	}
	case ObjType::Instance:
	{
		ObjInstance* instance = reinterpret_cast<ObjInstance*>(obj);
		markObject(reinterpret_cast<Obj*>(instance->klass));
		markTable(&instance->fields);
		break;
	}
	case ObjType::BoundMethod:
	{
		ObjBoundMethod* b = reinterpret_cast<ObjBoundMethod*>(obj);
		markValue(b->receiver);
		markObject(reinterpret_cast<Obj*>(b->method));
		break;
	}
	case ObjType::Closure:
	{
		ObjClosure* closure = reinterpret_cast<ObjClosure*>(obj);
		markObject(reinterpret_cast<Obj*>(closure->function));
		for (int i = 0; i < closure->upvalueCount; i++)
		{
			markObject(reinterpret_cast<Obj*>(closure->upvalues[i]));
		}
		break;
	}
	case ObjType::Function:
	{
		ObjFunction* f = reinterpret_cast<ObjFunction*>(obj);
		markObject(reinterpret_cast<Obj*>(f->name));
		markArray(&f->chunk.constants);
		break;
	}
	case ObjType::Upvalue:
		// クローズ上位値をマーク
		markValue(reinterpret_cast<ObjUpvalue*>(obj)->closed);
		break;
	case ObjType::Thread:
	{
		ObjThread* t = reinterpret_cast<ObjThread*>(obj);
		markThread(t->thread);
		break;
	}
	case ObjType::Native:
	case ObjType::String:
		break;
	}
}

void traceReferences()
{
	auto vm = getVM();
	while (vm->grayCount > 0)
	{
		Obj* obj = vm->grayStack[--vm->grayCount];
		blackenObject(obj);
	}
}

void sweep()
{
	auto vm = getVM();
	Obj* prev = nullptr;
	Obj* obj = vm->objects;

	// 全てのオブジェクトを辿り、マークされていない白色オブジェクトを解放する
	while (obj != nullptr)
	{
		if (obj->isMarked)
		{
			obj->isMarked = false;
			prev = obj;
			obj = obj->next;
		}
		else
		{
			Obj* unreached = obj;

#if DEBUG_LOG_GC
	printf("%p sweep ", obj);
	printValue(Value::toObj(obj));
	printf("\n");
#endif

			obj = obj->next;

			// LinkedList の繋ぎ直し
			if (prev != nullptr)
			{
				prev->next = obj;
			}
			else
			{
				// prev == nullptr なら、解放対象は先頭のオブジェクト
				// -> 先頭を更新する
				vm->objects = obj;
			}

			freeObject(unreached);
		}
	}
}

}

void* reallocate(void* ptr, int oldSize, int newSize)
{
	auto vm = getVM();
	vm->bytesAllocated += newSize - oldSize;

	if (newSize > oldSize)
	{
#if DEBUG_STRESS_GC
		collectGarbage();
#endif

		if (vm->bytesAllocated > vm->nextGC)
		{
			collectGarbage();
		}
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
	if (object->isMarked) return;

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
	auto vm = getVM();

#if DEBUG_LOG_GC
	printf("--- gc begin\n");
	size_t before = vm->bytesAllocated;
#endif

	markRoots();
	traceReferences();
	tableRemoveWhite(&getVM()->strings);
	sweep();

	// 一度 GC したら、次は使用メモリ量の FACTOR 倍になるまで GC しない
	// デフォルトは 2 倍
	vm->nextGC = vm->bytesAllocated * GC_HEAP_GROW_FACTOR;

#if DEBUG_LOG_GC
	printf("--- gc end\n");
	printf("   collected %zu bytes (from %zu to %zu) next at %zu\n",
		   before - vm->bytesAllocated, before, vm->bytesAllocated, vm->nextGC);
#endif
}
