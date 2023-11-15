#pragma once

struct Obj;
struct ObjString;

enum class ValueType
{
	Bool,
	Nil,
	Number,
	Obj,
};

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

	bool isBool() const
	{
		return this->type == ValueType::Bool;
	}

	bool isNil() const
	{
		return this->type == ValueType::Nil;
	}

	bool isNumber() const
	{
		return this->type == ValueType::Number;
	}

	bool isObj() const
	{
		return this->type == ValueType::Obj;
	}

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
};

void printValue(Value val);

struct ValueArray
{
	void Init();
	void Free();
	void Write(Value value);

	size_t capacity = 0;
	int count = 0;
	Value* values = nullptr;
};

bool valuesEqual(Value a, Value b);

