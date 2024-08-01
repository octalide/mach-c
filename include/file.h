#ifndef FILE_H
#define FILE_H

struct File;

typedef struct File
{
    char *path;
    char *source;
} File;

File *file_new(char *path);
void file_free(File *file);

#endif
