#include "config.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// simple toml parser for configuration
typedef struct TomlParser
{
    const char *input;
    size_t      pos;
    size_t      len;
} TomlParser;

static void toml_parser_init(TomlParser *parser, const char *input)
{
    parser->input = input;
    parser->pos   = 0;
    parser->len   = strlen(input);
}

static void toml_skip_whitespace(TomlParser *parser)
{
    while (parser->pos < parser->len)
    {
        char c = parser->input[parser->pos];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
        {
            parser->pos++;
        }
        else if (c == '#')
        {
            // skip comment line
            while (parser->pos < parser->len && parser->input[parser->pos] != '\n')
            {
                parser->pos++;
            }
        }
        else
        {
            break;
        }
    }
}

static char *toml_parse_string(TomlParser *parser)
{
    toml_skip_whitespace(parser);

    if (parser->pos >= parser->len)
        return NULL;

    char quote = parser->input[parser->pos];
    if (quote != '"' && quote != '\'')
        return NULL;

    parser->pos++; // skip opening quote

    size_t start = parser->pos;
    while (parser->pos < parser->len && parser->input[parser->pos] != quote)
    {
        parser->pos++;
    }

    if (parser->pos >= parser->len)
        return NULL;

    size_t len = parser->pos - start;
    parser->pos++; // skip closing quote

    char *result = malloc(len + 1);
    strncpy(result, parser->input + start, len);
    result[len] = '\0';

    return result;
}

static char *toml_parse_identifier(TomlParser *parser)
{
    toml_skip_whitespace(parser);

    if (parser->pos >= parser->len)
        return NULL;

    // handle quoted keys
    if (parser->input[parser->pos] == '"')
    {
        return toml_parse_string(parser);
    }

    size_t start = parser->pos;
    while (parser->pos < parser->len)
    {
        char c = parser->input[parser->pos];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-')
        {
            parser->pos++;
        }
        else
        {
            break;
        }
    }

    if (parser->pos == start)
        return NULL;

    size_t len    = parser->pos - start;
    char  *result = malloc(len + 1);
    strncpy(result, parser->input + start, len);
    result[len] = '\0';

    return result;
}

static char *toml_parse_section_name(TomlParser *parser)
{
    toml_skip_whitespace(parser);

    if (parser->pos >= parser->len || parser->input[parser->pos] != '[')
        return NULL;

    parser->pos++; // skip '['

    size_t start = parser->pos;
    while (parser->pos < parser->len)
    {
        char c = parser->input[parser->pos];
        if (c == ']')
        {
            break;
        }
        else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-')
        {
            parser->pos++;
        }
        else
        {
            return NULL; // invalid character in section name
        }
    }

    if (parser->pos >= parser->len || parser->input[parser->pos] != ']')
        return NULL;

    size_t len = parser->pos - start;
    parser->pos++; // skip ']'

    if (len == 0)
        return NULL;

    char *result = malloc(len + 1);
    strncpy(result, parser->input + start, len);
    result[len] = '\0';

    return result;
}

static void toml_skip_table_value(TomlParser *parser)
{
    toml_skip_whitespace(parser);

    if (parser->pos >= parser->len || parser->input[parser->pos] != '{')
    {
        // not a table, skip to end of line
        while (parser->pos < parser->len && parser->input[parser->pos] != '\n')
            parser->pos++;
        return;
    }

    parser->pos++; // skip '{'

    int brace_count = 1;
    while (parser->pos < parser->len && brace_count > 0)
    {
        if (parser->input[parser->pos] == '{')
            brace_count++;
        else if (parser->input[parser->pos] == '}')
            brace_count--;
        parser->pos++;
    }
}

static int toml_parse_number(TomlParser *parser)
{
    toml_skip_whitespace(parser);

    if (parser->pos >= parser->len)
        return 0;

    size_t start = parser->pos;
    while (parser->pos < parser->len)
    {
        char c = parser->input[parser->pos];
        if (c >= '0' && c <= '9')
        {
            parser->pos++;
        }
        else
        {
            break;
        }
    }

    if (parser->pos == start)
        return 0;

    char *number_str = malloc(parser->pos - start + 1);
    strncpy(number_str, parser->input + start, parser->pos - start);
    number_str[parser->pos - start] = '\0';

    int result = atoi(number_str);
    free(number_str);

    return result;
}

static bool toml_parse_bool(TomlParser *parser)
{
    toml_skip_whitespace(parser);

    if (parser->pos + 4 <= parser->len && strncmp(parser->input + parser->pos, "true", 4) == 0)
    {
        parser->pos += 4;
        return true;
    }
    else if (parser->pos + 5 <= parser->len && strncmp(parser->input + parser->pos, "false", 5) == 0)
    {
        parser->pos += 5;
        return false;
    }

    return false;
}

