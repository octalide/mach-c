#include "project.h"
#include "ioutil.h"
#include "ast.h"
#include "lexer.h"
#include "type.h"

#include <stdio.h>
#include <string.h>

File *file_read(char *path)
{
    File *file = calloc(sizeof(File), 1);
    file->path = strdup(path);

    file->source = read_file(path);
    if (file->source == NULL)
    {
        free(file);
        return NULL;
    }

    file->parser = parser_new(lexer_new(file->source));
    file->ast = NULL;

    return file;
}

void file_free(File *file)
{
    if (file == NULL)
    {
        return;
    }

    free(file->path);
    file->path = NULL;

    parser_free(file->parser);
    file->parser = NULL;

    node_free(file->ast);
    file->ast = NULL;

    free(file);
}

Module *module_new()
{
    Module *module = calloc(sizeof(Module), 1);
    module->name = NULL;
    module->ast = NULL;

    return module;
}

void module_free(Module *module)
{
    if (module == NULL)
    {
        return;
    }

    free(module->name);
    module->name = NULL;

    node_free(module->ast);
    module->ast = NULL;

    free(module);
}

Project *project_new()
{
    Project *project = calloc(sizeof(Project), 1);
    project->type = PROJECT_TYPE_EXE;
    project->path_project = NULL;
    project->name = NULL;
    project->version = NULL;
    project->path_dep = NULL;
    project->path_lib = NULL;
    project->path_out = NULL;
    project->path_src = NULL;

    project->files = calloc(sizeof(File *), 2);
    project->files[0] = NULL;

    project->modules = calloc(sizeof(Module *), 2);
    project->modules[0] = NULL;

    project->symbols = scope_new();

    return project;
}

void project_free(Project *project)
{
    if (project == NULL)
    {
        return;
    }

    free(project->path_project);
    project->path_project = NULL;

    free(project->name);
    project->name = NULL;

    free(project->version);
    project->version = NULL;

    free(project->path_dep);
    project->path_dep = NULL;

    free(project->path_lib);
    project->path_lib = NULL;

    free(project->path_out);
    project->path_out = NULL;

    free(project->path_src);
    project->path_src = NULL;

    free(project->entrypoint);
    project->entrypoint = NULL;

    free(project->targets);
    project->targets = NULL;

    for (size_t i = 0; project->files[i] != NULL; i++)
    {
        file_free(project->files[i]);
        project->files[i] = NULL;
    }
    free(project->files);
    project->files = NULL;

    for (size_t i = 0; project->modules[i] != NULL; i++)
    {
        module_free(project->modules[i]);
        project->modules[i] = NULL;
    }
    free(project->modules);
    project->modules = NULL;

    node_free(project->program);
    project->program = NULL;

    scope_free(project->symbols);
    project->symbols = NULL;

    free(project);
}

void file_parse(File *file)
{
    file->ast = parser_parse(file->parser, file->path);
}

char *str_replace(char *orig, char *rep, char *with)
{
    if (!orig || !rep || strlen(rep) == 0)
    {
        return NULL;
    }

    if (!with)
    {
        with = "";
    }

    int len_rep = strlen(rep);
    int len_with = strlen(with);
    int count = 0;

    char *ins = orig;
    while ((ins = strstr(ins, rep)))
    {
        count++;
        ins += len_rep;
    }

    char *result = malloc(strlen(orig) + (len_with - len_rep) * count + 1);
    if (!result)
        return NULL;

    char *tmp = result;
    while (count--)
    {
        ins = strstr(orig, rep);
        int len_front = ins - orig;
        tmp = strncpy(tmp, orig, len_front) + len_front;
        tmp = strcpy(tmp, with) + len_with;
        orig += len_front + len_rep;
    }

    strcpy(tmp, orig);

    return result;
}

