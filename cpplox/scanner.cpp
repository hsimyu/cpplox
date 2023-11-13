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

}

void initScanner(const char* source)
{
	scanner.start = source;
	scanner.current = source;
	scanner.line = 1;
}

Token scanToken()
{
	scanner.start = scanner.current;

	if (isAtEnd()) return makeToken(TOKEN_EOF);

	return errorToken("Unexpected character");
}