static bool toml_expect_char(TomlParser *parser, char expected)
{
    toml_skip_whitespace(parser);

    if (parser->pos >= parser->len || parser->input[parser->pos] != expected)
        return false;

    parser->pos++;
    return true;
}

static char *read_file_to_string(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (!file)
        return NULL;

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *buffer = malloc(size + 1);
    if (!buffer)
    {
        fclose(file);
        return NULL;
    }

    size_t read  = fread(buffer, 1, size, file);
    buffer[read] = '\0';
    fclose(file);

    return buffer;
}

void target_config_init(TargetConfig *target)
{
    memset(target, 0, sizeof(TargetConfig));
    target->opt_level   = 2;    // default optimization level
    target->emit_object = true; // default to emitting object files
}

void target_config_dnit(TargetConfig *target)
{
    free(target->name);
    free(target->target_triple);
    memset(target, 0, sizeof(TargetConfig));
}

TargetConfig *target_config_create(const char *name, const char *target_triple)
{
    TargetConfig *target = malloc(sizeof(TargetConfig));
    target_config_init(target);
    target->name          = strdup(name);
    target->target_triple = strdup(target_triple);
    return target;
}

DependencyConfig *dependency_config_create(const char *name, const char *type)
{
    DependencyConfig *dep = malloc(sizeof(DependencyConfig));
    dependency_config_init(dep);
    dep->name         = strdup(name);
    dep->project_name = strdup(name); // default project_name same as name
    dep->title        = NULL;         // optional, set by config parser
    dep->type         = strdup(type ? type : "local");

    return dep;
}

void dependency_config_init(DependencyConfig *dep)
{
    memset(dep, 0, sizeof(DependencyConfig));
}

void dependency_config_dnit(DependencyConfig *dep)
{
    free(dep->name);
    free(dep->project_name);
    free(dep->title);
    free(dep->type);
    memset(dep, 0, sizeof(DependencyConfig));
}

void config_init(ProjectConfig *config)
{
    memset(config, 0, sizeof(ProjectConfig));
}

void config_dnit(ProjectConfig *config)
{
    free(config->name);
    free(config->version);
    free(config->main_file);
    free(config->target_name);
    free(config->default_target);
    free(config->src_dir);
    free(config->dep_dir);
    free(config->lib_dir);
    free(config->out_dir);
    free(config->runtime_path);
    free(config->runtime_module);
    free(config->stdlib_path);

    // cleanup targets
    for (int i = 0; i < config->target_count; i++)
    {
        target_config_dnit(config->targets[i]);
        free(config->targets[i]);
    }
    free(config->targets);

    // cleanup dependencies
    for (int i = 0; i < config->dep_count; i++)
    {
        dependency_config_dnit(config->dependencies[i]);
        free(config->dependencies[i]);
    }
    free(config->dependencies);

    for (int i = 0; i < config->lib_dep_count; i++)
    {
        free(config->lib_dependencies[i]);
    }
    free(config->lib_dependencies);

    memset(config, 0, sizeof(ProjectConfig));
}

bool config_add_target(ProjectConfig *config, const char *name, const char *target_triple)
{
    // check if target already exists
    for (int i = 0; i < config->target_count; i++)
    {
        if (strcmp(config->targets[i]->name, name) == 0)
            return false; // already exists
    }

    // grow targets array if needed
    if (config->target_count == 0)
    {
        config->targets = malloc(sizeof(TargetConfig *));
    }
    else
    {
        config->targets = realloc(config->targets, (config->target_count + 1) * sizeof(TargetConfig *));
    }

    config->targets[config->target_count] = target_config_create(name, target_triple);
    config->target_count++;

    return true;
}

TargetConfig *config_get_target(ProjectConfig *config, const char *name)
{
    for (int i = 0; i < config->target_count; i++)
    {
        if (strcmp(config->targets[i]->name, name) == 0)
            return config->targets[i];
    }
    return NULL;
}

TargetConfig *config_get_target_by_triple(ProjectConfig *config, const char *target_triple)
{
    for (int i = 0; i < config->target_count; i++)
    {
        if (strcmp(config->targets[i]->target_triple, target_triple) == 0)
            return config->targets[i];
    }
    return NULL;
}

TargetConfig *config_get_default_target(ProjectConfig *config)
{
    if (config->default_target && strcmp(config->default_target, "all") != 0)
    {
        return config_get_target(config, config->default_target);
    }
    else if (config->target_count > 0)
    {
        return config->targets[0]; // first target as fallback
    }
    return NULL;
}