// build module path from the containing node, which has the format of chained
// identifier member accesses
char *module_parts_join(Node *module_path)
{
    if (module_path == NULL || (module_path->kind != NODE_EXPR_MEMBER && module_path->kind != NODE_IDENTIFIER))
    {
        return NULL;
    }

    if (module_path->kind == NODE_IDENTIFIER)
    {
        return strdup(module_path->data.identifier->name);
    }

    Node *current = module_path;
    char *joined = strdup(current->data.expr_member->member->data.identifier->name);
    while (current->kind == NODE_EXPR_MEMBER)
    {
        current = current->data.expr_member->target;
        if (current->kind == NODE_IDENTIFIER)
        {
            char *part = current->data.identifier->name;
            char *new_joined = malloc(strlen(joined) + strlen(part) + 2);
            sprintf(new_joined, "%s.%s", part, joined);
            free(joined);
            joined = new_joined;
            break;
        }

        char *part = current->data.expr_member->member->data.identifier->name;
        char *new_joined = malloc(strlen(joined) + strlen(part) + 2);
        sprintf(new_joined, "%s.%s", part, joined);
        free(joined);
        joined = new_joined;
    }

    return joined;
}

int module_add_file_ast(Module *module, File *file)
{
    if (module->ast == NULL)
    {
        module->ast = node_new(NODE_MODULE);
        module->ast->data.module->name = strdup(module->name);
    }

    if (file->ast == NULL)
    {
        // file ast cannot be null
        return 1;
    }

    if (module->ast->data.module->files == NULL)
    {
        module->ast->data.module->files = node_list_new();
    }

    node_list_add(&module->ast->data.module->files, file->ast);

    file->ast->parent = module->ast;

    return 0;
}

// resolve macros in paths. macros available are:
// - {path_project} -> project->path_project
// - {path_src}     -> project->path_src
// - {path_out}     -> project->path_out
// - {path_dep}     -> project->path_dep
// - {path_lib}     -> project->path_lib
char *project_resolve_macros(Project *project, char *str)
{
    char *path_project = project->path_project;
    char *path_src = project->path_src;
    char *path_out = project->path_out;
    char *path_dep = project->path_dep;
    char *path_lib = project->path_lib;

    char *str_resolved = str_replace(str, "{path_project}", path_project);
    str_resolved = str_replace(str_resolved, "{path_src}", path_src);
    str_resolved = str_replace(str_resolved, "{path_out}", path_out);
    str_resolved = str_replace(str_resolved, "{path_dep}", path_dep);
    str_resolved = str_replace(str_resolved, "{path_lib}", path_lib);

    return str_resolved;
}

Module *project_find_module(Project *project, char *name)
{
    if (project->modules == NULL)
    {
        return NULL;
    }

    for (size_t i = 0; project->modules[i] != NULL; i++)
    {
        if (strcmp(project->modules[i]->name, name) == 0)
        {
            return project->modules[i];
        }
    }

    return NULL;
}

void project_add_module(Project *project, Module *module)
{
    if (project->modules == NULL)
    {
        project->modules = calloc(sizeof(Module *), 2);
        project->modules[0] = module;
        project->modules[1] = NULL;
        return;
    }
    
    size_t i = 0;
    while (project->modules[i] != NULL)
    {
        i++;
    }

    project->modules = realloc(project->modules, sizeof(Module *) * (i + 2));
    project->modules[i] = module;
    project->modules[i + 1] = NULL;
}

void project_add_file(Project *project, File *file)
{
    if (project->files == NULL)
    {
        project->files = calloc(sizeof(File *), 2);
        project->files[0] = file;
        project->files[1] = NULL;
    }
    else
    {
        size_t i = 0;
        while (project->files[i] != NULL)
        {
            i++;
        }

        project->files = realloc(project->files, sizeof(File *) * (i + 2));
        project->files[i] = file;
        project->files[i + 1] = NULL;
    }
}

