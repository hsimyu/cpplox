#include "compiler.h"

#include "scanner.h"
#include "chunk.h"
#include "common.h"
#include "object.h"
#include "memory.h"

#if DEBUG_PRINT_CODE
#include "debug.h"
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace
{

struct Parser
{
	Token current;
	Token previous;
	bool hadError = false;
	bool panicMode = false;

	// 関数共通パラメータを引数を介さないで伝えるための adhoc な定義
	// もっと定義が増えたら、ParseFn を variant 化する方がよい
	bool canAssign = false;
};

// 優先順位 (値が小さいほど優先順位が低い)
enum Precedence
{
	PREC_NONE = 0,
	PREC_ASSIGNMENT, // =
	PREC_OR, // or
	PREC_AND, // and
	PREC_EQUALITY, // == !=
	PREC_COMPARISON, //  < > <= >=
	PREC_TERM, // + -
	PREC_FACTOR, // * /
	PREC_UNARY, // ! -
	PREC_CALL, // . ()
	PREC_PRIMARY,
};

using ParseFn = void (*)();

struct ParseRule
{
	ParseFn prefix;
	ParseFn infix;
	Precedence precedence;
};

struct Local
{
	Token name;
	int depth = 0;
	bool isCaptured = false;
};

struct Upvalue
{
	uint8_t index = 0;
	bool isLocal = false;
};

enum class FunctionType
{
	Function,
	Script,
};

struct Compiler
{
	Compiler* enclosing = nullptr;
	ObjFunction* function = nullptr;
	FunctionType type = FunctionType::Script;

	Local locals[LOCAL_VARIABLE_COUNT];
	int localCount = 0;

	Upvalue upvalues[UPVALUE_COUNT];

	int scopeDepth = 0;
};

Parser parser;
Compiler* current = nullptr;

Chunk* currentChunk()
{
	return &current->function->chunk;
}

void errorAt(Token* token, const char* message)
{
	if (parser.panicMode) return;

	parser.panicMode = true;
	fprintf(stderr, "[line %d] Error", token->line);

	if (token->type == TOKEN_EOF)
	{
		fprintf(stderr, " at end");
	}
	else if (token->type == TOKEN_ERROR)
	{
		// scanner が報告済みなので何もしない
	}
	else
	{
		fprintf(stderr, " at '%.*s'", token->length, token->start);
	}

	fprintf(stderr, ": %s\n", message);
	parser.hadError = true;
}

void error(const char* message)
{
	errorAt(&parser.previous, message);
}

void errorAtCurrent(const char* message)
{
	errorAt(&parser.current, message);
}

void advance()
{
	parser.previous = parser.current;
	for (;;)
	{
		parser.current = scanToken();
		if (parser.current.type != TOKEN_ERROR) break;
		errorAtCurrent(parser.current.start);
	}
}

void consume(TokenType type, const char* message)
{
	if (parser.current.type == type)
	{
		advance();
		return;
	}

	errorAtCurrent(message);
}

bool check(TokenType type)
{
	return parser.current.type == type;
}

bool match(TokenType type)
{
	if (!check(type)) return false;
	advance();
	return true;
}

void emitByte(uint8_t byte)
{
	currentChunk()->Write(byte, parser.previous.line);
}

void emitBytes(uint8_t byte1, uint8_t byte2)
{
	emitByte(byte1);
	emitByte(byte2);
}

void emitLoop(int loopStart)
{
	emitByte(OP_LOOP);

	// OP_LOOP は 3 要素の命令なので currentChunk + 2 から、loopStart を引いただけをオフセットとして格納する
	// 実行時にこのオフセットの分だけ後ろに戻る
	int offset = currentChunk()->count - loopStart + 2;
	if (offset > UINT16_MAX)
	{
		error("Loop body too large.");
	}

	emitByte((offset >> 8) & 0xFF);
	emitByte(offset & 0xFF);
}

int emitJump(uint8_t instruction)
{
	emitByte(instruction);
	// placeholder for back pacthing
	emitByte(0xFF);
	emitByte(0xFF);
	// return chunk offset for patching
	return currentChunk()->count - 2;
}

void patchJump(int offset)
{
	// jump すべき分を決定する
	// currentChunk の数にはジャンプ命令のダミーオペランド分も含まれているので、
	// -2 して差し引いておく
	int jump = currentChunk()->count - offset - 2;

	if (jump > UINT16_MAX)
	{
		// cannot jump larger than 65535 instruction
		error("Too match code to jump over");
	}

	currentChunk()->code[offset] = static_cast<uint8_t>((jump >> 8) & 0xFF);
	currentChunk()->code[offset + 1] = static_cast<uint8_t>(jump & 0xFF);
}

uint8_t makeConstant(Value value)
{
	int constant = currentChunk()->AddConstant(value);
	if (constant > UINT8_MAX)
	{
		error("Too many constants in one chunk.");
		return 0;
	}

	return static_cast<uint8_t>(constant);
}

void emitConstant(Value value)
{
	emitBytes(OP_CONSTANT, makeConstant(value));
}

void emitReturn()
{
	emitByte(OP_NIL); // 引数なしの return の場合は nil を返す
	emitByte(OP_RETURN);
}

void initCompiler(Compiler* compiler, FunctionType type)
{
	compiler->enclosing = current;
	compiler->function = nullptr;
	compiler->type = type;
	compiler->localCount = 0;
	compiler->scopeDepth = 0;

	// コンパイル対象となる関数オブジェクトをコンパイル時に生成する
	compiler->function = newFunction();
	current = compiler;

	if (type != FunctionType::Script)
	{
		// 関数名解析直後なので、一つ前のトークンから関数名を取得できる
		current->function->name = copyString(parser.previous.start, parser.previous.length);
	}

	// 0 番目のローカル変数を VM 用に予約
	Local* local = &current->locals[current->localCount++];
	local->depth = 0;
	local->isCaptured = false;
	local->name.start = "";
	local->name.length = 0;
}

ObjFunction* endCompiler()
{
	emitReturn();
	ObjFunction* f = current->function;

#if DEBUG_PRINT_CODE
	if (!parser.hadError)
	{
		disassembleChunk(currentChunk(), f->name != nullptr ? f->name->chars : "<script>");
	}
#endif

	current = current->enclosing;
	return f;
}

void beginScope()
{
	current->scopeDepth++;
}

void endScope()
{
	current->scopeDepth--;

	// pop local variables on stack
	while (current->localCount > 0 && current->locals[current->localCount - 1].depth > current->scopeDepth)
	{
		if (current->locals[current->localCount - 1].isCaptured)
		{
			// 上位値としてキャプチャ済みのローカル変数なら、POP ではなくクローズする
			emitByte(OP_CLOSE_UPVALUE);
		}
		else
		{
			emitByte(OP_POP);
		}
		current->localCount--;
	}
}

void and_();
void or_();
void number();
void str();
void variable();
void unary();
void binary();
void call();
void literal();
void grouping();
void expression();
void declaration();
void statement();

ParseRule rules[] = {
	// [前置パーサー、中置パーサー、中置パーサーの優先順位] の表
	/* TOKEN_LEFT_PAREN    */ {grouping, call, PREC_CALL},
	/* TOKEN_RIGHT_PAREN   */ {nullptr, nullptr, PREC_NONE},
	/* TOKEN_LEFT_BRACE    */ {nullptr, nullptr, PREC_NONE},
	/* TOKEN_RIGHT_BRACE   */ {nullptr, nullptr, PREC_NONE},
	/* TOKEN_COMMA         */ {nullptr, nullptr, PREC_NONE},
	/* TOKEN_DOT           */ {nullptr, nullptr, PREC_NONE},
	/* TOKEN_MINUS         */ {unary, binary, PREC_TERM},
	/* TOKEN_PLUS          */ {nullptr, binary, PREC_TERM},
	/* TOKEN_SEMICOLON     */ {nullptr, nullptr, PREC_NONE},
	/* TOKEN_SLASH         */ {nullptr, binary, PREC_FACTOR},
	/* TOKEN_STAR          */ {nullptr, binary, PREC_FACTOR},
	/* TOKEN_BANG          */ {unary, nullptr, PREC_NONE},
	/* TOKEN_BANG_EQUAL    */ {nullptr, binary, PREC_EQUALITY},
	/* TOKEN_EQUAL         */ {nullptr, nullptr, PREC_NONE},
	/* TOKEN_EQUAL_EQUAL   */ {nullptr, binary, PREC_EQUALITY},
	/* TOKEN_GREATER       */ {nullptr, binary, PREC_COMPARISON},
	/* TOKEN_GREATER_EQUAL */ {nullptr, binary, PREC_COMPARISON},
	/* TOKEN_LESS          */ {nullptr, binary, PREC_COMPARISON},
	/* TOKEN_LESS_EQUAL    */ {nullptr, binary, PREC_COMPARISON},
	/* TOKEN_IDENTIFIER    */ {variable, nullptr, PREC_NONE},
	/* TOKEN_STRING        */ {str, nullptr, PREC_NONE},
	/* TOKEN_NUMBER        */ {number, nullptr, PREC_NONE},
	/* TOKEN_AND           */ {nullptr, and_, PREC_AND},
	/* TOKEN_CLASS         */ {nullptr, nullptr, PREC_NONE},
	/* TOKEN_ELSE          */ {nullptr, nullptr, PREC_NONE},
	/* TOKEN_FALSE         */ {literal, nullptr, PREC_NONE},
	/* TOKEN_FOR           */ {nullptr, nullptr, PREC_NONE},
	/* TOKEN_FUN           */ {nullptr, nullptr, PREC_NONE},
	/* TOKEN_IF            */ {nullptr, nullptr, PREC_NONE},
	/* TOKEN_NIL           */ {literal, nullptr, PREC_NONE},
	/* TOKEN_OR            */ {nullptr, or_, PREC_OR},
	/* TOKEN_PRINT         */ {nullptr, nullptr, PREC_NONE},
	/* TOKEN_RETURN        */ {nullptr, nullptr, PREC_NONE},
	/* TOKEN_SUPER         */ {nullptr, nullptr, PREC_NONE},
	/* TOKEN_THIS          */ {nullptr, nullptr, PREC_NONE},
	/* TOKEN_TRUE          */ {literal, nullptr, PREC_NONE},
	/* TOKEN_VAR           */ {nullptr, nullptr, PREC_NONE},
	/* TOKEN_WHILE         */ {nullptr, nullptr, PREC_NONE},
	/* TOKEN_ERROR         */ {nullptr, nullptr, PREC_NONE},
	/* TOKEN_EOF           */ {nullptr, nullptr, PREC_NONE},
};

ParseRule* getRule(TokenType type)
{
	return &rules[type];
}

void parsePrecedence(Precedence precedence)
{
	// 指定された優先順位と同じか順位が低いものに遭遇するまで式をパースする
	advance();
	ParseFn prefixRule = getRule(parser.previous.type)->prefix;

	if (prefixRule == nullptr)
	{
		error("Expect expression.");
		return;
	}

	// 前置解析を行う
	bool canAssign = precedence <= PREC_ASSIGNMENT; // 優先順位が ASSIGNMENT より低い場合は代入不可

	parser.canAssign = canAssign;
	prefixRule();

	// 現在指しているトークンの優先度が指定した優先度と同じか低い間はループし続ける
	while (precedence <= getRule(parser.current.type)->precedence)
	{
		// prefixRule() の解析結果は中置演算子のオペランドだったとしてパースを進める
		advance();
		ParseFn infixRule = getRule(parser.previous.type)->infix;

		if (infixRule == nullptr)
		{
			error("Expect expression.");
			return;
		}

		parser.canAssign = canAssign;
		infixRule();
	}

	// "=" が式の一部として消費されなかった場合
	if (canAssign && match(TOKEN_EQUAL))
	{
		error("Invalid assignment target.");
	}
}

uint8_t identifierConstant(Token* name)
{
	// 変数名を表す文字列を、オブジェクトとして定数表に格納する
	return makeConstant(Value::toObj(copyString(name->start, name->length)));
}

bool identifierEqual(Token* a, Token* b)
{
	if (a->length != b->length) return false;
	return memcmp(a->start, b->start, a->length) == 0;
}

int resolveLocal(Compiler* compiler, Token* name)
{
	for (int i = compiler->localCount - 1; i >= 0; i--)
	{
		Local* local = &compiler->locals[i];
		if (identifierEqual(name, &local->name))
		{
			// found
			if (local->depth == -1)
			{
				error("Can't read local variable in its own initializer.");
			}
			return i;
		}
	}

	// not found
	return -1;
}

int addUpvalue(Compiler* compiler, uint8_t index, bool isLocal)
{
	int upvalueCount = compiler->function->upvalueCount;

	// すでに格納済みの上位値ならそのインデックスを返す
	for (int i = 0; i < upvalueCount; i++)
	{
		Upvalue* upvalue = &compiler->upvalues[i];
		if (upvalue->index == index && upvalue->isLocal == isLocal)
		{
			return i;
		}
	}

	if (upvalueCount == UPVALUE_COUNT)
	{
		error("Too many closure variables in function.");
		return 0;
	}

	compiler->upvalues[upvalueCount].isLocal = isLocal;
	compiler->upvalues[upvalueCount].index = index;
	return compiler->function->upvalueCount++;
}

int resolveUpvalue(Compiler* compiler, Token* name)
{
	if (compiler->enclosing == nullptr) return -1;

	// すぐ外側のローカル変数として解決可能であれば上位値である
	// そのスコープにおけるインデックスを保存する
	int local = resolveLocal(compiler->enclosing, name);
	if (local != -1)
	{
		// キャプチャされたことをマークしておき、スタックから抜けるときに解放されないようにする
		compiler->enclosing->locals[local].isCaptured = true;
		return addUpvalue(compiler, static_cast<uint8_t>(local), true);
	}

	// 外側の関数の上位値として解決できるかを再帰的に試みる
	// 関数宣言のコンパイル時、その呼出し元は必ず生きているという構造を利用している
	int upvalue = resolveUpvalue(compiler->enclosing, name);
	if (upvalue != -1)
	{
		// 見つけられた場合は自身の上位値として追加
		// ただし、直上のローカル変数ではない
		return addUpvalue(compiler, static_cast<uint8_t>(local), false);
	}

	return -1;
}

void addLocal(Token name)
{
	if (current->localCount == LOCAL_VARIABLE_COUNT)
	{
		error("Too many local variables in function.");
		return;
	}

	Local* local = &current->locals[current->localCount++];
	local->name = name;
	local->depth = -1;
	local->isCaptured = false;
}

void declareVariable()
{
	if (current->scopeDepth == 0) return;

	Token* name = &parser.previous;

	// search local variables in the same scope with the same name
	for (int i = current->localCount - 1; i >= 0; i--)
	{
		Local* local = &current->locals[i];
		if (local->depth != -1 && local->depth < current->scopeDepth)
		{
			// no variables in the same scope
			break;
		}

		if (identifierEqual(name, &local->name))
		{
			error("Already a variable with this name in this scope.");
		}
	}
	addLocal(*name);
}

uint8_t parseVariable(const char* errorMessage)
{
	consume(TOKEN_IDENTIFIER, errorMessage);

	declareVariable();
	if (current->scopeDepth > 0) return 0;

	return identifierConstant(&parser.previous);
}

void markInitialized()
{
	if (current->scopeDepth = 0) return;
	current->locals[current->localCount - 1].depth = current->scopeDepth;
}

void defineVariable(uint8_t global)
{
	if (current->scopeDepth > 0)
	{
		markInitialized();
		return;
	}
	emitBytes(OP_DEFINE_GLOBAL, global);
}

uint8_t argumentList()
{
	uint8_t argCount = 0;
	if (!check(TOKEN_RIGHT_PAREN))
	{
		do {
			expression();
			if (argCount == 255)
			{
				error("Can't have more than 255 parameters.");
			}
			argCount++;
		} while (match(TOKEN_COMMA));
	}
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
	return argCount;
}

void and_()
{
	// and 命令は "Falsey なら and 命令より優先順位が低い命令の実行完了までジャンプ" と解釈される
	int endJump = emitJump(OP_JUMP_IF_FALSE);
	// POP は Jump 対象に含まれるので、Jump した場合は値を捨てず、それが式の評価値になる
	emitByte(OP_POP);
	parsePrecedence(PREC_AND);
	patchJump(endJump);
}

void or_()
{
	// or 命令は "Falsey なら OP_JUMP をスキップし、次の式を実行する。Truthy なら OP_JUMP が実行され、式全体をスキップする" と解釈される
	// NOTE: 性能を追い求めるなら新規命令を実装して、発行する命令数を減らした方がいい
	int elseJump = emitJump(OP_JUMP_IF_FALSE);
	int endJump = emitJump(OP_JUMP);
	patchJump(elseJump);
	emitByte(OP_POP);

	parsePrecedence(PREC_OR);
	patchJump(endJump);
}

void number()
{
	// TODO: std::from_chars のがよい?
	double value = strtod(parser.previous.start, nullptr);
	emitConstant(Value::toNumber(value));
}

void str()
{
	// start + 1 で最初の二重引用符を除外
	// (length-1) - 1 で最後の二重引用符を除外
	emitConstant(toObjValue(copyString(parser.previous.start + 1, parser.previous.length - 2)));
}

void namedVariable(Token name)
{
	bool canAssign = parser.canAssign;

	uint8_t getOp, setOp;
	int arg = resolveLocal(current, &name);
	if (arg != -1)
	{
		getOp = OP_GET_LOCAL;
		setOp = OP_SET_LOCAL;
	}
	else if ((arg = resolveUpvalue(current, &name)) != -1)
	{
		getOp = OP_GET_UPVALUE;
		setOp = OP_SET_UPVALUE;
	}
	else
	{
		arg = identifierConstant(&name);
		getOp = OP_GET_GLOBAL;
		setOp = OP_SET_GLOBAL;
	}

	if (canAssign && match(TOKEN_EQUAL))
	{
		// identifier の後に = があったら、後段にあるものを右辺値としてセット命令で包む
		expression();
		emitBytes(setOp, static_cast<uint8_t>(arg));
	}
	else
	{
		emitBytes(getOp, static_cast<uint8_t>(arg));
	}
}

void variable()
{
	namedVariable(parser.previous);
}

void unary()
{
	auto opType = parser.previous.type;

	// オペランドをコンパイル
	parsePrecedence(PREC_UNARY);

	switch (opType)
	{
	case TOKEN_BANG:
		emitByte(OP_NOT);
		break;
	case TOKEN_MINUS:
		emitByte(OP_NEGATE);
		break;
	default:
		return; // Unreachable
	}
}

void binary()
{
	auto opType = parser.previous.type;
	ParseRule* rule = getRule(opType);

	// 現在の優先順位より一つだけ高いものまでパースする
	// つまり、自分と同じ優先度のものが来たらそれは拾わない => 左結合
	parsePrecedence(static_cast<Precedence>(rule->precedence + 1));

	switch (opType)
	{
	case TOKEN_BANG_EQUAL:
		emitBytes(OP_EQUAL, OP_NOT);
		break;
	case TOKEN_EQUAL_EQUAL:
		emitByte(OP_EQUAL);
		break;
	case TOKEN_GREATER:
		emitByte(OP_GREATER);
		break;
	case TOKEN_GREATER_EQUAL:
		emitBytes(OP_LESS, OP_NOT);
		break;
	case TOKEN_LESS:
		emitByte(OP_LESS);
		break;
	case TOKEN_LESS_EQUAL:
		emitBytes(OP_GREATER, OP_NOT);
		break;
	case TOKEN_PLUS:
		emitByte(OP_ADD);
		break;
	case TOKEN_MINUS:
		emitByte(OP_SUBTRACT);
		break;
	case TOKEN_STAR:
		emitByte(OP_MULTIPLY);
		break;
	case TOKEN_SLASH:
		emitByte(OP_DIVIDE);
		break;
	default:
		return; // Unreachable
	}
}

void call()
{
	uint8_t argCount = argumentList();
	emitBytes(OP_CALL, argCount);
}

void literal()
{
	switch (parser.previous.type)
	{
	case TOKEN_FALSE:
		emitByte(OP_FALSE);
		break;
	case TOKEN_NIL:
		emitByte(OP_NIL);
		break;
	case TOKEN_TRUE:
		emitByte(OP_TRUE);
		break;
	default:
		return; // Unreachable
	}
}

void expression()
{
	parsePrecedence(PREC_ASSIGNMENT);
}

void block()
{
	while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF))
	{
		declaration();
	}
	consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

