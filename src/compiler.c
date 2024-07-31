#include <time.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>

#include "ioutil.h"
#include "visitor.h"
#include "compiler.h"

Compiler *compiler_new(Options *options)
{
    Compiler *compiler = calloc(1, sizeof(Compiler));
    compiler->options = options;

    compiler->source_files = calloc(1, sizeof(SourceFile *));
    compiler->source_files[0] = NULL;

    compiler->program = NULL;
    compiler->analyzer = NULL;

    compiler->should_abort = false;

    return compiler;
}

void compiler_free(Compiler *compiler)
{
    if (compiler == NULL)
    {
        return;
    }

    for (int i = 0; compiler->source_files[i] != NULL; i++)
    {
        source_file_free(compiler->source_files[i]);
        compiler->source_files[i] = NULL;
    }

    free(compiler->source_files);
    compiler->source_files = NULL;

    // NOTE:
    // The compiler "program" is an AST that is made up of all the ASTs from
    //   the source files, which have all just been freed. Attempting to use
    //   `node_free(compiler->program)` will result in a double free as the
    //   AST does not own a single pointer used in its makeup. Just free head.
    free(compiler->program);
    compiler->program = NULL;

    analyzer_free(compiler->analyzer);
    compiler->analyzer = NULL;

    free(compiler);
    compiler = NULL;
}