void project_discover_files(Project *project)
{
    char **files = list_files_recursive(project->path_src, NULL, 0);
    if (files == NULL)
    {
        printf("  error: could not list files in directory: %s\n", project->path_src);
        return;
    }

    for (size_t i = 0; files[i] != NULL; i++)
    {
        char *ext = path_get_extension(files[i]);
        if (ext == NULL || strcmp(ext, "mach") != 0)
        {
            continue;
        }

        char *joined = path_join(project->path_src, files[i]);
        File *file = file_read(joined);
        free(joined);
        if (file == NULL)
        {
            printf("  error: could not read file: %s\n", files[i]);
            continue;
        }

        // check if file is empty
        if (file->source == NULL || strlen(file->source) == 0)
        {
            printf("  warning: file is empty: %s\n", files[i]);
            file_free(file);
            continue;
        }

        printf("  found file: %s\n", files[i]);
        project_add_file(project, file);
    }

    for (size_t i = 0; files[i] != NULL; i++)
    {
        free(files[i]);
    }
    free(files);
}

void project_parse_all(Project *project)
{
    if (project->files == NULL)
    {
        return;
    }

    for (size_t i = 0; project->files[i] != NULL; i++)
    {
        printf("  parsing file: %s\n", project->files[i]->path);
        file_parse(project->files[i]);
    }
}

typedef struct CBContextParserError
{
    Project *project;
    File *file;

    int count_errors;
} CBContextParserError;

void project_print_parse_errors_cb(void *context, Node *node, int depth)
{
    CBContextParserError *ctx = context;
    if (node->kind == NODE_ERROR)
    {
        ctx->count_errors++;
        printf("\n");
        parser_print_error(ctx->file->parser, node);
    }
}

int project_print_parse_errors(Project *project)
{
    if (project->files == NULL)
    {
        return 0;
    }

    int count_errors = 0;

    for (size_t i = 0; project->files[i] != NULL; i++)
    {
        if (project->files[i]->ast == NULL)
        {
            printf("error: could not parse file: %s\n", project->files[i]->path);
            continue;
        }

        CBContextParserError *ctx = calloc(sizeof(CBContextParserError), 1);
        ctx->project = project;
        ctx->file = project->files[i];

        node_walk(ctx, project->files[i]->ast, project_print_parse_errors_cb);

        count_errors += ctx->count_errors;
    }

    return count_errors;
}

int project_modularize_files(Project *project)
{
    int count_errors = 0;

    for (size_t i = 0; project->files[i] != NULL; i++)
    {
        if (project->files[i]->ast == NULL)
        {
            printf("error: file ast is NULL: %s\n", project->files[i]->path);
            continue;
        }

        // discover file module from `mod` node declared at start of file.
        // the module declaration statement has two rules:
        // - it must be the first statement in the file
        // - it must be the only module statement in the file
        // use these two rules to locate it and build a module path, then find
        // the module or create it based on the path. Note that, unlike other
        // languages, a file's module is NOT dependent on it's location on disk.
        Node *module_stmt = project->files[i]->ast->data.file->statements[0];
        if (module_stmt->kind != NODE_STMT_MOD)
        {
            printf("error: file does not start with module declaration: %s\n", project->files[i]->path);
            count_errors++;
            continue;
        }

        char *module_path = module_parts_join(module_stmt->data.stmt_mod->module_path);
        if (module_path == NULL)
        {
            printf("error: could not build module path: %s\n", project->files[i]->path);
            count_errors++;
            continue;
        }

        Module *module = project_find_module(project, module_path);
        if (module == NULL)
        {
            module = module_new();
            module->name = strdup(module_path);
            project_add_module(project, module);
        }

        int result = module_add_file_ast(module, project->files[i]);
        if (result != 0)
        {
            printf("error: could not add file to module: %s\n", project->files[i]->path);
            count_errors++;
            continue;
        }

        printf("  modularized file: %s -> %s\n", project->files[i]->path, module_path);

        free(module_path);
    }

    return count_errors;
}

void project_combine_modules(Project *project)
{
    Node *program = node_new(NODE_PROGRAM);
    program->data.program->name = strdup(project->name);

    for (size_t i = 0; project->modules[i] != NULL; i++)
    {
        project->modules[i]->ast->parent = program;
        node_list_add(&program->data.program->modules, project->modules[i]->ast);
    }

    project->program = program;
}

