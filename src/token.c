#include "token.h"

#include <stdlib.h>

Token token_new(TokenKind kind, int pos, int len)
{
    Token token;
    token.kind = kind;
    token.pos = pos;
    token.len = len;

    return token;
}

TokenList *token_list_new()
{
    TokenList *token_list = calloc(1, sizeof(TokenList));
    if (token_list != NULL)
    {
        token_list->tokens = calloc(TOKEN_LIST_INC_SIZE, sizeof(Token));
        if (token_list->tokens == NULL)
        {
            free(token_list);
            token_list = NULL;
        }
    }

    return token_list;
}

void token_list_free(TokenList *token_list)
{
    if (token_list == NULL)
    {
        return;
    }

    free(token_list->tokens);
    token_list->tokens = NULL;

    free(token_list);
}

int token_list_add(TokenList *token_list, Token token)
{
    if (token_list == NULL)
    {
        return TOKEN_LIST_ERROR;
    }

    if (token_list->size % TOKEN_LIST_INC_SIZE == 0)
    {
        Token *new_tokens = realloc(token_list->tokens, (token_list->size + TOKEN_LIST_INC_SIZE) * sizeof(Token));
        if (new_tokens == NULL)
        {
            return TOKEN_LIST_ERROR;
        }

        token_list->tokens = new_tokens;
    }

    token_list->tokens[token_list->size] = token;
    token_list->size++;

    return token_list->size - 1;
}

Token token_list_get_token(TokenList *token_list, int index)
{
    if (token_list == NULL || index < 0 || index >= token_list->size)
    {
        Token token;
        token.kind = TOKEN_LIST_ERROR;
        token.pos = -1;
        token.len = -1;

        return token;
    }

    return token_list->tokens[index];
}

TokenKind token_list_get_kind(TokenList *token_list, int index)
{
    if (token_list == NULL || index < 0 || index >= token_list->size)
    {
        return TOKEN_LIST_ERROR;
    }

    return token_list->tokens[index].kind;
}

const char *token_kind_to_string(TokenKind kind)
{
    switch (kind)
    {
    case TOKEN_LIST_ERROR:
        return "LIST_ERROR";
    case TOKEN_ERROR:
        return "ERROR";
    case TOKEN_UNKNOWN:
        return "UNKNOWN";
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
    case TOKEN_L_PAREN:
        return "L_PAREN";
    case TOKEN_R_PAREN:
        return "R_PAREN";
    case TOKEN_L_BRACKET:
        return "L_BRACKET";
    case TOKEN_R_BRACKET:
        return "R_BRACKET";
    case TOKEN_L_BRACE:
        return "L_BRACE";
    case TOKEN_R_BRACE:
        return "R_BRACE";
    case TOKEN_COLON:
        return "COLON";
    case TOKEN_SEMICOLON:
        return "SEMICOLON";
    case TOKEN_QUESTION:
        return "QUESTION";
    case TOKEN_AT:
        return "AT";
    case TOKEN_DOT:
        return "DOT";
    case TOKEN_COMMA:
        return "COMMA";
    case TOKEN_PLUS:
        return "PLUS";
    case TOKEN_MINUS:
        return "MINUS";
    case TOKEN_STAR:
        return "STAR";
    case TOKEN_PERCENT:
        return "PERCENT";
    case TOKEN_CARET:
        return "CARET";
    case TOKEN_AMPERSAND:
        return "AMPERSAND";
    case TOKEN_PIPE:
        return "PIPE";
    case TOKEN_TILDE:
        return "TILDE";
    case TOKEN_LESS:
        return "LESS";
    case TOKEN_GREATER:
        return "GREATER";
    case TOKEN_EQUAL:
        return "EQUAL";
    case TOKEN_BANG:
        return "BANG";
    case TOKEN_SLASH:
        return "SLASH";
    case TOKEN_BACKSLASH:
        return "BACKSLASH";
    case TOKEN_EQUAL_EQUAL:
        return "EQUAL_EQUAL";
    case TOKEN_BANG_EQUAL:
        return "BANG_EQUAL";
    case TOKEN_LESS_EQUAL:
        return "LESS_EQUAL";
    case TOKEN_GREATER_EQUAL:
        return "GREATER_EQUAL";
    case TOKEN_LESS_LESS:
        return "LESS_LESS";
    case TOKEN_GREATER_GREATER:
        return "GREATER_GREATER";
    case TOKEN_AMPERSAND_AMPERSAND:
        return "AMPERSAND_AMPERSAND";
    case TOKEN_PIPE_PIPE:
        return "PIPE_PIPE";
    case TOKEN_COLON_COLON:
        return "COLON_COLON";
    }
}
