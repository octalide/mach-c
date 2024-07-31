#ifndef LEXER_H
#define LEXER_H

#include "token.h"

#include <stdbool.h>

typedef struct Lexer
{
    char *source;
    int start;
    int current;
} Lexer;

Lexer *lexer_new(char *source);
void lexer_free(Lexer *lexer);

char *lexer_get_line(Lexer *lexer, int line);
int lexer_token_line(Lexer *lexer, Token *token);
int lexer_token_line_char_offset(Lexer *lexer, Token *token);

Token *lexer_error(Lexer *lexer, int pos, char *message);

char lexer_advance(Lexer *lexer);
char lexer_current(Lexer *lexer);
char lexer_peek(Lexer *lexer);

bool lexer_at_end(Lexer *lexer);

void lexer_skip_whitespace(Lexer *lexer);

Token *lexer_emit(Lexer *lexer, TokenKind type, int pos, int len);
Token *lexer_emit_identifier(Lexer *lexer);
Token *lexer_emit_number(Lexer *lexer);
Token *lexer_emit_lit_string(Lexer *lexer);
Token *lexer_emit_lit_char(Lexer *lexer);

Token *lexer_next(Lexer *lexer);

#endif // LEXER_H
