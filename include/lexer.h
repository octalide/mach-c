#ifndef LEXER_H
#define LEXER_H

#include "token.h"

typedef struct Lexer
{
    char *source;
    char *start;
    char *current;
    int line;
} Lexer;

void lexer_init(Lexer *lexer, const char *source);
void lexer_free(Lexer *lexer);

char *lexer_get_line(Lexer *lexer, int line);
int lexer_get_token_line_index(Lexer *lexer, Token *token);
int lexer_get_token_line_char_index(Lexer *lexer, Token *token);

char lexer_advance(Lexer *lexer);
char lexer_peek(Lexer *lexer);
char lexer_peek_next(Lexer *lexer);

Token next_token(Lexer *lexer);

#endif // LEXER_H
