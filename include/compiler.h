#ifndef COMPILER_H
#define COMPILER_H

#include "file.h"
#include "options.h"
#include "lexer.h"
#include "parser.h"
#include "analyzer.h"

typedef struct CompilerError
{
    const char *message;
} CompilerError;

typedef struct Compiler
{
    Options *options;

    Analyzer *analyzer;
    SourceFile **source_files;

    Node *program;

    CompilerError **errors;
} Compiler;

typedef struct CompilerVisitorContext
{
    Compiler *compiler;
    SourceFile *source_file;
} CompilerVisitorContext;

void compiler_init(Compiler *compiler, Options *options);
void compiler_free(Compiler *compiler);

void compiler_add_error(Compiler *compiler, const char *message);
void compiler_add_errorf(Compiler *compiler, const char *format, ...);
bool compiler_has_errors(Compiler *compiler);
void compiler_print_errors(Compiler *compiler);

void compiler_add_source_file(Compiler *compiler, const char *path);

void compiler_pass_initial(Compiler *compiler);

void compiler_compile(Compiler *compiler);

#endif // COMPILER_H
