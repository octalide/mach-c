#include "lexer.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>

#define MAX_NUMBER_LITERAL_LENGTH 1024

Lexer *lexer_new(char *source)
{
    Lexer *lexer = calloc(1, sizeof(Lexer));
    if (lexer == NULL)
    {
        fprintf(stderr, "error: failed to allocate memory for lexer\n");
        exit(1);
    }

    lexer->source = strdup(source);
    if (lexer->source == NULL)
    {
        fprintf(stderr, "error: failed to allocate memory for lexer source\n");
        free(lexer);
        exit(1);
    }

    lexer->start = 0;
    lexer->current = 0;

    return lexer;
}

void lexer_free(Lexer *lexer)
{
    if (lexer == NULL)
    {
        return;
    }

    free(lexer->source);
    lexer->source = NULL;

    free(lexer);
}

char *lexer_get_line(Lexer *lexer, int line)
{
    char *start = lexer->source;
    while (line > 1 && *start != '\0')
    {
        if (*start == '\n')
        {
            line--;
        }
        start++;
    }

    char *end = start;
    while (*end != '\n' && *end != '\0')
    {
        end++;
    }

    int length = (int)(end - start);
    char *line_buffer = malloc(length + 1);
    if (line_buffer == NULL)
    {
        fprintf(stderr, "error: failed to allocate memory for line buffer\n");
        return NULL;
    }

    strncpy(line_buffer, start, length);
    line_buffer[length] = '\0';

    return line_buffer;
}

int lexer_token_line(Lexer *lexer, Token *token)
{
    char *start = lexer->source;
    char *end = lexer->source + token->pos;

    int line = 1;
    while (start < end)
    {
        if (*start == '\n')
        {
            line++;
        }
        start++;
    }

    return line;
}

int lexer_token_line_char_offset(Lexer *lexer, Token *token)
{
    char *start = lexer->source;
    char *end = lexer->source + token->pos;

    int offset = 0;
    while (start < end)
    {
        if (*start == '\n')
        {
            offset = 0;
        }
        else
        {
            offset++;
        }
        start++;
    }

    return offset;
}

Token *lexer_error(Lexer *lexer, int pos, char *message)
{
    Token *token = lexer_emit(lexer, TOKEN_ERROR, pos, 1);
    if (token != NULL)
    {
        token->value.error = strdup(message);
    }
    return token;
}

char lexer_advance(Lexer *lexer)
{
    return lexer->source[lexer->current++];
}

char lexer_current(Lexer *lexer)
{
    return lexer->source[lexer->current];
}

char lexer_peek(Lexer *lexer)
{
    return lexer->source[lexer->current + 1];
}

bool lexer_at_end(Lexer *lexer)
{
    return lexer_current(lexer) == '\0';
}

void lexer_skip_whitespace(Lexer *lexer)
{
    while (isspace(lexer_current(lexer)) && !lexer_at_end(lexer))
    {
        lexer_advance(lexer);
    }
}

Token *lexer_emit(Lexer *lexer, TokenKind kind, int pos, int len)
{
    char *raw = malloc(len + 1);
    if (raw == NULL)
    {
        printf("error: failed to allocate memory for token raw\n");
    }

    strncpy(raw, lexer->source + pos, len);
    raw[len] = '\0';

    return token_new(kind, pos, raw);
}

Token *lexer_emit_identifier(Lexer *lexer)
{
    int start_pos = lexer->current;
   
    while (isalnum(lexer_current(lexer)) || lexer_current(lexer) == '_')
    {
        lexer_advance(lexer);
    }

    Token *token = lexer_emit(lexer, TOKEN_IDENTIFIER, start_pos, lexer->current - start_pos);
    if (token == NULL)
    {
        return NULL;
    }

    for (int i = 0; i < __KW_END__ - __KW_BEG__ - 1; i++)
    {
        if (strcmp(token->raw, token_keyword_map[i].keyword) == 0)
        {
            token->kind = token_keyword_map[i].kind;
            break;
        }
    }

    return token;
}

