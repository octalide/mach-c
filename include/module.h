#ifndef MODULE_H
#define MODULE_H

#include "ast.h"
#include "parser.h"
#include <stdbool.h>

typedef struct Module        Module;
typedef struct ModuleManager ModuleManager;

// forward statement
typedef struct SymbolTable SymbolTable;

// represents a loaded module
struct Module
{
    char        *name;        // module name
    char        *file_path;   // absolute file path
    AstNode     *ast;         // parsed AST
    SymbolTable *symbols;     // module's symbol table
    bool         is_parsed;   // parsing complete
    bool         is_analyzed; // semantic analysis complete
    Module      *next;        // linked list for dependencies
};

// error tracking for modules
typedef struct ModuleError
{
    char *module_path;
    char *file_path;
    char *message;
} ModuleError;

typedef struct ModuleErrorList
{
    ModuleError **errors;
    int           count;
    int           capacity;
} ModuleErrorList;

// manages module loading and dependency resolution
struct ModuleManager
{
    Module        **modules;      // hash table of loaded modules
    int             capacity;     // hash table size
    int             count;        // number of loaded modules
    char          **search_paths; // directories to search for modules
    int             search_count; // number of search paths
    ModuleErrorList errors;       // accumulated errors during loading
    bool            had_error;    // true if any module failed to load/parse
};

// module manager lifecycle
void module_manager_init(ModuleManager *manager);
void module_manager_dnit(ModuleManager *manager);

// search path management
void module_manager_add_search_path(ModuleManager *manager, const char *path);

// module loading
Module *module_manager_load_module(ModuleManager *manager, const char *module_path);
Module *module_manager_find_module(ModuleManager *manager, const char *name);

// dependency resolution
bool module_manager_resolve_dependencies(ModuleManager *manager, AstNode *program, const char *base_dir);

// error handling
void module_error_list_init(ModuleErrorList *list);
void module_error_list_dnit(ModuleErrorList *list);
void module_error_list_add(ModuleErrorList *list, const char *module_path, const char *file_path, const char *message);
void module_error_list_print(ModuleErrorList *list);

// module operations
void module_init(Module *module, const char *name, const char *file_path);
void module_dnit(Module *module);

// utility functions
char *module_path_to_file_path(ModuleManager *manager, const char *module_path);
bool  module_has_circular_dependency(ModuleManager *manager, Module *module, const char *target);

#endif
