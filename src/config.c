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

void config_init(ProjectConfig *config)
{
    memset(config, 0, sizeof(ProjectConfig));
    config->opt_level = 2; // default optimization level
}

void config_dnit(ProjectConfig *config)
{
    free(config->name);
    free(config->version);
    free(config->main_file);
    free(config->target_name);
    free(config->src_dir);
    free(config->dep_dir);
    free(config->lib_dir);
    free(config->out_dir);
    free(config->bin_dir);
    free(config->obj_dir);
    free(config->target_triple);
    free(config->runtime_path);
    free(config->stdlib_path);

    for (int i = 0; i < config->dep_count; i++)
    {
        free(config->dependencies[i]);
    }
    free(config->dependencies);

    memset(config, 0, sizeof(ProjectConfig));
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

    // simple key-value parsing
    while (parser.pos < parser.len)
    {
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

        // parse value based on key
        if (strcmp(key, "name") == 0)
        {
            config->name = toml_parse_string(&parser);
        }
        else if (strcmp(key, "version") == 0)
        {
            config->version = toml_parse_string(&parser);
        }
        else if (strcmp(key, "main") == 0)
        {
            config->main_file = toml_parse_string(&parser);
        }
        else if (strcmp(key, "target-name") == 0)
        {
            config->target_name = toml_parse_string(&parser);
        }
        else if (strcmp(key, "src-dir") == 0)
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
        else if (strcmp(key, "bin-dir") == 0)
        {
            config->bin_dir = toml_parse_string(&parser);
        }
        else if (strcmp(key, "obj-dir") == 0)
        {
            config->obj_dir = toml_parse_string(&parser);
        }
        else if (strcmp(key, "target-triple") == 0)
        {
            config->target_triple = toml_parse_string(&parser);
        }
        else if (strcmp(key, "runtime-path") == 0)
        {
            config->runtime_path = toml_parse_string(&parser);
        }
        else if (strcmp(key, "stdlib-path") == 0)
        {
            config->stdlib_path = toml_parse_string(&parser);
        }
        else if (strcmp(key, "opt-level") == 0)
        {
            config->opt_level = toml_parse_number(&parser);
        }
        else if (strcmp(key, "emit-ast") == 0)
        {
            config->emit_ast = toml_parse_bool(&parser);
        }
        else if (strcmp(key, "emit-ir") == 0)
        {
            config->emit_ir = toml_parse_bool(&parser);
        }
        else if (strcmp(key, "emit-asm") == 0)
        {
            config->emit_asm = toml_parse_bool(&parser);
        }
        else if (strcmp(key, "emit-object") == 0)
        {
            config->emit_object = toml_parse_bool(&parser);
        }
        else if (strcmp(key, "build-library") == 0)
        {
            config->build_library = toml_parse_bool(&parser);
        }
        else if (strcmp(key, "no-pie") == 0)
        {
            config->no_pie = toml_parse_bool(&parser);
        }

        free(key);
    }

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
        fprintf(file, "main = \"%s\"\n", config->main_file);
    if (config->target_name)
        fprintf(file, "target-name = \"%s\"\n", config->target_name);
    if (config->target_triple)
        fprintf(file, "target-triple = \"%s\"\n", config->target_triple);

    fprintf(file, "\n[directories]\n");
    if (config->src_dir)
        fprintf(file, "src-dir = \"%s\"\n", config->src_dir);
    if (config->dep_dir)
        fprintf(file, "dep-dir = \"%s\"\n", config->dep_dir);
    if (config->lib_dir)
        fprintf(file, "lib-dir = \"%s\"\n", config->lib_dir);
    if (config->out_dir)
        fprintf(file, "out-dir = \"%s\"\n", config->out_dir);
    if (config->bin_dir)
        fprintf(file, "bin-dir = \"%s\"\n", config->bin_dir);
    if (config->obj_dir)
        fprintf(file, "obj-dir = \"%s\"\n", config->obj_dir);

    fprintf(file, "\n[build]\n");
    fprintf(file, "opt-level = %d\n", config->opt_level);

    if (config->emit_ast)
        fprintf(file, "emit-ast = true\n");
    if (config->emit_ir)
        fprintf(file, "emit-ir = true\n");
    if (config->emit_asm)
        fprintf(file, "emit-asm = true\n");
    if (config->emit_object)
        fprintf(file, "emit-object = true\n");
    if (config->build_library)
        fprintf(file, "build-library = true\n");
    if (config->no_pie)
        fprintf(file, "no-pie = true\n");

    if (config->runtime_path || config->stdlib_path)
    {
        fprintf(file, "\n[runtime]\n");
        if (config->runtime_path)
            fprintf(file, "runtime-path = \"%s\"\n", config->runtime_path);
        if (config->stdlib_path)
            fprintf(file, "stdlib-path = \"%s\"\n", config->stdlib_path);
    }

    fclose(file);
    return true;
}