void project_add_base_symbols(Project *project)
{
    Symbol *s_void = symbol_new();
    s_void->name = malloc(5);
    sprintf(s_void->name, "void");
    s_void->type = type_new(TYPE_VOID);
    scope_add(project->symbols, s_void);

    Symbol *s_u8 = symbol_new();
    s_u8->name = malloc(3);
    sprintf(s_u8->name, "u8");
    s_u8->type = type_new(TYPE_U8);
    scope_add(project->symbols, s_u8);

    Symbol *s_u16 = symbol_new();
    s_u16->name = malloc(3);
    sprintf(s_u16->name, "u16");
    s_u16->type = type_new(TYPE_U16);
    scope_add(project->symbols, s_u16);

    Symbol *s_u32 = symbol_new();
    s_u32->name = malloc(3);
    sprintf(s_u32->name, "u32");
    s_u32->type = type_new(TYPE_U32);
    scope_add(project->symbols, s_u32);

    Symbol *s_u64 = symbol_new();
    s_u64->name = malloc(3);
    sprintf(s_u64->name, "u64");
    s_u64->type = type_new(TYPE_U64);
    scope_add(project->symbols, s_u64);

    Symbol *s_i8 = symbol_new();
    s_i8->name = malloc(3);
    sprintf(s_i8->name, "i8");
    s_i8->type = type_new(TYPE_I8);
    scope_add(project->symbols, s_i8);

    Symbol *s_i16 = symbol_new();
    s_i16->name = malloc(3);
    sprintf(s_i16->name, "i16");
    s_i16->type = type_new(TYPE_I16);
    scope_add(project->symbols, s_i16);

    Symbol *s_i32 = symbol_new();
    s_i32->name = malloc(3);
    sprintf(s_i32->name, "i32");
    s_i32->type = type_new(TYPE_I32);
    scope_add(project->symbols, s_i32);

    Symbol *s_i64 = symbol_new();
    s_i64->name = malloc(3);
    sprintf(s_i64->name, "i64");
    s_i64->type = type_new(TYPE_I64);
    scope_add(project->symbols, s_i64);

    Symbol *s_f32 = symbol_new();
    s_f32->name = malloc(3);
    sprintf(s_f32->name, "f32");
    s_f32->type = type_new(TYPE_F32);
    scope_add(project->symbols, s_f32);

    Symbol *s_f64 = symbol_new();
    s_f64->name = malloc(3);
    sprintf(s_f64->name, "f64");
    s_f64->type = type_new(TYPE_F64);
    scope_add(project->symbols, s_f64);
}

typedef struct CBContextError
{
    char *message;
    Node *node;
} CBContextError;

typedef struct CBContextProject
{
    Project *project;
    CBContextError **errors;
} CBContextProject;

int count_context_errors(CBContextProject *ctx)
{
    int count = 0;
    while (ctx->errors[count] != NULL)
    {
        count++;
    }

    return count;
}

void add_error_to_context(CBContextProject *ctx, char *message, Node *node)
{
    CBContextError *error = calloc(1, sizeof(CBContextError));
    error->message = strdup(message);
    error->node = node;

    size_t i = count_context_errors(ctx);

    ctx->errors = realloc(ctx->errors, sizeof(CBContextError *) * (i + 2));
    ctx->errors[i] = error;
    ctx->errors[i + 1] = NULL;
}

void cb_populate_symbol_names_val(void *context, Node *node, int depth)
{
    CBContextProject *ctx = context;

    if (node->kind != NODE_STMT_VAL)
    {
        return;
    }

    // find containing module
    Node *node_module = node_find_parent(node, NODE_MODULE);
    if (node_module == NULL)
    {
        add_error_to_context(ctx, "unable to find containing module", node);
        return;
    }

    // join node_module->name and node->data.stmt_val->identifier->name with `:`
    char *symbol_name = malloc(strlen(node_module->data.module->name) + strlen(node->data.stmt_val->identifier->data.identifier->name) + 2);
    sprintf(symbol_name, "%s:%s", node_module->data.module->name, node->data.stmt_val->identifier->data.identifier->name);

    // create symbol
    Symbol *symbol = symbol_new();
    symbol->name = symbol_name;

    // add symbol to symbol table
    scope_add(ctx->project->symbols, symbol);
}

