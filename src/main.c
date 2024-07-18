#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include "lexer.h"
#include "parser.h"

void lexer_debug_print(Lexer *lexer)
{
    Token token;
    do
    {
        token = next_token(lexer);

        // convert token type to readable string
        // fill with spaces up to 19 chars
        char *type = token_type_string(token.type);
        char type_str[19];
        for (int i = 0; i < 21; i++)
        {
            type_str[i] = ' ';
        }
        for (int i = 0; i < 21 && type[i] != '\0'; i++)
        {
            type_str[i] = type[i];
        }

        // format:
        // ln: <line> type: <type> len: <length> val: <value>
        printf("ln: %d type: %s len: %d val: %.*s\n", lexer->line, type_str, token.length, token.length, token.start);
    } while (token.type != TOKEN_EOF);
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <source file>\n", argv[0]);
        return 1;
    }

    FILE *file = fopen(argv[1], "r");
    if (!file)
    {
        fprintf(stderr, "Could not open file %s\n", argv[1]);
        return 1;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *source = malloc(length + 1);
    fread(source, 1, length, file);
    source[length] = '\0';
    fclose(file);

    Lexer lexer;
    lexer_init(&lexer, source);

    // lexer_debug_print(&lexer);

    Parser parser;
    init_parser(&parser, &lexer);

    NodeProgram *program = parse(&parser);

    free(source);
    return 0;
}
