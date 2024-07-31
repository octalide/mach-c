#ifndef COMPILER_H
#define COMPILER_H

#include "file.h"
#include "options.h"
#include "lexer.h"
#include "parser.h"
#include "analyzer.h"

#include <stdbool.h>

typedef struct CompilerError
{
    char *message;
} CompilerError;

typedef struct Compiler
{
    Options *options;

    SourceFile **source_files;

    Node *program;
    Analyzer *analyzer;

    bool should_abort;
} Compiler;

typedef struct CompilerVisitorContext
{
    Compiler *compiler;
    SourceFile *source_file;
} CompilerVisitorContext;

Compiler *compiler_new(Options *options);
void compiler_free(Compiler *compiler);

void compiler_source_file_add(Compiler *compiler, char *path);

void compiler_visit_use(void *context, Node *node);
void compiler_visit_error(void *context, Node *node);

void compiler_stage_prepare(Compiler *compiler);
void compiler_stage_parsing(Compiler *compiler);
void compiler_stage_analysis(Compiler *compiler);

void compiler_compile(Compiler *compiler);

#endif // COMPILER_H
