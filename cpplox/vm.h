#pragma once

#include "chunk.h"

struct VM
{
	Chunk* chunk = nullptr;
};

void initVM();
void freeVM();
