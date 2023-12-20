#pragma once

struct ObjFunction;

ObjFunction* compileImpl(const char* source);
void markCompilerRoots();
