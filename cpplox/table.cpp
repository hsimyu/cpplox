#include "table.h"

#include "memory.h"

namespace
{

Entry* findEntry(Entry* entries, int capacity, ObjString* key)
{
	// open-address hash table
	uint32_t index = key->hash % capacity;
	for (;;) {
		Entry* entryCandidate = &entries[index];
		if (entryCandidate->key == key || entryCandidate->key == nullptr)
		{
			// 指定した位置に入っているエントリのキーが一致しているか、未割り当てであればそれを返す
			return entryCandidate;
		}
		index = (index + 1) % capacity;
	}
}

void adjustCapacity(Table* table, int capacity)
{
	Entry* entries = allocate<Entry>(capacity);

	for (int i = 0; i < capacity; i++)
	{
		entries[i].key = nullptr;
		entries[i].value = Value::toNil();
	}

	// 新しい容量のハッシュテーブルに旧テーブルのエントリを割り当て直す
	for (int i = 0; i < table->capacity; i++)
	{
		Entry* source = &table->entries[i];
		if (source->key == nullptr) continue;

		Entry* dest = findEntry(entries, capacity, source->key);
		dest->key = source->key;
		dest->value = source->value;
	}

	free_array(table->entries, table->capacity);
	table->entries = entries;
	table->capacity = capacity;
}

}

// テーブルの占有率の閾値
constexpr double TABLE_MAX_LOAD = 0.75;

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

bool tableSet(Table* table, ObjString* key, Value value)
{
	// allocate table entries
	if (table->count + 1 > table->capacity * TABLE_MAX_LOAD)
	{
		int capacity = grow_capacity(table->capacity);
		adjustCapacity(table, capacity);
	}

	Entry* entry = findEntry(table->entries, table->capacity, key);
	bool isNewKey = (entry->key = nullptr);

	if (isNewKey)
	{
		table->count++;
	}

	entry->key = key;
	entry->value = value;
	return isNewKey;
}

void tableAddAll(Table* from, Table* to)
{
	for (int i = 0; i < from->capacity; i++)
	{
		Entry* entry = &from->entries[i];
		if (entry->key != nullptr)
		{
			tableSet(to, entry->key, entry->value);
		}
	}
}
