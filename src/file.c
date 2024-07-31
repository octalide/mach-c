#include <stdlib.h>
#include <string.h>

#include "file.h"

SourceFile *source_file_new(char *path, char *source)
{
    SourceFile *source_file = malloc(sizeof(SourceFile));
    source_file->path = strdup(path);
    source_file->parser = parser_new(source);
    source_file->ast = NULL;

    return source_file;
}

void source_file_free(SourceFile *source_file)
{
    if (source_file == NULL)
    {
        return;
    }

    free(source_file->path);
    source_file->path = NULL;

    node_free(source_file->ast);
    source_file->ast = NULL;

    parser_free(source_file->parser);
    source_file->parser = NULL;

    free(source_file);
}

void source_file_parse(SourceFile *source_file)
{
    source_file->ast = parser_parse(source_file->parser);
}