bool config_is_build_all_targets(ProjectConfig *config)
{
    return !config->default_target || strcmp(config->default_target, "all") == 0;
}

ProjectConfig *config_load(const char *config_path)
{
    char *content = read_file_to_string(config_path);
    if (!content)
        return NULL;

    ProjectConfig *config = malloc(sizeof(ProjectConfig));
    config_init(config);

    TomlParser parser;
    toml_parser_init(&parser, content);

    char *current_section = NULL;

    // enhanced section-aware parsing
    while (parser.pos < parser.len)
    {
        toml_skip_whitespace(&parser);

        if (parser.pos >= parser.len)
            break;

        // check for section header
        if (parser.input[parser.pos] == '[')
        {
            free(current_section);
            current_section = toml_parse_section_name(&parser);
            continue;
        }

        // parse key-value pair
        char *key = toml_parse_identifier(&parser);
        if (!key)
        {
            toml_skip_whitespace(&parser);
            if (parser.pos < parser.len)
                parser.pos++; // skip unknown character
            continue;
        }

        if (!toml_expect_char(&parser, '='))
        {
            free(key);
            continue;
        }

        // parse value based on section and key
        if (!current_section || strcmp(current_section, "project") == 0)
        {
            // handle project section keys
            if (strcmp(key, "name") == 0)
            {
                config->name = toml_parse_string(&parser);
            }
            else if (strcmp(key, "version") == 0)
            {
                config->version = toml_parse_string(&parser);
            }
            else if (strcmp(key, "entrypoint") == 0)
            {
                config->main_file = toml_parse_string(&parser);
            }
            else if (strcmp(key, "default-target") == 0)
            {
                config->default_target = toml_parse_string(&parser);
            }
        }
        else if (strcmp(current_section, "directories") == 0)
        {
            // handle directories section keys
            if (strcmp(key, "src-dir") == 0)
            {
                config->src_dir = toml_parse_string(&parser);
            }
            else if (strcmp(key, "dep-dir") == 0)
            {
                config->dep_dir = toml_parse_string(&parser);
            }
            else if (strcmp(key, "lib-dir") == 0)
            {
                config->lib_dir = toml_parse_string(&parser);
            }
            else if (strcmp(key, "out-dir") == 0)
            {
                config->out_dir = toml_parse_string(&parser);
            }
            // bin-dir and obj-dir are now computed dynamically per target
        }
        else if (strncmp(current_section, "targets.", 8) == 0)
        {
            // handle target-specific section - extract target name
            const char   *target_name = current_section + 8;
            TargetConfig *target      = config_get_target(config, target_name);

            // create target if it doesn't exist
            if (!target && strcmp(key, "target") == 0)
            {
                char *target_triple = toml_parse_string(&parser);
                if (target_triple)
                {
                    config_add_target(config, target_name, target_triple);
                    target = config_get_target(config, target_name);
                    free(target_triple);
                }
            }

            if (target)
            {
                // parse target-specific options
                if (strcmp(key, "target") == 0)
                {
                    // target triple already handled above
                    toml_skip_table_value(&parser);
                }
                else if (strcmp(key, "opt-level") == 0)
                {
                    target->opt_level = toml_parse_number(&parser);
                }
                else if (strcmp(key, "emit-ast") == 0)
                {
                    target->emit_ast = toml_parse_bool(&parser);
                }
                else if (strcmp(key, "emit-ir") == 0)
                {
                    target->emit_ir = toml_parse_bool(&parser);
                }
                else if (strcmp(key, "emit-asm") == 0)
                {
                    target->emit_asm = toml_parse_bool(&parser);
                }
                else if (strcmp(key, "emit-object") == 0)
                {
                    target->emit_object = toml_parse_bool(&parser);
                }
                else if (strcmp(key, "build-library") == 0)
                {
                    target->build_library = toml_parse_bool(&parser);
                }
                else if (strcmp(key, "no-pie") == 0)
                {
                    target->no_pie = toml_parse_bool(&parser);
                }
            }
            else
            {
                // skip unknown target options
                toml_skip_table_value(&parser);
            }
        }
        else if (strcmp(current_section, "build") == 0)
        {
            // legacy build section - skip for now
            toml_skip_table_value(&parser);
        }
        else if (strcmp(current_section, "runtime") == 0)
        {
            // handle runtime section keys
            if (strcmp(key, "runtime-path") == 0)
            {
                config->runtime_path = toml_parse_string(&parser);
            }
            else if (strcmp(key, "runtime") == 0 || strcmp(key, "runtime-module") == 0)
            {
                config->runtime_module = toml_parse_string(&parser);
            }
            else if (strcmp(key, "stdlib-path") == 0)
            {
                config->stdlib_path = toml_parse_string(&parser);
            }
        }
        else if (strcmp(current_section, "dependencies") == 0)
        {
            // simple format: dependencies = { std = "local", mylib = "git" }
            char *dep_type = toml_parse_string(&parser);
            if (dep_type)
            {
                config_add_dependency_full(config, key, dep_type);
                free(dep_type);
            }
        }
        else if (strncmp(current_section, "dependency.", 11) == 0)
        {
            // new format: [dependency.name] sections
            const char *project_name = current_section + 11; // skip "dependency."

            // find or create dependency
            DependencyConfig *dep = NULL;
            // search by project_name first
            for (int i = 0; i < config->dep_count; i++)
            {
                if (strcmp(config->dependencies[i]->project_name, project_name) == 0)
                {
                    dep = config->dependencies[i];
                    break;
                }
            }

            if (!dep)
            {
                // create new dependency with defaults
                config_add_dependency_full(config, project_name, "local");
                // get the newly created dependency and set project_name
                dep = config->dependencies[config->dep_count - 1];
                free(dep->project_name);
                dep->project_name = strdup(project_name);
            }

            if (dep)
            {
                // parse dependency properties
                printf("debug: parsing dependency key '%s'\n", key);
                if (strcmp(key, "type") == 0)
                {
                    char *new_type = toml_parse_string(&parser);
                    if (new_type)
                    {
                        free(dep->type);
                        dep->type = new_type;
                        printf("debug: set dependency type to '%s'\n", new_type);
                    }
                }
                else if (strcmp(key, "title") == 0)
                {
                    char *new_title = toml_parse_string(&parser);
                    if (new_title)
                    {
                        free(dep->title);
                        dep->title = new_title;
                    }
                }
                else if (strcmp(key, "type") == 0)
                {
                    char *new_type = toml_parse_string(&parser);
                    if (new_type)
                    {
                        free(dep->type);
                        dep->type = new_type;
                        printf("debug: set dependency type to '%s'\n", new_type);
                    }
                }
                else if (strcmp(key, "name") == 0)
                {
                    // override import name - this changes the key used for imports
                    char *new_name = toml_parse_string(&parser);
                    if (new_name)
                    {
                        free(dep->name);
                        dep->name = new_name;
                    }
                }
                else
                {
                    // skip unknown dependency properties
                    toml_skip_table_value(&parser);
                }
            }
            else
            {
                toml_skip_table_value(&parser);
            }
        }

        free(key);
    }

    free(current_section);
    free(content);
    return config;
}

