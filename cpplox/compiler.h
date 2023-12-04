#pragma once

struct ObjFunction;

ObjFunction* compile(const char* source);
void markCompilerRoots();
