#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>
#include <limits.h>

#include "options.h"
#include "ioutil.h"

#include "cJSON.h"

#ifdef _WIN32
#include <windows.h>
char *realpath(char *path, char *resolved_path)
{
    DWORD length = GetFullPathName(path, 260, resolved_path, NULL);
    if (length == 0 || length > 260)
    {
        return NULL;
    }
    return resolved_path;
}
#endif

// define default mach installation path by platform:
// - windows: C:\Program Files\mach
// - linux: /usr/local/mach
// - macos: /usr/local/mach
// - other: /usr/local/mach
#if defined(_WIN32)
#define DEFAULT_MACH_PATH "C:\\Program Files\\mach"
#elif defined(__linux__) || defined(__APPLE__)
#define DEFAULT_MACH_PATH "/usr/local/mach"
#else
#define DEFAULT_MACH_PATH "/usr/local/mach"
#endif

Options *options_new()
{
    char *env_mach_path = getenv("MACH_PATH");
    char *mach_path = env_mach_path != NULL ? env_mach_path : DEFAULT_MACH_PATH;
    
    Options *options = calloc(1, sizeof(Options));
    options->mach_installation = strdup(mach_path);

    options->version = "0.0.0";

    options->path_project = getcwd(NULL, 0);
    options_set_path_src(options, "{path_project}/src");
    options_set_path_dep(options, "{path_project}/dep");
    options_set_path_out(options, "{path_project}/out");
    options_set_path_entry(options, "{path_src}/main.mach");

    options->dump_irl = false;
    options->dump_tac = false;
    options->dump_asm = false;
    options->verbose_compile = false;
    options->verbose_parse = false;
    options->verbose_analysis = false;
    options->verbose_codegen = false;
    options->verbose_link = false;

    options->targets = NULL;
    options->target_count = 0;

    options->dependancies = NULL;
    options->dependancy_count = 0;

    return options;
}

void options_free(Options *options)
{
    free(options->version);
    options->version = NULL;
    free(options->path_project);
    options->path_project = NULL;
    free(options->path_src);
    options->path_src = NULL;
    free(options->path_dep);
    options->path_dep = NULL;
    free(options->path_out);
    options->path_out = NULL;
    free(options->path_entry);
    options->path_entry = NULL;
    free(options->targets);
    options->targets = NULL;
    free(options->dependancies);
    options->dependancies = NULL;

    free(options);
}