ProjectConfig *config_load_from_dir(const char *dir_path)
{
    char config_path[1024];
    snprintf(config_path, sizeof(config_path), "%s/mach.toml", dir_path);

    return config_load(config_path);
}

bool config_save(ProjectConfig *config, const char *config_path)
{
    FILE *file = fopen(config_path, "w");
    if (!file)
        return false;

    fprintf(file, "# mach project configuration\n");
    fprintf(file, "# this file defines the project structure and build settings\n\n");

    fprintf(file, "[project]\n");
    if (config->name)
        fprintf(file, "name = \"%s\"\n", config->name);
    if (config->version)
        fprintf(file, "version = \"%s\"\n", config->version);
    if (config->main_file)
        fprintf(file, "entrypoint = \"%s\"\n", config->main_file);
    if (config->target_name)
        fprintf(file, "target-name = \"%s\"\n", config->target_name);
    if (config->default_target)
        fprintf(file, "default-target = \"%s\"\n", config->default_target);

    fprintf(file, "\n[directories]\n");
    if (config->src_dir)
        fprintf(file, "src-dir = \"%s\"\n", config->src_dir);
    if (config->dep_dir)
        fprintf(file, "dep-dir = \"%s\"\n", config->dep_dir);
    if (config->lib_dir)
        fprintf(file, "lib-dir = \"%s\"\n", config->lib_dir);
    if (config->out_dir)
        fprintf(file, "out-dir = \"%s\"\n", config->out_dir);

    // save targets
    for (int i = 0; i < config->target_count; i++)
    {
        TargetConfig *target = config->targets[i];
        fprintf(file, "\n[targets.%s]\n", target->name);
        if (target->target_triple)
            fprintf(file, "target = \"%s\"\n", target->target_triple);
        fprintf(file, "opt-level = %d\n", target->opt_level);
        if (target->emit_ast)
            fprintf(file, "emit-ast = true\n");
        if (target->emit_ir)
            fprintf(file, "emit-ir = true\n");
        if (target->emit_asm)
            fprintf(file, "emit-asm = true\n");
        if (!target->emit_object)
            fprintf(file, "emit-object = false\n");
        if (target->build_library)
            fprintf(file, "build-library = true\n");
        if (target->no_pie)
            fprintf(file, "no-pie = true\n");
    }

    if (config->runtime_path || config->runtime_module || config->stdlib_path)
    {
        fprintf(file, "\n[runtime]\n");
        if (config->runtime_path)
            fprintf(file, "runtime-path = \"%s\"\n", config->runtime_path);
        if (config->runtime_module)
            fprintf(file, "runtime = \"%s\"\n", config->runtime_module);
        if (config->stdlib_path)
            fprintf(file, "stdlib-path = \"%s\"\n", config->stdlib_path);
    }

    // save dependencies using simple format
    if (config->dep_count > 0)
    {
        fprintf(file, "\n[dependencies]\n");
        for (int i = 0; i < config->dep_count; i++)
        {
            DependencyConfig *dep = config->dependencies[i];
            fprintf(file, "%s = \"%s\"\n", dep->name, dep->type ? dep->type : "local");
        }
    }

    // save library dependencies
    if (config->lib_dep_count > 0)
    {
        fprintf(file, "\n[lib-dependencies]\n");
        for (int i = 0; i < config->lib_dep_count; i++)
        {
            fprintf(file, "\"%s\" = { path = \"lib\" }\n", config->lib_dependencies[i]);
        }
    }

    fclose(file);
    return true;
}

