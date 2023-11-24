#include "table.h"

#include "memory.h"
#include <cstring>

namespace
{

Entry* findEntry(Entry* entries, int capacity, ObjString* key)
{
	// open-address hash table
	uint32_t index = key->hash % capacity;
	Entry* tombstone = nullptr;
	for (;;) {
		Entry* entry = &entries[index];
		if (entry->key == nullptr)
		{
			if (IS_NIL(entry->value))
			{
				// 空のエントリ
				return tombstone != nullptr ? tombstone : entry;
			}
			else
			{
				// 墓標を発見
				// 最初に発見した墓標を使い回せるように記録しておく
				// 空のエントリに到達するまでは探針を続ける
				if (tombstone == nullptr) tombstone = entry;
			}
		}
		else if (entry->key == key)
		{
			// キーを発見した
			return entry;
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

	// 墓標を除いた数を数え直す
	table->count = 0;

	// 新しい容量のハッシュテーブルに旧テーブルのエントリを割り当て直す
	for (int i = 0; i < table->capacity; i++)
	{
		Entry* source = &table->entries[i];
		if (source->key == nullptr) continue;

		Entry* dest = findEntry(entries, capacity, source->key);
		dest->key = source->key;
		dest->value = source->value;
		table->count++;
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

bool tableGet(Table* table, ObjString* key, Value* value)
{
	if (table->count == 0) return false;

	Entry* entry = findEntry(table->entries, table->capacity, key);
	if (entry->key == nullptr) return false;

	*value = entry->value;
	return true;
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
	bool isNewKey = (entry->key == nullptr);

	// 新しいキーかつ墓標でなければカウントを増やす
	if (isNewKey && IS_NIL(entry->value))
	{
		table->count++;
	}

	entry->key = key;
	entry->value = value;
	return isNewKey;
}

bool tableDelete(Table* table, ObjString* key)
{
	if (table->count == 0) return false;

	Entry* entry = findEntry(table->entries, table->capacity, key);
	if (entry->key == nullptr) return false;

	// エントリに墓標を立てる
	entry->key = nullptr;
	entry->value = Value::toBool(true);
	return true;
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

ObjString* tableFindString(Table* table, const char* chars, int length, uint32_t hash)
{
	if (table->count == 0) return nullptr;

	uint32_t index = hash % table->capacity;
	for (;;)
	{
		Entry* entry = &table->entries[index];
		if (entry->key == nullptr)
		{
			// nil が入っているエントリを見つけたら返す
			// nil ではない場合は墓標
			if (IS_NIL(entry->value)) return nullptr;
		}
		else if (entry->key->length == length && entry->key->hash == hash && memcmp(entry->key->chars, chars, length) == 0)
		{
			// 長さ、ハッシュ、メモリ内容全てが一致したキーを見つけたら、
			// 所望の文字列を発見したとみなせる
			return entry->key;
		}
		index = (index + 1) % table->capacity;
	}
}
