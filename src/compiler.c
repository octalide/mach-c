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

void compiler_init(Compiler *compiler, Options *options)
{
    compiler->options = options;

    compiler->analyzer = malloc(sizeof(Analyzer));
    analyzer_init(compiler->analyzer);

    compiler->source_files = calloc(1, sizeof(SourceFile *));
    compiler->source_files[0] = NULL;

    compiler->program = NULL;

    compiler->errors = calloc(1, sizeof(CompilerError *));
    compiler->errors[0] = NULL;
}

void compiler_free(Compiler *compiler)
{
    if (compiler == NULL)
    {
        return;
    }

    if (compiler->options != NULL)
    {
        free(compiler->options);
    }

    if (compiler->analyzer != NULL)
    {
        analyzer_free(compiler->analyzer);
        free(compiler->analyzer);
    }

    if (compiler->source_files != NULL)
    {
        for (int i = 0; compiler->source_files[i] != NULL; i++)
        {
            source_file_free(compiler->source_files[i]);
        }

        free(compiler->source_files);
    }

    if (compiler->program != NULL)
    {
        node_free(compiler->program);
        free(compiler->program);
    }

    if (compiler->errors != NULL)
    {
        for (int i = 0; compiler->errors[i] != NULL; i++)
        {
            free(compiler->errors[i]);
        }

        free(compiler->errors);
    }
}

void compiler_add_error(Compiler *compiler, const char *message)
{
    int i = 0;
    while (compiler->errors[i] != NULL)
    {
        i++;
    }

    compiler->errors = realloc(compiler->errors, (i + 2) * sizeof(CompilerError *));
    compiler->errors[i] = malloc(sizeof(CompilerError));
    compiler->errors[i]->message = strdup(message);
    compiler->errors[i + 1] = NULL;
}

void compiler_add_errorf(Compiler *compiler, const char *format, ...)
{
    va_list args;
    va_start(args, format);

    char buffer[1024];
    vsnprintf(buffer, 1024, format, args);

    compiler_add_error(compiler, buffer);
}

bool compiler_has_errors(Compiler *compiler)
{
    return compiler->errors[0] != NULL;
}

void compiler_print_errors(Compiler *compiler)
{
    for (int i = 0; compiler->errors[i] != NULL; i++)
    {
        printf("[ COMP ] error: %s\n", compiler->errors[i]->message);
    }
}

void compiler_add_source_file(Compiler *compiler, const char *path)
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
        compiler_add_errorf(compiler, "failed to open file: %s", path);
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
    source_file_init(source_file, path, source);
    source_file->parser->opt_verbose = compiler->options->verbose_parse;
    source_file->analyzer->opt_verbose = compiler->options->verbose_analysis;

    int i = 0;
    while (compiler->source_files[i] != NULL)
    {
        i++;
    }

    compiler->source_files = realloc(compiler->source_files, (i + 2) * sizeof(SourceFile));
    compiler->source_files[i] = source_file;
    compiler->source_files[i + 1] = NULL;
}