ProjectConfig *config_create_default(const char *project_name)
{
    ProjectConfig *config = malloc(sizeof(ProjectConfig));
    config_init(config);

    config->name           = strdup(project_name);
    config->version        = strdup("0.1.0");
    config->main_file      = strdup("main.mach");
    config->target_name    = strdup(project_name);
    config->runtime_module = strdup("dep.std.runtime"); // default runtime module

    // no default directories - user must configure them
    // this ensures all paths are explicitly configured

    return config;
}

bool config_has_main_file(ProjectConfig *config)
{
    return config && config->main_file && strlen(config->main_file) > 0;
}

bool config_should_emit_ast(ProjectConfig *config, const char *target_name)
{
    if (!config || !target_name)
        return false;
    TargetConfig *target = config_get_target(config, target_name);
    return target && target->emit_ast;
}

bool config_should_emit_ir(ProjectConfig *config, const char *target_name)
{
    if (!config || !target_name)
        return false;
    TargetConfig *target = config_get_target(config, target_name);
    return target && target->emit_ir;
}

bool config_should_emit_asm(ProjectConfig *config, const char *target_name)
{
    if (!config || !target_name)
        return false;
    TargetConfig *target = config_get_target(config, target_name);
    return target && target->emit_asm;
}

bool config_should_emit_object(ProjectConfig *config, const char *target_name)
{
    if (!config || !target_name)
        return false;
    TargetConfig *target = config_get_target(config, target_name);
    return target && target->emit_object;
}

bool config_should_build_library(ProjectConfig *config, const char *target_name)
{
    if (!config || !target_name)
        return false;
    TargetConfig *target = config_get_target(config, target_name);
    return target && target->build_library;
}

bool config_should_link_executable(ProjectConfig *config, const char *target_name)
{
    if (!config || !target_name)
        return true;
    TargetConfig *target = config_get_target(config, target_name);
    return target && !target->build_library;
}

char *config_resolve_main_file(ProjectConfig *config, const char *project_dir)
{
    if (!config || !config->main_file)
        return NULL;

    // if main file is absolute path, return as is
    if (config->main_file[0] == '/')
        return strdup(config->main_file);

    // resolve relative to src directory
    char *src_dir = config_resolve_src_dir(config, project_dir);
    if (!src_dir)
        return NULL;

    size_t len  = strlen(src_dir) + strlen(config->main_file) + 2;
    char  *path = malloc(len);
    snprintf(path, len, "%s/%s", src_dir, config->main_file);

    free(src_dir);
    return path;
}

char *config_resolve_src_dir(ProjectConfig *config, const char *project_dir)
{
    if (!config || !config->src_dir)
        return NULL;

    // if src dir is absolute path, return as is
    if (config->src_dir[0] == '/')
        return strdup(config->src_dir);

    // resolve relative to project directory
    size_t len  = strlen(project_dir) + strlen(config->src_dir) + 2;
    char  *path = malloc(len);
    snprintf(path, len, "%s/%s", project_dir, config->src_dir);

    return path;
}

