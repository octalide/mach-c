#ifndef IOUTIL_H
#define IOUTIL_H

#include <stdbool.h>

bool is_directory(char *path);
bool file_exists(char *path);
bool path_is_absolute(char *path);
char *path_dirname(char *path);
char *path_join(char *a, char *b);

char *read_file(char *path);

#endif
