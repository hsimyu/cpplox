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
#include <ctime>

VM vm; // global vm instance

namespace
{

Value clockNative(int argCount, Value* args)
{
	return Value::toNumber(static_cast<double>(clock()) / CLOCKS_PER_SEC);
}

}

namespace
{

void runtimeError(const char* format, ...);

Value peek(int distance)
{
	return vm.stackTop[-1 - distance];
}

bool call(ObjClosure* closure, int argCount)
{
	if (argCount != closure->function->arity)
	{
		runtimeError("Expected %d arguments but got %d.", closure->function->arity, argCount);
		return false;
	}

	if (vm.frameCount == FRAMES_MAX)
	{
		runtimeError("Stack overflow.");
		return false;
	}

	CallFrame* frame = &vm.frames[vm.frameCount++];
	frame->closure = closure;
	frame->ip = closure->function->chunk.code;

	// stack 中に乗っている引数の数を引いた位置が、frame が参照するスタックの開始位置になる
	// argCount + 1 なのは、予約分の 0 番の分
	frame->slots = vm.stackTop - (argCount + 1);

	return true;
}

bool callValue(Value callee, int argCount)
{
	if (IS_OBJ(callee))
	{
		switch (OBJ_TYPE(callee))
		{
		// All ObjType::Function are wrapped as closure
		case ObjType::Closure:
			return call(AS_CLOSURE(callee), argCount);
		case ObjType::Native:
		{
			// Native 関数呼び出しの場合は、ここで即座に呼び出す
			NativeFn native = AS_NATIVE(callee)->function;
			Value result = native(argCount, vm.stackTop - argCount);
			vm.stackTop -= argCount + 1;
			push(result);
			return true;
		}
		default:
			// Not Callable Object
			break;
		}
	}
	runtimeError("Can only call functions and classes.");
	return false;
}

ObjUpvalue* captureUpvalue(Value* local)
{
	ObjUpvalue* prevUpvalue = nullptr;
	ObjUpvalue* upvalue = vm.openUpvalues;

	// キャプチャしようとしているローカルより後方にいるオープン上位値があるかを探す
	// 対象よりスタックの後方にいるオープン上位値を見つけた、
	// もしくはオープンな上位値リストの末尾 (nullptr) に到達したら抜ける
	while (upvalue != nullptr && upvalue->location > local)
	{
		prevUpvalue = upvalue;
		upvalue = upvalue->next;
	}

	if (upvalue != nullptr && upvalue->location == local)
	{
		// 既存のオープン上位値としてキャプチャ済みだったのでそれを返す
		return upvalue;
	}

	ObjUpvalue* createdValue = newUpvalue(local);
	createdValue->next = upvalue; // 

	if (prevUpvalue == nullptr)
	{
		// prev がない == 先頭の上位値
		vm.openUpvalues = createdValue;
	}
	else
	{
		// LinkedList の末尾に繋げる
		prevUpvalue->next = createdValue;
	}

	return createdValue;
}

void resetStack()
{
	vm.stackTop = vm.stack;
	vm.frameCount = 0;
	vm.openUpvalues = nullptr;
}

void runtimeError(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);

	fputs("\n", stderr);

	// print stack trace
	for (int i = vm.frameCount - 1; i >= 0; i--)
	{
		CallFrame* frame = &vm.frames[i];
		ObjFunction* function = frame->closure->function;

		size_t instruction = frame->ip - function->chunk.code - 1;
		fprintf(stderr, "[line %d] in ", function->chunk.lines[instruction]);

		if (function->name == nullptr)
		{
			fprintf(stderr, "script\n");
		}
		else
		{
			fprintf(stderr, "%s()\n", function->name->chars);
		}
	}

	CallFrame* frame = &vm.frames[vm.frameCount - 1];

	// コードを読んだあとに ip++ されているので、エラーを起こしたのは現在実行しているコードの一つ前になる
	ObjFunction* function = frame->closure->function;
	size_t instruction = frame->ip - function->chunk.code - 1;
	int line = function->chunk.lines[instruction];
	fprintf(stderr, "[line %d] in script\n", line);

	resetStack();
}

void defineNative(const char* name, NativeFn function)
{
	// 割当てたオブジェクトが即座に GC の対象になったりしないように、スタックに入れておく
	push(Value::toObj(copyString(name, static_cast<int>(strlen(name)))));
	push(Value::toObj(newNative(function)));

	// ネイティブ関数は global に入れる
	// TODO: ここでスタックは空になっている前提で合っている？
	tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);

	pop();
	pop();
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
	CallFrame* frame = &vm.frames[vm.frameCount - 1];

#define READ_BYTE() (*frame->ip++)

// 2 instruction 消費して 16bit 整数として読み取る
#define READ_SHORT() \
	(frame->ip += 2, \
	static_cast<uint16_t>(frame->ip[-2] << 8 | frame->ip[-1]))

#define READ_CONSTANT() \
	(frame->closure->function->chunk.constants.values[READ_BYTE()])