void options_from_args(Options *options, int argc, char **argv)
{
    for (int i = 1; i < argc; ++i)
    {
        // if the first arg does not start with '-', attempt to parse it as a1 path
        // there are a few options for formatting the path:
        // 1. absolute to `mach.json` file
        // 2. relative to `mach.json` file
        // 3. absolute to directory
        // 4. relative to directory
        // option solutions:
        // 1. check if the file exists, if it does, parse with `options_from_file`
        // 2. convert to absolute, check if the file exists, if it does, parse with `options_from_file`
        // 3. check if the directory exists, if it does, check for `mach.json` file inside and parse with `options_from_file`
        // 4. convert to absolute, check if the directory exists, if it does, check for `mach.json` file inside and parse with `options_from_file`
        if (i == 1 && argv[i][0] != '-')
        {
            char *abs_path = malloc(strlen(argv[i]) + 1);
            
            if (realpath(argv[i], abs_path) == NULL)
            {
                fprintf(stderr, "fatal: failed to resolve path: %s\n", argv[i]);
                exit(1);
            }

            if (!file_exists(abs_path))
            {
                fprintf(stderr, "fatal: file does not exist: %s\n", abs_path);
                exit(1);
            }

            if (!is_directory(abs_path))
            {
                // if the path ends with `.mach`, leave options default and set
                //   `path_project` and `path_src` to the file's directory, then
                //   set `path_entry` to the file. Try to compile file directly.
                // if the path ends with `mach.json`, try to parse the file as
                //   a project config.
                if (strcmp(abs_path + strlen(abs_path) - 5, ".mach") == 0)
                {
                    options->path_project = dirname(abs_path);
                    options->path_src = dirname(abs_path);
                    options->path_dep = dirname(abs_path);
                    options->path_out = dirname(abs_path);
                    options->path_entry = abs_path;
                }
                else if (strcmp(abs_path + strlen(abs_path) - 9, "mach.json") == 0)
                {
                    options->path_project = dirname(abs_path);
                    options_set_path_src(options, "{path_project}/src");
                    options_set_path_dep(options, "{path_project}/dep");
                    options_set_path_out(options, "{path_project}/out");
                    options_set_path_entry(options, "{path_src}/main.mach");
                    options_from_file(options, abs_path);
                }
                else
                {
                    fprintf(stderr, "fatal: file is not a mach source file or project config: %s\n", abs_path);
                    exit(1);
                }
                continue;
            }

            char *mach_json_path = malloc(strlen(abs_path) + 11);
            strcpy(mach_json_path, abs_path);
            strcat(mach_json_path, "/mach.json");

            if (!file_exists(mach_json_path))
            {
                fprintf(stderr, "fatal: project file not found: %s\n", mach_json_path);
                exit(1);
            }

            options->path_project = abs_path;
            options_set_path_src(options, "{path_project}/src");
            options_set_path_dep(options, "{path_project}/dep");
            options_set_path_out(options, "{path_project}/out");
            options_set_path_entry(options, "{path_src}/main.mach");
            options_from_file(options, mach_json_path);
            continue;
        }

        if (strcmp(argv[i], "--version") == 0)
        {
            if (i + 1 < argc)
            {
                options->version = argv[++i];
            }
        }
        else if (strcmp(argv[i], "--path-project") == 0)
        {
            if (i + 1 < argc)
            {
                options_set_path_project(options, argv[++i]);
            }
        }
        else if (strcmp(argv[i], "--path-src") == 0)
        {
            if (i + 1 < argc)
            {
                options_set_path_src(options, argv[++i]);
            }
        }
        else if (strcmp(argv[i], "--path-dep") == 0)
        {
            if (i + 1 < argc)
            {
                options_set_path_dep(options, argv[++i]);
            }
        }
        else if (strcmp(argv[i], "--path-out") == 0)
        {
            if (i + 1 < argc)
            {
                options_set_path_out(options, argv[++i]);
            }
        }
        else if (strcmp(argv[i], "--path-entry") == 0)
        {
            if (i + 1 < argc)
            {
                options_set_path_entry(options, argv[++i]);
            }
        }
        else if (strcmp(argv[i], "--target") == 0)
        {
            if (i + 1 < argc)
            {
                options_add_target(options, target_from_string(argv[++i]));
            }
        }
        else if (strcmp(argv[i], "--dump-irl") == 0)
        {
            options->dump_irl = true;
        }
        else if (strcmp(argv[i], "--dump-tac") == 0)
        {
            options->dump_tac = true;
        }
        else if (strcmp(argv[i], "--dump-asm") == 0)
        {
            options->dump_asm = true;
        }
        else if (strcmp(argv[i], "--verbose-compiler") == 0)
        {
            options->verbose_compile = true;
        }
        else if (strcmp(argv[i], "--verbose-parse") == 0)
        {
            options->verbose_parse = true;
        }
        else if (strcmp(argv[i], "--verbose-analysis") == 0)
        {
            options->verbose_analysis = true;
        }
        else if (strcmp(argv[i], "--verbose-codegen") == 0)
        {
            options->verbose_codegen = true;
        }
        else if (strcmp(argv[i], "--verbose-link") == 0)
        {
            options->verbose_link = true;
        }
    }
}