void function(FunctionType type)
{
	Compiler compiler;
	initCompiler(&compiler, type);
	beginScope();

	consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");

	if (!check(TOKEN_RIGHT_PAREN))
	{
		// , とマッチし続ける限り仮引数をパースし続ける
		do {
			current->function->arity++;
			if (current->function->arity > 255)
			{
				errorAtCurrent("Can't have more than 255 parameters.");
			}
			uint8_t constant = parseVariable("Expect parameter name.");
			defineVariable(constant);
		} while (match(TOKEN_COMMA));
	}
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");

	consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");

	block();

	// NOTE: 関数が終わるとコンパイラが終了するので endScope() は不要
	ObjFunction* f = endCompiler();

	// クロージャと上位値のリストを吐き出す
	// OP_CLOSURE のサイズは可変になる
	emitBytes(OP_CLOSURE, makeConstant(Value::toObj(f)));
	for (int i = 0; i < f->upvalueCount; i++)
	{
		emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
		emitByte(compiler.upvalues[i].index);
	}
}

void classDeclaration()
{
	// classDecl := "class" identifier { ... }
	consume(TOKEN_IDENTIFIER, "Expect class name.");

	uint8_t nameConstant = identifierConstant(&parser.previous);
	declareVariable();

	emitBytes(OP_CLASS, nameConstant);
	defineVariable(nameConstant);

	consume(TOKEN_LEFT_BRACE, "Expect '{' before class body.");
	consume(TOKEN_RIGHT_BRACE, "Expect '}' before class body.");
}

