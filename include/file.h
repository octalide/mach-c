#ifndef FILE_H
#define FILE_H

#include "ast.h"
#include "parser.h"
#include "analyzer.h"

// Each SourceFile has its own lexer, parser, and analyzer in order to separate
//   the parsing and analysis of each file. This allows for parallelization of
//   the parsing and analysis steps as well as highly customizable error
//   reporting in the compilation stages before the file ASTs are combined into
//   a single program AST for symbol resolution and code generation.
typedef struct SourceFile
{
    char *path;
    char *source;
    
    Lexer *lexer;
    Parser *parser;
    Analyzer *analyzer;

    Node *ast;
} SourceFile;

void source_file_init(SourceFile *source_file, const char *filename, const char *source);
void source_file_free(SourceFile *source_file);

void source_file_parse(SourceFile *source_file);
void source_file_analyze(SourceFile *source_file);

#endif // FILE_H
