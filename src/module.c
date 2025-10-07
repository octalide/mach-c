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
#include <stdint.h>
#include <llvm-c/Target.h>

// forward declaration
static char *generate_target_module_source(ModuleManager *manager);

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

// build a best-guess file path for diagnostics even if the file doesn't exist
static char *module_guess_file_path(ModuleManager *manager, const char *module_fqn)
{
    if (!module_fqn)
        return NULL;

    // if config is present, try config-based resolve (even if it may point to non-existent)
    if (manager->config && manager->project_dir)
    {
        char *guess = config_resolve_module_fqn((ProjectConfig *)manager->config, manager->project_dir, module_fqn);
        if (guess)
            return guess;
    }

    const char *dot      = strchr(module_fqn, '.');
    size_t      head_len = dot ? (size_t)(dot - module_fqn) : strlen(module_fqn);
    const char  *tail    = dot ? dot + 1 : NULL;

    // alias paths take precedence
    for (int i = 0; i < manager->alias_count; i++)
    {
        if (strlen(manager->alias_names[i]) == head_len && strncmp(manager->alias_names[i], module_fqn, head_len) == 0)
        {
            const char *base = manager->alias_paths[i];
            size_t      base_len = strlen(base);
            if (tail)
            {
                size_t tail_len = strlen(tail);
                char  *tail_buf = malloc(tail_len + 1);
                if (!tail_buf) return NULL;
                memcpy(tail_buf, tail, tail_len + 1);
                for (char *p = tail_buf; *p; ++p) if (*p == '.') *p = '/';
                size_t path_len = base_len + 1 + tail_len + 5 + 1;
                char  *path     = malloc(path_len);
                if (!path) { free(tail_buf); return NULL; }
                snprintf(path, path_len, "%s/%s.mach", base, tail_buf);
                free(tail_buf);
                return path;
            }
            else
            {
                size_t path_len = base_len + 1 + 4 + 5 + 1; // "/" + "main" + ".mach" + NUL
                char  *path     = malloc(path_len);
                if (!path) return NULL;
                snprintf(path, path_len, "%s/main.mach", base);
                return path;
            }
        }
    }

    // fallback to first search path, if any
    if (manager->search_count > 0)
    {
        const char *base = manager->search_paths[0];
        size_t      base_len = strlen(base);
        if (tail)
        {
            size_t tail_len = strlen(tail);
            char  *tail_buf = malloc(tail_len + 1);
            if (!tail_buf) return NULL;
            memcpy(tail_buf, tail, tail_len + 1);
            for (char *p = tail_buf; *p; ++p) if (*p == '.') *p = '/';
            size_t path_len = base_len + 1 + head_len + 1 + tail_len + 5 + 1;
            char  *path     = malloc(path_len);
            if (!path) { free(tail_buf); return NULL; }
            snprintf(path, path_len, "%s/%.*s/%s.mach", base, (int)head_len, module_fqn, tail_buf);
            free(tail_buf);
            return path;
        }
        else
        {
            size_t path_len = base_len + 1 + head_len + 1 + 4 + 5 + 1;
            char  *path     = malloc(path_len);
            if (!path) return NULL;
            snprintf(path, path_len, "%s/%.*s/main.mach", base, (int)head_len, module_fqn);
            return path;
        }
    }

    return NULL;
}

void module_manager_init(ModuleManager *manager)
{
    manager->capacity     = 32;
    manager->count        = 0;
    manager->modules      = calloc(manager->capacity, sizeof(Module *));
    manager->search_paths = NULL;
    manager->search_count = 0;
    manager->alias_names  = NULL;
    manager->alias_paths  = NULL;
    manager->alias_count  = 0;
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

    // clean up aliases
    for (int i = 0; i < manager->alias_count; i++)
    {
        free(manager->alias_names[i]);
        free(manager->alias_paths[i]);
    }
    free(manager->alias_names);
    free(manager->alias_paths);

    // clean up errors
    module_error_list_dnit(&manager->errors);
}

