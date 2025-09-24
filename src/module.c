#include "module.h"
#include "config.h"
#include "codegen.h"
#include "ioutil.h"
#include "lexer.h"
#include "symbol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "semantic.h"

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

static Module *module_manager_find_canonical(ModuleManager *manager, const char *canonical_name)
{
    unsigned int index  = hash_module_name(canonical_name, manager->capacity);
    Module      *module = manager->modules[index];

    while (module)
    {
        if (strcmp(module->name, canonical_name) == 0)
        {
            return module;
        }
        module = module->next;
    }

    return NULL;
}

static char *module_manager_expand_module(ModuleManager *manager, const char *module_fqn)
{
    if (!module_fqn)
        return NULL;
    if (!manager->config)
        return strdup(module_fqn);
    return config_expand_module_path((ProjectConfig *)manager->config, module_fqn);
}

void module_manager_init(ModuleManager *manager)
{
    manager->capacity     = 32;
    manager->count        = 0;
    manager->modules      = calloc(manager->capacity, sizeof(Module *));
    manager->search_paths = NULL;
    manager->search_count = 0;
    manager->config       = NULL;
    manager->project_dir  = NULL;

    module_error_list_init(&manager->errors);
    manager->had_error = false;
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

void module_manager_set_config(ModuleManager *manager, void *config, const char *project_dir)
{
    manager->config      = config;
    manager->project_dir = project_dir;
}

char *module_path_to_file_path(ModuleManager *manager, const char *module_fqn)
{
    if (!(manager->config && manager->project_dir))
        return NULL;
    return config_resolve_module_fqn((ProjectConfig *)manager->config, manager->project_dir, module_fqn);
}

Module *module_manager_find_module(ModuleManager *manager, const char *name)
{
    char *canonical = module_manager_expand_module(manager, name);
    if (!canonical)
        return NULL;

    Module *module = module_manager_find_canonical(manager, canonical);
    free(canonical);
    return module;
}

static Module *module_manager_load_module_internal(ModuleManager *manager, const char *module_fqn, const char *base_dir)
{
    (void)base_dir; // currently unused, for future relative path support

    char *canonical = module_manager_expand_module(manager, module_fqn);
    if (!canonical)
    {
        module_error_list_add(&manager->errors, module_fqn, "<unknown>", "could not resolve module path");
        manager->had_error = true;
        return NULL;
    }

    // check if already loaded
    Module *existing = module_manager_find_canonical(manager, canonical);
    if (existing)
    {
        free(canonical);
        return existing;
    }

    // check for circular dependencies in already loaded modules
    for (int i = 0; i < manager->capacity; i++)
    {
        Module *module = manager->modules[i];
        while (module)
        {
            if (module_has_circular_dependency(manager, module, canonical))
            {
                module_error_list_add(&manager->errors, canonical, "<unknown>", "Circular dependency detected");
                manager->had_error = true;
                free(canonical);
                return NULL;
            }
            module = module->next;
        }
    }

    // find file path via canonical FQN
    char *file_path = module_path_to_file_path(manager, canonical);
    if (!file_path)
    {
        module_error_list_add(&manager->errors, canonical, "<unknown>", "Could not find module file");
        manager->had_error = true;
        free(canonical);
        return NULL;
    }

    printf("parsing `%s`... ", file_path);

    // read and parse module
    char *source = read_file(file_path);
    if (!source)
    {
        printf("failed\n");
        module_error_list_add(&manager->errors, canonical, file_path, "Could not read module file");
        manager->had_error = true;
        free(file_path);
        free(canonical);
        return NULL;
    }

    Lexer lexer;
    lexer_init(&lexer, source);

    Parser parser;
    parser_init(&parser, &lexer);

    AstNode *ast = parser_parse_program(&parser); // modules inside will keep raw paths; normalization handled earlier

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
            module_error_list_add(&manager->errors, canonical, file_path, error_msg);
        }
        else
        {
            module_error_list_add(&manager->errors, canonical, file_path, "Failed to parse module");
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
        free(canonical);

        return NULL;
    }

    printf("done\n");

    // create module
    Module *module = malloc(sizeof(Module));
    module_init(module, canonical, file_path);
    module->ast         = ast;
    module->is_parsed   = true;
    module->is_analyzed = false;

    // add to hash table
    unsigned int index      = hash_module_name(canonical, manager->capacity);
    module->next            = manager->modules[index];
    manager->modules[index] = module;
    manager->count++;

    // clean up
    parser_dnit(&parser);
    lexer_dnit(&lexer);
    free(source);
    free(canonical);

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
            char *normalized = module_manager_expand_module(manager, stmt->use_stmt.module_path);
            if (!normalized)
            {
                module_error_list_add(&manager->errors, stmt->use_stmt.module_path, "<unknown>", "could not resolve module path");
                manager->had_error = true;
                return false;
            }

            char *original = stmt->use_stmt.module_path;
            stmt->use_stmt.module_path = normalized;
            free(original);

            // load the module
            Module *module = module_manager_load_module_internal(manager, stmt->use_stmt.module_path, base_dir);
            if (!module)
            {
                // error already recorded in load_module_internal
                return false;
            }

            // mark module as needing linking since it's imported
            module->needs_linking = true;

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
    module->name          = strdup(name);
    module->file_path     = strdup(file_path);
    module->object_path   = NULL;
    module->ast           = NULL;
    module->symbols       = NULL;
    module->is_parsed     = false;
    module->is_analyzed   = false;
    module->is_compiled   = false;
    module->needs_linking = false;
    module->next          = NULL;
}