// compiler_prepare performs the following tasks:
// 1. checks options for errors
// 2. checks that dependencies are installed
// 3. checks that the program entrypoint file exists and adds it to the list of source files
void compiler_prepare(Compiler *compiler)
{
    if (compiler->options->verbose_compiler)
    {
        printf("[ COMP ] preparing compiler...\n");
    }

    if (compiler->options->mach_installation == NULL)
    {
        compiler_add_error(compiler, "mach installation path not set");
    }

    if (compiler->options->path_project == NULL)
    {
        compiler_add_error(compiler, "project path not set");
    }

    if (compiler->options->path_src == NULL)
    {
        compiler_add_error(compiler, "source path not set");
    }

    if (compiler->options->path_dep == NULL)
    {
        compiler_add_error(compiler, "dependancy path not set");
    }

    if (compiler->options->path_out == NULL)
    {
        compiler_add_error(compiler, "output path not set");
    }

    if (compiler->options->path_entry == NULL)
    {
        compiler_add_error(compiler, "program entrypoint not set");
    }

    if (compiler_has_errors(compiler))
    {
        return;
    }

    if (compiler->options->verbose_compiler)
    {
        printf("[ COMP ] | - mach installation:  %s\n", compiler->options->mach_installation);
        printf("[ COMP ] | - path_project:       %s\n", compiler->options->path_project);
        printf("[ COMP ] | - path_src:           %s\n", compiler->options->path_src);
        printf("[ COMP ] | - path_dep:           %s\n", compiler->options->path_dep);
        printf("[ COMP ] | - path_out:           %s\n", compiler->options->path_out);
        printf("[ COMP ] | - path_entry:         %s\n", compiler->options->path_entry);
    }

    if (compiler->options->path_entry == NULL)
    {
        compiler_add_error(compiler, "program entrypoint not set");
    }

    if (!path_is_absolute(compiler->options->path_project))
    {
        compiler_add_error(compiler, "project path is not absolute");
    }

    if (!path_is_absolute(compiler->options->path_src))
    {
        compiler_add_error(compiler, "source path is not absolute");
    }

    if (!path_is_absolute(compiler->options->path_dep))
    {
        compiler_add_error(compiler, "dependancy path is not absolute");
    }

    if (!path_is_absolute(compiler->options->path_out))
    {
        compiler_add_error(compiler, "output path is not absolute");
    }

    if (!path_is_absolute(compiler->options->path_entry))
    {
        compiler_add_error(compiler, "entrypoint path is not absolute");
    }

    if (compiler_has_errors(compiler))
    {
        return;
    }

    if (!file_exists(compiler->options->path_entry))
    {
        compiler_add_errorf(compiler, "program entrypoint does not exist: %s", compiler->options->path_entry);
    }

    if (compiler_has_errors(compiler))
    {
        return;
    }

    compiler_add_source_file(compiler, compiler->options->path_entry);
}

// this function performs the actions in accordance with the documentation for
//   `compiler_pass_initial()` below.
// Specifically, this is a visitor callback that is called for each `use`
//   statement in a source file. The use statement path is extracted and turned
//   into two filesystem paths:
// - one relative to the current file with ".mach" appended
// - one relative to `options->path_dep` with ".mach" appended as a fallback
// If the file does not exist at either path, a compiler error is created.
// If no errors are encountered, the file is added to the list of source files.
//
// A pointer to the Compiler in use is stored in `visitor->data_one` and the
//   current SourceFile is stored in `visitor->data_two`.
bool visitor_cb_visit_stmt_use(void *context, Node *node)
{
    if (context == NULL)
    {
        printf("[ COMP ] fatal: visitor context not set\n");
        exit(1);
    }

    Compiler *compiler = ((CompilerVisitorContext *)context)->compiler;
    SourceFile *source_file = ((CompilerVisitorContext *)context)->source_file;

    if (compiler == NULL)
    {
        printf("[ COMP ] fatal: visitor context compiler not set\n");
        exit(1);
    }

    if (source_file == NULL)
    {
        printf("[ COMP ] fatal: visitor context source file not set\n");
        exit(1);
    }

    Token token = node->data.stmt_use.path->data.expr_lit_string.value;
    const char *path_file = source_file->path;
    char *path_use_raw = token_raw(&token);

    // only preserve text between the first quote and the last, noninclusive
    char *quote = strchr(path_use_raw, '\"');
    char *path_use = quote + 1;
    quote = strrchr(path_use, '\"');
    *quote = '\0';

    size_t path_use_len = strlen(path_use);
    char *path_use_with_ext = malloc(path_use_len + 6);

    // copy the original path and append ".mach"
    strcpy(path_use_with_ext, path_use);
    strcat(path_use_with_ext, ".mach");

    char *dir_file = path_dirname(path_file);

    char *path_rel = path_join(dir_file, path_use_with_ext);
    char *path_dep = path_join(compiler->options->path_dep, path_use_with_ext);

    if (file_exists(path_rel))
    {
        if (compiler->options->verbose_compiler)
        {
            printf("[ COMP ] | | | found source file at: %s\n", path_rel);
        }

        compiler_add_source_file(compiler, path_rel);
    }
    else if (file_exists(path_dep))
    {
        if (compiler->options->verbose_compiler)
        {
            printf("[ COMP ] | | | found source file at: %s\n", path_dep);
        }

        compiler_add_source_file(compiler, path_dep);
    }
    else
    {
        printf("[ COMP ] | | | could not find source file. checked: \n- %s\n- %s\n", path_rel, path_dep);
        compiler_add_errorf(compiler, "could not find source file. checked: \n- %s\n- %s", path_rel, path_dep);
    }

    free(dir_file);
    free(path_rel);
    free(path_dep);
    free(path_use_with_ext); // Free the allocated buffer

    return true;
}

