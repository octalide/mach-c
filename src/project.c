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

    project->symbol_table = symbol_table_new();

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

    symbol_table_free(project->symbol_table);
    project->symbol_table = NULL;

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

// Type *resolve_nested_node_type(Project *project, Node *node) {
//     switch (node->kind) {
//         case NODE_TYPE_POINTER:
//             Type *type_ptr = type_new(TYPE_POINTER);
//             type_ptr->data.type_pointer->base_type = resolve_nested_node_type(project, node->data.type_pointer->type);
//             return type_ptr;
//         case NODE_TYPE_ARRAY:
//             Type *type_arr = type_new(TYPE_ARRAY);
//             // array size has a few possible definitions:
//             // 1. empty for unbounded size
//             // 2. literal integer expression
//             // 3. expression that must be evaluated at compile time
//             // 4. identifier that links to a symobol of integer type

//             // case 1:
//             if (node->data.type_array->size == NULL)
//             {
//                 type_arr->data.type_array->length = -1;
//             }

//             switch (node->data.type_array->size->kind) {
//                 // case 2:
//                 case NODE_LIT_INT:
//                     // find containing file node
//                     Node *node_file = node_find_parent(node, NODE_FILE);
//                     if (node_file == NULL || node_file->data.file->parser == NULL || node_file->data.file->parser->lexer == NULL)
//                     {
//                         return NULL;
//                     }

//                     // evaluate literal integer expression
//                     type_arr->data.type_array->length = lexer_eval_lit_int(node_file->data.file->parser->lexer, node->data.type_array->size->token);
//                     break;
//                 // case 3:
//                 // NOTE:
//                 //   Inside of unary and binary expressions, all leaf nodes must
//                 //   be compile time constant integers or symbols that point to
//                 //   them.
//                 case NODE_EXPR_UNARY:
//                     // examine each member to ensure compile time definition
//                     // NOTE: this restriction excludes the usability of anything
//                     //   but basic types using basic arithematic.
//                     // The only valid unary operators here are OP_ADD and OP_SUB
//                     switch (node->data.type_array->size->data.expr_unary->op)
//                     {
//                         case OP_ADD:
//                             switch (node->data.type_array->size->data.expr_unary->target->kind)
//                             {
//                                 case NODE_LIT_INT:
//                                     type_arr->data.type_array->length = lexer_eval_lit_int(node_file->data.file->parser->lexer, node->data.type_array->size->data.expr_unary->target->token);
//                                     break;
//                                 case NODE_IDENTIFIER:
//                                     break;
//                                 case NODE_EXPR_MEMBER:
//                                 default:
//                                     return NULL;
//                             }

//                         case OP_SUB:
//                         default:
//                             return NULL;
//                     }

//                     break;
//                 case NODE_EXPR_BINARY:
//                     // the only valid biary operators here are mathematical:
//                     // - OP_ADD
//                     // - OP_SUB
//                     // - OP_MUL
//                     // - OP_DIV
//                     // - OP_MOD
//                     // - OP_BITWISE_NOT
//                     // - OP_BITWISE_AND
//                     // - OP_BITWISE_OR
//                     // - OP_BITWISE_XOR
//                     // - OP_BITWISE_SHL
//                     // - OP_BITWISE_SHR

//                     switch (node->data.type_array->size->data.expr_binary->op)
//                     {
//                         case OP_ADD:
//                         case OP_SUB:
//                         case OP_MUL:
//                         case OP_DIV:
//                         case OP_MOD:
//                         case OP_BITWISE_NOT:
//                         case OP_BITWISE_AND:
//                         case OP_BITWISE_OR:
//                         case OP_BITWISE_XOR:
//                         case OP_BITWISE_SHL:
//                         case OP_BITWISE_SHR:
//                         default:
//                             return NULL;
//                     }

//                     break;
//                 // case 4:
//                 case NODE_EXPR_MEMBER:
//                 default:
//                     return NULL;
//             }

//             return type_arr;
//         default:
//             return NULL;
//     }
// }

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
    symbol->type = type_new(TYPE_LAZY);

    // add symbol to symbol table
    symbol_table_add(ctx->project->symbol_table, symbol);
}

void cb_populate_symbols(void *context, Node *node, int depth)
{
    CBContextProject *ctx = context;

    // nodes capable of creating types:
    // - def
    // - ext
    // - str
    // - uni
    // - fun
    //
    // nodes capable of defining symbols:
    // - val
    // - var
    // - def
    // - ext
    // - str
    // - uni
    // - fun
    //
    // Symbol names take the format:
    // <module_path>:<symbol_name>

    switch (node->kind)
    {
    case NODE_STMT_VAL:
    case NODE_STMT_VAR:
    case NODE_STMT_DEF:
        Symbol *symbol = symbol_new();

        // pull identifier from def statement
        symbol->name = strdup(node->data.stmt_def->identifier->data.identifier->name);

        // check if the def statement uses a predefined symbol (alias) or if it
        // defines its own type. simple check for identifier is enough.
        Node *def_type = node->data.stmt_def->type;
        switch (def_type->kind) {
            case NODE_IDENTIFIER:
                Symbol *base = symbol_table_get(ctx->project->symbol_table, def_type->data.identifier->name);

                // if symbol not found, mark for lazy type resolution
                // if symbol found, link to the original type
                if (base == NULL)
                {
                    symbol->type = type_new(TYPE_LAZY);
                    symbol->type->name = strdup(def_type->data.identifier->name);
                } else {
                    symbol->type = base->type;
                }
                
                break;
            case NODE_TYPE_FUN:
                Type *type = type_new(TYPE_FUNCTION);
                for (size_t i = 0; def_type->data.type_function->parameters[i] != NULL; i++)
                {
                    Type *param = type_new(TYPE_LAZY);
                    param->name = strdup(def_type->data.type_function->parameters[i]->data.identifier->name);
                    type->data.type_function->parameters[i] = param;
                }

                type->data.type_function->parameters;
                type->data.type_function->return_type;

                break;
            case NODE_TYPE_STR:
            case NODE_TYPE_UNI:
            case NODE_TYPE_POINTER:
            case NODE_TYPE_ARRAY:
                symbol->type = type_new(TYPE_ARRAY);
                break;
            default:
                break;
        }
    case NODE_STMT_EXT:
    case NODE_STMT_STR:
    case NODE_STMT_UNI:
    case NODE_STMT_FUN:
    default:
        break;
    }
}

int project_populate_symbols(Project *project) {
    CBContextProject *ctx = calloc(1, sizeof(CBContextProject));
    ctx->project = project;
    ctx->errors = calloc(1, sizeof(CBContextError *));
    ctx->errors[0] = NULL;

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

    // print symbols currently in symbol table
    symbol_table_print(project->symbol_table);

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
