#ifndef IOUTIL_H
#define IOUTIL_H

#include <stdbool.h>
#include <sys/stat.h>

bool is_directory(const char *path);
bool file_exists(const char *path);
bool path_is_absolute(const char *path);
char *path_dirname(const char *path);
char *path_join(const char *path1, const char *path2);

#endif // IOUTIL_H
