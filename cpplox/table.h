#pragma once

#include "object.h"

struct Entry {
	ObjString* key = nullptr;
	Value value;
};

struct Table {
	int count = 0;
	int capacity = 0;
	Entry* entries = nullptr;
};

void initTable(Table* table);
void freeTable(Table* table);
bool tableGet(Table* table, ObjString* key, Value* value);
bool tableSet(Table* table, ObjString* key, Value value);
bool tableDelete(Table* table, ObjString* key);
void tableAddAll(Table* from, Table* to);
ObjString* tableFindString(Table* table, const char* chars, int length, uint32_t hash);
