#include "project.h"
#include "ioutil.h"
#include "ast.h"

#include <stdio.h>

File *file_read(char *path)
{
    File *file = calloc(sizeof(File), 1);
    file->path = path;

    file->source = read_file(path);
    if (file->source == NULL)
    {
        free(file);
        return NULL;
    }

    file->lexer = lexer_new(file->source);
    file->parser = parser_new(file->lexer);
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

    free(file->source);
    file->source = NULL;

    lexer_free(file->lexer);
    file->lexer = NULL;

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
    module->files = NULL;
    module->symbols = NULL;

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

    for (size_t i = 0; module->files[i] != NULL; i++)
    {
        file_free(module->files[i]);
    }
    free(module->files);
    module->files = NULL;

    symbol_table_free(module->symbols);
    module->symbols = NULL;

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
    project->modules = NULL;
    project->symbols = NULL;

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

    for (size_t i = 0; project->modules[i] != NULL; i++)
    {
        module_free(project->modules[i]);
    }
    free(project->modules);
    project->modules = NULL;

    symbol_table_free(project->symbols);
    project->symbols = NULL;

    free(project);
}

void file_parse(File *file)
{
    file->ast = parser_parse(file->parser);
}

void module_add_file(Module *module, File *file)
{
    if (module->files == NULL)
    {
        module->files = calloc(sizeof(File *), 2);
        module->files[0] = file;
        module->files[1] = NULL;
        return;
    }

    size_t i = 0;
    while (module->files[i] != NULL)
    {
        i++;
    }

    module->files = realloc(module->files, sizeof(File *) * (i + 2));
    module->files[i] = file;
    module->files[i + 1] = NULL;
}

void module_add_symbol(Module *module, Symbol *symbol)
{
    symbol_table_add(module->symbols, symbol);
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

// module path is the local path of the file's directory relative to the project path.
// separated by a dot delimiter.
// build this module path then try to find the module. if it does not exist,
//   create it.
void project_add_file(Project *project, File *file)
{
    char *path_rel = path_relative(project->path_project, file->path);
    if (path_rel == NULL)
    {
        printf("error: could not determine relative path for file: %s\n", file->path);
        return;
    }

    // strip the file off path_rel and exchange slashes for dots
    char *module_name = path_dirname(path_rel);
    for (size_t i = 0; module_name[i] != '\0'; i++)
    {
        if (module_name[i] == '/')
        {
            module_name[i] = '.';
        }

        if (module_name[i] == '\\')
        {
            module_name[i] = '.';
        }
    }

    // if module name is ".", change to "src"
    if (strcmp(module_name, ".") == 0)
    {
        module_name = project->name;
    }

    Module *module = project_find_module(project, module_name);
    if (module == NULL)
    {
        module = module_new();
        module->name = module_name;
        module->files = NULL;
        module->symbols = symbol_table_new();

        project_add_module(project, module);
    }

    module_add_file(module, file);
}

void project_add_symbol(Project *project, Symbol *symbol)
{
    symbol_table_add(project->symbols, symbol);
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

        File *file = file_read(path_join(project->path_src, files[i]));
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
}

void project_parse_all(Project *project)
{
    if (project->modules == NULL)
    {
        return;
    }

    for (size_t i = 0; project->modules[i] != NULL; i++)
    {
        for (size_t j = 0; project->modules[i]->files[j] != NULL; j++)
        {
            printf("  parsing file: %s\n", project->modules[i]->files[j]->path);
            file_parse(project->modules[i]->files[j]);
        }
    }
}

typedef struct ParserErrorCBContext
{
    Project *project;
    Module *module;
    File *file;

    int count_errors;
} ParserErrorCBContext;

void project_print_parse_errors_cb(void *context, Node *node, int depth)
{
    ParserErrorCBContext *ctx = context;
    if (node->kind == NODE_ERROR)
    {
        parser_print_error(ctx->file->parser, node, ctx->file->path);
    }
}

int project_print_parse_errors(Project *project)
{
    if (project->modules == NULL)
    {
        return 0;
    }

    int count_errors = 0;

    for (size_t i = 0; project->modules[i] != NULL; i++)
    {
        for (size_t j = 0; project->modules[i]->files[j] != NULL; j++)
        {
            if (project->modules[i]->files[j]->ast == NULL)
            {
                printf("error: could not parse file: %s\n", project->modules[i]->files[j]->path);
                continue;
            }

            ParserErrorCBContext *ctx = calloc(sizeof(ParserErrorCBContext), 1);
            ctx->project = project;
            ctx->module = project->modules[i];
            ctx->file = project->modules[i]->files[j];

            node_walk(ctx, project->modules[i]->files[j]->ast, project_print_parse_errors_cb);

            count_errors += ctx->count_errors;
        }
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

    for (size_t i = 0; project->modules[i] != NULL; i++)
    {
        for (size_t j = 0; project->modules[i]->files[j] != NULL; j++)
        {
            if (project->modules[i]->files[j]->ast == NULL)
            {
                printf("warning: file ast is NULL: %s\n", project->modules[i]->files[j]->path);
                continue;
            }
        }
    }

    return count_errors;
}
