#ifndef BUILD_H
#define BUILD_H

#include "context.h"

int build_project_lib(Context *context, Project *project);
int build_project_exe(Context *context, Project *project);
int build_project(Context *context, Project *project);
int build_target_file(Context *context, char *path);
int build_target_project(Context *context, char *path);

#endif
