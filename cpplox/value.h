#pragma once

enum class ValueType
{
	Bool,
	Nil,
	Number,
};

struct Value
{
	ValueType type;
	union {
		bool boolean;
		double number;
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

