#include "token.h"

#include <stdlib.h>

void token_init(Token *token, TokenKind kind, int pos, int len)
{
    token->kind = kind;
    token->pos = pos;
    token->len = len;
}

void token_dnit(Token *token)
{
    token->kind = TOKEN_ERROR;
    token->pos = -1;
    token->len = -1;
}

void token_copy(Token *src, Token *dst)
{
    if (src == NULL || dst == NULL)
    {
        return;
    }

    dst->kind = src->kind;
    dst->pos = src->pos;
    dst->len = src->len;
}

char *token_kind_to_string(TokenKind kind)
{
    switch (kind)
    {
    case TOKEN_ERROR:
        return "ERROR";
    case TOKEN_EOF:
        return "EOF";
    case TOKEN_COMMENT:
        return "COMMENT";

    case TOKEN_LIT_INT:
        return "LIT_INT";
    case TOKEN_LIT_FLOAT:
        return "LIT_FLOAT";
    case TOKEN_LIT_CHAR:
        return "LIT_CHAR";
    case TOKEN_LIT_STRING:
        return "LIT_STRING";

    case TOKEN_IDENTIFIER:
        return "IDENTIFIER";

    case TOKEN_KW_USE:
        return "use";
    case TOKEN_KW_EXT:
        return "ext";
    case TOKEN_KW_DEF:
        return "def";
    case TOKEN_KW_STR:
        return "str";
    case TOKEN_KW_UNI:
        return "uni";
    case TOKEN_KW_VAL:
        return "val";
    case TOKEN_KW_VAR:
        return "var";
    case TOKEN_KW_FUN:
        return "fun";
    case TOKEN_KW_RET:
        return "ret";
    case TOKEN_KW_IF:
        return "if";
    case TOKEN_KW_OR:
        return "or";
    case TOKEN_KW_FOR:
        return "for";
    case TOKEN_KW_CNT:
        return "cnt";
    case TOKEN_KW_BRK:
        return "brk";

    case TOKEN_TYPE_NAT:
        return "nat";
    case TOKEN_TYPE_I8:
        return "i8";
    case TOKEN_TYPE_I16:
        return "i16";
    case TOKEN_TYPE_I32:
        return "i32";
    case TOKEN_TYPE_I64:
        return "i64";
    case TOKEN_TYPE_U8:
        return "u8";
    case TOKEN_TYPE_U16:
        return "u16";
    case TOKEN_TYPE_U32:
        return "u32";
    case TOKEN_TYPE_U64:
        return "u64";
    case TOKEN_TYPE_F32:
        return "f32";
    case TOKEN_TYPE_F64:
        return "f64";

    case TOKEN_L_PAREN:
        return "(";
    case TOKEN_R_PAREN:
        return ")";
    case TOKEN_L_BRACKET:
        return "[";
    case TOKEN_R_BRACKET:
        return "]";
    case TOKEN_L_BRACE:
        return "{";
    case TOKEN_R_BRACE:
        return "}";
    case TOKEN_COLON:
        return ":";
    case TOKEN_SEMICOLON:
        return ";";
    case TOKEN_QUESTION:
        return "?";
    case TOKEN_AT:
        return "@";
    case TOKEN_DOT:
        return ".";
    case TOKEN_COMMA:
        return ",";
    case TOKEN_UNDERSCORE:
        return "_";

    case TOKEN_PLUS:
        return "+";
    case TOKEN_MINUS:
        return "-";
    case TOKEN_STAR:
        return "*";
    case TOKEN_PERCENT:
        return "%";
    case TOKEN_CARET:
        return "^";
    case TOKEN_AMPERSAND:
        return "&";
    case TOKEN_PIPE:
        return "|";
    case TOKEN_TILDE:
        return "~";
    case TOKEN_LESS:
        return "<";
    case TOKEN_GREATER:
        return ">";
    case TOKEN_EQUAL:
        return "=";
    case TOKEN_BANG:
        return "!";
    case TOKEN_SLASH:
        return "/";

    case TOKEN_EQUAL_EQUAL:
        return "==";
    case TOKEN_BANG_EQUAL:
        return "!=";
    case TOKEN_LESS_EQUAL:
        return "<=";
    case TOKEN_GREATER_EQUAL:
        return ">=";
    case TOKEN_LESS_LESS:
        return "<<";
    case TOKEN_GREATER_GREATER:
        return ">>";
    case TOKEN_AMPERSAND_AMPERSAND:
        return "&&";
    case TOKEN_PIPE_PIPE:
        return "||";
    case TOKEN_COLON_COLON:
        return "::";

    default:
        return "UNKNOWN";
    }
}
