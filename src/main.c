#include "ast.h"
#include "lexer.h"
#include "parser.h"
#include "sema.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(const char *program_name)
{
    fprintf(stderr, "Usage: %s <command> [options]\n", program_name);
    fprintf(stderr, "Commands:\n");
    fprintf(stderr, "  build <file>    Build a Mach source file\n");
    fprintf(stderr, "  help            Show this help message\n");
}

static char *read_file(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (!file)
    {
        fprintf(stderr, "Error: Could not open file '%s'\n", path);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *buffer = malloc(size + 1);
    if (!buffer)
    {
        fprintf(stderr, "Error: Could not allocate memory for file\n");
        fclose(file);
        return NULL;
    }

    size_t read  = fread(buffer, 1, size, file);
    buffer[read] = '\0';
    fclose(file);

    return buffer;
}

static int build_command(const char *filename)
{
    // read source file
    char *source = read_file(filename);
    if (!source)
    {
        return 1;
    }

    // initialize lexer
    Lexer lexer;
    lexer_init(&lexer, source);

    // initialize parser
    Parser parser;
    parser_init(&parser, &lexer);

    // parse program
    AstNode *program = parser_parse_program(&parser);

    // check for parse errors
    if (parser.had_error)
    {
        fprintf(stderr, "\nParsing failed with %d error(s):\n", parser.errors.count);
        parser_error_list_print(&parser.errors, &lexer, filename);

        // cleanup
        ast_node_dnit(program);
        free(program);
        parser_dnit(&parser);
        lexer_dnit(&lexer);
        free(source);

        return 1;
    }

    printf("Parsing successful!\n");

    // semantic analysis
    Sema sema;
    sema_init(&sema);

    bool sema_success = sema_analyze(&sema, program);

    if (!sema_success)
    {
        fprintf(stderr, "\nSemantic analysis failed with %d error(s):\n", sema.errors.count);
        sema_error_list_print(&sema.errors, &lexer);

        // cleanup
        sema_dnit(&sema);
        ast_node_dnit(program);
        free(program);
        parser_dnit(&parser);
        lexer_dnit(&lexer);
        free(source);

        return 1;
    }

    printf("Semantic analysis successful!\n\n");
    printf("AST:\n");
    ast_print(program, 0);

    // cleanup
    sema_dnit(&sema);
    ast_node_dnit(program);
    free(program);
    parser_dnit(&parser);
    lexer_dnit(&lexer);
    free(source);

    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        print_usage(argv[0]);
        return 1;
    }

    const char *command = argv[1];

    if (strcmp(command, "help") == 0)
    {
        print_usage(argv[0]);
        return 0;
    }
    else if (strcmp(command, "build") == 0)
    {
        if (argc < 3)
        {
            fprintf(stderr, "Error: 'build' command requires a filename\n");
            print_usage(argv[0]);
            return 1;
        }
        return build_command(argv[2]);
    }
    else
    {
        fprintf(stderr, "Error: Unknown command '%s'\n", command);
        print_usage(argv[0]);
        return 1;
    }
}
