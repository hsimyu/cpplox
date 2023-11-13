#include "scanner.h"

#include <cstring>

namespace
{

struct Scanner {
	const char* start = nullptr;
	const char* current = nullptr;
	int line = 0;
};

Scanner scanner;

bool isAtEnd()
{
	return *scanner.current == '\0';
}

char peek()
{
	return *scanner.current;
}

char advance()
{
	scanner.current++;
	return scanner.current[-1]; // current の一つ前を返す
}

bool match(char expected)
{
	if (isAtEnd()) return false;
	if (*scanner.current != expected) return false;
	scanner.current++;
	return true;
}

Token makeToken(TokenType type)
{
	return {
		.type = type,
		.start = scanner.start,
		.length = static_cast<int>(scanner.current - scanner.start),
		.line = scanner.line,
	};
}

Token errorToken(const char* message)
{
	return {
		.type = TOKEN_ERROR,
		.start = message,
		.length = static_cast<int>(strlen(message)),
		.line = scanner.line,
	};
}

void skipWhiteSpace()
{
	for (;;)
	{
		char c = peek();
		switch (c)
		{
		case ' ':
		case '\r':
		case '\t':
			advance();
			break;
		case '\n':
			scanner.line++;
			advance();
			break;
		default:
		   return;
		}
	}
}

}

void initScanner(const char* source)
{
	scanner.start = source;
	scanner.current = source;
	scanner.line = 1;
}

Token scanToken()
{
	skipWhiteSpace();
	scanner.start = scanner.current;

	if (isAtEnd()) return makeToken(TOKEN_EOF);

	char c = advance();
	switch (c)
	{
		case '(': return makeToken(TOKEN_LEFT_PAREN);
		case ')': return makeToken(TOKEN_RIGHT_PAREN);
		case '{': return makeToken(TOKEN_LEFT_BRACE);
		case '}': return makeToken(TOKEN_RIGHT_BRACE);
		case ';': return makeToken(TOKEN_SEMICOLON);
		case ',': return makeToken(TOKEN_COMMA);
		case '.': return makeToken(TOKEN_DOT);
		case '-': return makeToken(TOKEN_MINUS);
		case '+': return makeToken(TOKEN_PLUS);
		case '/': return makeToken(TOKEN_SLASH);
		case '*': return makeToken(TOKEN_STAR);
		case '!':
			return makeToken(match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
		case '=':
			return makeToken(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
		case '<':
			return makeToken(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
		case '>':
			return makeToken(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
	}

	return errorToken("Unexpected character");
}
