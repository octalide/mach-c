#ifndef LEXER_H
#define LEXER_H

#include "token.h"

typedef struct
{
    const char *start;
    const char *current;
    int line;
} Lexer;

void lexer_init(Lexer *lexer, const char *source);
Token next_token(Lexer *lexer);
char lexer_advance(Lexer *lexer);
char lexer_peek(Lexer *lexer);
char lexer_peek_next(Lexer *lexer);

#endif // LEXER_H
