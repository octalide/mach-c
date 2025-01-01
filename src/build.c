#include "build.h"
#include "lexer.h"
#include "parser.h"
#include "project.h"
#include "ioutil.h"
#include "cJSON.h"
#include "symbols.h"
#include "ast.h"

#include <stdlib.h>
#include <stdio.h>

void callback_node_debug_printout(void *context, Node *node, int depth)
{
    printf("%*s", depth * 2, "");
    printf("%s\n", node_kind_to_string(node->kind));
}

int build_project_lib(Project *project)
{
    printf("discovering project files...\n");
    project_discover_files(project);

    printf("parsing project files...\n");
    project_parse_all(project);
    project_print_parse_errors(project);

    return 0;
}

int build_project_exe(Project *project)
{
    printf("discovering project files...\n");
    project_discover_files(project);

    printf("parsing project files...\n");
    project_parse_all(project);

    int count_parse_errors = project_print_parse_errors(project);
    if (count_parse_errors == 0)
    {
        printf("no parse errors\n");
    } else {
        printf("%d parse errors. Cannot continue.\n", count_parse_errors);
        return 1;
    }

    printf("combining files into modules for analysis...\n");
    int count_modularize_errors = project_modularize_files(project);
    if (count_modularize_errors == 0)
    {
        printf("no modularization errors\n");
    } else {
        printf("%d modularization errors. Cannot continue.\n", count_modularize_errors);
        return 2;
    }

    printf("project modules:\n");
    for (size_t i = 0; project->modules[i] != NULL; i++)
    {
        printf("  %s\n", project->modules[i]->name);
    }

    printf("project module node structure debug printout:\n");
    for (size_t i = 0; project->modules[i] != NULL; i++)
    {
        printf("  %s\n", project->modules[i]->name);
        node_walk(NULL, project->modules[i]->ast, callback_node_debug_printout);
    }

    printf("performing analysis...\n");
    int count_analysis_errors = project_analysis(project);
    if (count_analysis_errors == 0)
    {
        printf("no analysis errors\n");
    }

    return 0;
}

int build_project(Project *project)
{
    if (project->type == PROJECT_TYPE_EXE)
    {
        return build_project_exe(project);
    }

    return build_project_lib(project);
}

int build_target_file(char *path, int argc, char **argv)
{
    Project *project = project_new();
    project->type = PROJECT_TYPE_EXE;
    project->path_mach_root = getenv("MACH_ROOT") != NULL ? getenv("MACH_ROOT") : DEFAULT_MACH_PATH;
    project->path_project = path_dirname(path);
    project->name = path_dirname(path);
    project->version = calloc(1, 6);
    sprintf(project->version, "0.0.0");
    project->path_src = path_dirname(path);
    project->path_out = path_dirname(path);
    project->path_dep = path_dirname(path);
    project->path_lib = path_dirname(path);
    project->entrypoint = malloc(strlen(project->name) + 6);
    sprintf(project->entrypoint, "%s:main", project->name);

    project->targets = calloc(sizeof(Target), 1);
    project->targets[0] = target_current();
    project->target_count += 1;

    project->symbol_table = symbol_table_new();

    build_project(project);

    project_free(project);

    return 1;
}