// The initial pass parses and analyzes each source file individually, starting
//   with `options->path_entry`.
// If `use` statements are found in the resulting AST, the statement path is
//   extracted and parsed into two filesystem paths:
// - one relative to the current file with ".mach" appended
// - one relative to `options->path_dep` with ".mach" appended as a fallback
// A compiler error is created if a file does not exist at either path.
// The file is then added to the list of source files and the process repeats
//   until no new files are added.
// The program AST is seeded with the first file's AST.
void compiler_pass_initial(Compiler *compiler)
{
    if (compiler->options->verbose_compiler)
    {
        printf("[ COMP ] initial pass...\n");
    }

    Visitor visitor;
    visitor_init(&visitor);
    visitor.cb_visit_stmt_use = visitor_cb_visit_stmt_use;

    CompilerVisitorContext *context = malloc(sizeof(CompilerVisitorContext));
    context->compiler = compiler;

    for (int i = 0; compiler->source_files[i] != NULL; i++)
    {
        SourceFile *source_file = compiler->source_files[i];

        if (compiler->options->verbose_compiler)
        {
            printf("[ COMP ] | processing file: %s\n", source_file->path);
            printf("[ COMP ] | | parsing file...\n");
        }

        source_file_parse(source_file);
        if (parser_has_errors(source_file->parser))
        {
            if (compiler->options->verbose_compiler)
            {
                printf("[ COMP ] | | found errors during parsing:\n");
            }
            parser_print_errors(source_file->parser);

            compiler_add_errorf(compiler, "found errors while parsing file: %s", source_file->path);
        }

        if (compiler->options->verbose_compiler)
        {
            printf("[ COMP ] | | analyzing file...\n");
        }

        source_file_analyze(source_file);
        if (analyzer_has_errors(source_file->analyzer))
        {
            if (compiler->options->verbose_compiler)
            {
                printf("[ COMP ] | | found errors during analysis:\n");
            }
            analyzer_print_errors(source_file->analyzer);

            compiler_add_errorf(compiler, "found errors while analyzing file: %s", source_file->path);
        }

        if (compiler->options->verbose_compiler)
        {
            printf("[ COMP ] | | checking for imports...\n");
        }

        // set up context for visitor
        context->source_file = source_file;
        visitor.context = context;
        visit_node(&visitor, source_file->ast);
    }

    compiler->program = compiler->source_files[0]->ast;
    if (compiler->program == NULL)
    {
        compiler_add_error(compiler, "program entrypoint has no AST");
    }

    free(context);
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
    clock_t
        c_total,
        c_prepare,
        c_pass_initial;
    double
        s_total,
        ms_prepare,
        ms_pass_initial;

    if (compiler->options->verbose_compiler)
    {
        printf("[ COMP ] beginning compilation...\n");
    }

    c_total = clock();

    c_prepare = clock();
    compiler_prepare(compiler);
    ms_prepare = (double)(clock() - c_prepare) * 1000 / CLOCKS_PER_SEC;
    if (compiler_has_errors(compiler))
    {
        return;
    }

    c_pass_initial = clock();
    compiler_pass_initial(compiler);
    ms_pass_initial = (double)(clock() - c_pass_initial) * 1000 / CLOCKS_PER_SEC;
    if (compiler_has_errors(compiler))
    {
        return;
    }

    s_total = (double)(clock() - c_total) / CLOCKS_PER_SEC;

    if (compiler->options->verbose_compiler)
    {
        printf("[ COMP ] compiled in %.2f seconds:\n", s_total);
        printf("[ COMP ] - preparation:  %.2f ms\n", ms_prepare);
        printf("[ COMP ] - initial pass: %.2f ms\n", ms_pass_initial);
    }
}
