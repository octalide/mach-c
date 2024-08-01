#ifndef TOKEN_H
#define TOKEN_H

static const int TOKEN_LIST_INC_SIZE = 16;

enum TokenKind;
struct Token;
struct TokenList;

typedef enum TokenKind
{
    TOKEN_LIST_ERROR = -3,
    TOKEN_ERROR,

    TOKEN_EOF,
    TOKEN_UNKNOWN,
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

typedef struct TokenList
{
    Token *tokens;
    int size;
} TokenList;

Token token_new(TokenKind kind, int pos, int len);
TokenList *token_list_new();
void token_list_free(TokenList *token_list);

int token_list_add(TokenList *token_list, Token token);
Token token_list_get_token(TokenList *token_list, int index);
TokenKind token_list_get_kind(TokenList *token_list, int index);

const char *token_kind_to_string(TokenKind kind);

#endif
