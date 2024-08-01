#include "context.h"
#include "file.h"
#include "lexer.h"

#include <stdlib.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
    int verbosity = VERBOSITY_LOW;
    for (int i = 1; i < argc; i++)
    {
        // set verbosity based on -v argument where verbosity is set from 0-3 based
        //   on the number of 'v's in the flag
        // NOTE: using just `-v` silences any non-error output
        if (argv[i][0] == '-' && argv[i][1] == 'v')
        {
            verbosity = VERBOSITY_NONE;
            for (int j = 2; argv[i][j] == 'v'; j++)
            {
                if (verbosity >= VERBOSITY_HIGH)
                {
                    break;
                }

                verbosity++;
            }
        }
    }

    char *path_target = NULL;
    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] != '-')
        {
            path_target = argv[i];
            break;
        }
    }

    switch (verbosity)
    {
    case VERBOSITY_NONE:
        break;
    case VERBOSITY_LOW:
        printf("verbosity set to low\n");
        break;
    case VERBOSITY_MEDIUM:
        printf("verbosity set to medium\n");
        break;
    case VERBOSITY_HIGH:
        printf("verbosity set to high\n");
        break;
    }

    Context *context = context_new();
    context->verbosity = verbosity;

    File *file = file_new(path_target);
    if (file == NULL)
    {
        fprintf(stderr, "error: failed to read file\n");
        context_free(context);
        return 1;
    }

    Lexer *lexer = lexer_new(file->source);
    while (!lexer_at_end(lexer))
    {
        int index = lexer_next(lexer);

        if (context->verbosity < VERBOSITY_HIGH)
        {
            continue;
        }

        Token token = token_list_get(lexer->token_list, index);
        printf("kind: %20s lexeme: `", token_kind_to_string(token.kind));

        for (int i = 0; i < token.len; i++)
        {
            printf("%c", lexer->source[token.pos + i]);
        }

        printf("`");

        switch (token.kind)
        {
        case TOKEN_LIT_INT:
            int ival = lexer_eval_lit_int(lexer, token);
            printf(" value: `%d`", ival);
            break;
        case TOKEN_LIT_FLOAT:
            float fval = lexer_eval_lit_float(lexer, token);
            printf(" value: `%f`", fval);
            break;
        case TOKEN_LIT_CHAR:
            char cval = lexer_eval_lit_char(lexer, token);
            printf(" value: `%c`", cval);
            break;
        case TOKEN_LIT_STRING:
            char *sval = lexer_eval_lit_string(lexer, token);
            printf(" value: `%s`", sval);
            free(sval);
            break;
        default:
            break;
        }

        printf("\n");
    }

    lexer_free(lexer);
    lexer = NULL;
    file_free(file);
    file = NULL;
    context_free(context);
    context = NULL;

    return 0;
}
