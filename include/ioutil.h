#ifndef IOUTIL_H
#define IOUTIL_H

#include <stdbool.h>
#include <sys/stat.h>

bool is_directory(char *path);
bool file_exists(char *path);
bool path_is_absolute(char *path);
char *path_dirname(char *path);
char *path_join(char *path1, char *path2);

#endif // IOUTIL_H
