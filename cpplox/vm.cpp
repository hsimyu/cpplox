﻿#include "vm.h"

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
	return TO_NUMBER(static_cast<double>(clock()) / CLOCKS_PER_SEC);
}

Value toStringNative(int argCount, Value* args)
{
	return TO_OBJ(toString(args[0]));
}

}

namespace
{

void runtimeError(const char* format, ...);

Value peek(int distance)
{
	return vm.mainThread.stackTop[-1 - distance];
}

bool call(ObjClosure* closure, int argCount)
{
	if (argCount != closure->function->arity)
	{
		runtimeError("Expected %d arguments but got %d.", closure->function->arity, argCount);
		return false;
	}

	if (vm.mainThread.frameCount == FRAMES_MAX)
	{
		runtimeError("Stack overflow.");
		return false;
	}

	CallFrame* frame = &vm.mainThread.frames[vm.mainThread.frameCount++];
	frame->closure = closure;
	frame->ip = closure->function->chunk.code;

	// stack 中に乗っている引数の数を引いた位置が、frame が参照するスタックの開始位置になる
	// argCount + 1 なのは、予約分の 0 番の分
	frame->slots = vm.mainThread.stackTop - (argCount + 1);

	return true;
}

bool callValue(Value callee, int argCount)
{
	if (IS_OBJ(callee))
	{
		switch (OBJ_TYPE(callee))
		{
		case ObjType::BoundMethod:
		{
			ObjBoundMethod* bound = AS_BOUND_METHOD(callee);
			// メソッド呼び出しのスロット 0 (予約) にインスタンスを差し込む
			vm.mainThread.stackTop[-argCount - 1] = bound->receiver;
			return call(bound->method, argCount);
		}
		case ObjType::Class:
		{
			ObjClass* klass = AS_CLASS(callee);
			vm.mainThread.stackTop[-argCount - 1] = TO_OBJ(newInstance(klass));

			Value initializer;
			if (tableGet(&klass->methods, vm.initString, &initializer))
			{
				// "init" 関数があればそれを初期化子として呼び出す
				return call(AS_CLOSURE(initializer), argCount);
			}
			else if (argCount != 0)
			{
				// "init" が定義されていないのに引数が渡されていた場合はエラーにする
				runtimeError("Expected 0 arguments but got %d.", argCount);
				return false;
			}
			return true;
		}
		// All ObjType::Function are wrapped as closure
		case ObjType::Closure:
			return call(AS_CLOSURE(callee), argCount);
		case ObjType::Native:
		{
			// Native 関数呼び出しの場合は、ここで即座に呼び出す
			NativeFn native = AS_NATIVE(callee)->function;
			Value result = native(argCount, vm.mainThread.stackTop - argCount);
			vm.mainThread.stackTop -= argCount + 1;
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

bool invokeFromClass(ObjClass* klass, ObjString* name, int argCount)
{
	Value method;
	if (!tableGet(&klass->methods, name, &method))
	{
		runtimeError("Undefine property '%s'.", name->chars);
		return false;
	}
	return call(AS_CLOSURE(method), argCount);
}

bool invoke(ObjString* name, int argCount)
{
	Value receiver = peek(argCount); // インスタンスが入っている位置を狙う
	if (!IS_INSTANCE(receiver))
	{
		runtimeError("Only instances have methods.");
		return false;
	}

	ObjInstance* instance = AS_INSTANCE(receiver);

	Value value;
	if (tableGet(&instance->fields, name, &value))
	{
		// プロパティがフィールドだった場合はそれを普通の関数として呼び出す
		vm.mainThread.stackTop[-argCount - 1] = value;
		return callValue(value, argCount);
	}

	return invokeFromClass(instance->klass, name, argCount);
}

bool bindMethod(ObjClass* klass, ObjString* name)
{
	Value method;
	if (!tableGet(&klass->methods, name, &method))
	{
		return false;
	}

	// スタックトップにバインド対象のインスタンスがいるはず
	ObjBoundMethod* bound = newBoundMethod(peek(0), AS_CLOSURE(method));
	pop(); // instance
	push(TO_OBJ(bound));
	return true;
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

void closeUpvalues(Value* last)
{
	// オープン上位値かつ現在のスコープ内のローカル変数であるものを辿る
	while (vm.openUpvalues != nullptr && vm.openUpvalues->location >= last)
	{
		ObjUpvalue* upvalue = vm.openUpvalues;
		upvalue->closed = *upvalue->location; // Value をコピー
		upvalue->location = &upvalue->closed; // location がコピーした値を指すように変更
		vm.openUpvalues = upvalue->next;
	}
}

void defineMethod(ObjString* methodName)
{
	// 型チェックはコンパイル時に行われているので不要
	Value method = peek(0); // ObjClosure* のはず
	ObjClass* klass = AS_CLASS(peek(1));
	tableSet(&klass->methods, methodName, method);
	pop(); // method を pop
}

void resetStack(Thread* thread)
{
	thread->stackTop = thread->stack;
	thread->frameCount = 0;

	// TODO: これも Thread に持たせるべき?
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
	for (int i = vm.mainThread.frameCount - 1; i >= 0; i--)
	{
		CallFrame* frame = &vm.mainThread.frames[i];
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

	CallFrame* frame = &vm.mainThread.frames[vm.mainThread.frameCount - 1];

	// コードを読んだあとに ip++ されているので、エラーを起こしたのは現在実行しているコードの一つ前になる
	ObjFunction* function = frame->closure->function;
	size_t instruction = frame->ip - function->chunk.code - 1;
	int line = function->chunk.lines[instruction];
	fprintf(stderr, "[line %d] in script\n", line);

	resetStack(&vm.mainThread);
}

void defineNative(const char* name, NativeFn function)
{
	// 割当てたオブジェクトが即座に GC の対象になったりしないように、スタックに入れておく
	push(TO_OBJ(copyString(name, static_cast<int>(strlen(name)))));
	push(TO_OBJ(newNative(function)));

	// ネイティブ関数は global に入れる
	// TODO: ここでスタックは空になっている前提で合っている？
	tableSet(&vm.globals, AS_STRING(vm.mainThread.stack[0]), vm.mainThread.stack[1]);

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
		push(TO_##ValueType(a op b)); \
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
	// ここでスタックから文字列オブジェクトを取り出してしまうと、次の allocate 時に回収される可能性がある
	// 処理が完了するまでは peek() で参照する
	ObjString* b = AS_STRING(peek(0));
	ObjString* a = AS_STRING(peek(1));

	int length = a->length + b->length;
	char* chars = allocate<char>(length + 1);
	memcpy(chars, a->chars, a->length);
	memcpy(chars + a->length, b->chars, b->length);
	chars[length] = '\0';

	ObjString* result = takeString(chars, length);
	pop(); // b
	pop(); // a
	push(toObjValue(result)); // result
}

InterpretResult run()
{
	CallFrame* frame = &vm.mainThread.frames[vm.mainThread.frameCount - 1];

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
		for (Value* slot = vm.mainThread.stack; slot < vm.mainThread.stackTop; slot++)
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
			push(TO_NIL());
			break;

		case OP_TRUE:
			push(TO_BOOL(true));
			break;

		case OP_FALSE:
			push(TO_BOOL(false));
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

		case OP_GET_PROPERTY:
		{
			// アクセス対象の instance がスタックに積まれているはず
			if (!IS_INSTANCE(peek(0)))
			{
				runtimeError("Only instances have properties.");
				return RuntimeError;
			}

			ObjInstance* instance = AS_INSTANCE(peek(0));
			ObjString* name = READ_STRING();

			Value value;
			if (tableGet(&instance->fields, name, &value))
			{
				pop(); // instance
				push(value);
				break;
			}

			if (!bindMethod(instance->klass, name))
			{
				runtimeError("Undefine property '%s'.", name->chars);
				return RuntimeError;
			}

			break;
		}

		case OP_SET_PROPERTY:
		{
			// スタックトップには代入する Value
			// スタックの 2 番目に代入先の Instance
			if (!IS_INSTANCE(peek(1)))
			{
				runtimeError("Only instances have fields.");
				return RuntimeError;
			}

			ObjInstance* instance = AS_INSTANCE(peek(1));
			tableSet(&instance->fields, READ_STRING(), peek(0));

			Value value = pop();
			pop(); // instance
			push(value); // 評価値
			break;
		}

		case OP_GET_SUPER:
		{
			ObjString* name = READ_STRING();
			ObjClass* superclass = AS_CLASS(pop());
			if (!bindMethod(superclass, name))
			{
				runtimeError("Undefine property '%s'.", name->chars);
				return RuntimeError;
			}
			break;
		}

		case OP_EQUAL:
		{
			Value b = pop();
			Value a = pop();
			push(TO_BOOL(valuesEqual(a, b)));
			break;
		}

		case OP_GREATER: BINARY_OP(BOOL, >); break;
		case OP_LESS: BINARY_OP(BOOL, <); break;
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
				push(TO_NUMBER(a + b)); \
			}
			else
			{
				runtimeError("Operand must be two numbers or two strings.");
				return InterpretResult::RuntimeError;
			}
			break;
		}
		case OP_SUBTRACT: BINARY_OP(NUMBER, -); break;
		case OP_MULTIPLY: BINARY_OP(NUMBER, *); break;
		case OP_DIVIDE: BINARY_OP(NUMBER, /); break;

		case OP_NOT:
			push(TO_BOOL(isFalsey(pop())));
			break;

		case OP_NEGATE: {
			if (!IS_NUMBER(peek(0)))
			{
				runtimeError("Operand must be a number.");
				return RuntimeError;
			}
			push(TO_NUMBER(-AS_NUMBER(pop())));
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
			frame = &vm.mainThread.frames[vm.mainThread.frameCount - 1];
			break;
		}

		case OP_INVOKE:
		{
			ObjString* method = READ_STRING();
			int argCount = READ_BYTE();
			if (!invoke(method, argCount))
			{
				return RuntimeError;
			}
			frame = &vm.mainThread.frames[vm.mainThread.frameCount - 1];
			break;
		}

		case OP_SUPER_INVOKE:
		{
			ObjString* method = READ_STRING();
			int argCount = READ_BYTE();
			ObjClass* superclass = AS_CLASS(pop());
			// super クラスのメソッド呼び出し時は、フィールドを探索しなくていいのでメソッドとして直接呼び出ししてよい
			if (!invokeFromClass(superclass, method, argCount))
			{
				return RuntimeError;
			}
			frame = &vm.mainThread.frames[vm.mainThread.frameCount - 1];
			break;
		}

		case OP_CLOSURE: {
			ObjFunction* function = AS_FUNCTION(READ_CONSTANT());
			ObjClosure* closure = newClosure(function);
			push(TO_OBJ(closure));

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

		case OP_CLOSE_UPVALUE:
		{
			closeUpvalues(vm.mainThread.stackTop - 1);
			pop();
			break;
		}

		case OP_RETURN: {
			Value result = pop();
			closeUpvalues(frame->slots);
			vm.mainThread.frameCount--;
			if (vm.mainThread.frameCount == 0)
			{
				// 実行終了
				pop();
				return Ok;
			}

			vm.mainThread.stackTop = frame->slots; // スタックを復元
			push(result);
			frame = &vm.mainThread.frames[vm.mainThread.frameCount - 1]; // 呼び出し元フレームを一つ上に
			// frame が書き換わることで、関数呼び出し位置の ip から実行が再開する
			break;
		}

		case OP_CLASS:
		{
			push(TO_OBJ(newClass(READ_STRING())));
			break;
		}

		case OP_INHERIT:
		{
			Value superClass = peek(1);
			if (!IS_CLASS(superClass))
			{
				runtimeError("Superclass must be a class.");
				return RuntimeError;
			}

			ObjClass* subClass = AS_CLASS(peek(0));
			// 親クラスのメソッドを全て子クラスに突っ込む
			tableAddAll(&AS_CLASS(superClass)->methods, &subClass->methods);
			pop();
			break;
		}

		case OP_METHOD:
		{
			defineMethod(READ_STRING());
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

void initThread(Thread* thread)
{
	thread->frameCount = 0;
	thread->stackTop = nullptr;
}

void initVM()
{
	initThread(&vm.mainThread);

	vm.objects = nullptr;
	vm.bytesAllocated = 0;
	vm.nextGC = 1024 * 1024; // 1MiB

	vm.grayCount = 0;
	vm.grayCapacity = 0;
	vm.grayStack = nullptr;

	initTable(&vm.globals);
	initTable(&vm.strings);

	// 初期化子関数名は "init" で固定
	vm.initString = copyString("init", 4);

	defineNative("clock", clockNative);
	defineNative("tostring", toStringNative);
}

void freeVM()
{
	freeTable(&vm.globals);
	freeTable(&vm.strings);
	vm.initString = nullptr;

	freeObjects();

	free(vm.grayStack);
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
	push(TO_OBJ(function));

	// 新しいフレームとして関数呼び出し
	ObjClosure* closure = newClosure(function);
	pop(); // 関数オブジェクトを取り出す
	push(TO_OBJ(closure));
	call(closure, 0);

	return run();
}

void push(Value value)
{
	*vm.mainThread.stackTop = value;
	vm.mainThread.stackTop++;
}

Value pop()
{
	vm.mainThread.stackTop--;
	return *vm.mainThread.stackTop;
}

