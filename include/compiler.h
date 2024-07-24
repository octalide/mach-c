#ifndef COMPILER_H
#define COMPILER_H

#include "file.h"
#include "options.h"
#include "lexer.h"
#include "parser.h"
#include "semantic_analyzer.h"

typedef struct Compiler
{
    Options *options;
    SemanticAnalyzer *analyzer;

    SourceFile *source_files;
    int file_count;

    Node *program;

    const char *error;
} Compiler;

void compiler_init(Compiler *compiler, Options *options);
void compiler_free(Compiler *compiler);

void compiler_error(Compiler *compiler, const char *message);
bool compiler_has_error(Compiler *compiler);
void compiler_add_file(Compiler *compiler, const char *path);
void compiler_parse_source_file(Compiler *compiler, SourceFile *source_file);
void compiler_analyze_source_file(Compiler *compiler, SourceFile *source_file);

void compiler_pass_import(Compiler *compiler);

void compiler_compile(Compiler *compiler);

#endif // COMPILER_H