char *config_resolve_dep_dir(ProjectConfig *config, const char *project_dir)
{
    if (!config || !config->dep_dir)
        return NULL;

    // if dep dir is absolute path, return as is
    if (config->dep_dir[0] == '/')
        return strdup(config->dep_dir);

    // resolve relative to project directory
    size_t len  = strlen(project_dir) + strlen(config->dep_dir) + 2;
    char  *path = malloc(len);
    snprintf(path, len, "%s/%s", project_dir, config->dep_dir);

    return path;
}

char *config_resolve_lib_dir(ProjectConfig *config, const char *project_dir)
{
    if (!config || !config->lib_dir)
        return NULL;

    // if lib dir is absolute path, return as is
    if (config->lib_dir[0] == '/')
        return strdup(config->lib_dir);

    // resolve relative to project directory
    size_t len  = strlen(project_dir) + strlen(config->lib_dir) + 2;
    char  *path = malloc(len);
    snprintf(path, len, "%s/%s", project_dir, config->lib_dir);

    return path;
}

char *config_resolve_out_dir(ProjectConfig *config, const char *project_dir)
{
    if (!config || !config->out_dir)
        return NULL;

    // if out dir is absolute path, return as is
    if (config->out_dir[0] == '/')
        return strdup(config->out_dir);

    // resolve relative to project directory
    size_t len  = strlen(project_dir) + strlen(config->out_dir) + 2;
    char  *path = malloc(len);
    snprintf(path, len, "%s/%s", project_dir, config->out_dir);

    return path;
}

char *config_resolve_bin_dir(ProjectConfig *config, const char *project_dir, const char *target_name)
{
    char *out_dir = config_resolve_out_dir(config, project_dir);
    if (!out_dir)
        return NULL;

    if (!target_name)
    {
        free(out_dir);
        return NULL;
    }

    // compute bin directory: out_dir/target_name/bin
    size_t len  = strlen(out_dir) + strlen(target_name) + strlen("/bin") + 3;
    char  *path = malloc(len);
    snprintf(path, len, "%s/%s/bin", out_dir, target_name);

    free(out_dir);
    return path;
}

char *config_resolve_obj_dir(ProjectConfig *config, const char *project_dir, const char *target_name)
{
    char *out_dir = config_resolve_out_dir(config, project_dir);
    if (!out_dir)
        return NULL;

    if (!target_name)
    {
        free(out_dir);
        return NULL;
    }

    // compute obj directory: out_dir/target_name/obj
    size_t len  = strlen(out_dir) + strlen(target_name) + strlen("/obj") + 3;
    char  *path = malloc(len);
    snprintf(path, len, "%s/%s/obj", out_dir, target_name);

    free(out_dir);
    return path;
}

char *config_resolve_runtime_path(ProjectConfig *config, const char *project_dir)
{
    if (!config || !config->runtime_path)
        return NULL;

    // if runtime path is absolute, return as is
    if (config->runtime_path[0] == '/')
        return strdup(config->runtime_path);

    // resolve relative to project directory
    size_t len  = strlen(project_dir) + strlen(config->runtime_path) + 2;
    char  *path = malloc(len);
    snprintf(path, len, "%s/%s", project_dir, config->runtime_path);

    return path;
}

// runtime module management
bool config_set_runtime_module(ProjectConfig *config, const char *module_path)
{
    if (!config || !module_path)
        return false;

    free(config->runtime_module);
    config->runtime_module = strdup(module_path);
    return true;
}

char *config_get_runtime_module(ProjectConfig *config)
{
    if (!config || !config->runtime_module)
        return NULL;
    return strdup(config->runtime_module);
}

bool config_has_runtime_module(ProjectConfig *config)
{
    return config && config->runtime_module && strlen(config->runtime_module) > 0;
}

static bool ensure_directory_exists(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0)
        return S_ISDIR(st.st_mode);

    // create directory with mkdir -p equivalent
    char *path_copy = strdup(path);
    char *p         = path_copy;

    // skip leading slash
    if (*p == '/')
        p++;

    while (*p)
    {
        if (*p == '/')
        {
            *p = '\0';
            mkdir(path_copy, 0755);
            *p = '/';
        }
        p++;
    }

    int result = mkdir(path_copy, 0755);
    free(path_copy);

    return result == 0 || errno == EEXIST;
}

