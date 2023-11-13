#include "compiler.h"

#include "scanner.h"

#include <cstdio>

void compile(const char* source)
{
	initScanner(source);
	int line = -1;

	for (;;) {
		Token token = scanToken();
		if (token.line != line)
		{
			printf("%04d ", token.line);
			line = token.line;
		}
		else
		{
			printf(" | ");
		}
		printf("%02d '%.*s'\n", token.type, token.length, token.start);

		if (token.type == TOKEN_EOF) break;
	}
}
