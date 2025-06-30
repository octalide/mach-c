#include "module.h"
#include "ioutil.h"
#include "lexer.h"
#include "semantic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// simple hash function for module names
static unsigned int hash_module_name(const char *name, int capacity)
{
    unsigned int hash = 5381;
    for (int i = 0; name[i]; i++)
    {
        hash = ((hash << 5) + hash) + name[i];
    }
    return hash % capacity;
}

void module_manager_init(ModuleManager *manager)
{
    manager->capacity     = 16; // initial hash table size
    manager->count        = 0;
    manager->modules      = calloc(manager->capacity, sizeof(Module *));
    manager->search_paths = NULL;
    manager->search_count = 0;

    // add current directory as default search path
    module_manager_add_search_path(manager, ".");
}

void module_manager_dnit(ModuleManager *manager)
{
    // clean up modules
    for (int i = 0; i < manager->capacity; i++)
    {
        Module *module = manager->modules[i];
        while (module)
        {
            Module *next = module->next;
            module_dnit(module);
            free(module);
            module = next;
        }
    }
    free(manager->modules);

    // clean up search paths
    for (int i = 0; i < manager->search_count; i++)
    {
        free(manager->search_paths[i]);
    }
    free(manager->search_paths);
}

void module_manager_add_search_path(ModuleManager *manager, const char *path)
{
    manager->search_paths                        = realloc(manager->search_paths, (manager->search_count + 1) * sizeof(char *));
    manager->search_paths[manager->search_count] = strdup(path);
    manager->search_count++;
}

char *module_path_to_file_path(ModuleManager *manager, const char *module_path)
{
    // convert module.path to module/path.mach
    char *file_path = malloc(strlen(module_path) + 6); // ".mach" + null
    strcpy(file_path, module_path);

    // replace dots with slashes
    for (char *p = file_path; *p; p++)
    {
        if (*p == '.')
        {
            *p = '/';
        }
    }
    strcat(file_path, ".mach");

    // try each search path
    for (int i = 0; i < manager->search_count; i++)
    {
        char *full_path = malloc(strlen(manager->search_paths[i]) + strlen(file_path) + 2);
        sprintf(full_path, "%s/%s", manager->search_paths[i], file_path);

        FILE *test = fopen(full_path, "r");
        if (test)
        {
            fclose(test);
            free(file_path);
            return full_path;
        }
        free(full_path);
    }

    free(file_path);
    return NULL;
}

Module *module_manager_find_module(ModuleManager *manager, const char *name)
{
    unsigned int index  = hash_module_name(name, manager->capacity);
    Module      *module = manager->modules[index];

    while (module)
    {
        if (strcmp(module->name, name) == 0)
        {
            return module;
        }
        module = module->next;
    }

    return NULL;
}

Module *module_manager_load_module(ModuleManager *manager, const char *module_path)
{
    // check if already loaded
    Module *existing = module_manager_find_module(manager, module_path);
    if (existing)
    {
        return existing;
    }

    // find file path
    char *file_path = module_path_to_file_path(manager, module_path);
    if (!file_path)
    {
        fprintf(stderr, "error: Could not find module '%s'\n", module_path);
        return NULL;
    }

    // read and parse module
    char *source = read_file(file_path);
    if (!source)
    {
        fprintf(stderr, "error: Could not read module file '%s'\n", file_path);
        free(file_path);
        return NULL;
    }

    Lexer lexer;
    lexer_init(&lexer, source);

    Parser parser;
    parser_init(&parser, &lexer);

    AstNode *ast = parser_parse_program(&parser);
    if (!ast || parser.had_error)
    {
        fprintf(stderr, "error: Failed to parse module '%s'\n", module_path);
        if (ast)
        {
            ast_node_dnit(ast);
            free(ast);
        }
        parser_dnit(&parser);
        lexer_dnit(&lexer);
        free(source);
        free(file_path);
        return NULL;
    }

    // create module
    Module *module = malloc(sizeof(Module));
    module_init(module, module_path, file_path);
    module->ast         = ast;
    module->is_parsed   = true;
    module->is_analyzed = false;

    // add to hash table
    unsigned int index      = hash_module_name(module_path, manager->capacity);
    module->next            = manager->modules[index];
    manager->modules[index] = module;
    manager->count++;

    // clean up
    parser_dnit(&parser);
    lexer_dnit(&lexer);
    free(source);
    free(file_path);

    return module;
}

bool module_manager_resolve_dependencies(ModuleManager *manager, AstNode *program)
{
    if (program->kind != AST_PROGRAM)
    {
        return false;
    }

    // find all use declarations
    for (int i = 0; i < program->program.decls->count; i++)
    {
        AstNode *decl = program->program.decls->items[i];
        if (decl->kind == AST_USE_DECL)
        {
            // load the module
            Module *module = module_manager_load_module(manager, decl->use_decl.module_path);
            if (!module)
            {
                fprintf(stderr, "error: Failed to load module '%s'\n", decl->use_decl.module_path);
                return false;
            }

            // store module reference in AST node for semantic analysis
            decl->use_decl.module_sym = (Symbol *)module; // cast for now

            // recursively resolve dependencies of the loaded module
            if (!module_manager_resolve_dependencies(manager, module->ast))
            {
                return false;
            }
        }
    }

    return true;
}

void module_init(Module *module, const char *name, const char *file_path)
{
    module->name        = strdup(name);
    module->file_path   = strdup(file_path);
    module->ast         = NULL;
    module->symbols     = NULL;
    module->is_parsed   = false;
    module->is_analyzed = false;
    module->next        = NULL;
}

void module_dnit(Module *module)
{
    free(module->name);
    free(module->file_path);
    if (module->ast)
    {
        ast_node_dnit(module->ast);
        free(module->ast);
    }
    if (module->symbols)
    {
        symbol_table_dnit(module->symbols);
        free(module->symbols);
    }
}