void module_manager_add_search_path(ModuleManager *manager, const char *path)
{
    manager->search_paths                        = realloc(manager->search_paths, (manager->search_count + 1) * sizeof(char *));
    manager->search_paths[manager->search_count] = strdup(path);
    manager->search_count++;
}

void module_manager_add_alias(ModuleManager *manager, const char *name, const char *base_dir)
{
    if (!name || !base_dir)
        return;
    manager->alias_names = realloc(manager->alias_names, (manager->alias_count + 1) * sizeof(char *));
    manager->alias_paths = realloc(manager->alias_paths, (manager->alias_count + 1) * sizeof(char *));
    manager->alias_names[manager->alias_count] = strdup(name);
    manager->alias_paths[manager->alias_count] = strdup(base_dir);
    manager->alias_count++;
}

void module_manager_set_config(ModuleManager *manager, void *config, const char *project_dir)
{
    manager->config      = config;
    manager->project_dir = project_dir;
}

char *module_path_to_file_path(ModuleManager *manager, const char *module_fqn)
{
    if (!module_fqn)
        return NULL;

    // prefer config resolution if available
    if (manager->config && manager->project_dir)
        return config_resolve_module_fqn((ProjectConfig *)manager->config, manager->project_dir, module_fqn);

    const char *dot      = strchr(module_fqn, '.');
    size_t      head_len = dot ? (size_t)(dot - module_fqn) : strlen(module_fqn);
    const char  *tail    = dot ? dot + 1 : NULL;

    // aliases: name -> base dir
    for (int i = 0; i < manager->alias_count; i++)
    {
        if (strlen(manager->alias_names[i]) == head_len && strncmp(manager->alias_names[i], module_fqn, head_len) == 0)
        {
            const char *base = manager->alias_paths[i];
            size_t      base_len = strlen(base);
            if (tail)
            {
                size_t tail_len = strlen(tail);
                char  *tail_buf = malloc(tail_len + 1);
                if (!tail_buf) return NULL;
                memcpy(tail_buf, tail, tail_len + 1);
                for (char *p = tail_buf; *p; ++p) if (*p == '.') *p = '/';
                size_t path_len = base_len + 1 + tail_len + 5 + 1; // '/' + tail + '.mach' + NUL
                char  *path = malloc(path_len);
                if (!path) { free(tail_buf); return NULL; }
                snprintf(path, path_len, "%s/%s.mach", base, tail_buf);
                free(tail_buf);
                FILE *f = fopen(path, "r");
                if (f) { fclose(f); return path; }
                free(path);
                return NULL;
            }
            else
            {
                size_t path_len = base_len + 1 + 4 + 5 + 1; // '/' + 'main' + '.mach' + NUL
                char  *path = malloc(path_len);
                if (!path) return NULL;
                snprintf(path, path_len, "%s/main.mach", base);
                FILE *f = fopen(path, "r");
                if (f) { fclose(f); return path; }
                free(path);
                return NULL;
            }
        }
    }

    // search paths: base/name/(tail or main).mach
    for (int si = 0; si < manager->search_count; si++)
    {
        const char *base = manager->search_paths[si];
        size_t      base_len = strlen(base);
        if (tail)
        {
            size_t tail_len = strlen(tail);
            char  *tail_buf = malloc(tail_len + 1);
            if (!tail_buf) continue;
            memcpy(tail_buf, tail, tail_len + 1);
            for (char *p = tail_buf; *p; ++p) if (*p == '.') *p = '/';
            size_t path_len = base_len + 1 + head_len + 1 + tail_len + 5 + 1;
            char  *path = malloc(path_len);
            if (!path) { free(tail_buf); continue; }
            snprintf(path, path_len, "%s/%.*s/%s.mach", base, (int)head_len, module_fqn, tail_buf);
            free(tail_buf);
            FILE *f = fopen(path, "r");
            if (f) { fclose(f); return path; }
            free(path);
        }
        else
        {
            size_t path_len = base_len + 1 + head_len + 1 + 4 + 5 + 1;
            char  *path = malloc(path_len);
            if (!path) continue;
            snprintf(path, path_len, "%s/%.*s/main.mach", base, (int)head_len, module_fqn);
            FILE *f = fopen(path, "r");
            if (f) { fclose(f); return path; }
            free(path);
        }
    }

    return NULL;
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

    // special synthetic builtin module: "target"
    bool  is_target_builtin = (strcmp(canonical, "target") == 0);
    char *file_path         = NULL;
    char *source            = NULL;

    if (is_target_builtin)
    {
        file_path = strdup("<builtin:target>");
        source    = generate_target_module_source(manager);
        if (!source)
        {
            module_error_list_add(&manager->errors, canonical, file_path, "failed to generate target module");
            manager->had_error = true;
            free(file_path);
            free(canonical);
            return NULL;
        }
    }
    else
    {
        // find file path via canonical FQN
        file_path = module_path_to_file_path(manager, canonical);
        if (!file_path)
        {
            char *guess = module_guess_file_path(manager, canonical);
            module_error_list_add(&manager->errors, canonical, guess ? guess : "<unknown>", "Could not find module file");
            if (guess) free(guess);
            manager->had_error = true;
            free(canonical);
            return NULL;
        }

    // no info output

        // read and parse module
        source = read_file(file_path);
        if (!source)
        {
            module_error_list_add(&manager->errors, canonical, file_path, "Could not read module file");
            manager->had_error = true;
            free(file_path);
            free(canonical);
            return NULL;
        }
    }

    if (is_target_builtin)
    {
        // no info output
    }

    Lexer lexer;
    lexer_init(&lexer, source);

    Parser parser;
    parser_init(&parser, &lexer);

    AstNode *ast = parser_parse_program(&parser); // modules inside will keep raw paths; normalization handled earlier

    // check for parse errors
    if (!ast || parser.had_error)
    {
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

    // no info output

    // create module
    Module *module = malloc(sizeof(Module));
    module_init(module, canonical, file_path);
    module->ast         = ast;
    module->is_parsed   = true;
    if (is_target_builtin)
        module->needs_linking = true; // ensure builtin target object is emitted
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

// helper: map substring presence (case sensitive) in triple to numeric constants
static unsigned int map_os_id(const char *os_part)
{
    if (!os_part) return 255u;
    if (strstr(os_part, "linux")) return 1u;
    if (strstr(os_part, "darwin") || strstr(os_part, "apple")) return 2u;
    if (strstr(os_part, "windows") || strstr(os_part, "mingw") || strstr(os_part, "win32")) return 3u;
    if (strstr(os_part, "freebsd")) return 4u;
    if (strstr(os_part, "netbsd")) return 5u;
    if (strstr(os_part, "openbsd")) return 6u;
    if (strstr(os_part, "dragonfly")) return 7u;
    if (strstr(os_part, "wasm")) return 8u;
    return 255u; // unknown
}

static unsigned int map_arch_id(const char *arch_part)
{
    if (!arch_part) return 255u;
    if (strncmp(arch_part, "x86_64", 6) == 0) return 1u;
    if (strncmp(arch_part, "aarch64", 7) == 0) return 2u;
    if (strncmp(arch_part, "riscv64", 7) == 0) return 3u;
    if (strncmp(arch_part, "wasm32", 6) == 0) return 4u;
    if (strncmp(arch_part, "wasm64", 6) == 0) return 5u;
    return 255u; // unknown
}

static char *dup_fragment(const char *start, size_t len)
{
    char *s = malloc(len + 1);
    if (!s) return NULL;
    memcpy(s, start, len);
    s[len] = '\0';
    return s;
}

static void split_triple(const char *triple, char **arch_out, char **os_out)
{
    *arch_out = NULL;
    *os_out   = NULL;
    if (!triple) return;
    const char *first = strchr(triple, '-');
    if (!first)
    {
        *arch_out = strdup(triple);
        return;
    }
    *arch_out = dup_fragment(triple, (size_t)(first - triple));
    const char *rest = first + 1; // vendor or os
    // find second dash to locate os portion
    const char *second = strchr(rest, '-');
    if (!second)
    {
        *os_out = strdup(rest);
        return;
    }
    const char *third = strchr(second + 1, '-');
    if (!third)
    {
        // assume second segment is vendor, third (second->end) is os
        *os_out = strdup(second + 1);
    }
    else
    {
        // treat segment between second and third as os
        *os_out = dup_fragment(second + 1, (size_t)(third - (second + 1)));
    }
}

static char *generate_target_module_source(ModuleManager *manager)
{
    const char *triple = NULL;
    if (manager && manager->config)
    {
        ProjectConfig *pc = (ProjectConfig *)manager->config;
        TargetConfig  *tc = config_get_default_target(pc);
        if (tc && tc->target_triple)
            triple = tc->target_triple;
    }
    if (!triple)
    {
        triple = LLVMGetDefaultTargetTriple();
    }

    char *arch_part = NULL;
    char *os_part   = NULL;
    split_triple(triple, &arch_part, &os_part);

    unsigned int os_id   = map_os_id(os_part);
    unsigned int arch_id = map_arch_id(arch_part);

    // pointer width & endianness
    unsigned int ptr_width = (unsigned int)(sizeof(void *) * 8);
    union { uint16_t v; unsigned char b[2]; } u; u.v = 0x0102;
    unsigned int endian = (u.b[0] == 0x02) ? 0u : 1u; // 0 little, 1 big

    // debug flag heuristic: treat non-optimized builds as debug (opt_level 0 or 1)
    unsigned int debug_flag = 0u;
    // we cannot easily read opt level here without invasive changes; leave 0 for now

    // build source buffer
    char buffer[2048];
    snprintf(buffer, sizeof(buffer),
             "pub val OS_LINUX: u32 = 1;\n"
             "pub val OS_DARWIN: u32 = 2;\n"
             "pub val OS_WINDOWS: u32 = 3;\n"
             "pub val OS_FREEBSD: u32 = 4;\n"
             "pub val OS_NETBSD: u32 = 5;\n"
             "pub val OS_OPENBSD: u32 = 6;\n"
             "pub val OS_DRAGONFLY: u32 = 7;\n"
             "pub val OS_WASM: u32 = 8;\n"
             "pub val OS_UNKNOWN: u32 = 255;\n"
             "pub val ARCH_X86_64: u32 = 1;\n"
             "pub val ARCH_AARCH64: u32 = 2;\n"
             "pub val ARCH_RISCV64: u32 = 3;\n"
             "pub val ARCH_WASM32: u32 = 4;\n"
             "pub val ARCH_WASM64: u32 = 5;\n"
             "pub val ARCH_UNKNOWN: u32 = 255;\n"
             "pub val OS: u32 = %u;\n"
             "pub val ARCH: u32 = %u;\n"
             "pub val PTR_WIDTH: u8 = %u;\n"
             "pub val ENDIAN: u8 = %u;\n"
             "pub val DEBUG: u8 = %u;\n"
             "pub val OS_NAME: []u8 = \"%s\";\n"
             "pub val ARCH_NAME: []u8 = \"%s\";\n",
             os_id, arch_id, ptr_width, endian, debug_flag,
             os_part ? os_part : "unknown", arch_part ? arch_part : "unknown");

    char *out = strdup(buffer);
    free(arch_part);
    free(os_part);
    // if we allocated triple via LLVM fallback (char*) we leak if not freed; acceptable minimal impact
    return out;
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
                semantic_analyzer_set_module(&analyzer, module->name);

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

            // no info output

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