void module_dnit(Module *module)
{
    free(module->name);
    free(module->file_path);
    free(module->object_path);
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

// helper declarations
static bool compile_module_to_object(ModuleManager *manager, Module *module, const char *output_dir, int opt_level, bool no_pie);

static bool ensure_dirs_recursive(const char *dir)
{
    if (!dir || !*dir)
        return true;

    char *copy = strdup(dir);
    if (!copy)
        return false;

    for (char *p = copy + 1; *p; ++p)
    {
        if (*p == '/')
        {
            *p = '\0';
            mkdir(copy, 0755);
            *p = '/';
        }
    }
    int r = mkdir(copy, 0755);
    (void)r;
    free(copy);
    return true;
}

char *module_make_object_path(const char *output_dir, const char *module_name)
{
    if (!output_dir || !module_name)
        return NULL;

    const char *name = module_name;
    if (strncmp(name, "dep.", 4) == 0)
        name += 4; // strip dep prefix

    size_t len = strlen(name);
    char  *rel = malloc(len + 1);
    if (!rel)
        return NULL;
    for (size_t i = 0; i < len; i++)
        rel[i] = (name[i] == '.') ? '/' : name[i];
    rel[len] = '\0';

    size_t dir_len  = strlen(output_dir);
    size_t path_len = dir_len + 1 + strlen(rel) + 3;
    char  *path     = malloc(path_len);
    if (!path)
    {
        free(rel);
        return NULL;
    }
    snprintf(path, path_len, "%s/%s.o", output_dir, rel);

    // ensure parent directories exist
    char *last_slash = strrchr(path, '/');
    if (last_slash)
    {
        *last_slash = '\0';
        ensure_dirs_recursive(path);
        *last_slash = '/';
    }

    free(rel);
    return path;
}

char *module_sanitize_name(const char *name)
{
    if (!name)
        return NULL;

    size_t len  = strlen(name);
    char  *copy = malloc(len + 1);
    if (!copy)
        return NULL;
    for (size_t i = 0; i < len; i++)
    {
        char c = name[i];
        copy[i] = (c == '.' || c == '/' || c == ':') ? '_' : c;
    }
    copy[len] = '\0';
    return copy;
}

bool module_manager_compile_dependencies(ModuleManager *manager, const char *output_dir, int opt_level, bool no_pie)
{
    if (!manager)
        return false;

    for (int i = 0; i < manager->capacity; i++)
    {
        Module *module = manager->modules[i];
        while (module)
        {
            if (!module->needs_linking)
            {
                module = module->next;
                continue;
            }

            if (!module->ast)
            {
                module_error_list_add(&manager->errors, module->name, module->file_path ? module->file_path : "<unknown>", "module missing AST");
                manager->had_error = true;
                return false;
            }

            if (!module->is_analyzed)
            {
                SemanticAnalyzer analyzer;
                semantic_analyzer_init(&analyzer);
                module_manager_set_config(&analyzer.module_manager, manager->config, manager->project_dir);

                bool analyzed = semantic_analyze(&analyzer, module->ast);
                if (!analyzed)
                {
                    // propagate semantic errors to module manager errors
                    for (int ei = 0; ei < analyzer.errors.count; ei++)
                    {
                        module_error_list_add(&manager->errors, module->name, module->file_path ? module->file_path : "<unknown>", analyzer.errors.errors[ei].message ? analyzer.errors.errors[ei].message : "semantic error");
                    }
                    manager->had_error = true;
                    semantic_analyzer_dnit(&analyzer);
                    return false;
                }

                if (module->symbols)
                {
                    symbol_table_dnit(module->symbols);
                    free(module->symbols);
                }
                module->symbols  = malloc(sizeof(SymbolTable));
                *module->symbols = analyzer.symbol_table;
                analyzer.symbol_table.global_scope  = NULL;
                analyzer.symbol_table.current_scope = NULL;

                // prevent freeing symbol tables of analyzer-managed modules
                for (int mi = 0; mi < analyzer.module_manager.capacity; mi++)
                {
                    Module *m = analyzer.module_manager.modules[mi];
                    while (m)
                    {
                        m->symbols = NULL;
                        m          = m->next;
                    }
                }

                module->is_analyzed = true;
                semantic_analyzer_dnit(&analyzer);
            }

            if (module->is_compiled && module->object_path)
            {
                module = module->next;
                continue;
            }

            printf("compiling dependency '%s'...\n", module->name);
            fflush(stdout);

            if (!compile_module_to_object(manager, module, output_dir, opt_level, no_pie))
            {
                return false;
            }

            module->is_compiled = true;
            module              = module->next;
        }
    }

    return true;
}

bool module_manager_get_link_objects(ModuleManager *manager, char ***object_files, int *count)
{
    // count modules that need linking
    int link_count = 0;
    for (int i = 0; i < manager->capacity; i++)
    {
        Module *module = manager->modules[i];
        while (module)
        {
            if (module->needs_linking && module->object_path)
            {
                link_count++;
            }
            module = module->next;
        }
    }

    if (link_count == 0)
    {
        *object_files = NULL;
        *count        = 0;
        return true;
    }

    // allocate array for object file paths
    char **objects = malloc(link_count * sizeof(char *));
    int    idx     = 0;

    // collect object file paths
    for (int i = 0; i < manager->capacity; i++)
    {
        Module *module = manager->modules[i];
        while (module)
        {
            if (module->needs_linking && module->object_path)
            {
                objects[idx++] = strdup(module->object_path);
            }
            module = module->next;
        }
    }

    *object_files = objects;
    *count        = link_count;
    return true;
}

static bool compile_module_to_object(ModuleManager *manager, Module *module, const char *output_dir, int opt_level, bool no_pie)
{
    if (!module || !module->ast)
        return false;

    char *object_path = module_make_object_path(output_dir, module->name);
    if (!object_path)
    {
        module_error_list_add(&manager->errors, module->name, module->file_path ? module->file_path : "<unknown>", "failed to allocate object path");
        manager->had_error = true;
        return false;
    }

    if (module->object_path)
    {
        free(module->object_path);
        module->object_path = NULL;
    }
    module->object_path = object_path;

    CodegenContext ctx;
    codegen_context_init(&ctx, module->name, no_pie);
    ctx.opt_level = opt_level;

    char *package_name = module_sanitize_name(module->name);
    if (package_name)
        ctx.package_name = package_name;

    if (module->file_path)
    {
        const char *slash     = strrchr(module->file_path, '/');
        const char *file_name = slash ? slash + 1 : module->file_path;
        if (strcmp(file_name, "runtime.mach") == 0)
            ctx.is_runtime = true;
    }

    SemanticAnalyzer stub;
    memset(&stub, 0, sizeof(stub));
    if (module->symbols)
    {
        stub.symbol_table                  = *module->symbols;
        stub.symbol_table.current_scope    = stub.symbol_table.global_scope;
        stub.symbol_table.module_scope     = module->symbols->module_scope;
    }

    bool success = codegen_generate(&ctx, module->ast, module->symbols ? &stub : NULL);
    if (success)
    {
        success = codegen_emit_object(&ctx, module->object_path);
        if (!success)
        {
            module_error_list_add(&manager->errors, module->name, module->file_path ? module->file_path : "<unknown>", "failed to emit object file");
            manager->had_error = true;
        }
    }
    else
    {
        codegen_print_errors(&ctx);
        module_error_list_add(&manager->errors, module->name, module->file_path ? module->file_path : "<unknown>", "code generation failed");
        manager->had_error = true;
    }

    codegen_context_dnit(&ctx);
    return success;
}
