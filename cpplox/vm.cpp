#include "vm.h"

#include "common.h"
#include "compiler.h"
#include "object.h"
#include "memory.h"

#if DEBUG_TRACE_EXECUTION
#include "debug.h"
#endif

#include <cstdio>
#include <cstdarg>
#include <cstring>

VM vm; // global vm instance

namespace
{

uint8_t read_byte()
{
	return *vm.ip++;
}

Value read_constant()
{
	return (vm.chunk->constants.values[read_byte()]);
}

ObjString* read_string()
{
	return AS_STRING(read_constant());
}

Value peek(int distance)
{
	return vm.stackTop[-1 - distance];
}

void resetStack()
{
	vm.stackTop = vm.stack;
}

void runtimeError(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);

	fputs("\n", stderr);

	// コードを読んだあとに ip++ されているので、エラーを起こしたのは現在実行しているコードの一つ前になる
	size_t instruction = vm.ip - vm.chunk->code - 1;
	int line = vm.chunk->lines[instruction];
	fprintf(stderr, "[line %d] in script\n", line);

	resetStack();
}

#define BINARY_OP(ValueType, op) \
	do { \
        if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
            runtimeError("Operand must be numbers."); \
            return InterpretResult::RuntimeError; \
        } \
		double b = AS_NUMBER(pop()); \
		double a = AS_NUMBER(pop()); \
		push(Value::to##ValueType(a op b)); \
	} while (false)

bool isFalsey(Value value)
{
	// nil: falsey
	// boolean ではない: truthy
	// boolean で true: truthy
	// boolean で false: falsey
	return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

void concatenate()
{
	ObjString* b = AS_STRING(pop());
	ObjString* a = AS_STRING(pop());

	int length = a->length + b->length;
	char* chars = allocate<char>(length + 1);
	memcpy(chars, a->chars, a->length);
	memcpy(chars + a->length, b->chars, b->length);
	chars[length] = '\0';

	ObjString* result = takeString(chars, length);
	push(toObjValue(result));
}

InterpretResult run()
{
	for (;;)
	{
#if DEBUG_TRACE_EXECUTION
		printf("          ");
		for (Value* slot = vm.stack; slot < vm.stackTop; slot++)
		{
			printf("[ ");
			printValue(*slot);
			printf(" ]");
		}
		printf("\n");
		disassembleInstruction(vm.chunk, static_cast<int>(vm.ip - vm.chunk->code));
#endif

		using enum InterpretResult;
		uint8_t instruction = read_byte();
		switch (instruction)
		{

		case OP_CONSTANT: {
			Value constant = read_constant();
			push(constant);
			break;
		}

		case OP_NIL:
			push(Value::toNil());
			break;

		case OP_TRUE:
			push(Value::toBool(true));
			break;

		case OP_FALSE:
			push(Value::toBool(false));
			break;

		case OP_POP:
			pop();
			break;

		case OP_GET_GLOBAL:
		{
			ObjString* name = read_string();
			Value value;
			if (!tableGet(&vm.globals, name, &value)) {
				runtimeError("Undefined variable '%s'.", name->chars);
				return RuntimeError;
			}
			push(value);
			break;
		}

		case OP_DEFINE_GLOBAL:
		{
			ObjString* name = read_string();
			tableSet(&vm.globals, name, peek(0));
			pop();
			break;
		}

		case OP_EQUAL:
		{
			Value b = pop();
			Value a = pop();
			push(Value::toBool(valuesEqual(a, b)));
			break;
		}

		case OP_GREATER: BINARY_OP(Bool, >); break;
		case OP_LESS: BINARY_OP(Bool, <); break;
		case OP_ADD:
		{
			if (IS_STRING(peek(0)) && IS_STRING(peek(1)))
			{
				concatenate();
			}
			else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1)))
			{
				double b = AS_NUMBER(pop()); \
				double a = AS_NUMBER(pop()); \
				push(Value::toNumber(a + b)); \
			}
			else
			{
				runtimeError("Operand must be two numbers or two strings.");
				return InterpretResult::RuntimeError;
			}
			break;
		}
		case OP_SUBTRACT: BINARY_OP(Number, -); break;
		case OP_MULTIPLY: BINARY_OP(Number, *); break;
		case OP_DIVIDE: BINARY_OP(Number, /); break;

		case OP_NOT:
			push(Value::toBool(isFalsey(pop())));
			break;

		case OP_NEGATE: {
			if (!IS_NUMBER(peek(0)))
			{
				runtimeError("Operand must be a number.");
				return RuntimeError;
			}
			push(Value::toNumber(-AS_NUMBER(pop())));
			break;
		}

		case OP_PRINT: {
			// stack トップに expression の評価結果が置かれているはず
			printValue(pop());
			printf("\n");
			break;
		}

		case OP_RETURN: {
			return Ok;
		}

		default:
			return RuntimeError;

		}
	}
}

#undef BINARY_OP

void freeObjects()
{
	Obj* target = vm.objects;
	while (target != nullptr)
	{
		Obj* next = target->next;
		freeObject(target);
		target = next;
	}
}

}

void initVM()
{
	resetStack();
	vm.objects = nullptr;
	initTable(&vm.globals);
	initTable(&vm.strings);
}

void freeVM()
{
	freeTable(&vm.globals);
	freeTable(&vm.strings);
	freeObjects();
}

VM* getVM()
{
	return &vm;
}

InterpretResult interpret(Chunk* chunk)
{
	vm.chunk = chunk;
	vm.ip = vm.chunk->code;
	return run();
}

InterpretResult interpret(const char* source)
{
	Chunk chunk{};

	if (!compile(source, &chunk))
	{
		chunk.Free();
		return InterpretResult::CompileError;
	}

	vm.chunk = &chunk;
	vm.ip = vm.chunk->code;

	auto result = run();
	chunk.Free();
	return result;
}

void push(Value value)
{
	*vm.stackTop = value;
	vm.stackTop++;
}

Value pop()
{
	vm.stackTop--;
	return *vm.stackTop;
}

