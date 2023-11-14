#include "vm.h"

#include "common.h"
#include "compiler.h"

#if DEBUG_TRACE_EXECUTION
#include "debug.h"
#endif

#include <cstdio>
#include <cstdarg>

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
        if (!peek(0).isNumber() || !peek(1).isNumber()) { \
            runtimeError("Operand must be numbers."); \
            return InterpretResult::RuntimeError; \
        } \
		double b = pop().as.number; \
		double a = pop().as.number; \
		push(Value::to##ValueType(a op b)); \
	} while (false)

bool isFalsey(Value value)
{
	// nil: falsey
	// boolean ではない: truthy
	// boolean で true: truthy
	// boolean で false: falsey
	return value.isNil() || (value.isBool() && !value.as.boolean);
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

		case OP_ADD: BINARY_OP(Number, +); break;
		case OP_SUBTRACT: BINARY_OP(Number, -); break;
		case OP_MULTIPLY: BINARY_OP(Number, *); break;
		case OP_DIVIDE: BINARY_OP(Number, /); break;

		case OP_NOT:
			push(Value::toBool(isFalsey(pop())));
			break;

		case OP_NEGATE: {
			if (!peek(0).isNumber())
			{
				runtimeError("Operand must be a number.");
				return RuntimeError;
			}
			push(Value::toNumber(-pop().as.number));
			break;
		}

		case OP_RETURN: {
			printValue(pop());
			printf("\n");
			return Ok;
		}

		default:
			return RuntimeError;

		}
	}
}

#undef BINARY_OP
}

void initVM()
{
	resetStack();
}

void freeVM()
{
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

