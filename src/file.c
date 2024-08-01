#include "file.h"
#include "ioutil.h"

#include <stdlib.h>
#include <string.h>

File *file_new(char *path)
{
    File *file = calloc(1, sizeof(File));
    file->path = strdup(path);
    file->source = read_file(path);
    if (file->source == NULL)
    {
        free(file);
        return NULL;
    }

    return file;
}

void file_free(File *file)
{
    free(file->path);
    file->path = NULL;
    
    free(file->source);
    file->source = NULL;

    free(file);
}
