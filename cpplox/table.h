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
bool tableSet(Table* table, ObjString* key, Value value);
void tableAddAll(Table* from, Table* to);