int build_target_project(char *path, int argc, char **argv)
{
    char *file = read_file(path);
    if (file == NULL)
    {
        fprintf(stderr, "error: could not read project file: `%s`\n", path);
        return 1;
    }

    cJSON *json = cJSON_Parse(file);
    if (json == NULL)
    {
        fprintf(stderr, "error: could not parse project file: `%s`\n", path);
        free(file);
        return 1;
    }

    // create project object and insert defaults
    Project *project = project_new();
    project->path_project = path_dirname(path);
    project->path_mach_root = getenv("MACH_ROOT") != NULL ? getenv("MACH_ROOT") : DEFAULT_MACH_PATH;
    project->name = path_dirname(path);
    project->version = calloc(1, 6);
    sprintf(project->version, "0.0.0");
    project->path_src = path_join(project->path_project, path_dirname(path));
    project->path_out = path_join(project->path_project, path_dirname(path));
    project->path_dep = path_join(project->path_project, path_dirname(path));
    project->path_lib = path_join(project->path_project, path_dirname(path));
    project->entrypoint = malloc(strlen(project->name) + 6);
    sprintf(project->entrypoint, "%s:main", project->name);

    project->targets = calloc(sizeof(Target), 1);
    project->targets[0] = target_current();
    project->target_count += 1;

    project->symbol_table = symbol_table_new();
    project->modules = NULL;

    const cJSON *version = cJSON_GetObjectItemCaseSensitive(json, "version");
    if (cJSON_IsString(version) && (version->valuestring != NULL))
    {
        project->version = strdup(version->valuestring);
    }

    const cJSON *name = cJSON_GetObjectItemCaseSensitive(json, "name");
    if (cJSON_IsString(name) && (name->valuestring != NULL))
    {
        project->name = strdup(name->valuestring);
    }

    const cJSON *type = cJSON_GetObjectItemCaseSensitive(json, "type");
    if (cJSON_IsString(type) && (type->valuestring != NULL))
    {
        if (strcmp(type->valuestring, "exe") == 0)
        {
            project->type = PROJECT_TYPE_EXE;
        }
        else if (strcmp(type->valuestring, "lib_static") == 0)
        {
            project->type = PROJECT_TYPE_LIB_STATIC;
        }
        else if (strcmp(type->valuestring, "lib_shared") == 0)
        {
            project->type = PROJECT_TYPE_LIB_SHARED;
        }
        else
        {
            fprintf(stderr, "error: unknown project type: `%s`\n", type->valuestring);
            cJSON_Delete(json);
            free(file);
            return 1;
        }
    }

    const cJSON *paths = cJSON_GetObjectItemCaseSensitive(json, "paths");
    if (cJSON_IsObject(paths))
    {
        const cJSON *path_src = cJSON_GetObjectItemCaseSensitive(paths, "src");
        if (cJSON_IsString(path_src) && (path_src->valuestring != NULL))
        {
            project->path_src = strdup(path_src->valuestring);
        }

        const cJSON *path_out = cJSON_GetObjectItemCaseSensitive(paths, "out");
        if (cJSON_IsString(path_out) && (path_out->valuestring != NULL))
        {
            project->path_out = strdup(path_out->valuestring);
        }

        const cJSON *path_dep = cJSON_GetObjectItemCaseSensitive(paths, "dep");
        if (cJSON_IsString(path_dep) && (path_dep->valuestring != NULL))
        {
            project->path_dep = strdup(path_dep->valuestring);
        }

        const cJSON *path_lib = cJSON_GetObjectItemCaseSensitive(paths, "lib");
        if (cJSON_IsString(path_lib) && (path_lib->valuestring != NULL))
        {
            project->path_lib = strdup(path_lib->valuestring);
        }
    }

    project->path_src = project_resolve_macros(project, project->path_src);
    project->path_out = project_resolve_macros(project, project->path_out);
    project->path_dep = project_resolve_macros(project, project->path_dep);
    project->path_lib = project_resolve_macros(project, project->path_lib);

    const cJSON *entrypoint = cJSON_GetObjectItemCaseSensitive(json, "entrypoint");
    if (cJSON_IsString(entrypoint) && (entrypoint->valuestring != NULL))
    {
        project->entrypoint = strdup(entrypoint->valuestring);
    }

    const cJSON *targets = cJSON_GetObjectItemCaseSensitive(json, "targets");
    if (cJSON_IsArray(targets))
    {
        project->targets = calloc(sizeof(Target), cJSON_GetArraySize(targets));
        project->target_count = cJSON_GetArraySize(targets);

        for (int i = 0; i < cJSON_GetArraySize(targets); i++)
        {
            const cJSON *target = cJSON_GetArrayItem(targets, i);
            if (cJSON_IsString(target) && (target->valuestring != NULL))
            {
                project->targets[i] = target_from_string(target->valuestring);
            }
        }
    }

    cJSON_free(json);

    printf("building project: %s\n", project->name);
    printf("  version:    %s\n", project->version);
    printf("  type:       %d\n", project->type);
    printf("  src:        %s\n", project->path_src);
    printf("  out:        %s\n", project->path_out);
    printf("  dep:        %s\n", project->path_dep);
    printf("  lib:        %s\n", project->path_lib);
    printf("  entrypoint: `%s`\n", project->entrypoint);
    printf("  targets:    %d\n", project->target_count);
    for (int i = 0; i < project->target_count; i++)
    {
        printf("    %d: %s\n", i, target_to_string(project->targets[i]));
    }
    printf("\n");

    build_project(project);

    project_free(project);

    return 0;
}