void compiler_source_file_add(Compiler *compiler, char *path)
{
    // check if source file has already been added (avoids self and circular
    //   imports that would lead to infinite recursion)
    for (int i = 0; compiler->source_files[i] != NULL; i++)
    {
        if (strcmp(compiler->source_files[i]->path, path) == 0)
        {
            printf("[ COMP ] source file already added: %s\n", path);
            return;
        }
    }

    FILE *file = fopen(path, "r");
    if (!file)
    {
        printf("[ COMP ] could not open source file: %s\n", path);
        return;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *source = malloc(length + 1);
    fread(source, 1, length, file);
    source[length] = '\0';
    fclose(file);

    SourceFile *source_file = source_file_new(path, source);
    source_file->parser->opt_verbose = compiler->options->verbose_parse;

    int i = 0;
    while (compiler->source_files[i] != NULL)
    {
        i++;
    }

    compiler->source_files = realloc(compiler->source_files, (i + 2) * sizeof(SourceFile));
    compiler->source_files[i] = source_file;
    compiler->source_files[i + 1] = NULL;
}

// compiler_stage_prepare performs the following tasks:
// 1. checks options for errors
// 2. checks that dependencies are installed
// 3. checks that the program entrypoint file exists and adds it to the list of source files
void compiler_stage_prepare(Compiler *compiler)
{
    if (compiler->options->verbose_compile)
    {
        printf("[ COMP ] preparing compiler...\n");
    }

    if (compiler->options->mach_installation == NULL)
    {
        printf("[ COMP ] | mach installation path not set\n");
        compiler->should_abort = true;
        return;
    }

    if (compiler->options->path_project == NULL)
    {
        printf("[ COMP ] | project path not set\n");
        compiler->should_abort = true;
        return;
    }

    if (compiler->options->path_src == NULL)
    {
        printf("[ COMP ] | source path not set\n");
        compiler->should_abort = true;
        return;
    }

    if (compiler->options->path_dep == NULL)
    {
        printf("[ COMP ] | dependancy path not set\n");
        compiler->should_abort = true;
        return;
    }

    if (compiler->options->path_out == NULL)
    {
        printf("[ COMP ] | output path not set\n");
        compiler->should_abort = true;
        return;
    }

    if (compiler->options->path_entry == NULL)
    {
        printf("[ COMP ] | program entrypoint not set\n");
        compiler->should_abort = true;
        return;
    }

    if (compiler->options->target_count == 0)
    {
        printf("[ COMP ] | no targets set\n");
        compiler->should_abort = true;
        return;
    }

    if (compiler->options->verbose_compile)
    {
        printf("[ COMP ] | - mach installation:  %s\n", compiler->options->mach_installation);
        printf("[ COMP ] | - path_project:       %s\n", compiler->options->path_project);
        printf("[ COMP ] | - path_src:           %s\n", compiler->options->path_src);
        printf("[ COMP ] | - path_dep:           %s\n", compiler->options->path_dep);
        printf("[ COMP ] | - path_out:           %s\n", compiler->options->path_out);
        printf("[ COMP ] | - path_entry:         %s\n", compiler->options->path_entry);
        printf("[ COMP ] | - targets:\n");

        for (int i = 0; i < compiler->options->target_count; i++)
        {
            printf("[ COMP ] | | - %s\n", target_to_string(compiler->options->targets[i]));
        }
    }

    if (compiler->options->path_entry == NULL)
    {
        printf("[ COMP ] | program entrypoint not set\n");
        compiler->should_abort = true;
        return;
    }

    if (!path_is_absolute(compiler->options->path_project))
    {
        printf("[ COMP ] | project path is not absolute\n");
        compiler->should_abort = true;
        return;
    }

    if (!path_is_absolute(compiler->options->path_src))
    {
        printf("[ COMP ] | source path is not absolute\n");
        compiler->should_abort = true;
        return;
    }

    if (!path_is_absolute(compiler->options->path_dep))
    {
        printf("[ COMP ] | dependancy path is not absolute\n");
        compiler->should_abort = true;
        return;
    }

    if (!path_is_absolute(compiler->options->path_out))
    {
        printf("[ COMP ] | output path is not absolute\n");
        compiler->should_abort = true;
        return;
    }

    if (!path_is_absolute(compiler->options->path_entry))
    {
        printf("[ COMP ] | entrypoint path is not absolute\n");
        compiler->should_abort = true;
        return;
    }

    if (!file_exists(compiler->options->path_entry))
    {
        printf("[ COMP ] | program entrypoint does not exist: %s\n", compiler->options->path_entry);
        compiler->should_abort = true;
        return;
    }

    compiler_source_file_add(compiler, compiler->options->path_entry);
}

void compiler_visit_use(void *context, Node *node)
{
    if (context == NULL)
    {
        printf("[ COMP ] error: visitor context not set\n");
        return;
    }

    Compiler *compiler = ((CompilerVisitorContext *)context)->compiler;
    SourceFile *source_file = ((CompilerVisitorContext *)context)->source_file;

    if (compiler == NULL)
    {
        printf("[ COMP ] error: visitor context compiler not set\n");
        return;
    }

    if (source_file == NULL)
    {
        printf("[ COMP ] error: visitor context source file not set\n");
        return;
    }

    char *path_file = source_file->path;
    char *path_use = strdup(node->stmt_use.path->token->raw);

    // remove surrounding quotes (included in string literal tokens)
    path_use++;
    path_use[strlen(path_use) - 1] = '\0';

    // append ".mach"
    char *path_use_with_ext = malloc(strlen(path_use) + 6);
    strcpy(path_use_with_ext, path_use);
    strcat(path_use_with_ext, ".mach");

    char *dir_file = path_dirname(path_file);

    char *path_rel = path_join(dir_file, path_use_with_ext);
    char *path_dep = path_join(compiler->options->path_dep, path_use_with_ext);

    if (file_exists(path_rel))
    {
        if (compiler->options->verbose_compile)
        {
            printf("[ COMP ] | | | found source file at: %s\n", path_rel);
        }

        compiler_source_file_add(compiler, path_rel);
    }
    else if (file_exists(path_dep))
    {
        if (compiler->options->verbose_compile)
        {
            printf("[ COMP ] | | | found source file at: %s\n", path_dep);
        }

        compiler_source_file_add(compiler, path_dep);
    }
    else
    {
        printf("[ COMP ] | | | could not find source file. checked: \n- %s\n- %s\n", path_rel, path_dep);
    }

    free(dir_file);
    free(path_rel);
    free(path_dep);
    free(path_use_with_ext);
}

void compiler_visit_error(void *context, Node *node)
{
    if (context == NULL)
    {
        printf("[ COMP ] error: visitor context not set\n");
        return;
    }

    Compiler *compiler = ((CompilerVisitorContext *)context)->compiler;
    SourceFile *source_file = ((CompilerVisitorContext *)context)->source_file;

    if (compiler == NULL)
    {
        printf("[ COMP ] error: visitor context compiler not set\n");
        return;
    }

    if (source_file == NULL)
    {
        printf("[ COMP ] error: visitor context source file not set\n");
        return;
    }

    compiler->should_abort = true;
    parser_print_error(source_file->parser, node, source_file->path);
}

void compiler_stage_parsing(Compiler *compiler)
{
    if (compiler->options->verbose_compile)
    {
        printf("[ COMP ] initial pass...\n");
    }

    Visitor *visitor_use = visitor_new(NODE_STMT_USE);
    Visitor *visitor_error = visitor_new(NODE_ERROR);
    visitor_use->callback = compiler_visit_use;
    visitor_error->callback = compiler_visit_error;

    CompilerVisitorContext *context = malloc(sizeof(CompilerVisitorContext));
    context->compiler = compiler;
    visitor_use->context = context;
    visitor_error->context = context;

    for (int i = 0; compiler->source_files[i] != NULL; i++)
    {
        SourceFile *source_file = compiler->source_files[i];

        if (compiler->options->verbose_compile)
        {
            printf("[ COMP ] | processing file: %s\n", source_file->path);
            printf("[ COMP ] | | parsing file...\n");
        }

        source_file_parse(source_file);

        if (compiler->options->verbose_compile)
        {
            printf("[ COMP ] | | checking for imports...\n");
        }

        context->source_file = source_file;
        visitor_visit(visitor_use, source_file->ast);
    }

    if (compiler->options->verbose_compile)
    {
        printf("[ COMP ] | checking for parse errors...\n");
    }

    for (int i = 0; compiler->source_files[i] != NULL; i++)
    {

        context->source_file = compiler->source_files[i];
        visitor_visit(visitor_error, compiler->source_files[i]->ast);
    }

    visitor_free(visitor_use);
    visitor_free(visitor_error);
    free(context);
}

void compiler_stage_analysis(Compiler *compiler)
{
    if (compiler->options->verbose_compile)
    {
        printf("[ COMP ] merging ASTs for analysis...\n");
    }

    compiler->program = node_new(NODE_PROGRAM, NULL);

    for (int i = 0; compiler->source_files[i] != NULL; i++)
    {
        Node *ast = compiler->source_files[i]->ast;
        if (ast == NULL)
        {
            continue;
        }

        if (compiler->program->program.statements == NULL)
        {
            compiler->program->program.statements = ast->program.statements;
        }
        else
        {
            compiler->program->program.statements->next = ast->program.statements;
        }
    }

    for (int i = 0; i < compiler->options->target_count; i++)
    {
        Target target = compiler->options->targets[i];
        if (compiler->options->verbose_compile)
        {
            printf("[ COMP ] analyzing program for target: %s\n", target_to_string(target));
        }

        compiler->analyzer = analyzer_new(target);

        compiler->analyzer->resolve_all = true;
        compiler->analyzer->opt_verbose = compiler->options->verbose_analysis;

        analyzer_analyze(compiler->analyzer, compiler->program);
    }

    analyzer_free(compiler->analyzer);
}

void compiler_compile(Compiler *compiler)
{
    clock_t
        c_total,
        c_prepare,
        c_parse,
        c_analysis;
    double
        ms_total,
        ms_prepare,
        ms_parse,
        ms_analysis;

    if (compiler->options->verbose_compile)
    {
        printf("[ COMP ] beginning compilation...\n");
    }

    c_total = clock();

    c_prepare = clock();
    compiler_stage_prepare(compiler);
    ms_prepare = (double)(clock() - c_prepare) * 1000 / CLOCKS_PER_SEC;
    if (compiler->should_abort)
    {
        printf("[ COMP ] compilation aborted during preparation stage\n");
        return;
    }

    c_parse = clock();
    compiler_stage_parsing(compiler);
    ms_parse = (double)(clock() - c_parse) * 1000 / CLOCKS_PER_SEC;
    if (compiler->should_abort)
    {
        printf("[ COMP ] compilation aborted during parsing stage\n");
        return;
    }

    c_analysis = clock();
    compiler_stage_analysis(compiler);
    ms_analysis = (double)(clock() - c_analysis) * 1000 / CLOCKS_PER_SEC;
    if (compiler->should_abort)
    {
        printf("[ COMP ] compilation aborted during analysis stage\n");
        return;
    }

    ms_total = (double)(clock() - c_total) * 1000 / CLOCKS_PER_SEC;

    if (compiler->options->verbose_compile)
    {
        printf("[ COMP ] compiled in %.2f seconds:\n", ms_total / 1000);
        printf("[ COMP ] - preparation:  (%%%2.0f) %5.2f ms\n", (ms_prepare / ms_total) * 100, ms_prepare);
        printf("[ COMP ] - parse:        (%%%2.0f) %5.2f ms\n", (ms_parse / ms_total) * 100, ms_parse);
        printf("[ COMP ] - analysis:     (%%%2.0f) %5.2f ms\n", (ms_analysis / ms_total) * 100, ms_analysis);
    }
}
