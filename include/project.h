#ifndef PROJECT_H
#define PROJECT_H

#include "lexer.h"
#include "parser.h"
#include "type.h"
#include "symbols.h"

#include <stdbool.h>

typedef struct File
{
    char *path;

    char *source;
    Lexer *lexer;
    Parser *parser;
    Node *ast;
} File;

typedef struct Module
{
    char *name;

    File **files;

    SymbolTable *symbols;
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

    char *path_project;

    char *name;
    char *version;
    char *path_dep;
    char *path_lib;
    char *path_out;
    char *path_src;

    Module **modules;
    SymbolTable *symbols;
} Project;

File *file_read(char *path);
void file_free(File *file);

Module *module_new();
void module_free(Module *module);

Project *project_new();
void project_free(Project *project);

void file_parse(File *file);

void module_add_file(Module *module, File *file);
void module_add_symbol(Module *module, Symbol *symbol);

Module *project_find_module(Project *project, char *name);
void project_add_module(Project *project, Module *module);
void project_add_file(Project *project, File *file);
void project_add_symbol(Project *project, Symbol *symbol);

void project_discover_files(Project *project);

void project_parse_all(Project *project);
int project_print_parse_errors(Project *project);

int project_analysis(Project *project);

#endif // PROJECT_H
