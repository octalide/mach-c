#ifndef TOKEN_H
#define TOKEN_H

typedef enum TokenKind
{
    TOKEN_LIST_ERROR = -2,
    TOKEN_ERROR,

    TOKEN_EOF,
    TOKEN_COMMENT,

    TOKEN_IDENTIFIER,

    TOKEN_LIT_INT,
    TOKEN_LIT_FLOAT,
    TOKEN_LIT_CHAR,
    TOKEN_LIT_STRING,

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
} TokenKind;

typedef struct Token
{
    TokenKind kind;
    int pos;
    int len;
} Token;

Token *token_new(TokenKind kind, int pos, int len);
void token_free(Token *token);

Token *token_copy(Token *token);

char *token_kind_to_string(TokenKind kind);

#endif
