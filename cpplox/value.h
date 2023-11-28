#pragma once

struct Obj;
struct ObjFunction;
struct ObjNative;
struct ObjString;

enum class ValueType
{
	Bool,
	Nil,
	Number,
	Obj,
};

#define IS_BOOL(value) ((value).type == ValueType::Bool)
#define IS_NIL(value) ((value).type == ValueType::Nil)
#define IS_NUMBER(value) ((value).type == ValueType::Number)
#define IS_OBJ(value) ((value).type == ValueType::Obj)

#define AS_BOOL(value) ((value).as.boolean)
#define AS_NUMBER(value) ((value).as.number)
#define AS_OBJ(value) ((value).as.obj)

struct Value
{
	ValueType type;
	union {
		bool boolean;
		double number;
		Obj* obj;
	} as;

	static Value toBool(bool value)
	{
		return {
			.type = ValueType::Bool,
			.as = { .boolean = value },
		};
	}

	static Value toNil()
	{
		return {
			.type = ValueType::Nil,
			.as = { .number = 0 },
		};
	}

	static Value toNumber(double number)
	{
		return {
			.type = ValueType::Number,
			.as = { .number = number },
		};
	}

	static Value toObj(Obj* obj)
	{
		return {
			.type = ValueType::Obj,
			.as = { .obj = obj },
		};
	}

	static Value toObj(ObjFunction* obj)
	{
		return {
			.type = ValueType::Obj,
			.as = { .obj = reinterpret_cast<Obj*>(obj) },
		};
	}

	static Value toObj(ObjNative* obj)
	{
		return {
			.type = ValueType::Obj,
			.as = { .obj = reinterpret_cast<Obj*>(obj) },
		};
	}

	static Value toObj(ObjString* obj)
	{
		return {
			.type = ValueType::Obj,
			.as = { .obj = reinterpret_cast<Obj*>(obj) },
		};
	}
};

void printValue(Value val);

struct ValueArray
{
	void Init();
	void Free();
	void Write(Value value);

	int capacity = 0;
	int count = 0;
	Value* values = nullptr;
};

bool valuesEqual(Value a, Value b);