#define READ_STRING() \
	AS_STRING(READ_CONSTANT())

#if DEBUG_TRACE_EXECUTION
	printf("== run() ==\n");
#endif

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
		disassembleInstruction(&frame->closure->function->chunk, static_cast<int>(frame->ip - frame->closure->function->chunk.code));
#endif

		using enum InterpretResult;
		uint8_t instruction = READ_BYTE();
		switch (instruction)
		{

		case OP_CONSTANT: {
			Value constant = READ_CONSTANT();
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

		case OP_GET_LOCAL:
		{
			// ローカル変数のインデックスはスタックのインデックスと一致している
			uint8_t slot = READ_BYTE();
			push(frame->slots[slot]);
			break;
		}

		case OP_SET_LOCAL:
		{
			// ローカル変数のインデックスはスタックのインデックスと一致している
			uint8_t slot = READ_BYTE();
			frame->slots[slot] = peek(0); // 値がそのまま代入文の評価値になるので、pop() しない
			break;
		}

		case OP_GET_GLOBAL:
		{
			ObjString* name = READ_STRING();
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
			ObjString* name = READ_STRING();
			tableSet(&vm.globals, name, peek(0));
			pop();
			break;
		}

		case OP_SET_GLOBAL:
		{
			ObjString* name = READ_STRING();
			if (tableSet(&vm.globals, name, peek(0)))
			{
				// "新しいキーだったら" ランタイムエラーにする
				tableDelete(&vm.globals, name);
				runtimeError("Undefined variable '%s'.", name->chars);
				return RuntimeError;
			}
			break;
		}

		case OP_GET_UPVALUE:
		{
			uint8_t slot = READ_BYTE();
			push(*frame->closure->upvalues[slot]->location);
			break;
		}

		case OP_SET_UPVALUE:
		{
			uint8_t slot = READ_BYTE();
			*frame->closure->upvalues[slot]->location = peek(0);
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

		case OP_JUMP: {
			// 無条件 jump
			uint16_t offset = READ_SHORT();
			frame->ip += offset;
			break;
		}

		case OP_JUMP_IF_FALSE: {
			// false なら jump
			uint16_t offset = READ_SHORT();
			if (isFalsey(peek(0))) frame->ip += offset; // then 節をスキップ
			break;
		}

		case OP_LOOP: {
			uint16_t offset = READ_SHORT();
			frame->ip -= offset; // back jump
			break;
		}

		case OP_CALL: {
			int argCount = READ_BYTE();
			if (!callValue(peek(argCount), argCount))
			{
				return RuntimeError;
			}
			// 呼び出しが成功したので呼び出し元を frame 変数にキャッシュしておく
			// NOTE: Native 関数の場合、frame の指し位置は変わらない
			frame = &vm.frames[vm.frameCount - 1];
			break;
		}

		case OP_CLOSURE: {
			ObjFunction* function = AS_FUNCTION(READ_CONSTANT());
			ObjClosure* closure = newClosure(function);
			push(Value::toObj(closure));

			// 上位値のポインタをオブジェクト配列として保持する
			for (int i = 0; i < closure->upvalueCount; i++)
			{
				uint8_t isLocal = READ_BYTE();
				uint8_t index = READ_BYTE();
				if (isLocal)
				{
					// ローカルスコープの上位値の場合は、キャプチャする
					// ローカル変数なので、フレームのスタック + index 分で Value* を取れる
					closure->upvalues[i] = captureUpvalue(frame->slots + index);
				}
				else
				{
					// ローカルでない場合は外側の関数の上位値なので、そのポインタへの参照をコピーすればいい
					closure->upvalues[i] = frame->closure->upvalues[index];
				}
			}
			break;
		}

		case OP_RETURN: {
			Value result = pop();
			vm.frameCount--;
			if (vm.frameCount == 0)
			{
				// 実行終了
				pop();
				return Ok;
			}

			vm.stackTop = frame->slots; // スタックを復元
			push(result);
			frame = &vm.frames[vm.frameCount - 1]; // 呼び出し元フレームを一つ上に
			// frame が書き換わることで、関数呼び出し位置の ip から実行が再開する
			break;
		}

		default:
			return RuntimeError;

		}
	}

#undef READ_STRING
#undef READ_CONSTANT
#undef READ_SHORT
#undef READ_BYTE

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

	defineNative("clock", clockNative);
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

InterpretResult interpret(const char* source)
{
	ObjFunction* function = compile(source);
	if (function == nullptr) return InterpretResult::CompileError;

	// 確保済みのスタック 0 番に関数オブジェクト自身を格納する
	// ここで push が必要なのは、ガベージコレクション回避のため
	push(Value::toObj(function));

	// 新しいフレームとして関数呼び出し
	ObjClosure* closure = newClosure(function);
	pop(); // 関数オブジェクトを取り出す
	push(Value::toObj(closure));
	call(closure, 0);

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