bool config_ensure_directories(ProjectConfig *config, const char *project_dir)
{
    if (!config)
        return false;

    // ensure all configured directories exist
    char *src_dir = config_resolve_src_dir(config, project_dir);
    if (src_dir)
    {
        if (!ensure_directory_exists(src_dir))
        {
            free(src_dir);
            return false;
        }
        free(src_dir);
    }

    char *dep_dir = config_resolve_dep_dir(config, project_dir);
    if (dep_dir)
    {
        if (!ensure_directory_exists(dep_dir))
        {
            free(dep_dir);
            return false;
        }
        free(dep_dir);
    }

    char *lib_dir = config_resolve_lib_dir(config, project_dir);
    if (lib_dir)
    {
        if (!ensure_directory_exists(lib_dir))
        {
            free(lib_dir);
            return false;
        }
        free(lib_dir);
    }

    char *out_dir = config_resolve_out_dir(config, project_dir);
    if (out_dir)
    {
        if (!ensure_directory_exists(out_dir))
        {
            free(out_dir);
            return false;
        }
        free(out_dir);
    }

    // create target-specific directories for each target
    for (int i = 0; i < config->target_count; i++)
    {
        const char *target_name = config->targets[i]->name;

        char *bin_dir = config_resolve_bin_dir(config, project_dir, target_name);
        if (bin_dir)
        {
            if (!ensure_directory_exists(bin_dir))
            {
                free(bin_dir);
                return false;
            }
            free(bin_dir);
        }

        char *obj_dir = config_resolve_obj_dir(config, project_dir, target_name);
        if (obj_dir)
        {
            if (!ensure_directory_exists(obj_dir))
            {
                free(obj_dir);
                return false;
            }
            free(obj_dir);
        }
    }

    return true;
}

bool config_validate(ProjectConfig *config)
{
    if (!config)
        return false;

    if (!config->name || strlen(config->name) == 0)
        return false;

    if (!config->main_file || strlen(config->main_file) == 0)
        return false;

    // validate that required directories are configured
    if (!config->src_dir)
        return false;

    if (!config->out_dir)
        return false;

    // validate targets
    if (config->target_count == 0)
        return false;

    for (int i = 0; i < config->target_count; i++)
    {
        TargetConfig *target = config->targets[i];
        if (!target->name || strlen(target->name) == 0)
            return false;
        if (!target->target_triple || strlen(target->target_triple) == 0)
            return false;
        if (target->opt_level < 0 || target->opt_level > 3)
            return false;
    }

    return true;
}

// dependency management functions
bool config_add_dependency_full(ProjectConfig *config, const char *name, const char *type)
{
    if (!config || !name)
        return false;

    // check if dependency already exists
    for (int i = 0; i < config->dep_count; i++)
    {
        if (strcmp(config->dependencies[i]->name, name) == 0)
            return false; // already exists
    }

    // grow dependencies array if needed
    if (config->dep_count == 0)
    {
        config->dependencies = malloc(sizeof(DependencyConfig *));
    }
    else
    {
        DependencyConfig **new_deps = realloc(config->dependencies, (config->dep_count + 1) * sizeof(DependencyConfig *));
        if (!new_deps)
            return false;
        config->dependencies = new_deps;
    }

    config->dependencies[config->dep_count] = dependency_config_create(name, type);
    config->dep_count++;

    return true;
}

bool config_add_dependency(ProjectConfig *config, const char *dep_name, const char *dep_source)
{
    // for backward compatibility, assume local type and ignore source path for local deps
    (void)dep_source; // suppress unused parameter warning
    return config_add_dependency_full(config, dep_name, "local");
}

DependencyConfig *config_get_dependency(ProjectConfig *config, const char *dep_name)
{
    if (!config || !dep_name)
        return NULL;

    for (int i = 0; i < config->dep_count; i++)
    {
        if (strcmp(config->dependencies[i]->name, dep_name) == 0)
            return config->dependencies[i];
    }
    return NULL;
}

bool config_has_dependency(ProjectConfig *config, const char *dep_name)
{
    return config_get_dependency(config, dep_name) != NULL;
}

bool config_remove_dependency(ProjectConfig *config, const char *dep_name)
{
    if (!config || !dep_name)
        return false;

    for (int i = 0; i < config->dep_count; i++)
    {
        if (strcmp(config->dependencies[i]->name, dep_name) == 0)
        {
            // free the dependency
            dependency_config_dnit(config->dependencies[i]);
            free(config->dependencies[i]);

            // shift remaining dependencies
            for (int j = i; j < config->dep_count - 1; j++)
            {
                config->dependencies[j] = config->dependencies[j + 1];
            }
            config->dep_count--;

            return true;
        }
    }
    return false;
}

