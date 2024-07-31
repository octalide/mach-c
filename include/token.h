#ifndef TOKEN_H
#define TOKEN_H

#include <stdbool.h>

typedef enum TokenKind
{
    TOKEN_ERROR,
    TOKEN_EOF,
    TOKEN_COMMENT,

    TOKEN_IDENTIFIER,

    TOKEN_INT,
    TOKEN_FLOAT,
    TOKEN_CHARACTER,
    TOKEN_STRING,

    TOKEN_L_PAREN,   // (
    TOKEN_R_PAREN,   // )
    TOKEN_L_BRACKET, // [
    TOKEN_R_BRACKET, // ]
    TOKEN_L_BRACE,   // {
    TOKEN_R_BRACE,   // }
    TOKEN_COLON,     // :
    TOKEN_SEMICOLON, // ;
    TOKEN_QUESTION,  // ?
    TOKEN_AT,        // @
    TOKEN_DOT,       // .
    TOKEN_COMMA,     // ,
    TOKEN_PLUS,      // +
    TOKEN_MINUS,     // -
    TOKEN_STAR,      // *
    TOKEN_PERCENT,   // %
    TOKEN_CARET,     // ^
    TOKEN_AMPERSAND, // &
    TOKEN_PIPE,      // |
    TOKEN_TILDE,     // ~
    TOKEN_LESS,      // <
    TOKEN_GREATER,   // >
    TOKEN_EQUAL,     // =
    TOKEN_BANG,      // !
    TOKEN_SLASH,     // /
    TOKEN_BACKSLASH,
    TOKEN_EQUAL_EQUAL,         // ==
    TOKEN_BANG_EQUAL,          // !=
    TOKEN_LESS_EQUAL,          // <=
    TOKEN_GREATER_EQUAL,       // >=
    TOKEN_LESS_LESS,           // <<
    TOKEN_GREATER_GREATER,     // >>
    TOKEN_AMPERSAND_AMPERSAND, // &&
    TOKEN_PIPE_PIPE,           // ||
    TOKEN_COLON_COLON,         // ::

    __KW_BEG__,
    TOKEN_KW_VAL, // val
    TOKEN_KW_VAR, // var
    TOKEN_KW_DEF, // def
    TOKEN_KW_USE, // use
    TOKEN_KW_FUN, // fun
    TOKEN_KW_NEW, // new
    TOKEN_KW_STR, // str
    TOKEN_KW_UNI, // uni
    TOKEN_KW_IF,  // if
    TOKEN_KW_OR,  // or
    TOKEN_KW_FOR, // for
    TOKEN_KW_BRK, // brk
    TOKEN_KW_CNT, // cnt
    TOKEN_KW_RET, // ret
    TOKEN_KW_EXT, // ext
    TOKEN_KW_ASM, // asm
    __KW_END__,

    TOKEN_COUNT
} TokenKind;

typedef struct token_keyword_map
{
    char *keyword;
    TokenKind kind;
} TokenKeywordMap;

static const TokenKeywordMap token_keyword_map[] = {
    {"val", TOKEN_KW_VAL},
    {"var", TOKEN_KW_VAR},
    {"def", TOKEN_KW_DEF},
    {"use", TOKEN_KW_USE},
    {"fun", TOKEN_KW_FUN},
    {"new", TOKEN_KW_NEW},
    {"str", TOKEN_KW_STR},
    {"uni", TOKEN_KW_UNI},
    {"if", TOKEN_KW_IF},
    {"or", TOKEN_KW_OR},
    {"for", TOKEN_KW_FOR},
    {"brk", TOKEN_KW_BRK},
    {"cnt", TOKEN_KW_CNT},
    {"ret", TOKEN_KW_RET},
    {"ext", TOKEN_KW_EXT},
    {"asm", TOKEN_KW_ASM},
};

typedef struct
{
    TokenKind kind;
    int pos;
    char *raw;

    union
    {
        long long i;
        double f;
        char *error;
    } value;
} Token;

Token *token_new(TokenKind kind, int pos, char *raw);
void token_free(Token *token);

char *token_kind_string(TokenKind kind);

#endif // TOKEN_H
