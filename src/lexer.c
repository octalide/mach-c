#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "lexer.h"

void lexer_init(Lexer *lexer, const char *source)
{
    lexer->source = source;
    lexer->start = source;
    lexer->current = source;
    lexer->line = 1;
}

void lexer_free(Lexer *lexer)
{
    if (!lexer)
    {
        return;
    }

    if (lexer->start != NULL)
    {
        free(lexer->start);
    }

    if (lexer->current != NULL)
    {
        free(lexer->current);
    }

    if (lexer->source != NULL)
    {
        free(lexer->source);
    }
}

char *lexer_get_line(Lexer *lexer, int line)
{
    const char *start = lexer->source;
    while (line > 1)
    {
        if (*start == '\0')
        {
            return NULL;
        }

        if (*start == '\n')
        {
            line--;
        }

        start++;
    }

    const char *end = start;
    while (*end != '\n' && *end != '\0')
    {
        end++;
    }

    int length = (int)(end - start);
    char *line_buffer = malloc(length + 1);
    strncpy(line_buffer, start, length);
    line_buffer[length] = '\0';

    return line_buffer;
}

int lexer_get_token_line_index(Lexer *lexer, Token *token)
{
    if (!token)
    {
        return -1;
    }

    const char *start = lexer->source;
    int line = 1;
    while (start < token->start)
    {
        if (*start == '\n')
        {
            line++;
        }

        start++;
    }

    return line;
}

int lexer_get_token_line_char_index(Lexer *lexer, Token *token)
{
    if (!token)
    {
        return -1;
    }

    const char *line_start = token->start;
    while (line_start > lexer->source && *line_start != '\n')
    {
        line_start--;
    }

    return (int)(token->start - line_start);
}

bool is_at_end(Lexer *lexer)
{
    return *lexer->current == '\0';
}

char lexer_advance(Lexer *lexer)
{
    lexer->current++;

    return lexer->current[-1];
}

char lexer_peek(Lexer *lexer)
{
    return *lexer->current;
}

char lexer_peek_next(Lexer *lexer)
{
    if (is_at_end(lexer))
        return '\0';

    return lexer->current[1];
}

Token make_token(Lexer *lexer, TokenType type)
{
    Token token;
    token.type = type;
    token.start = lexer->start;
    token.length = (int)(lexer->current - lexer->start);

    return token;
}

Token make_token_error(const char *message)
{
    Token token;
    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = (int)strlen(message);

    return token;
}

void skip_whitespace(Lexer *lexer)
{
    for (;;)
    {
        char c = lexer_peek(lexer);
        switch (c)
        {
        case ' ':
        case '\r':
        case '\t':
            lexer_advance(lexer);
            break;
        case '\n':
            lexer_advance(lexer);
            lexer->line++;
            break;
        default:
            return;
        }
    }
}

Token identifier(Lexer *lexer)
{
    while (isalnum(lexer_peek(lexer)) || lexer_peek(lexer) == '_')
    {
        lexer_advance(lexer);
    }

    return make_token(lexer, TOKEN_IDENTIFIER);
}

Token number(Lexer *lexer)
{
    // check for number formats:
    // - '0b': binary
    // - '0o': octal
    // - '0x': hexadecimal
    if (lexer_peek(lexer) == '0')
    {
        switch (lexer_peek_next(lexer))
        {
        case 'b':
        case 'B':
            lexer_advance(lexer);
            lexer_advance(lexer);

            while (isdigit(lexer_peek(lexer)) || lexer_peek(lexer) == '_')
            {
                if (!(lexer_peek(lexer) == '_') && (lexer_peek(lexer) != '0' && lexer_peek(lexer) != '1'))
                {
                    return make_token_error("invalid binary digit");
                }

                lexer_advance(lexer);
            }

            return make_token(lexer, TOKEN_NUMBER);
        case 'o':
        case 'O':
            lexer_advance(lexer);
            lexer_advance(lexer);

            while (isdigit(lexer_peek(lexer)) || lexer_peek(lexer) == '_')
            {
                if (!(lexer_peek(lexer) == '_') && (lexer_peek(lexer) < '0' || lexer_peek(lexer) > '7'))
                {
                    return make_token_error("invalid octal digit");
                }

                lexer_advance(lexer);
            }

            return make_token(lexer, TOKEN_NUMBER);
        case 'x':
        case 'X':
            lexer_advance(lexer);
            lexer_advance(lexer);

            while (isxdigit(lexer_peek(lexer)) || lexer_peek(lexer) == '_')
            {
                if (!(lexer_peek(lexer) == '_') && !isxdigit(lexer_peek(lexer)))
                {
                    return make_token_error("invalid hexadecimal digit");
                }

                lexer_advance(lexer);
            }

            return make_token(lexer, TOKEN_NUMBER);
        }
    }

    while (isdigit(lexer_peek(lexer)) || lexer_peek(lexer) == '_')
    {
        lexer_advance(lexer);
    }

    if (lexer_peek(lexer) == '.' && isdigit(lexer_peek_next(lexer)))
    {
        lexer_advance(lexer);

        while (isdigit(lexer_peek(lexer)))
        {
            lexer_advance(lexer);
        }
    }

    return make_token(lexer, TOKEN_NUMBER);
}

