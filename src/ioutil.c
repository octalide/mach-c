#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "ioutil.h"

bool is_directory(const char *path)
{
    struct stat statbuf;
    if (stat(path, &statbuf) != 0)
    {
        return 0;
    }

    return S_ISDIR(statbuf.st_mode);
}

bool file_exists(const char *path)
{
    struct stat statbuf;
    return stat(path, &statbuf) == 0;
}


bool path_is_absolute(const char *path)
{
    if (path == NULL || strlen(path) < 2)
    {
        return false;
    }

#ifdef _WIN32
    // "C:\ or C:/"
    return (path[1] == ':' && (path[2] == '\\' || path[2] == '/'));
#else
    // "/home/user"
    return path[0] == '/';
#endif
}

char *path_dirname(const char *path)
{
    char *last_slash = strrchr(path, '/');
    if (last_slash == NULL)
    {
        return NULL;
    }

    size_t length = last_slash - path;
    char *dirname = malloc(length + 1);
    strncpy(dirname, path, length);
    dirname[length] = '\0';

    return dirname;
}

char *path_join(const char *path1, const char *path2)
{
    size_t length = strlen(path1) + strlen(path2) + 2;
    char *joined = malloc(length);

    strcpy(joined, path1);
    strcat(joined, "/");
    strcat(joined, path2);

    return joined;
}
