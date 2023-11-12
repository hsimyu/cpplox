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

InterpretResult run()
{
	for (;;)
	{
#if DEBUG_TRACE_EXECUTION
		disassembleInstruction(vm.chunk, static_cast<int>(vm.ip - vm.chunk->code));
#endif

		using enum InterpretResult;
		uint8_t instruction = read_byte();
		switch (instruction)
		{

		case OP_CONSTANT: {
			Value constant = read_constant();
			printValue(constant);
			printf("\n");
			break;
		}

		case OP_RETURN: {
			return INTERPRET_OK;
		}

		default:
			return INTERPRET_RUNTIME_ERROR;

		}
	}
}

}

void initVM()
{
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