Token string(Lexer *lexer)
{
    while (!is_at_end(lexer) && (lexer_peek(lexer) != '"' || (lexer_peek(lexer) == '\\' && lexer_peek_next(lexer) == '"')))
    {
        // special case for escaped double quote
        if (lexer_peek(lexer) == '\\' && (lexer_peek_next(lexer) == '"' || lexer_peek_next(lexer) == '\\'))
        {
            lexer_advance(lexer);
        }

        lexer_advance(lexer);
    }

    if (is_at_end(lexer) || lexer_peek(lexer) == '\n')
    {
        return make_token_error("unterminated string literal");
    }

    lexer_advance(lexer);

    return make_token(lexer, TOKEN_STRING);
}

Token character(Lexer *lexer)
{
    while ((lexer_peek(lexer) != '\'' && lexer_peek(lexer) != '\n') && !is_at_end(lexer))
    {
        lexer_advance(lexer);
    }

    if (lexer_peek(lexer) == '\n' || is_at_end(lexer))
    {
        return make_token_error("unterminated character literal");
    }

    lexer_advance(lexer);

    return make_token(lexer, TOKEN_CHARACTER);
}

Token comment(Lexer *lexer)
{
    while (lexer_peek(lexer) != '\n' && !is_at_end(lexer))
    {
        lexer_advance(lexer);
    }

    return make_token(lexer, TOKEN_COMMENT);
}

Token next_token(Lexer *lexer)
{
    skip_whitespace(lexer);
    lexer->start = lexer->current;

    if (is_at_end(lexer))
    {
        return make_token(lexer, TOKEN_EOF);
    }

    char c = lexer_peek(lexer);

    if (isalpha(c) || c == '_')
    {
        // check for string types:
        // - 'f': format string
        if (c == 'f')
        {
            if (lexer_peek_next(lexer) == '"')
            {
                lexer_advance(lexer);
                lexer_advance(lexer);

                return string(lexer);
            }
        }

        return identifier(lexer);
    }

    if (isdigit(c))
    {
        return number(lexer);
    }

    switch (lexer_advance(lexer))
    {
    case '\'':
        return character(lexer);
    case '\"':
        return string(lexer);

    case '(':
        return make_token(lexer, TOKEN_LEFT_PAREN);
    case ')':
        return make_token(lexer, TOKEN_RIGHT_PAREN);
    case '{':
        return make_token(lexer, TOKEN_LEFT_BRACE);
    case '}':
        return make_token(lexer, TOKEN_RIGHT_BRACE);
    case '[':
        return make_token(lexer, TOKEN_LEFT_BRACKET);
    case ']':
        return make_token(lexer, TOKEN_RIGHT_BRACKET);

    case ':':
        return make_token(lexer, TOKEN_COLON);
    case ';':
        return make_token(lexer, TOKEN_SEMICOLON);

    case '?':
        return make_token(lexer, TOKEN_QUESTION);
    case '@':
        return make_token(lexer, TOKEN_AT);
    case '#':
        return make_token(lexer, TOKEN_HASH);

    case '.':
        return make_token(lexer, TOKEN_DOT);
    case ',':
        return make_token(lexer, TOKEN_COMMA);
    case '+':
        return make_token(lexer, TOKEN_PLUS);
    case '-':
        return make_token(lexer, TOKEN_MINUS);
    case '*':
        return make_token(lexer, TOKEN_STAR);
    case '%':
        return make_token(lexer, TOKEN_PERCENT);
    case '^':
        return make_token(lexer, TOKEN_CARET);
    case '&':
        switch (lexer_peek(lexer))
        {
        case '&':
            lexer_advance(lexer);
            return make_token(lexer, TOKEN_AMPERSAND_AMPERSAND);
        }

        return make_token(lexer, TOKEN_AMPERSAND);
    case '|':
        switch (lexer_peek(lexer))
        {
        case '|':
            lexer_advance(lexer);
            return make_token(lexer, TOKEN_PIPE_PIPE);
        }

        return make_token(lexer, TOKEN_PIPE);
    case '~':
        return make_token(lexer, TOKEN_TILDE);
    case '<':
        switch (lexer_peek(lexer))
        {
        case '<':
            lexer_advance(lexer);
            return make_token(lexer, TOKEN_LESS_LESS);
        case '=':
            lexer_advance(lexer);
            return make_token(lexer, TOKEN_LESS_EQUAL);
        }

        return make_token(lexer, TOKEN_LESS);
    case '>':
        switch (lexer_peek(lexer))
        {
        case '>':
            lexer_advance(lexer);
            return make_token(lexer, TOKEN_GREATER_GREATER);
        case '=':
            lexer_advance(lexer);
            return make_token(lexer, TOKEN_GREATER_EQUAL);
        }

        return make_token(lexer, TOKEN_GREATER);
    case '=':
        switch (lexer_peek(lexer))
        {
        case '=':
            lexer_advance(lexer);
            return make_token(lexer, TOKEN_EQUAL_EQUAL);
        }

        return make_token(lexer, TOKEN_EQUAL);
    case '!':
        switch (lexer_peek(lexer))
        {
        case '=':
            lexer_advance(lexer);
            return make_token(lexer, TOKEN_BANG_EQUAL);
        }

        return make_token(lexer, TOKEN_BANG);
    case '/':
        switch (lexer_peek(lexer))
        {
        case '/':
            lexer_advance(lexer);
            return comment(lexer);
        }

        return make_token(lexer, TOKEN_SLASH);
    case '\\':
        return make_token(lexer, TOKEN_BACKSLASH);
    }

    return make_token_error("unexpected character");
}
