#include "scanner.h"

struct Scanner {
	const char* start = nullptr;
	const char* current = nullptr;
	int line = 0;
};

Scanner scanner;

void initScanner(const char* source)
{
	scanner.start = source;
	scanner.current = source;
	scanner.line = 1;
}
