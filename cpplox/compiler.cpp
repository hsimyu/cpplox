#include "compiler.h"

#include "scanner.h"
#include "chunk.h"
#include "common.h"
#include "object.h"

#if DEBUG_PRINT_CODE
#include "debug.h"
#endif

#include <cstdio>
#include <cstdlib>

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
};

struct Compiler
{
	Local locals[LOCAL_VARIABLE_COUNT];
	int localCount = 0;
	int scopeDepth = 0;
};

Parser parser;
Compiler* current = nullptr;
Chunk* compilingChunk = nullptr;

Chunk* currentChunk()
{
	return compilingChunk;
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
	emitByte(OP_RETURN);
}

void initCompiler(Compiler* compiler)
{
	compiler->localCount = 0;
	compiler->scopeDepth = 0;
	current = compiler;
}

void endCompiler()
{
	emitReturn();

#if DEBUG_PRINT_CODE
	if (!parser.hadError)
	{
		disassembleChunk(currentChunk(), "code");
	}
#endif
}

void beginScope()
{
	current->scopeDepth++;
}

void endScope()
{
	current->scopeDepth--;
}

void number();
void str();
void variable();
void unary();
void binary();
void literal();
void grouping();
void expression();
void declaration();
void statement();

ParseRule rules[] = {
	// [前置パーサー、中置パーサー、中置パーサーの優先順位] の表
	/* TOKEN_LEFT_PAREN    */ {grouping, nullptr, PREC_NONE},
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
	/* TOKEN_AND           */ {nullptr, nullptr, PREC_NONE},
	/* TOKEN_CLASS         */ {nullptr, nullptr, PREC_NONE},
	/* TOKEN_ELSE          */ {nullptr, nullptr, PREC_NONE},
	/* TOKEN_FALSE         */ {literal, nullptr, PREC_NONE},
	/* TOKEN_FOR           */ {nullptr, nullptr, PREC_NONE},
	/* TOKEN_FUN           */ {nullptr, nullptr, PREC_NONE},
	/* TOKEN_IF            */ {nullptr, nullptr, PREC_NONE},
	/* TOKEN_NIL           */ {literal, nullptr, PREC_NONE},
	/* TOKEN_OR            */ {nullptr, nullptr, PREC_NONE},
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

void addLocal(Token name)
{
	if (current->localCount == LOCAL_VARIABLE_COUNT)
	{
		error("Too many local variables in function.");
		return;
	}

	Local* local = &current->locals[current->localCount++];
	local->name = name;
	local->depth = current->scopeDepth;
}

void declareVariable()
{
	if (current->scopeDepth == 0) return;

	Token* name = &parser.previous;
	addLocal(*name);
}

uint8_t parseVariable(const char* errorMessage)
{
	consume(TOKEN_IDENTIFIER, errorMessage);

	declareVariable();
	if (current->scopeDepth > 0) return 0;

	return identifierConstant(&parser.previous);
}

void defineVariable(uint8_t global)
{
	if (current->scopeDepth > 0)
	{
		return;
	}
	emitBytes(OP_DEFINE_GLOBAL, global);
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
	uint8_t arg = identifierConstant(&name);

	if (canAssign && match(TOKEN_EQUAL))
	{
		// identifier の後に = があったら、後段にあるものを右辺値としてセット命令で包む
		expression();
		emitBytes(OP_SET_GLOBAL, arg);
	}
	else
	{
		emitBytes(OP_GET_GLOBAL, arg);
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

void printStatement()
{
	// printStmt := "print" expression ";" ;
	expression();
	consume(TOKEN_SEMICOLON, "Expect ';' after value.");
	emitByte(OP_PRINT);
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
	// declaration := varDecl | statement
	if (match(TOKEN_VAR))
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

bool compile(const char* source, Chunk* chunk)
{
	initScanner(source);

	Compiler compiler;
	initCompiler(&compiler);

	compilingChunk = chunk;

	advance();

	while (!match(TOKEN_EOF))
	{
		declaration();
	}

	endCompiler();
	return !parser.hadError;
}
