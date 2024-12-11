#ifndef BUILD_H
#define BUILD_H

#include "project.h"

int build_project_lib(Project *project);
int build_project_exe(Project *project);
int build_project(Project *project);

int build_target_file(char *path, int argc, char **argv);
int build_target_project(char *path, int argc, char **argv);

#endif
