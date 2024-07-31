#ifndef FILE_H
#define FILE_H

#include "node.h"
#include "parser.h"
#include "analyzer.h"

typedef struct SourceFile
{
    char *path;
    
    Parser *parser;

    Node *ast;
} SourceFile;

SourceFile *source_file_new(char *path, char *source);
void source_file_free(SourceFile *source_file);

void source_file_parse(SourceFile *source_file);

#endif // FILE_H
