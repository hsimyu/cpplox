#include "vm.h"

#include "common.h"
#include <cstdio>

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

#define BINARY_OP(op) \
	do { \
		double b = pop(); \
		double a = pop(); \
		push(a op b); \
	} while (false)

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

		case OP_ADD: BINARY_OP(+); break;
		case OP_SUBTRACT: BINARY_OP(-); break;
		case OP_MULTIPLY: BINARY_OP(*); break;
		case OP_DIVIDE: BINARY_OP(/); break;

		case OP_NEGATE: {
			// push し直すより直接書き換えた方が速そう…?
			push(-pop());
			break;
		}

		case OP_RETURN: {
			printValue(pop());
			printf("\n");
			return INTERPRET_OK;
		}

		default:
			return INTERPRET_RUNTIME_ERROR;

		}
	}
}

#undef BINARY_OP

void resetStack()
{
	vm.stackTop = vm.stack;
}

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
