#pragma once

struct Obj;
struct ObjClass;
struct ObjInstance;
struct ObjBoundMethod;
struct ObjFunction;
struct ObjNative;
struct ObjClosure;
struct ObjUpvalue;
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

#define TO_BOOL(value) Value{ValueType::Bool, {.boolean = value} }
#define TO_NIL() Value{ValueType::Nil, {.number = 0} }
#define TO_NUMBER(value) Value{ValueType::Number, {.number = value} }
#define TO_OBJ(value) Value{ValueType::Obj, {.obj = reinterpret_cast<Obj*>(value)} }

struct Value
{
	ValueType type;
	union {
		bool boolean;
		double number;
		Obj* obj;
	} as;
};

#endif

void printValue(Value val);
ObjString* toString(Value val);

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

