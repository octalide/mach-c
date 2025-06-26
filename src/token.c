#include "token.h"

#include <stdlib.h>
#include <string.h>

void token_init(Token *token, TokenKind kind, int pos, int len)
{
    token->kind = kind;
    token->pos  = pos;
    token->len  = len;
}

void token_dnit(Token *token)
{
    token->kind = TOKEN_ERROR;
    token->pos  = -1;
    token->len  = -1;
}

void token_copy(Token *src, Token *dst)
{
    if (src == NULL || dst == NULL)
    {
        return;
    }

    dst->kind = src->kind;
    dst->pos  = src->pos;
    dst->len  = src->len;
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

    case TOKEN_TYPE_ANY:
        return "any";
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

TokenKind token_kind_from_identifier(const char *text, int len)
{
    // check built-in types first
    if (len == 2)
    {
        if (strncmp(text, "if", 2) == 0)
        {
            return TOKEN_KW_IF;
        }
        if (strncmp(text, "or", 2) == 0)
        {
            return TOKEN_KW_OR;
        }
        if (strncmp(text, "i8", 2) == 0)
        {
            return TOKEN_TYPE_I8;
        }
        if (strncmp(text, "u8", 2) == 0)
        {
            return TOKEN_TYPE_U8;
        }
    }
    if (len == 3)
    {
        if (strncmp(text, "use", 3) == 0)
        {
            return TOKEN_KW_USE;
        }
        if (strncmp(text, "ext", 3) == 0)
        {
            return TOKEN_KW_EXT;
        }
        if (strncmp(text, "def", 3) == 0)
        {
            return TOKEN_KW_DEF;
        }
        if (strncmp(text, "str", 3) == 0)
        {
            return TOKEN_KW_STR;
        }
        if (strncmp(text, "uni", 3) == 0)
        {
            return TOKEN_KW_UNI;
        }
        if (strncmp(text, "val", 3) == 0)
        {
            return TOKEN_KW_VAL;
        }
        if (strncmp(text, "var", 3) == 0)
        {
            return TOKEN_KW_VAR;
        }
        if (strncmp(text, "fun", 3) == 0)
        {
            return TOKEN_KW_FUN;
        }
        if (strncmp(text, "ret", 3) == 0)
        {
            return TOKEN_KW_RET;
        }
        if (strncmp(text, "if", 3) == 0)
        {
            return TOKEN_KW_IF;
        }
        if (strncmp(text, "or", 3) == 0)
        {
            return TOKEN_KW_OR;
        }
        if (strncmp(text, "for", 3) == 0)
        {
            return TOKEN_KW_FOR;
        }
        if (strncmp(text, "cnt", 3) == 0)
        {
            return TOKEN_KW_CNT;
        }
        if (strncmp(text, "brk", 3) == 0)
        {
            return TOKEN_KW_BRK;
        }
        if (strncmp(text, "any", 3) == 0)
        {
            return TOKEN_TYPE_ANY;
        }
        if (strncmp(text, "i8", 3) == 0)
        {
            return TOKEN_TYPE_I8;
        }
        if (strncmp(text, "i16", 3) == 0)
        {
            return TOKEN_TYPE_I16;
        }
        if (strncmp(text, "i32", 3) == 0)
        {
            return TOKEN_TYPE_I32;
        }
        if (strncmp(text, "i64", 3) == 0)
        {
            return TOKEN_TYPE_I64;
        }
        if (strncmp(text, "u8", 3) == 0)
        {
            return TOKEN_TYPE_U8;
        }
        if (strncmp(text, "u16", 3) == 0)
        {
            return TOKEN_TYPE_U16;
        }
        if (strncmp(text, "u32", 3) == 0)
        {
            return TOKEN_TYPE_U32;
        }
        if (strncmp(text, "u64", 3) == 0)
        {
            return TOKEN_TYPE_U64;
        }
        if (strncmp(text, "f32", 3) == 0)
        {
            return TOKEN_TYPE_F32;
        }
        if (strncmp(text, "f64", 3) == 0)
        {
            return TOKEN_TYPE_F64;
        }
    }

    return TOKEN_IDENTIFIER;
}