int project_populate_symbols(Project *project) {
    // add symbols for basic types
    project_add_base_symbols(project);

    CBContextProject *ctx = calloc(1, sizeof(CBContextProject));
    ctx->project = project;
    ctx->errors = calloc(1, sizeof(CBContextError *));
    ctx->errors[0] = NULL;
    
    for (size_t i = 0; project->modules[i] != NULL; i++)
    {
        node_walk(ctx, project->modules[i]->ast, cb_populate_symbol_names_val);

        // populate symbol table with the names from any `str` keywords
        // populate symbol table with the names from any `uni` keywords
        // populate symbol table with the names from any `fun` keywords
        // populate symbol table with the names from any `def` keywords
        // populate symbol table with the names from any `ext` keywords
    }

    // populate symbol table with the names from any compile-time constants
    //   declared with the `val` keyword.
    node_walk(ctx, project->program, cb_populate_symbol_names_val);
    int count_errors = count_context_errors(ctx);
    if (count_errors != 0)
    {
        for (size_t i = 0; ctx->errors[i] != NULL; i++)
        {
            printf("error: %s\n", ctx->errors[i]->message);
        }

        return count_errors;
    }

    // rules/assumptions:
    // - project scope contains all builtin symbols
    // - each module scope is localized
    // - each scope inside of a module is directly heirarchal
    //
    // implemenation:
    // - direct parent discovery
    // - no sibling discovery
    // - every module is a sibling (?)
    //   - std.foo     <- NOT "parent" of `foo.bar`, distinct
    //   - std.foo.bar
    //
    // code explanation:
    // ``` src/foo/bar/baz.mach
    // mod foo.bar;  # current module
    //
    // use foo: foo; # required to import foo, even though is organized as "parent"
    //
    // def baz: u32; # alias for type, defined only in `foo.bar`
    // ```
    //
    // ``` src/foo/qux.mach
    // mod foo;
    //
    // use foo.bar; # short form for `use bar: foo.bar;`
    //
    // fun main(): u32 {
    //     ret bar.baz; # fully resolves to `foo.bar.baz`
    // }
    // ```

    // node_walk(ctx, project->program, project_populate_symbols_cb);
    // count_errors = count_context_errors(ctx);
    // if (count_errors != 0)
    // {
    //     for (size_t i = 0; ctx->errors[i] != NULL; i++)
    //     {
    //         printf("error: %s\n", ctx->errors[i]->message);
    //     }
    // }

    return 0;
}

int project_validate_types(Project *project) {
    return 0;
}

int project_validate_control_flow(Project *project) {
    return 0;
}

int project_validate_data_flow(Project *project) {
    return 0;
}

int project_analyze(Project *project)
{
    if (project->program == NULL || project->program->kind != NODE_PROGRAM)
    {
        printf("error: program node is invalid\n");
        return 1;
    }

    int count_error_populate_symbols = project_populate_symbols(project);
    if (count_error_populate_symbols != 0) {
        printf("populate symbols returned %d errors\n", count_error_populate_symbols);
        return count_error_populate_symbols;
    }

    int count_error_validate_types = project_validate_types(project);
    if (count_error_validate_types != 0) {
        printf("validate types returned %d errors\n", count_error_validate_types);
        return count_error_validate_types;
    }

    int count_error_validate_control_flow = project_validate_control_flow(project);
    if (count_error_validate_control_flow != 0) {
        printf("validate control_flow returned %d errors\n", count_error_validate_control_flow);
        return count_error_validate_control_flow;
    }

    int count_error_validate_data_flow = project_validate_data_flow(project);
    if (count_error_validate_data_flow != 0) {
        printf("validate data_flow returned %d errors\n", count_error_validate_data_flow);
        return count_error_validate_data_flow;
    }
    
    return 0;
}
