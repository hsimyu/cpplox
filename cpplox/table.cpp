#include "table.h"

#include "memory.h"

void initTable(Table* table)
{
	table->count = 0;
	table->capacity = 0;
	table->entries = nullptr;
}

void freeTable(Table* table)
{
	free_array(table->entries, table->capacity);
	initTable(table);
}