void funDeclaration()
{
	// funDecl := "fun" "(" ")" block

	// 関数名も変数としてパースする
	// 未初期化変数としてマークしておくことで、本文内で参照可能にする
	uint8_t global = parseVariable("Expect function name.");
	markInitialized();
	function(FunctionType::Function);
	defineVariable(global);
}

void varDeclaration()
{
	uint8_t global = parseVariable("Expect variable name.");

	if (match(TOKEN_EQUAL))
	{
		expression();
	}
	else
	{
		// var foo; は var foo = nil; と等価とする
		emitByte(OP_NIL);
	}

	consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");
	defineVariable(global);
}

void expressionStatement()
{
	// expressionStmt := expression ";" ;
	expression();
	consume(TOKEN_SEMICOLON, "Expect ';' after value.");
	emitByte(OP_POP);
}

void forStatement()
{
	beginScope();

	// forStmt := "for" "(" (";" | varDecl | expressionStmt) (";" | expression) (expression)* ")" statement ;
	consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");

	if (match(TOKEN_SEMICOLON))
	{
		// 何もしない
	}
	else if (match(TOKEN_VAR))
	{
		// NOTE: ここで単に declaration としてしまうと全ての statement を許してしまう
		varDeclaration();
	}
	else
	{
		expressionStatement();
	}

	int loopStart = currentChunk()->count;
	int exitJump = -1;
	if (!match(TOKEN_SEMICOLON))
	{
		expression();
		consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");

		// 条件が偽のとき、評価値をスタックに残したままループを抜ける
		exitJump = emitJump(OP_JUMP_IF_FALSE);
		emitByte(OP_POP);
	}

	if (!match(TOKEN_RIGHT_PAREN))
	{
		// インクリメント節がある場合、まずインクリメント節を飛び越えて本文を実行する (OP_JUMP)
		// そして、本文実行後のジャンプ先をインクリメント節の実行開始に置き換えておく
		// 本文実行後にインクリメント節を実行し、その後条件節の実行前までジャンプする
		int bodyJump = emitJump(OP_JUMP);
		int incrementStart = currentChunk()->count;
		expression();
		emitByte(OP_POP);
		consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

		emitLoop(loopStart);
		loopStart = incrementStart; // ジャンプ先差し替え
		patchJump(bodyJump);
	}

	statement();
	emitLoop(loopStart);

	if (exitJump != -1)
	{
		// exit の場合はループ命令を通り越した位置まで Jump
		patchJump(exitJump);
		// 条件の評価値を POP
		emitByte(OP_POP);
	}

	endScope();
}