void options_from_file(Options *options, char *path)
{
    // open file
    FILE *file = fopen(path, "r");
    if (!file)
    {
        fprintf(stderr, "fatal: failed to open file: %s\n", path);
        exit(1);
    }

    // go to the end of the file
    if (fseek(file, 0, SEEK_END) != 0)
    {
        fprintf(stderr, "fatal: failed to seek file: %s\n", path);
        fclose(file);
        exit(1);
    }

    // get the size of the file
    size_t length = ftell(file);
    if (length == (size_t)-1)
    {
        fprintf(stderr, "fatal: failed to get the file size: %s\n", path);
        fclose(file);
        exit(1);
    }

    // go back to the start of the file
    if (fseek(file, 0, SEEK_SET) != 0)
    {
        fprintf(stderr, "fatal: failed to seek file: %s\n", path);
        fclose(file);
        exit(1);
    }

    // read the file into a string
    char *data = malloc(length + 1);
    if (!data)
    {
        fprintf(stderr, "fatal: memory allocation failed\n");
        fclose(file);
        exit(1);
    }
    if (fread(data, 1, length, file) != length)
    {
        fprintf(stderr, "fatal: failed to read file: %s\n", path);
        free(data);
        fclose(file);
        exit(1);
    }
    data[length] = '\0';

    // parse the JSON data
    cJSON *json = cJSON_Parse(data);
    if (!json)
    {
        fprintf(stderr, "fatal: failed to parse JSON\n");
        free(data);
        fclose(file);
        exit(1);
    }

    // "version": "0.0.0"
    const cJSON *version = cJSON_GetObjectItemCaseSensitive(json, "version");
    if (cJSON_IsString(version) && (version->valuestring != NULL))
    {
        options->version = strdup(version->valuestring);
    }

    // "paths": { "project": ".", ... }
    const cJSON *paths = cJSON_GetObjectItemCaseSensitive(json, "paths");
    if (cJSON_IsObject(paths))
    {
        const cJSON *path_project = cJSON_GetObjectItemCaseSensitive(paths, "project");
        if (cJSON_IsString(path_project) && (path_project->valuestring != NULL))
        {
            options_set_path_project(options, path_project->valuestring);
        }

        const cJSON *path_src = cJSON_GetObjectItemCaseSensitive(paths, "src");
        if (cJSON_IsString(path_src) && (path_src->valuestring != NULL))
        {
            options_set_path_src(options, path_src->valuestring);
        }

        const cJSON *path_dep = cJSON_GetObjectItemCaseSensitive(paths, "dep");
        if (cJSON_IsString(path_dep) && (path_dep->valuestring != NULL))
        {
            options_set_path_dep(options, path_dep->valuestring);
        }

        const cJSON *path_out = cJSON_GetObjectItemCaseSensitive(paths, "out");
        if (cJSON_IsString(path_out) && (path_out->valuestring != NULL))
        {
            options_set_path_out(options, path_out->valuestring);
        }

        const cJSON *path_entry = cJSON_GetObjectItemCaseSensitive(paths, "entry");
        if (cJSON_IsString(path_entry) && (path_entry->valuestring != NULL))
        {
            options_set_path_entry(options, path_entry->valuestring);
        }
    }

    // "targets": [ "current", ... ]
    const cJSON *targets = cJSON_GetObjectItemCaseSensitive(json, "targets");
    if (cJSON_IsArray(targets))
    {
        const cJSON *target = NULL;
        cJSON_ArrayForEach(target, targets)
        {
            if (cJSON_IsString(target) && (target->valuestring != NULL))
            {
                options_add_target(options, target_from_string(target->valuestring));
            }
        }
    }

    // "options": { "dump-irl": true, ... }
    const cJSON *options_json = cJSON_GetObjectItemCaseSensitive(json, "options");
    if (cJSON_IsObject(options_json))
    {
        const cJSON *dump_irl = cJSON_GetObjectItemCaseSensitive(options_json, "dump_irl");
        if (cJSON_IsBool(dump_irl))
        {
            options->dump_irl = cJSON_IsTrue(dump_irl);
        }

        const cJSON *dump_tac = cJSON_GetObjectItemCaseSensitive(options_json, "dump_tac");
        if (cJSON_IsBool(dump_tac))
        {
            options->dump_tac = cJSON_IsTrue(dump_tac);
        }

        const cJSON *dump_asm = cJSON_GetObjectItemCaseSensitive(options_json, "dump_asm");
        if (cJSON_IsBool(dump_asm))
        {
            options->dump_asm = cJSON_IsTrue(dump_asm);
        }

        const cJSON *verbose_lex = cJSON_GetObjectItemCaseSensitive(options_json, "verbose_compile");
        if (cJSON_IsBool(verbose_lex))
        {
            options->verbose_compile = cJSON_IsTrue(verbose_lex);
        }

        const cJSON *verbose_parse = cJSON_GetObjectItemCaseSensitive(options_json, "verbose_parse");
        if (cJSON_IsBool(verbose_parse))
        {
            options->verbose_parse = cJSON_IsTrue(verbose_parse);
        }

        const cJSON *verbose_analysis = cJSON_GetObjectItemCaseSensitive(options_json, "verbose_analysis");
        if (cJSON_IsBool(verbose_analysis))
        {
            options->verbose_analysis = cJSON_IsTrue(verbose_analysis);
        }

        const cJSON *verbose_codegen = cJSON_GetObjectItemCaseSensitive(options_json, "verbose_codegen");
        if (cJSON_IsBool(verbose_codegen))
        {
            options->verbose_codegen = cJSON_IsTrue(verbose_codegen);
        }

        const cJSON *verbose_link = cJSON_GetObjectItemCaseSensitive(options_json, "verbose_link");
        if (cJSON_IsBool(verbose_link))
        {
            options->verbose_link = cJSON_IsTrue(verbose_link);
        }
    }

    // "dependancies": [ { "type": "local", "name": "std", "version": "0.0.0", "path": "..." }, ... ]
    const cJSON *dependancies = cJSON_GetObjectItemCaseSensitive(json, "dependancies");
    if (cJSON_IsArray(dependancies))
    {
        const cJSON *dependancy = NULL;
        cJSON_ArrayForEach(dependancy, dependancies)
        {
            if (cJSON_IsObject(dependancy))
            {
                const cJSON *type = cJSON_GetObjectItemCaseSensitive(dependancy, "type");
                const cJSON *name = cJSON_GetObjectItemCaseSensitive(dependancy, "name");
                const cJSON *version = cJSON_GetObjectItemCaseSensitive(dependancy, "version");
                const cJSON *path = cJSON_GetObjectItemCaseSensitive(dependancy, "path");

                if (cJSON_IsString(type) && (type->valuestring != NULL) &&
                    cJSON_IsString(name) && (name->valuestring != NULL) &&
                    cJSON_IsString(version) && (version->valuestring != NULL) &&
                    cJSON_IsString(path) && (path->valuestring != NULL))
                {
                    Dependancy dep = {
                        .type = dependancy_type_from_string(type->valuestring),
                        .name = strdup(name->valuestring),
                        .path = strdup(path->valuestring),
                    };

                    if (dependancy_type_valid(dep.type))
                    {
                        options_add_dependancy(options, dep);
                    }
                }
            }
        }
    }

    cJSON_Delete(json);
    free(data);
    fclose(file);
}

