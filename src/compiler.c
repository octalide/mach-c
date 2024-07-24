#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "compiler.h"
#include "ioutil.h"

void compiler_init(Compiler *compiler, Options *options)
{
    compiler->options = options;
    compiler->analyzer = malloc(sizeof(SemanticAnalyzer));
    compiler->source_files = NULL;
    compiler->file_count = 0;
    compiler->program = NULL;
    compiler->error = NULL;
}

void compiler_free(Compiler *compiler)
{
    for (int i = 0; i < compiler->file_count; i++)
    {
        free((char *)compiler->source_files[i].filename);
        free((char *)compiler->source_files[i].source);

        node_free(compiler->source_files[i].root);
        parser_free(compiler->source_files[i].parser);
        free(compiler->source_files[i].parser);
        free(compiler->source_files[i].root);
    }

    node_free(compiler->program);
    semantic_analyzer_free(compiler->analyzer);
    free(compiler->program);
    free(compiler->analyzer);
}

void compiler_error(Compiler *compiler, const char *message)
{
    char *copy = malloc(strlen(message) + 1);
    strcpy(copy, message);
    compiler->error = copy;
}

bool compiler_has_error(Compiler *compiler)
{
    return compiler->error != NULL;
}

void compiler_add_file(Compiler *compiler, const char *path)
{
    FILE *file = fopen(path, "r");
    if (!file)
    {
        char message[256];
        sprintf(message, "failed to open file %s", path);
        compiler_error(compiler, message);
        return;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *source = malloc(length + 1);
    fread(source, 1, length, file);
    source[length] = '\0';
    fclose(file);

    SourceFile *source_file = malloc(sizeof(SourceFile));
    source_file->filename = path;
    source_file->source = source;
    source_file->root = NULL;
    source_file->lexer = NULL;
    source_file->parser = NULL;

    SourceFile *temp = realloc(compiler->source_files, (compiler->file_count + 1) * sizeof(SourceFile));
    if (!temp)
    {
        compiler_error(compiler, "failed to allocate memory for source files");
        return;
    }
    compiler->source_files = temp;
    compiler->source_files[compiler->file_count++] = *source_file;
}

void compiler_parse_source_file(Compiler *compiler, SourceFile *source_file)
{
    source_file->lexer = malloc(sizeof(Lexer));
    if (!source_file->lexer)
    {
        compiler_error(compiler, "failed to allocate memory for lexer");
        return;
    }

    source_file->parser = malloc(sizeof(Parser));
    if (!source_file->parser)
    {
        compiler_error(compiler, "failed to allocate memory for parser");
        return;
    }

    lexer_init(source_file->lexer, source_file->source);
    parser_init(source_file->parser, source_file->lexer);

    source_file->root = parse(source_file->parser);
    if (source_file->root == NULL)
    {
        int err_line_index = lexer_get_token_line_index(source_file->lexer, &source_file->parser->error->token);
        int err_char_index = lexer_get_token_line_char_index(source_file->lexer, &source_file->parser->error->token);
        const char *filename = source_file->filename;

        char message[256];
        sprintf(message, "%s:%d:%d: error: %s", filename, err_line_index, err_char_index, source_file->parser->error->message);
        compiler_error(compiler, message);
        return;
    }

    return;
}

void compiler_analyze_source_file(Compiler *compiler, SourceFile *source_file)
{
    // TODO
    (void)compiler;
    (void)source_file;
}

// compiler_prepare performs the following tasks:
// 1. checks options for errors
// 2. checks that dependencies are installed
// 3. checks that the program entrypoint file exists and adds it to the list of source files
void compiler_prepare(Compiler *compiler)
{
    if (compiler->options->mach_installation == NULL)
    {
        compiler_error(compiler, "mach installation path not set");
    }

    if (compiler->options->path_entry == NULL)
    {
        compiler_error(compiler, "program entrypoint not set");
    }

    if (compiler->options->path_entry != NULL && !file_exists(compiler->options->path_entry))
    {
        char message[256];
        sprintf(message, "program entrypoint %s does not exist", compiler->options->path_entry);
        compiler_error(compiler, message);
    }

    if (compiler_has_error(compiler))
    {
        return;
    }

    compiler_add_file(compiler, compiler->options->path_entry);
}

void compiler_pass_import(Compiler *compiler)
{
    for (int i = 0; i < compiler->file_count; i++)
    {
        compiler_parse_source_file(compiler, &compiler->source_files[i]);
        if (compiler_has_error(compiler))
        {
            printf("%s\n", compiler->error);
            parser_print_error(compiler->source_files[i].parser);
            compiler->error = NULL;
            return;
        }

        compiler_analyze_source_file(compiler, &compiler->source_files[i]);
        if (compiler_has_error(compiler))
        {
            printf("%s\n", compiler->error);
            compiler->error = NULL;
            return;
        }

        if (!compiler->source_files[i].root)
        {
            return;
        }

        // collect use statements from the root node (only allowed at root level)
        switch (compiler->source_files[i].root->type)
        {
        case NODE_BLOCK:
            Node *block = compiler->source_files[i].root;
            for (int j = 0; j < node_list_length(block->data.node_block.statements); j++)
            {
                Node *statement = block->data.node_block.statements[j];
                if (statement->type != NODE_STMT_USE)
                {
                    continue;
                }

                if (statement->data.stmt_use.path == NULL)
                {
                    char message[256];
                    sprintf(message, "use statement in source file %s has no path", compiler->source_files[i].filename);
                    compiler_error(compiler, message);
                    return;
                }

                // extract path from token
                Token path_tok = statement->data.stmt_use.path->data.expr_lit_string.value;
                char *path_lit = malloc(path_tok.length);
                strncpy(path_lit, path_tok.start, path_tok.length);

                // only keep everything between double quotes in the string lit
                path_lit[(int)path_tok.length] = '\0';
                path_lit++;
                path_lit[(int)path_tok.length - 2] = '\0';

                // add ".mach" to path and make sure it's terminated with '\0'
                char *path = malloc(strlen(path_lit) + 5);
                strcpy(path, path_lit);
                strcat(path, ".mach");
                path[strlen(path)] = '\0';

                // create and check two paths:
                // - one that starts in the project src directory
                // - one that starts in the project dep directory as a fallback
                char *path_src = malloc(strlen(compiler->options->path_src) + strlen(path) + 2);
                strcpy(path_src, compiler->options->path_src);
                strcat(path_src, "/");
                strcat(path_src, path);

                char *path_dep = malloc(strlen(compiler->options->path_dep) + strlen(path) + 2);
                strcpy(path_dep, compiler->options->path_dep);
                strcat(path_dep, "/");
                strcat(path_dep, path);

                if (file_exists(path_src) && !is_directory(path_src))
                {
                    compiler_add_file(compiler, path_src);
                    if (compiler_has_error(compiler))
                    {
                        printf("%s\n", compiler->error);
                        compiler->error = NULL;
                        return;
                    }
                }
                else if (file_exists(path_dep) && !is_directory(path_dep))
                {
                    compiler_add_file(compiler, path_dep);
                    if (compiler_has_error(compiler))
                    {
                        printf("%s\n", compiler->error);
                        compiler->error = NULL;
                        return;
                    }
                }
                else
                {
                    char message[PATH_MAX];
                    sprintf(message, "unable to locate source file specified by use statement: %s", path_lit);
                    compiler_error(compiler, message);
                    return;
                }
            }
            break;
        default:
            char message[256];
            sprintf(message, "unexpected root node type in source file %s", compiler->source_files[i].filename);
            compiler_error(compiler, message);
            return;
        }

        // this is guaranteed to be our entry program as the compiler only starts
        // with knowledge of one file. If that file does not parse correctly,
        // no other files will be parsed.
        // seed the program AST with the first file's AST
        if (!compiler->program)
        {
            compiler->program = compiler->source_files[i].root;
        }
    }
}

// compiler_compile performs the following tasks:
// 1. prepares the compiler
// 2. parses each file, starting with the program entrypoint (set during prep)
//   1. initial parse
//   2. initial semantic analysis, which leaves out unknown identifiers
//   3. processes `use` statements and adds the target to the list of source files
//   4. recurses until no files have unresolved imports
// 3. combines each file's AST into a single program AST
// 4. performs semantic analysis on the entire program AST
void compiler_compile(Compiler *compiler)
{
    compiler_prepare(compiler);
    if (compiler_has_error(compiler))
    {
        return;
    }

    int last_file_count = 0;
    while (true)
    {
        compiler_pass_import(compiler);
        if (compiler_has_error(compiler))
        {
            return;
        }

        last_file_count = compiler->file_count;

        // check that all files have been parsed
        bool all_parsed = true;
        for (int i = 0; i < compiler->file_count; i++)
        {
            if (!compiler->source_files[i].root)
            {
                all_parsed = false;
                break;
            }
        }

        if (all_parsed)
        {
            break;
        }

        if (compiler->file_count == last_file_count)
        {
            // NOTE: this is an emergency case to break out of an infinite loop.
            // There are no circumstances in which the compiler should
            //   successfully parse and analyze only some files without an error
            //   from either step. This is to handle that impossible scenario.
            // Seeing this in the console gets you a gold star.
            compiler_error(compiler, "failed to resolve all imports");
            return;
        }
    }

    if (compiler_has_error(compiler))
    {
        return;
    }

    printf("compiled successfully\n");
}