bool config_add_lib_dependency(ProjectConfig *config, const char *lib_name, const char *lib_path)
{
    if (!config || !lib_name || !lib_path)
        return false;

    // check if lib dependency already exists
    for (int i = 0; i < config->lib_dep_count; i++)
    {
        if (strcmp(config->lib_dependencies[i], lib_name) == 0)
            return false; // already exists
    }

    // resize array if needed
    if (config->lib_dep_count % 8 == 0)
    {
        int    new_capacity = config->lib_dep_count + 8;
        char **new_deps     = realloc(config->lib_dependencies, new_capacity * sizeof(char *));
        if (!new_deps)
            return false;
        config->lib_dependencies = new_deps;
    }

    // add lib dependency
    config->lib_dependencies[config->lib_dep_count] = strdup(lib_name);
    config->lib_dep_count++;

    return true;
}

char **config_get_dependency_paths(ProjectConfig *config, const char *project_dir)
{
    if (!config || !project_dir)
        return NULL;

    char **paths = malloc((config->dep_count + 1) * sizeof(char *));
    if (!paths)
        return NULL;

    char *dep_dir = config_resolve_dep_dir(config, project_dir);
    if (!dep_dir)
    {
        free(paths);
        return NULL;
    }

    for (int i = 0; i < config->dep_count; i++)
    {
        DependencyConfig *dep = config->dependencies[i];
        size_t            len = strlen(dep_dir) + strlen(dep->project_name) + 2;
        paths[i]              = malloc(len);
        snprintf(paths[i], len, "%s/%s", dep_dir, dep->project_name);
    }
    paths[config->dep_count] = NULL; // null terminate

    free(dep_dir);
    return paths;
}

char *config_resolve_dependency_module_path(ProjectConfig *config, const char *project_dir, const char *module_path)
{
    if (!config || !project_dir || !module_path)
        return NULL;

    printf("debug: trying to resolve module path '%s'\n", module_path);

    // check if module_path starts with a dependency name
    char *first_part = strdup(module_path);
    char *dot_pos    = strchr(first_part, '.');
    if (dot_pos)
        *dot_pos = '\0';

    printf("debug: looking for dependency named '%s'\n", first_part);
    printf("debug: config has %d dependencies\n", config ? config->dep_count : -1);

    // look for dependency with this name
    DependencyConfig *dep = config_get_dependency(config, first_part);
    if (dep)
    {
        printf("debug: found dependency '%s' of type '%s'\n", dep->name, dep->type ? dep->type : "NULL");
        // resolve dependency directory: <project_dep_dir>/<dep_name>
        char *dep_dir = config_resolve_dep_dir(config, project_dir);
        if (dep_dir)
        {
            char dependency_path[1024];
            snprintf(dependency_path, sizeof(dependency_path), "%s/%s", dep_dir, dep->project_name);

            // load the dependency's configuration to get its src_dir
            char dep_config_path[1024];
            snprintf(dep_config_path, sizeof(dep_config_path), "%s/mach.toml", dependency_path);

            ProjectConfig *dep_config    = config_load(dep_config_path);
            char          *resolved_path = NULL;

            if (dep_config)
            {
                // get the dependency's src directory (default to "src" if not specified)
                const char *dep_src_dir = dep_config->src_dir ? dep_config->src_dir : "src";

                if (dot_pos)
                {
                    // module_path has sub-path: "std.io.console" -> "dep/std/src/io/console.mach"
                    const char *sub_path = module_path + strlen(first_part) + 1; // skip dep name and dot

                    // convert dots to slashes for the remaining path
                    char *sub_path_copy = strdup(sub_path);
                    for (char *p = sub_path_copy; *p; p++)
                    {
                        if (*p == '.')
                            *p = '/';
                    }

                    size_t len    = strlen(dependency_path) + strlen(dep_src_dir) + strlen(sub_path_copy) + strlen(".mach") + 4;
                    resolved_path = malloc(len);
                    snprintf(resolved_path, len, "%s/%s/%s.mach", dependency_path, dep_src_dir, sub_path_copy);

                    free(sub_path_copy);
                }
                else
                {
                    // module_path is just dependency name: "std" -> "dep/std/src/main.mach"
                    size_t len    = strlen(dependency_path) + strlen(dep_src_dir) + strlen("/main.mach") + 3;
                    resolved_path = malloc(len);
                    snprintf(resolved_path, len, "%s/%s/main.mach", dependency_path, dep_src_dir);
                }

                config_dnit(dep_config);
                free(dep_config);
            }

            free(dep_dir);
            free(first_part);
            return resolved_path;
        }
    }

    free(first_part);
    return NULL; // not a dependency module path
}