void options_set_path_project(Options *options, char *path_project)
{
    options->path_project = options_format_string_variables(options, path_project);
}

void options_set_path_src(Options *options, char *path_src)
{
    options->path_src = options_format_string_variables(options, path_src);
}

void options_set_path_dep(Options *options, char *path_dep)
{
    options->path_dep = options_format_string_variables(options, path_dep);
}

void options_set_path_out(Options *options, char *path_out)
{
    options->path_out = options_format_string_variables(options, path_out);
}

void options_set_path_entry(Options *options, char *path_entry)
{
    options->path_entry = options_format_string_variables(options, path_entry);
}

void options_add_target(Options *options, Target target)
{
    // check if an identical compilation target has already been added
    for (int i = 0; i < options->target_count; i++)
    {
        if (options->targets[i].platform == target.platform &&
            options->targets[i].architecture == target.architecture)
        {
            return;
        }
    }

    Target *temp = realloc(options->targets, sizeof(Target) * (options->target_count + 1));
    if (!temp)
    {
        fprintf(stderr, "fatal: target add: memory allocation failed\n");
        exit(1);
    }

    options->targets = temp;
    options->targets[options->target_count++] = target;
}

void options_add_dependancy(Options *options, Dependancy dependancy)
{
    // check if a dependancy with an identical name has already been added
    for (int i = 0; i < options->dependancy_count; i++)
    {
        if (strcmp(options->dependancies[i].name, dependancy.name) == 0)
        {
            return;
        }
    }

    Dependancy *temp = realloc(options->dependancies, sizeof(Dependancy) * (options->dependancy_count + 1));
    if (!temp)
    {
        fprintf(stderr, "fatal: dependancy add: memory allocation failed\n");
        exit(1);
    }

    options->dependancies = temp;
    options->dependancies[options->dependancy_count++] = dependancy;
}

char *str_replace(char *str, char *find, char *replace)
{
    char *result;
    char *i = strstr(str, find);
    if (!i)
    {
        return strdup(str);
    }

    size_t find_len = strlen(find);
    size_t replace_len = strlen(replace);
    size_t size = strlen(str) + 1 + (replace_len - find_len);
    result = malloc(size);
    if (!result)
    {
        return NULL;
    }

    memcpy(result, str, i - str);
    memcpy(result + (i - str), replace, replace_len);
    memcpy(result + (i - str) + replace_len, i + find_len, strlen(i + find_len) + 1);

    return result;
}

// available variables:
// - {version}
// - {path_project}
// - {path_src}
// - {path_out}
// - {path_entry}
// - {mach_installation}
// - {mach_std}
// usage:
// "path_src":   "{path_project}/src"
// "path_entry": "{path_src}/main.mach"
char *options_format_string_variables(Options *options, char *str)
{
    char *result = strdup(str);
    result = str_replace(result, "{version}", options->version);
    result = str_replace(result, "{path_project}", options->path_project);
    result = str_replace(result, "{path_src}", options->path_src);
    result = str_replace(result, "{path_out}", options->path_out);
    result = str_replace(result, "{path_entry}", options->path_entry);
    result = str_replace(result, "{mach_installation}", options->mach_installation);

    char *mach_std = malloc(strlen(options->mach_installation) + 5);
    strcpy(mach_std, options->mach_installation);
    strcat(mach_std, "/std");

    result = str_replace(result, "{mach_std}", mach_std);

    free(mach_std);

    return result;
}
