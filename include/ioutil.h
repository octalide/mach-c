#ifndef IOUTIL_H
#define IOUTIL_H

#include <stdbool.h>
#include <sys/stat.h>

bool is_directory(const char *path);
bool file_exists(const char *path);

#endif // IOUTIL_H