void ifStatement()
{
	// ifStmt := "if" "(" expression ")" statement ("else" statement)*
	consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
	expression();
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

	int thenJmp = emitJump(OP_JUMP_IF_FALSE);
	emitByte(OP_POP); // clean stack before then statement
	statement();

	int elseJump = emitJump(OP_JUMP);

	patchJump(thenJmp);
	emitByte(OP_POP); // clean stack before else statement

	if (match(TOKEN_ELSE))
	{
		statement();
	}

	patchJump(elseJump);
}

void printStatement()
{
	// printStmt := "print" expression ";" ;
	expression();
	consume(TOKEN_SEMICOLON, "Expect ';' after value.");
	emitByte(OP_PRINT);
}

void returnStatement()
{
	if (current->type == FunctionType::Script)
	{
		error("Can't return from top-level code.");
	}
	// returnStmt := "return" (expression)* ";" ;
	if (match(TOKEN_SEMICOLON))
	{
		emitReturn();
	}
	else
	{
		expression();
		consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
		emitByte(OP_RETURN);
	}
}

void whileStatement()
{
	int loopStart = currentChunk()->count;

	// whileStmt := "while" "(" expression ")" statement;
	consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
	expression();
	consume(TOKEN_RIGHT_PAREN, "Expect '(' after condition.");

	// condition が Falsey なら statement 実行完了箇所までジャンプ
	int exitJump = emitJump(OP_JUMP_IF_FALSE);
	emitByte(OP_POP);
	statement();
	emitLoop(loopStart);

	patchJump(exitJump);
	emitByte(OP_POP);
}