Token *lexer_emit_number(Lexer *lexer)
{
    int start_pos = lexer->current;
    int base = 10;

    if (lexer_current(lexer) == '0')
    {
        lexer_advance(lexer);
        char next = lexer_current(lexer);
        if (next == 'b' || next == 'B')
        {
            base = 2;
            lexer_advance(lexer);
        }
        else if (next == 'o' || next == 'O')
        {
            base = 8;
            lexer_advance(lexer);
        }
        else if (next == 'x' || next == 'X')
        {
            base = 16;
            lexer_advance(lexer);
        }
        else
        {
            lexer->current = start_pos; // Reset if not a valid base prefix
        }
    }

    bool is_float = false;
    char buffer[MAX_NUMBER_LITERAL_LENGTH] = {0};
    int buffer_pos = 0;

    while (!lexer_at_end(lexer))
    {
        char c = lexer_current(lexer);
        if (c == '_')
        {
            lexer_advance(lexer);
            continue;
        }

        if (c == '.' && base == 10 && !is_float)
        {
            is_float = true;
            buffer[buffer_pos++] = c;
            lexer_advance(lexer);
            continue;
        }

        if ((c >= '0' && c <= '9') ||
            (base == 16 && ((c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))))
        {
            buffer[buffer_pos++] = c;
            lexer_advance(lexer);
        }
        else
        {
            break;
        }

        if (buffer_pos >= MAX_NUMBER_LITERAL_LENGTH)
        {
            return lexer_error(lexer, start_pos, "number literal too long");
        }
    }

    buffer[buffer_pos] = '\0';

    if (buffer_pos == 0)
    {
        return lexer_error(lexer, start_pos, "invalid number literal");
    }

    char *endptr;

    if (is_float)
    {
        double value = strtod(buffer, &endptr);
        if (*endptr != '\0')
        {
            return lexer_error(lexer, start_pos, "invalid float literal");
        }

        Token *token = lexer_emit(lexer, TOKEN_FLOAT, start_pos, lexer->current - start_pos);
        if (token != NULL)
        {
            token->value.f = value;
        }

        return token;
    }

    long long value = strtoll(buffer, &endptr, base);
    if (*endptr != '\0')
    {
        return lexer_error(lexer, start_pos, "invalid integer literal");
    }

    if (value > LLONG_MAX || value < LLONG_MIN)
    {
        return lexer_error(lexer, start_pos, "integer literal out of range");
    }

    Token *token = lexer_emit(lexer, TOKEN_INT, start_pos, lexer->current - start_pos);
    if (token != NULL)
    {
        token->value.i = value;
    }

    return token;
}

Token *lexer_emit_lit_string(Lexer *lexer)
{
    int start_pos = lexer->current;
    lexer_advance(lexer);

    while (!lexer_at_end(lexer) && lexer_current(lexer) != '"')
    {
        char c = lexer_current(lexer);
        if (c == '\\')
        {
            lexer_advance(lexer);
            c = lexer_current(lexer);

            switch (c)
            {
            case 'n': c = '\n'; break;
            case 't': c = '\t'; break;
            case 'r': c = '\r'; break;
            case '0': c = '\0'; break;
            case '\\': c = '\\'; break;
            case '"': c = '"'; break;
            default:
                return lexer_error(lexer, lexer->current, "invalid escape sequence");
            }
        }

        lexer_advance(lexer);
    }

    if (lexer_at_end(lexer))
    {
        return lexer_error(lexer, start_pos, "unterminated string literal");
    }

    lexer_advance(lexer);

    return lexer_emit(lexer, TOKEN_STRING, start_pos, lexer->current - start_pos);
}

Token *lexer_emit_lit_char(Lexer *lexer)
{
    int start_pos = lexer->current;
    lexer_advance(lexer);

    char c = lexer_current(lexer);
    if (c == '\\')
    {
        lexer_advance(lexer);
        c = lexer_current(lexer);

        switch (c)
        {
        case 'n': c = '\n'; break;
        case 't': c = '\t'; break;
        case 'r': c = '\r'; break;
        case '0': c = '\0'; break;
        case '\\': c = '\\'; break;
        case '\'': c = '\''; break;
        default:
            return lexer_error(lexer, lexer->current, "invalid escape sequence");
        }
    }

    lexer_advance(lexer);

    if (lexer_current(lexer) != '\'')
    {
        return lexer_error(lexer, lexer->current, "unterminated char literal");
    }

    lexer_advance(lexer);

    return lexer_emit(lexer, TOKEN_CHARACTER, start_pos, lexer->current - start_pos);
}

