#include "build.h"
#include "lexer.h"
#include "parser.h"
#include "project.h"
#include "context.h"
#include "ioutil.h"

#include <stdlib.h>
#include <stdio.h>

#include "ast.h"

int build_project_lib(Context *context, Project *project)
{
    project_discover_files(project);
    project_parse_all(project);
    project_print_parse_errors(project);

    return 0;
}

int build_project_exe(Context *context, Project *project)
{
    project_discover_files(project);
    project_parse_all(project);

    int count_parse_errors = project_print_parse_errors(project);
    if (count_parse_errors == 0)
    {
        printf("No parse errors.\n");
    }

    project_analysis(project);
    int count_analysis_errors = project_print_analysis_errors(project);
    if (count_analysis_errors == 0)
    {
        printf("No analysis errors.\n");
    }

    return 0;
}

int build_project(Context *context, Project *project)
{
    if (project->type == PROJECT_TYPE_EXE)
    {
        return build_project_exe(context, project);
    }

    return build_project_lib(context, project);

    return -1;
}

int build_target_file(Context *context, char *path)
{
    Project *project = project_new();
    project->type = PROJECT_TYPE_EXE;
    project->name = path_dirname(path);
    project->version = "0.0.0";
    project->path_project = path_dirname(path);
    project->path_src = path_dirname(path);
    project->path_out = path_dirname(path);
    project->path_dep = path_dirname(path);
    project->path_lib = path_dirname(path);
    project->symbols = symbol_table_new();
    project->modules = NULL;

    build_project(context, project);

    return 1;
}

int build_target_project(Context *context, char *path)
{
    (void)context;
    (void)path;

    // TODO: implement project configuration file

    return 0;
}
