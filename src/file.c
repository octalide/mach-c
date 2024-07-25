#include <stdlib.h>
#include <string.h>

#include "file.h"

void source_file_init(SourceFile *source_file, const char *path, const char *source)
{
    source_file->path = malloc(strlen(path) + 1);
    source_file->source = malloc(strlen(source) + 1);
    strcpy(source_file->path, path);
    strcpy(source_file->source, source);

    source_file->lexer = malloc(sizeof(Lexer));
    lexer_init(source_file->lexer, source_file->source);

    source_file->parser = malloc(sizeof(Parser));
    parser_init(source_file->parser, source_file->lexer);

    source_file->analyzer = malloc(sizeof(Analyzer));
    analyzer_init(source_file->analyzer);

    source_file->ast = NULL;
}

void source_file_free(SourceFile *source_file)
{
    if (source_file == NULL)
    {
        return;
    }

    if (source_file->lexer != NULL)
    {
        lexer_free(source_file->lexer);
        free(source_file->lexer);
    }

    if (source_file->parser != NULL)
    {
        parser_free(source_file->parser);
        free(source_file->parser);
    }

    if (source_file->analyzer != NULL)
    {
        analyzer_free(source_file->analyzer);
        free(source_file->analyzer);
    }

    if (source_file->ast != NULL)
    {
        node_free(source_file->ast);
        free(source_file->ast);
    }

    if (source_file->path != NULL)
    {
        free(source_file->path);
    }

    if (source_file->source != NULL)
    {
        free(source_file->source);
    }
}

void source_file_parse(SourceFile *source_file)
{
    source_file->ast = parser_parse(source_file->parser);
}

void source_file_analyze(SourceFile *source_file)
{
    analyzer_analyze(source_file->analyzer, source_file->ast);
}
