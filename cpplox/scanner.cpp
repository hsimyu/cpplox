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

char advance()
{
	scanner.current++;
	return scanner.current[-1]; // current の一つ前を返す
}

char peek()
{
	return *scanner.current;
}

char peekNext() {
	if (isAtEnd()) return '\0';
	return scanner.current[1];
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
		case '/':
			if (peekNext() == '/')
			{
				// 行コメントは行末まで無視
				while (peek() != '\n' && !isAtEnd())
				{
					advance();
				}
				break;
			}
			else
			{
				// 最初の / も有効な字句なので抜ける
				return;
			}
		default:
		   return;
		}
	}
}

bool isAlpha(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool isDigit(char c)
{
	return c >= '0' && c <= '9';
}

TokenType checkKeyword(int start, int length, const char* rest, TokenType type)
{
	// 文字列長が一致していて、残り部分の値が一致していたら、指定したキーワードと認識する
	if (scanner.current - scanner.start == start + length && memcmp(scanner.start + start, rest, length) == 0)
	{
		return type;
	}
	return TOKEN_IDENTIFIER;
}

TokenType identifierType()
{
	switch (scanner.start[0])
	{
		case 'a': return checkKeyword(1, 2, "nd", TOKEN_AND);
		case 'c': return checkKeyword(1, 4, "lass", TOKEN_CLASS);
		case 'e': return checkKeyword(1, 3, "lse", TOKEN_ELSE);
		case 'f':
			if (scanner.current - scanner.start > 1) {
				switch (scanner.start[1])
				{
					case 'a': return checkKeyword(2, 3, "lse", TOKEN_FALSE);
					case 'o': return checkKeyword(2, 1, "r", TOKEN_FOR);
					case 'u': return checkKeyword(2, 1, "n", TOKEN_FUN);
				}
			}
			break;
		case 'i': return checkKeyword(1, 1, "f", TOKEN_IF);
		case 'n': return checkKeyword(1, 2, "il", TOKEN_NIL);
		case 'o': return checkKeyword(1, 1, "r", TOKEN_OR);
		case 'p': return checkKeyword(1, 4, "rint", TOKEN_PRINT);
		case 'r': return checkKeyword(1, 5, "eturn", TOKEN_RETURN);
		case 's': return checkKeyword(1, 4, "uper", TOKEN_SUPER);
		case 't':
			if (scanner.current - scanner.start > 1) {
				switch (scanner.start[1])
				{
					case 'h': return checkKeyword(2, 2, "is", TOKEN_THIS);
					case 'r': return checkKeyword(2, 2, "ue", TOKEN_TRUE);
				}
			}
			break;
		case 'v': return checkKeyword(1, 2, "ar", TOKEN_VAR);
		case 'w': return checkKeyword(1, 4, "hile", TOKEN_WHILE);
		case 'y': return checkKeyword(1, 4, "ield", TOKEN_YIELD);
		default:
			break;
	}
	return TOKEN_IDENTIFIER;
}

Token identifier()
{
	// 最初の一つは isAlpha() でマッチしているはず
	// その後は数字を含んでよい
	while (isAlpha(peek()) || isDigit(peek())) advance();
	return makeToken(identifierType());
}

Token number()
{
	while (isDigit(peek())) advance();

	// 小数点数
	if (peek() == '.' && isDigit(peekNext()))
	{
		advance(); // "." を消費
		while (isDigit(peek())) advance();
	}

	return makeToken(TOKEN_NUMBER);
}

Token string()
{
	// 次の二重引用符まで進める
	while (peek() != '"' && !isAtEnd()) {
		if (peek() == '\n') scanner.line++;

		advance();
	}

	if (isAtEnd()) return errorToken("Unterminated string.");

	// 閉じ引用符を消費
	advance();
	return makeToken(TOKEN_STRING);
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

	if (isAlpha(c)) return identifier();
	if (isDigit(c)) return number();

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
		case '"':
			return string();
	}

	return errorToken("Unexpected character");
}
