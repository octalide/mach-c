#include "ioutil.h"

#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>

bool is_directory(char *path)
{
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISDIR(path_stat.st_mode);
}

bool file_exists(char *path)
{
    struct stat path_stat;
    return stat(path, &path_stat) == 0;
}

bool path_is_absolute(char *path)
{
#ifdef _WIN32
    // "C:\ or C:/"
    return (path[1] == ':' && (path[2] == '\\' || path[2] == '/'));
#else
    // "/home/user"
    return path[0] == '/';
#endif
}

char *path_dirname(char *path)
{
#ifdef _WIN32
    char *last_slash = strrchr(path, '\\');
#else
    char *last_slash = strrchr(path, '/');
#endif

    if (last_slash == NULL)
    {
        return ".";
    }

    int len = last_slash - path;
    char *dirname = malloc(len + 1);
    strncpy(dirname, path, len);
    dirname[len] = '\0';
    return dirname;
}

char *path_join(char *a, char *b)
{
    int len_a = strlen(a);
    int len_b = strlen(b);
    char *joined = malloc(len_a + len_b + 2);

    strcpy(joined, a);

#ifdef _WIN32
    if (a[len_a - 1] != '\\' && a[len_a - 1] != '/')
    {
        strcat(joined, "\\");
    }
#else
    if (a[len_a - 1] != '/')
    {
        strcat(joined, "/");
    }
#endif

    strcat(joined, b);
    return joined;
}

char *read_file(char *path)
{
    FILE *file = fopen(path, "r");
    if (file == NULL)
    {
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *buffer = malloc(length + 1);
    fread(buffer, 1, length, file);
    buffer[length] = '\0';

    fclose(file);
    return buffer;
}