ProjectConfig *config_create_default(const char *project_name)
{
    ProjectConfig *config = malloc(sizeof(ProjectConfig));
    config_init(config);

    config->name        = strdup(project_name);
    config->version     = strdup("0.1.0");
    config->main_file   = strdup("main.mach");
    config->target_name = strdup(project_name);

    // no default directories - user must configure them
    // this ensures all paths are explicitly configured

    return config;
}

bool config_has_main_file(ProjectConfig *config)
{
    return config && config->main_file && strlen(config->main_file) > 0;
}

bool config_should_emit_ast(ProjectConfig *config)
{
    return config && config->emit_ast;
}

bool config_should_emit_ir(ProjectConfig *config)
{
    return config && config->emit_ir;
}

bool config_should_emit_asm(ProjectConfig *config)
{
    return config && config->emit_asm;
}

bool config_should_emit_object(ProjectConfig *config)
{
    return config && config->emit_object;
}

bool config_should_build_library(ProjectConfig *config)
{
    return config && config->build_library;
}

bool config_should_link_executable(ProjectConfig *config)
{
    return config && !config->build_library && !config->emit_object;
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

char *config_resolve_bin_dir(ProjectConfig *config, const char *project_dir)
{
    char *out_dir = config_resolve_out_dir(config, project_dir);
    if (!out_dir)
        return NULL;

    if (!config->bin_dir)
    {
        free(out_dir);
        return NULL;
    }

    // if bin dir is absolute path, return as is
    if (config->bin_dir[0] == '/')
        return strdup(config->bin_dir);

    // resolve relative to out directory
    size_t len  = strlen(out_dir) + strlen(config->bin_dir) + 2;
    char  *path = malloc(len);
    snprintf(path, len, "%s/%s", out_dir, config->bin_dir);

    free(out_dir);
    return path;
}

char *config_resolve_obj_dir(ProjectConfig *config, const char *project_dir)
{
    char *out_dir = config_resolve_out_dir(config, project_dir);
    if (!out_dir)
        return NULL;

    if (!config->obj_dir)
    {
        free(out_dir);
        return NULL;
    }

    // if obj dir is absolute path, return as is
    if (config->obj_dir[0] == '/')
        return strdup(config->obj_dir);

    // resolve relative to out directory
    size_t len  = strlen(out_dir) + strlen(config->obj_dir) + 2;
    char  *path = malloc(len);
    snprintf(path, len, "%s/%s", out_dir, config->obj_dir);

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

    char *bin_dir = config_resolve_bin_dir(config, project_dir);
    if (bin_dir)
    {
        if (!ensure_directory_exists(bin_dir))
        {
            free(bin_dir);
            return false;
        }
        free(bin_dir);
    }

    char *obj_dir = config_resolve_obj_dir(config, project_dir);
    if (obj_dir)
    {
        if (!ensure_directory_exists(obj_dir))
        {
            free(obj_dir);
            return false;
        }
        free(obj_dir);
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

    if (config->opt_level < 0 || config->opt_level > 3)
        return false;

    // validate that required directories are configured
    if (!config->src_dir)
        return false;

    if (!config->out_dir)
        return false;

    if (!config->bin_dir)
        return false;

    if (!config->obj_dir)
        return false;

    return true;
}
