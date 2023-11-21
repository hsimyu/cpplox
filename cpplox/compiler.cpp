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

Parser parser;
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

void number();
void str();
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
	/* TOKEN_IDENTIFIER    */ {nullptr, nullptr, PREC_NONE},
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

		infixRule();
	}
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

void printStatement()
{
	// printStmt := "print" expression ";" ;
	expression();
	consume(TOKEN_SEMICOLON, "Expect ';' after value.");
	emitByte(OP_PRINT);
}

void expressionStatement()
{
	// expressionStmt := expression ";" ;
	expression();
	consume(TOKEN_SEMICOLON, "Expect ';' after value.");
	emitByte(OP_POP);
}

void declaration()
{
	// declaration := statement
	statement();
}

void statement()
{
	// statement := printStmt | expressionStmt
	if (match(TOKEN_PRINT))
	{
		printStatement();
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
	compilingChunk = chunk;

	advance();

	while (!match(TOKEN_EOF))
	{
		declaration();
	}

	endCompiler();
	return !parser.hadError;
}
