#include "module.h"
#include "ioutil.h"
#include "lexer.h"
#include "symbol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// error handling functions
void module_error_list_init(ModuleErrorList *list)
{
    list->errors   = NULL;
    list->count    = 0;
    list->capacity = 0;
}

void module_error_list_dnit(ModuleErrorList *list)
{
    for (int i = 0; i < list->count; i++)
    {
        free(list->errors[i]->module_path);
        free(list->errors[i]->file_path);
        free(list->errors[i]->message);
        free(list->errors[i]);
    }
    free(list->errors);
}

void module_error_list_add(ModuleErrorList *list, const char *module_path, const char *file_path, const char *message)
{
    if (list->count >= list->capacity)
    {
        list->capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        list->errors   = realloc(list->errors, list->capacity * sizeof(ModuleError *));
    }

    ModuleError *error = malloc(sizeof(ModuleError));
    error->module_path = strdup(module_path);
    error->file_path   = strdup(file_path);
    error->message     = strdup(message);

    list->errors[list->count++] = error;
}

void module_error_list_print(ModuleErrorList *list)
{
    for (int i = 0; i < list->count; i++)
    {
        ModuleError *error = list->errors[i];
        fprintf(stderr, "error in module '%s' (%s): %s\n", error->module_path, error->file_path, error->message);
    }
}

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
    manager->had_error    = false;

    module_error_list_init(&manager->errors);

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

    // clean up errors
    module_error_list_dnit(&manager->errors);
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
        if (strcmp(manager->search_paths[i], ".") == 0)
        {
            strcpy(full_path, file_path);
        }
        else
        {
            sprintf(full_path, "%s/%s", manager->search_paths[i], file_path);
        }

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

static Module *module_manager_load_module_internal(ModuleManager *manager, const char *module_path, const char *base_dir)
{
    (void)base_dir; // currently unused, for future relative path support

    // check if already loaded
    Module *existing = module_manager_find_module(manager, module_path);
    if (existing)
    {
        return existing;
    }

    // check for circular dependencies in already loaded modules
    for (int i = 0; i < manager->capacity; i++)
    {
        Module *module = manager->modules[i];
        while (module)
        {
            if (module_has_circular_dependency(manager, module, module_path))
            {
                module_error_list_add(&manager->errors, module_path, "<unknown>", "Circular dependency detected");
                manager->had_error = true;
                return NULL;
            }
            module = module->next;
        }
    }

    // find file path
    char *file_path = module_path_to_file_path(manager, module_path);
    if (!file_path)
    {
        module_error_list_add(&manager->errors, module_path, "<unknown>", "Could not find module file");
        manager->had_error = true;
        return NULL;
    }

    printf("parsing `%s`... ", file_path);

    // read and parse module
    char *source = read_file(file_path);
    if (!source)
    {
        printf("failed\n");
        module_error_list_add(&manager->errors, module_path, file_path, "Could not read module file");
        manager->had_error = true;
        free(file_path);
        return NULL;
    }

    Lexer lexer;
    lexer_init(&lexer, source);

    Parser parser;
    parser_init(&parser, &lexer);

    AstNode *ast = parser_parse_program(&parser);

    // check for parse errors
    if (!ast || parser.had_error)
    {
        printf("failed\n");

        if (parser.had_error)
        {
            fprintf(stderr, "parsing failed with %d error(s):\n", parser.errors.count);
            parser_error_list_print(&parser.errors, &lexer, file_path);

            // add parsing errors to module error list
            char error_msg[256];
            snprintf(error_msg, sizeof(error_msg), "parsing failed with %d error(s)", parser.errors.count);
            module_error_list_add(&manager->errors, module_path, file_path, error_msg);
        }
        else
        {
            module_error_list_add(&manager->errors, module_path, file_path, "Failed to parse module");
        }

        manager->had_error = true;

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

    printf("done\n");

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

    return module;
}

bool module_manager_resolve_dependencies(ModuleManager *manager, AstNode *program, const char *base_dir)
{
    if (program->kind != AST_PROGRAM)
    {
        return false;
    }

    // find all use statements
    for (int i = 0; i < program->program.stmts->count; i++)
    {
        AstNode *stmt = program->program.stmts->items[i];
        if (stmt->kind == AST_STMT_USE)
        {
            // load the module
            Module *module = module_manager_load_module_internal(manager, stmt->use_stmt.module_path, base_dir);
            if (!module)
            {
                // error already recorded in load_module_internal
                return false;
            }

            // store module reference in AST node for semantic analysis
            stmt->use_stmt.module_sym = (Symbol *)module; // cast for now

            // recursively resolve dependencies of the loaded module
            if (!module_manager_resolve_dependencies(manager, module->ast, base_dir))
            {
                return false;
            }
        }
    }

    return !manager->had_error;
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

Module *module_manager_load_module(ModuleManager *manager, const char *module_path)
{
    return module_manager_load_module_internal(manager, module_path, ".");
}

bool module_has_circular_dependency(ModuleManager *manager, Module *module, const char *target)
{
    if (!module || !module->ast)
    {
        return false;
    }

    if (strcmp(module->name, target) == 0)
    {
        return true;
    }

    // check all dependencies
    for (int i = 0; i < module->ast->program.stmts->count; i++)
    {
        AstNode *stmt = module->ast->program.stmts->items[i];
        if (stmt->kind == AST_STMT_USE)
        {
            Module *dep = module_manager_find_module(manager, stmt->use_stmt.module_path);
            if (dep && module_has_circular_dependency(manager, dep, target))
            {
                return true;
            }
        }
    }

    return false;
}
