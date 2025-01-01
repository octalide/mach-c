#include "project.h"
#include "ioutil.h"
#include "ast.h"

#include <stdio.h>

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

    for (size_t i = 0; project->modules[i] != NULL; i++)
    {
        module_free(project->modules[i]);
        project->modules[i] = NULL;
    }
    free(project->modules);
    project->modules = NULL;

    for (size_t i = 0; project->files[i] != NULL; i++)
    {
        file_free(project->files[i]);
        project->files[i] = NULL;
    }
    free(project->files);
    project->files = NULL;

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

char *module_parts_join(char **parts)
{
    if (parts == NULL)
    {
        return NULL;
    }

    char *path = NULL;
    for (size_t i = 0; parts[i] != NULL; i++)
    {
        if (path == NULL)
        {
            path = strdup(parts[i]);
        }
        else
        {
            path = str_replace(path, ".", parts[i]);
        }
    }

    return path;
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

void project_add_symbol(Project *project, Symbol *symbol)
{
    symbol_table_add(project->symbol_table, symbol);
}

void project_discover_files(Project *project)
{
    char **files = list_files_recursive(project->path_src, NULL, 0);
    if (files == NULL)
    {
        printf("error: could not list files in directory: %s\n", project->path_src);
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
            printf("error: could not read file: %s\n", files[i]);
            continue;
        }

        // check if file is empty
        if (file->source == NULL || strlen(file->source) == 0)
        {
            printf("warning: file is empty: %s\n", files[i]);
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

typedef struct ParserErrorCBContext
{
    Project *project;
    File *file;

    int count_errors;
} ParserErrorCBContext;

void project_print_parse_errors_cb(void *context, Node *node, int depth)
{
    ParserErrorCBContext *ctx = context;
    if (node->kind == NODE_ERROR)
    {
        ctx->count_errors++;
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

        ParserErrorCBContext *ctx = calloc(sizeof(ParserErrorCBContext), 1);
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

        char *module_path = module_parts_join(module_stmt->data.stmt_mod->module_path->data.module_path->parts);
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

int project_analysis(Project *project)
{
    if (project->modules == NULL)
    {
        return 0;
    }

    int count_errors = 0;

    return count_errors;
}
