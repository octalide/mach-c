#include "lexer.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

Lexer *lexer_new(char *source)
{
    Lexer *lexer = calloc(1, sizeof(Lexer));
    lexer->source = strdup(source);
    lexer->pos = 0;

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

bool lexer_at_end(Lexer *lexer)
{
    return lexer_current(lexer) == '\0';
}

char lexer_current(Lexer *lexer)
{
    return lexer->source[lexer->pos];
}

char lexer_peek(Lexer *lexer, int offset)
{
    if (lexer->pos + offset >= (int)strlen(lexer->source))
    {
        return '\0';
    }

    return lexer->source[lexer->pos + offset];
}

char lexer_advance(Lexer *lexer)
{
    if (!lexer_at_end(lexer))
    {
        lexer->pos++;
    }

    return lexer_current(lexer);
}

int lexer_get_pos_line(Lexer *lexer, int pos)
{
    int line = 0;
    for (int i = 0; i < pos; i++)
    {
        if (lexer->source[i] == '\n')
        {
            line++;
        }
    }

    return line;
}

int lexer_get_pos_line_offset(Lexer *lexer, int pos)
{
    int offset = 0;
    for (int i = 0; i < pos; i++)
    {
        if (lexer->source[i] == '\n')
        {
            offset = 0;
        }

        offset++;
    }

    return offset;
}

char *lexer_get_line_text(Lexer *lexer, int line)
{
    int line_start = 0;
    int current_line = 0;

    while (current_line < line && lexer->source[line_start] != '\0')
    {
        if (lexer->source[line_start] == '\n')
        {
            current_line++;
        }
        line_start++;
    }

    if (lexer->source[line_start] == '\0')
    {
        return strdup("");
    }

    int line_end = line_start;
    while (lexer->source[line_end] != '\n' && lexer->source[line_end] != '\0')
    {
        line_end++;
    }

    int line_len = line_end - line_start;

    char *line_text = calloc(line_len + 1, sizeof(char));
    strncpy(line_text, lexer->source + line_start, line_len);

    return line_text;
}

void lexer_skip_whitespace(Lexer *lexer)
{
    while (!lexer_at_end(lexer) && isspace(lexer_current(lexer)))
    {
        lexer_advance(lexer);
    }
}

// identifier formatting rules:
// - only alphanumeric characters and underscores are acceptable
Token *lexer_parse_identifier(Lexer *lexer)
{
    int start = lexer->pos;

    while (!lexer_at_end(lexer) && (isalnum(lexer_current(lexer)) || lexer_current(lexer) == '_'))
    {
        lexer_advance(lexer);
    }

    return token_new(TOKEN_IDENTIFIER, start, lexer->pos - start);
}

// integer formatting rules:
// - only digits and underscores are available
// floating point formatting rules:
// - only digits, underscores, and a single period are available
// integer prefix rules:
// - `0x` or `0X` for hexadecimal
// - `0b` or `0B` for binary
// - `0o` or `0O` for octal
// - integers following a prefix must be within the range of the prefix
// - numbers with a prefix cannot be floats
// returns TOKEN_LIT_INT and TOKEN_LIT_FLOAT depending on what number kind is
//   found.
Token *lexer_parse_lit_number(Lexer *lexer)
{
    int start = lexer->pos;

    if (lexer_current(lexer) == '0')
    {
        int base = 10;

        switch (lexer_peek(lexer, 1))
        {
        case 'x':
        case 'X':
            base = 16;
            lexer_advance(lexer);
            lexer_advance(lexer);
            break;
        case 'b':
        case 'B':
            base = 2;
            lexer_advance(lexer);
            lexer_advance(lexer);
            break;
        case 'o':
        case 'O':
            base = 8;
            lexer_advance(lexer);
            lexer_advance(lexer);
            break;
        }

        switch (base)
        {
        case 2:
            while (!lexer_at_end(lexer) && ((lexer_current(lexer) == '0' || lexer_current(lexer) == '1') || lexer_current(lexer) == '_'))
            {
                lexer_advance(lexer);
            }
            return token_new(TOKEN_LIT_INT, start, lexer->pos - start);
        case 8:
            while (!lexer_at_end(lexer) && ((lexer_current(lexer) >= '0' && lexer_current(lexer) <= '7') || lexer_current(lexer) == '_'))
            {
                lexer_advance(lexer);
            }
            return token_new(TOKEN_LIT_INT, start, lexer->pos - start);
        case 16:
            while (!lexer_at_end(lexer) && (isxdigit(lexer_current(lexer)) || lexer_current(lexer) == '_'))
            {
                lexer_advance(lexer);
            }
            return token_new(TOKEN_LIT_INT, start, lexer->pos - start);
        default:
            lexer_advance(lexer);
            break;
        }
    }

    while (!lexer_at_end(lexer) && (isdigit(lexer_current(lexer)) || lexer_current(lexer) == '_'))
    {
        lexer_advance(lexer);
    }

    if (lexer_current(lexer) == '.')
    {
        lexer_advance(lexer);

        while (!lexer_at_end(lexer) && (isdigit(lexer_current(lexer)) || lexer_current(lexer) == '_'))
        {
            lexer_advance(lexer);
        }

        return token_new(TOKEN_LIT_FLOAT, start, lexer->pos - start);
    }

    return token_new(TOKEN_LIT_INT, start, lexer->pos - start);
}

// char formatting rules:
// - must be surrounded by single quotes
// - only one character is allowed
// the following escape sequences are allowed:
// - `\'` for single quote
// - `\"` for double quote
// - `\\` for backslash
// - `\n` for newline
// - `\t` for tab
// - `\r` for carriage return
// - `\0` for null
Token *lexer_parse_lit_char(Lexer *lexer)
{
    int start = lexer->pos;

    if (lexer_current(lexer) != '\'')
    {
        return token_new(TOKEN_ERROR, start, 1);
    }

    lexer_advance(lexer);

    if (lexer_at_end(lexer))
    {
        return token_new(TOKEN_ERROR, start, 1);
    }

    if (lexer_current(lexer) == '\\')
    {
        lexer_advance(lexer);

        switch (lexer_current(lexer))
        {
        case '\'':
        case '\"':
        case '\\':
        case 'n':
        case 't':
        case 'r':
        case '0':
            lexer_advance(lexer);
            break;
        default:
            return token_new(TOKEN_ERROR, start, lexer->pos - start);
        }
    }
    else
    {
        lexer_advance(lexer);
    }

    if (lexer_at_end(lexer) || lexer_current(lexer) != '\'')
    {
        return token_new(TOKEN_ERROR, start, lexer->pos - start);
    }

    lexer_advance(lexer);

    return token_new(TOKEN_LIT_CHAR, start, lexer->pos - start);
}

// string formatting rules:
// - must be surrounded by double quotes
// - any character is allowed
// the following escape sequences are allowed:
// - `\'` for single quote
// - `\"` for double quote
// - `\\` for backslash
// - `\n` for newline
// - `\t` for tab
// - `\r` for carriage return
// - `\0` for null
Token *lexer_parse_lit_string(Lexer *lexer)
{
    int start = lexer->pos;

    if (lexer_current(lexer) != '\"')
    {
        return token_new(TOKEN_ERROR, start, 1);
    }

    lexer_advance(lexer);

    while (!lexer_at_end(lexer) && lexer_current(lexer) != '\"')
    {
        if (lexer_current(lexer) == '\\')
        {
            lexer_advance(lexer);

            switch (lexer_current(lexer))
            {
            case '\'':
            case '\"':
            case '\\':
            case 'n':
            case 't':
            case 'r':
            case '0':
                lexer_advance(lexer);
                break;
            default:
                return token_new(TOKEN_ERROR, start, lexer->pos - start);
            }
        }
        else
        {
            lexer_advance(lexer);
        }
    }

    if (lexer_at_end(lexer) || lexer_current(lexer) != '\"')
    {
        return token_new(TOKEN_ERROR, start, lexer->pos - start);
    }

    lexer_advance(lexer);

    return token_new(TOKEN_LIT_STRING, start, lexer->pos - start);
}

unsigned long long lexer_eval_lit_int(Lexer *lexer, Token *token)
{
    int base = 10;
    int skip = 0;
    char *end = NULL;

    if (lexer->source[token->pos] == '0')
    {
        switch (lexer->source[token->pos + 1])
        {
        case 'x':
        case 'X':
            base = 16;
            skip = 2;
            break;
        case 'b':
        case 'B':
            base = 2;
            skip = 2;
            break;
        case 'o':
        case 'O':
            base = 8;
            skip = 2;
            break;
        default:
            base = 10;
            break;
        }
    }

    char *cleaned = malloc(token->len + 1);
    int j = 0;
    for (int i = 0; i < token->len; i++)
    {
        if (lexer->source[token->pos + i] != '_')
        {
            cleaned[j++] = lexer->source[token->pos + i];
        }
    }
    cleaned[j] = '\0';

    long long value = strtoull(cleaned + skip, &end, base);
    free(cleaned);

    return value;
}

double lexer_eval_lit_float(Lexer *lexer, Token *token)
{
    char *end = NULL;

    char *cleaned = malloc(token->len + 1);
    int j = 0;
    for (int i = 0; i < token->len; i++)
    {
        if (lexer->source[token->pos + i] != '_')
        {
            cleaned[j++] = lexer->source[token->pos + i];
        }
    }
    cleaned[j] = '\0';

    double value = strtod(cleaned, &end);
    free(cleaned);

    return value;
}

char lexer_eval_lit_char(Lexer *lexer, Token *token)
{
    char value = lexer->source[token->pos + 1];

    if (lexer->source[token->pos + 1] == '\\')
    {
        switch (lexer->source[token->pos + 2])
        {
        case '\'':
            value = '\'';
            break;
        case '\"':
            value = '\"';
            break;
        case '\\':
            value = '\\';
            break;
        case 'n':
            value = '\n';
            break;
        case 't':
            value = '\t';
            break;
        case 'r':
            value = '\r';
            break;
        case '0':
            value = '\0';
            break;
        }
    }

    return value;
}

char *lexer_eval_lit_string(Lexer *lexer, Token *token)
{
    char *value = calloc(token->len - 1, sizeof(char));

    for (int i = 0; i < token->len - 2; i++)
    {
        if (lexer->source[token->pos + i + 1] == '\\')
        {
            switch (lexer->source[token->pos + i + 2])
            {
            case '\'':
                value[i] = '\'';
                break;
            case '\"':
                value[i] = '\"';
                break;
            case '\\':
                value[i] = '\\';
                break;
            case 'n':
                value[i] = '\n';
                break;
            case 't':
                value[i] = '\t';
                break;
            case 'r':
                value[i] = '\r';
                break;
            case '0':
                value[i] = '\0';
                break;
            }
        }
        else
        {
            value[i] = lexer->source[token->pos + i + 1];
        }
    }

    return value;
}

char *lexer_raw_value(Lexer *lexer, Token *token)
{
    char *value = calloc(token->len + 1, sizeof(char));
    for (int i = 0; i < token->len; i++)
    {
        value[i] = lexer->source[token->pos + i];
    }

    return value;
}

Token *lexer_emit(Lexer *lexer, TokenKind kind, int len)
{
    Token *token = token_new(kind, lexer->pos, len);
    for (int i = 0; i < len; i++)
    {
        lexer_advance(lexer);
    }

    return token;
}

Token *lexer_next(Lexer *lexer)
{
    lexer_skip_whitespace(lexer);

    switch (lexer_current(lexer))
    {
    case '#':;
        int start = lexer->pos;
        while (!lexer_at_end(lexer) && lexer_current(lexer) != '\n')
        {
            lexer_advance(lexer);
        }
        return token_new(TOKEN_COMMENT, start, lexer->pos - start);
    case 'a' ... 'z':
    case 'A' ... 'Z':
    case '_':
        return lexer_parse_identifier(lexer);
    case '0' ... '9':
        return lexer_parse_lit_number(lexer);
    case '\'':
        return lexer_parse_lit_char(lexer);
    case '\"':
        return lexer_parse_lit_string(lexer);
    case '(':
        return lexer_emit(lexer, TOKEN_L_PAREN, 1);
    case ')':
        return lexer_emit(lexer, TOKEN_R_PAREN, 1);
    case '[':
        return lexer_emit(lexer, TOKEN_L_BRACKET, 1);
    case ']':
        return lexer_emit(lexer, TOKEN_R_BRACKET, 1);
    case '{':
        return lexer_emit(lexer, TOKEN_L_BRACE, 1);
    case '}':
        return lexer_emit(lexer, TOKEN_R_BRACE, 1);
    case ':':
        switch (lexer_peek(lexer, 1))
        {
        case ':':
            return lexer_emit(lexer, TOKEN_COLON_COLON, 2);
        default:
            return lexer_emit(lexer, TOKEN_COLON, 1);
        }
        break;
    case ';':
        return lexer_emit(lexer, TOKEN_SEMICOLON, 1);
    case '?':
        return lexer_emit(lexer, TOKEN_QUESTION, 1);
    case '@':
        return lexer_emit(lexer, TOKEN_AT, 1);
    case '.':
        return lexer_emit(lexer, TOKEN_DOT, 1);
    case ',':
        return lexer_emit(lexer, TOKEN_COMMA, 1);
    case '+':
        return lexer_emit(lexer, TOKEN_PLUS, 1);
    case '-':
        return lexer_emit(lexer, TOKEN_MINUS, 1);
    case '*':
        return lexer_emit(lexer, TOKEN_STAR, 1);
    case '%':
        return lexer_emit(lexer, TOKEN_PERCENT, 1);
    case '^':
        return lexer_emit(lexer, TOKEN_CARET, 1);
    case '&':
        switch (lexer_peek(lexer, 1))
        {
        case '&':
            return lexer_emit(lexer, TOKEN_AMPERSAND_AMPERSAND, 2);
        default:
            return lexer_emit(lexer, TOKEN_AMPERSAND, 1);
        }
        break;
    case '|':
        switch (lexer_peek(lexer, 1))
        {
        case '|':
            return lexer_emit(lexer, TOKEN_PIPE_PIPE, 2);
        default:
            return lexer_emit(lexer, TOKEN_PIPE, 1);
        }
        break;
    case '~':
        return lexer_emit(lexer, TOKEN_TILDE, 1);
    case '<':
        switch (lexer_peek(lexer, 1))
        {
        case '<':
            return lexer_emit(lexer, TOKEN_LESS_LESS, 2);
        case '=':
            return lexer_emit(lexer, TOKEN_LESS_EQUAL, 2);
        default:
            return lexer_emit(lexer, TOKEN_LESS, 1);
        }
        break;
    case '>':
        switch (lexer_peek(lexer, 1))
        {
        case '>':
            return lexer_emit(lexer, TOKEN_GREATER_GREATER, 2);
        case '=':
            return lexer_emit(lexer, TOKEN_GREATER_EQUAL, 2);
        default:
            return lexer_emit(lexer, TOKEN_GREATER, 1);
        }
        break;
    case '=':
        switch (lexer_peek(lexer, 1))
        {
        case '=':
            return lexer_emit(lexer, TOKEN_EQUAL_EQUAL, 2);
        default:
            return lexer_emit(lexer, TOKEN_EQUAL, 1);
        }
        break;
    case '!':
        switch (lexer_peek(lexer, 1))
        {
        case '=':
            return lexer_emit(lexer, TOKEN_BANG_EQUAL, 2);
        default:
            return lexer_emit(lexer, TOKEN_BANG, 1);
        }
        break;
    case '/':
        return lexer_emit(lexer, TOKEN_SLASH, 1);
    case '\\':
        return lexer_emit(lexer, TOKEN_BACKSLASH, 1);
    case '\0':
        return lexer_emit(lexer, TOKEN_EOF, 1);
    default:
        return lexer_emit(lexer, TOKEN_ERROR, 1);
    }
}
