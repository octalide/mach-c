#ifndef TOKEN_H
#define TOKEN_H

#include <stdbool.h>

typedef enum
{
    TOKEN_ERROR,
    TOKEN_EOF,
    TOKEN_COMMENT,

    TOKEN_IDENTIFIER,
    
    TOKEN_LEFT_PAREN,    // (
    TOKEN_RIGHT_PAREN,   // )
    TOKEN_LEFT_BRACKET,  // [
    TOKEN_RIGHT_BRACKET, // ]
    TOKEN_LEFT_BRACE,    // {
    TOKEN_RIGHT_BRACE,   // }

    TOKEN_COLON,     // :
    TOKEN_SEMICOLON, // ;

    TOKEN_QUESTION, // ?
    TOKEN_AT,       // @
    TOKEN_HASH,     // #

    TOKEN_DOT,                 // .
    TOKEN_COMMA,               // ,
    TOKEN_PLUS,                // +
    TOKEN_MINUS,               // -
    TOKEN_STAR,                // *
    TOKEN_PERCENT,             // %
    TOKEN_CARET,               // ^
    TOKEN_AMPERSAND,           // &
    TOKEN_PIPE,                // |
    TOKEN_TILDE,               // ~
    TOKEN_LESS,                // <
    TOKEN_GREATER,             // >
    TOKEN_EQUAL,               // =
    TOKEN_BANG,                // !
    TOKEN_SLASH,               // /
    TOKEN_BACKSLASH,
    TOKEN_EQUAL_EQUAL,         // ==
    TOKEN_BANG_EQUAL,          // !=
    TOKEN_LESS_EQUAL,          // <=
    TOKEN_GREATER_EQUAL,       // >=
    TOKEN_LESS_LESS,           // <<
    TOKEN_GREATER_GREATER,     // >>
    TOKEN_AMPERSAND_AMPERSAND, // &&
    TOKEN_PIPE_PIPE,           // ||

    TOKEN_NUMBER,
    TOKEN_CHARACTER,
    TOKEN_STRING,
} TokenType;

typedef struct
{
    TokenType type;
    const char *start;
    char length;
} Token;

char *token_type_string(TokenType type);

#endif // TOKEN_H
