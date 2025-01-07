#ifndef PROJECT_H
#define PROJECT_H

#include "parser.h"
#include "scope.h"
#include "target.h"

#include <stdbool.h>

// define default mach installation path by platform:
// - windows: C:\Program Files\mach
// - linux: /usr/local/mach
// - macos: /usr/local/mach
// - other: /usr/local/mach
#if defined(_WIN32)
#define DEFAULT_MACH_PATH "C:\\Program Files\\mach"
#elif defined(__linux__) || defined(__APPLE__)
#define DEFAULT_MACH_PATH "/usr/local/mach"
#else
#define DEFAULT_MACH_PATH "/usr/local/mach"
#endif

typedef struct File
{
    char *path;

    char *source;
    Parser *parser;
    
    // file ast represents the node structure of a single file
    Node *ast;
} File;

typedef struct Module
{
    char *name;

    // module ast strips the program node off of each file ast and combines them
    //   into one ast for the whole module
    Node *ast;
} Module;

typedef enum ProjectType
{
    PROJECT_TYPE_EXE,
    PROJECT_TYPE_LIB_STATIC,
    PROJECT_TYPE_LIB_SHARED
} ProjectType;

typedef struct Project
{
    ProjectType type;
    
    char *path_mach_root;
    char *path_project;

    char *name;
    char *version;
    char *path_dep;
    char *path_lib;
    char *path_out;
    char *path_src;
    char *entrypoint;
    Target *targets;
    int target_count;

    File **files;
    Module **modules;
    Node *program;

    // root symbol table holds things like base types and
    // builtin definitions
    Scope *symbols;
} Project;

File *file_read(char *path);
void file_free(File *file);

Module *module_new();
void module_free(Module *module);

Project *project_new();
void project_free(Project *project);

void file_parse(File *file);

char *module_parts_join(Node *module_path);
int module_add_file_ast(Module *module, File *file);

char *project_resolve_macros(Project *project, char *str);

Module *project_find_module(Project *project, char *name);
void project_add_module(Project *project, Module *module);
void project_add_file(Project *project, File *file);

void project_discover_files(Project *project);

void project_parse_all(Project *project);
int project_print_parse_errors(Project *project);
int project_modularize_files(Project *project);
void project_combine_modules(Project *project);

void project_add_base_symbols(Project *project);

int project_populate_symbols(Project *project);
int project_validate_types(Project *project);
int project_validate_control_flow(Project *project);
int project_validate_data_flow(Project *project);

int project_analyze(Project *project);

#endif
