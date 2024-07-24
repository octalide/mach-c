#ifndef FILE_H
#define FILE_H

#include "ast.h"
#include "parser.h"

typedef struct SourceFile
{
    const char *filename;
    const char *source;
    
    Lexer *lexer;
    Parser *parser;
    Node *root;
} SourceFile;

#endif // FILE_H
