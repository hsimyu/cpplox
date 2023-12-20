#pragma once

#include "value.h"

constexpr size_t FRAMES_MAX = 64;
constexpr size_t STACK_COUNT_MAX = 1024;

struct ObjClosure;
struct ObjUpvalue;

struct CallFrame
{
	ObjClosure* closure = nullptr;
	uint8_t* ip = nullptr;
	Value* slots = nullptr;
};

struct Thread
{
	// 関数スタック
	CallFrame frames[FRAMES_MAX] = { };
	int frameCount = 0;

	Value stack[STACK_COUNT_MAX] = { };
	Value* stackTop = nullptr;

	ObjUpvalue* openUpvalues = nullptr;
};
void initThread(Thread* thread);
void freeThread(Thread* thread);

