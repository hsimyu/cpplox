#include "compiler.h"

#include "scanner.h"
#include "chunk.h"

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
}

void number()
{
	// TODO: std::from_chars のがよい?
	double value = strtod(parser.previous.start, nullptr);
	emitConstant(value);
}

void unary()
{
	auto opType = parser.previous.type;

	// オペランドをコンパイル
	parsePrecedence(PREC_UNARY);

	switch (opType)
	{
	case TOKEN_MINUS:
		emitByte(OP_NEGATE);
		break;
	default:
		return; // Unreachable
	}
}

void parsePrecedence(Precedence precedence)
{
	// 指定された優先順位より低いものに遭遇するまで式をパースする
}

void expression()
{
	parsePrecedence(PREC_ASSIGNMENT);
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

	expression();

	consume(TOKEN_EOF, "Expect end of expression.");
	endCompiler();
	return !parser.hadError;
}