void synchronize()
{
	parser.panicMode = false;

	while (parser.current.type != TOKEN_EOF)
	{
		if (parser.previous.type == TOKEN_SEMICOLON) return; // ; を見つけたので同期完了

		switch (parser.current.type)
		{
		case TOKEN_CLASS:
		case TOKEN_FUN:
		case TOKEN_VAR:
		case TOKEN_FOR:
		case TOKEN_IF:
		case TOKEN_WHILE:
		case TOKEN_PRINT:
		case TOKEN_RETURN:
			return; // 同期できるので抜ける
		default:
			break;
		}

		advance();
	}
}

void declaration()
{
	// declaration := classDecl | funDecl | varDecl | statement
	if (match(TOKEN_CLASS))
	{
		classDeclaration();
	}
	else if (match(TOKEN_FUN))
	{
		funDeclaration();
	}
	else if (match(TOKEN_VAR))
	{
		varDeclaration();
	}
	else
	{
		statement();
	}

	if (parser.panicMode) synchronize();
}

void statement()
{
	// statement := printStmt | expressionStmt | block;
	if (match(TOKEN_PRINT))
	{
		printStatement();
	}
	else if (match(TOKEN_FOR))
	{
		forStatement();
	}
	else if (match(TOKEN_IF))
	{
		ifStatement();
	}
	else if (match(TOKEN_RETURN))
	{
		returnStatement();
	}
	else if (match(TOKEN_WHILE))
	{
		whileStatement();
	}
	else if (match(TOKEN_LEFT_BRACE))
	{
		beginScope();
		block();
		endScope();
	}
	else
	{
		expressionStatement();
	}
}

void grouping()
{
	expression();
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

}

ObjFunction* compile(const char* source)
{
	initScanner(source);

	Compiler compiler;
	initCompiler(&compiler, FunctionType::Script);

	advance();

	while (!match(TOKEN_EOF))
	{
		declaration();
	}

	auto f = endCompiler();
	return parser.hadError ? nullptr : f;
}

void markCompilerRoots()
{
	// 現在コンパイル中の関数とそれを包む上位関数オブジェクトをマーク
	Compiler* compiler = current;
	while (compiler != nullptr)
	{
		markObject(reinterpret_cast<Obj*>(compiler->function));
		compiler = compiler->enclosing;
	}
}