Token *lexer_next(Lexer *lexer)
{
    lexer_skip_whitespace(lexer);
    lexer->start = lexer->current;

    if (lexer_at_end(lexer))
    {
        return lexer_emit(lexer, TOKEN_EOF, lexer->current, 0);
    }

    char c = lexer_current(lexer);

    if (isalpha(c) || c == '_')
    {
        return lexer_emit_identifier(lexer);
    }

    if (isdigit(c))
    {
        return lexer_emit_number(lexer);
    }

    int beg = lexer->start;
    int len = 1;
    TokenKind kind = TOKEN_ERROR;

    switch (c)
    {
    case '#':
        while (lexer_current(lexer) != '\n' && !lexer_at_end(lexer))
        {
            lexer_advance(lexer);
        }
        if (!lexer_at_end(lexer))
        {
            lexer_advance(lexer);
        }
        int pos = (lexer->current - lexer->start) - 1; // ignore '\n'
        return lexer_emit(lexer, TOKEN_COMMENT, lexer->start, pos);
    case '\'':
        return lexer_emit_lit_char(lexer);
    case '"':
        return lexer_emit_lit_string(lexer);
    case '(':
        kind = TOKEN_L_PAREN;
        break;
    case ')':
        kind = TOKEN_R_PAREN;
        break;
    case '[':
        kind = TOKEN_L_BRACKET;
        break;
    case ']':
        kind = TOKEN_R_BRACKET;
        break;
    case '{':
        kind = TOKEN_L_BRACE;
        break;
    case '}':
        kind = TOKEN_R_BRACE;
        break;
    case ':':
        switch (lexer_peek(lexer))
        {
        case ':':
            lexer_advance(lexer);
            kind = TOKEN_COLON_COLON;
            len = 2;
            break;
        default:
            kind = TOKEN_COLON;
            break;
        }
        break;
    case ';':
        kind = TOKEN_SEMICOLON;
        break;
    case '?':
        kind = TOKEN_QUESTION;
        break;
    case '@':
        kind = TOKEN_AT;
        break;
    case '.':
        kind = TOKEN_DOT;
        break;
    case ',':
        kind = TOKEN_COMMA;
        break;
    case '+':
        kind = TOKEN_PLUS;
        break;
    case '-':
        kind = TOKEN_MINUS;
        break;
    case '*':
        kind = TOKEN_STAR;
        break;
    case '%':
        kind = TOKEN_PERCENT;
        break;
    case '^':
        kind = TOKEN_CARET;
        break;
    case '&':
        switch (lexer_peek(lexer))
        {
        case '&':
            lexer_advance(lexer);
            kind = TOKEN_AMPERSAND_AMPERSAND;
            len = 2;
            break;
        default:
            kind = TOKEN_AMPERSAND;
            break;
        }
        break;
    case '|':
        switch (lexer_peek(lexer))
        {
        case '|':
            lexer_advance(lexer);
            kind = TOKEN_PIPE_PIPE;
            len = 2;
            break;
        default:
            kind = TOKEN_PIPE;
            break;
        }
        break;
    case '~':
        kind = TOKEN_TILDE;
        break;
    case '<':
        switch (lexer_peek(lexer))
        {
        case '<':
            lexer_advance(lexer);
            kind = TOKEN_LESS_LESS;
            len = 2;
            break;
        case '=':
            lexer_advance(lexer);
            kind = TOKEN_LESS_EQUAL;
            len = 2;
            break;
        default:
            kind = TOKEN_LESS;
            break;
        }
        break;
    case '>':
        switch (lexer_peek(lexer))
        {
        case '>':
            lexer_advance(lexer);
            kind = TOKEN_GREATER_GREATER;
            len = 2;
            break;
        case '=':
            lexer_advance(lexer);
            kind = TOKEN_GREATER_EQUAL;
            len = 2;
            break;
        default:
            kind = TOKEN_GREATER;
            break;
        }
        break;
    case '=':
        switch (lexer_peek(lexer))
        {
        case '=':
            lexer_advance(lexer);
            kind = TOKEN_EQUAL_EQUAL;
            len = 2;
            break;
        default:
            kind = TOKEN_EQUAL;
            break;
        }
        break;
    case '!':
        switch (lexer_peek(lexer))
        {
        case '=':
            lexer_advance(lexer);
            kind = TOKEN_BANG_EQUAL;
            len = 2;
            break;
        default:
            kind = TOKEN_BANG;
            break;
        }
        break;
    case '/':
        kind = TOKEN_SLASH;
        break;
    case '\\':
        kind = TOKEN_BACKSLASH;
        break;
    default:
        kind = TOKEN_ERROR;
        break;
    }

    Token *token;

    if (kind == TOKEN_ERROR)
    {
        char *message = calloc(1, 26);
        if (message == NULL)
        {
            fprintf(stderr, "error: failed to allocate memory for error message\n");
            exit(1);
        }

        snprintf(message, 26, "unexpected character '%c'", c);
        token = lexer_error(lexer, beg, message);
        free(message);
    }
    else {
        token = lexer_emit(lexer, kind, beg, len);
    }

    lexer_advance(lexer);

    return token;
}
