#include "ast.h"
#include "codegen.h"
#include "config.h"
#include "lexer.h"
#include "module.h"
#include "parser.h"
#include "semantic.h"

#include <dirent.h>
#include <llvm-c/Target.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct BuildOptions
{
    bool        emit_ast;
    bool        emit_ir;
    bool        emit_asm;
    bool        emit_object;
    bool        link_executable;
    bool        build_library; // build as library instead of executable
    bool        no_pie;        // disable PIE
    bool        include_runtime;
    const char *output_file;
    int         opt_level;
    char      **link_objects;  // additional object files to link
    int         link_count;    // number of link objects
    int         link_capacity; // capacity of link_objects array
} BuildOptions;

// forward declarations
static int  dep_add_command(int argc, char **argv);
static int  dep_remove_command(int argc, char **argv);
static int  dep_list_command(int argc, char **argv);
static int  examine_command(int argc, char **argv);
static bool copy_directory(const char *src, const char *dest);
static bool is_valid_mach_project(const char *path);
static bool ensure_directory(const char *path);
static bool ensure_runtime_module(SemanticAnalyzer *analyzer, const char *module_path);
typedef struct StringList { char **items; int count; int cap; } StringList;
static bool sl_add(StringList *list, const char *s);
static void sl_free(StringList *list);
static bool collect_library_directory(ProjectConfig *config,
                                      const char    *dir_path,
                                      const char    *package_name,
                                      const char    *relative_prefix,
                                      StringList    *out_modules);
static bool prepare_library_modules(SemanticAnalyzer *analyzer, ProjectConfig *config, const char *project_dir);

static void print_usage(const char *program_name)
{
    fprintf(stderr, "usage: %s <command> [options]\n", program_name);
    fprintf(stderr, "commands:\n");
    fprintf(stderr, "  init                      initialize a new project\n");
    fprintf(stderr, "  build [file/dir] [options] build project or single file\n");
    fprintf(stderr, "  clean                     clean build artifacts\n");
    fprintf(stderr, "  dep <subcommand>          dependency management\n");
    fprintf(stderr, "  help [command]            show help message\n");
    fprintf(stderr, "\nbuild options:\n");
    fprintf(stderr, "  -o <file>     set output file name\n");
    fprintf(stderr, "  -O<level>     optimization level (0-3, default: 2)\n");
    fprintf(stderr, "  --lib         build as library (shared object)\n");
    fprintf(stderr, "  --emit-ast    emit abstract syntax tree (.ast file)\n");
    fprintf(stderr, "  --emit-ir     emit llvm ir (.ll file)\n");
    fprintf(stderr, "  --emit-asm    emit assembly (.s file)\n");
    fprintf(stderr, "  --emit-obj    emit object file (.o file)\n");
    fprintf(stderr, "  --no-link     don't create executable (just compile)\n");
    fprintf(stderr, "  --no-pie      disable position independent executable\n");
    fprintf(stderr, "  --link <obj>  link with additional object file\n");
    fprintf(stderr, "  --no-runtime  skip linking runtime support\n");
}

static void print_dep_usage(void)
{
    fprintf(stderr, "usage: cmach dep <subcommand> [options]\n");
    fprintf(stderr, "dependency management subcommands:\n");
    fprintf(stderr, "  add <name> <path>         add a dependency with explicit path\n");
    fprintf(stderr, "  remove <name>             remove a dependency\n");
    fprintf(stderr, "  list                      list all dependencies\n");
    fprintf(stderr, "\nexamples:\n");
    fprintf(stderr, "  cmach dep add std $MACH_STD    # add standard library\n");
    fprintf(stderr, "  cmach dep add mylib ./libs/mylib  # add local dependency\n");
}

static bool ends_with(const char *s, const char *suffix)
{
    if (!s || !suffix)
        return false;
    size_t ls = strlen(s), lf = strlen(suffix);
    if (lf > ls)
        return false;
    return strncmp(s + (ls - lf), suffix, lf) == 0;
}

// removed: get_default_target_triple no longer used during init

static char *read_file(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (!file)
    {
        fprintf(stderr, "error: could not open file '%s'\n", path);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *buffer = malloc(size + 1);
    if (!buffer)
    {
        fprintf(stderr, "error: could not allocate memory for file\n");
        fclose(file);
        return NULL;
    }

    size_t read  = fread(buffer, 1, size, file);
    buffer[read] = '\0';
    fclose(file);

    return buffer;
}

static char *get_filename(const char *path)
{
    const char *last_slash = strrchr(path, '/');
    return last_slash ? strdup(last_slash + 1) : strdup(path);
}

static char *get_base_filename(const char *path)
{
    const char *last_slash = strrchr(path, '/');
    const char *filename   = last_slash ? last_slash + 1 : path;

    const char *last_dot = strrchr(filename, '.');
    if (!last_dot)
        return strdup(filename);

    size_t len  = last_dot - filename;
    char  *base = malloc(len + 1);
    strncpy(base, filename, len);
    base[len] = '\0';

    return base;
}

static char *get_executable_path(ProjectConfig *config, const char *project_dir, const char *target_name)
{
    if (!config || !config->target_name || !target_name)
        return NULL;

    char *bin_dir = config_resolve_bin_dir(config, project_dir, target_name);
    if (!bin_dir)
        return NULL;

    size_t len  = strlen(bin_dir) + strlen(config->target_name) + 2;
    char  *path = malloc(len);
    snprintf(path, len, "%s/%s", bin_dir, config->target_name);

    free(bin_dir);
    return path;
}

// produce default library output path inside bin dir (e.g., bin/lib<name>.so)
static char *get_library_output_path(ProjectConfig *config, const char *project_dir, const char *target_name)
{
    if (!config || !project_dir || !target_name)
        return NULL;
    bool  shared  = config_is_shared_library(config, target_name);
    char *bin_dir = config_resolve_bin_dir(config, project_dir, target_name);
    if (!bin_dir)
        return NULL;
    char *lib_name = config_default_library_name(config, shared);
    if (!lib_name)
    {
        free(bin_dir);
        return NULL;
    }
    size_t len  = strlen(bin_dir) + 1 + strlen(lib_name) + 1;
    char  *path = malloc(len);
    snprintf(path, len, "%s/%s", bin_dir, lib_name);
    free(bin_dir);
    free(lib_name);
    return path;
}

static char *get_directory(const char *path)
{
    const char *last_slash = strrchr(path, '/');
    if (!last_slash)
        return strdup(".");

    size_t len = last_slash - path;
    char  *dir = malloc(len + 1);
    strncpy(dir, path, len);
    dir[len] = '\0';

    return dir;
}

static char *create_output_filename(const char *input_file, const char *extension)
{
    char  *base   = get_base_filename(input_file);
    size_t len    = strlen(base) + strlen(extension) + 1;
    char  *output = malloc(len);
    snprintf(output, len, "%s%s", base, extension);
    free(base);
    return output;
}

static bool ensure_directory(const char *path)
{
    if (!path)
        return false;

    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode))
    {
        return true;
    }

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "mkdir -p \"%s\"", path);
    return system(cmd) == 0;
}

static bool ensure_runtime_module(SemanticAnalyzer *analyzer, const char *module_path)
{
    if (!analyzer || !module_path || module_path[0] == '\0')
        return false;

    Module *module = module_manager_load_module(&analyzer->module_manager, module_path);
    if (!module)
        return false;

    if (!module_manager_resolve_dependencies(&analyzer->module_manager, module->ast, "."))
        return false;

    module->needs_linking = true;

    AstNode use_stub;
    memset(&use_stub, 0, sizeof(use_stub));
    use_stub.kind                   = AST_STMT_USE;
    use_stub.use_stmt.module_path   = strdup(module_path);
    use_stub.use_stmt.alias         = strdup("__mach_runtime");
    use_stub.use_stmt.module_sym    = NULL;

    if (!use_stub.use_stmt.module_path || !use_stub.use_stmt.alias)
    {
        free(use_stub.use_stmt.module_path);
        free(use_stub.use_stmt.alias);
        return false;
    }

    bool success = semantic_analyze_use_stmt(analyzer, &use_stub);
    if (success)
        success = semantic_analyze_imported_module(analyzer, &use_stub);

    free(use_stub.use_stmt.module_path);
    free(use_stub.use_stmt.alias);

    return success;
}

static bool sl_add(StringList *list, const char *s)
{
    if (list->count == list->cap)
    {
        int   ncap = list->cap ? list->cap * 2 : 16;
        char **n    = realloc(list->items, ncap * sizeof(char *));
        if (!n) return false;
        list->items = n; list->cap = ncap;
    }
    list->items[list->count++] = strdup(s);
    return true;
}

static void sl_free(StringList *list)
{
    for (int i = 0; i < list->count; i++) free(list->items[i]);
    free(list->items);
    memset(list, 0, sizeof(*list));
}

static bool collect_library_directory(ProjectConfig *config,
                                      const char    *dir_path,
                                      const char    *package_name,
                                      const char    *relative_prefix,
                                      StringList    *out_modules)
{
    DIR *dir = opendir(dir_path);
    if (!dir)
        return false;

    bool             success = true;
    struct dirent   *entry;
    while (success && (entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        size_t path_len = strlen(dir_path) + strlen(entry->d_name) + 2;
        char  *child     = malloc(path_len);
        if (!child)
        {
            success = false;
            break;
        }
        snprintf(child, path_len, "%s/%s", dir_path, entry->d_name);

        struct stat st;
        if (stat(child, &st) != 0)
        {
            free(child);
            success = false;
            break;
        }

        if (S_ISDIR(st.st_mode))
        {
            size_t new_prefix_len = relative_prefix ? strlen(relative_prefix) : 0;
            size_t name_len       = strlen(entry->d_name);
            size_t total_len      = new_prefix_len + (new_prefix_len > 0 ? 1 : 0) + name_len + 1;
            char  *new_prefix     = malloc(total_len);
            if (!new_prefix)
            {
                free(child);
                success = false;
                break;
            }

            if (new_prefix_len > 0)
                snprintf(new_prefix, total_len, "%s.%s", relative_prefix, entry->d_name);
            else
                snprintf(new_prefix, total_len, "%s", entry->d_name);

            success = collect_library_directory(config, child, package_name, new_prefix, out_modules);
            free(new_prefix);
            free(child);
            continue;
        }

        const char *dot = strrchr(entry->d_name, '.');
        if (!dot || strcmp(dot, ".mach") != 0)
        {
            free(child);
            continue;
        }

        size_t base_len = (size_t)(dot - entry->d_name);
        char  *base     = malloc(base_len + 1);
        if (!base)
        {
            free(child);
            success = false;
            break;
        }
        memcpy(base, entry->d_name, base_len);
        base[base_len] = '\0';

        size_t prefix_len  = relative_prefix ? strlen(relative_prefix) : 0;
        size_t module_len  = strlen(package_name) + 1 + prefix_len + (prefix_len > 0 ? 1 : 0) + base_len + 1;
        char  *module_path = malloc(module_len);
        if (!module_path)
        {
            free(base);
            free(child);
            success = false;
            break;
        }

        if (prefix_len > 0)
            snprintf(module_path, module_len, "%s.%s.%s", package_name, relative_prefix, base);
        else
            snprintf(module_path, module_len, "%s.%s", package_name, base);

    success = sl_add(out_modules, module_path);

        free(module_path);
        free(base);
        free(child);
    }

    closedir(dir);
    return success;
}

static bool prepare_library_modules(SemanticAnalyzer *analyzer, ProjectConfig *config, const char *project_dir)
{
    if (!analyzer || !config || !config->name)
        return false;

    char *src_dir = config_get_package_src_dir(config, project_dir, config->name);
    if (!src_dir)
        return false;

    StringList modules = {0};
    bool       ok      = collect_library_directory(config, src_dir, config->name, NULL, &modules);
    free(src_dir);
    if (!ok)
    {
        sl_free(&modules);
        return false;
    }

    // Build a synthetic program with 'use <module> as __lib_i' for each module
    AstNode *program = malloc(sizeof(AstNode));
    if (!program)
    {
        sl_free(&modules);
        return false;
    }
    ast_node_init(program, AST_PROGRAM);
    program->program.stmts = malloc(sizeof(AstList));
    if (!program->program.stmts)
    {
        free(program);
        sl_free(&modules);
        return false;
    }
    ast_list_init(program->program.stmts);

    for (int i = 0; i < modules.count; i++)
    {
        AstNode *use = malloc(sizeof(AstNode));
        if (!use)
        {
            ast_node_dnit(program); free(program); sl_free(&modules); return false;
        }
        ast_node_init(use, AST_STMT_USE);
        use->use_stmt.module_path = strdup(modules.items[i]);
        char aliasbuf[64]; snprintf(aliasbuf, sizeof(aliasbuf), "__lib_%d", i);
        use->use_stmt.alias = strdup(aliasbuf);
        use->use_stmt.module_sym = NULL;
    ast_list_append(program->program.stmts, use);
    }

    bool success = semantic_analyze(analyzer, program);

    ast_node_dnit(program);
    free(program);
    sl_free(&modules);
    return success;
}

static int init_command(int argc, char **argv)
{
    const char *project_name = "mach-project";
    const char *project_dir  = ".";

    // if project name is provided, create a new directory
    if (argc >= 3)
    {
        project_name = argv[2];
        project_dir  = project_name;

        // create project directory
        if (mkdir(project_name, 0755) != 0)
        {
            fprintf(stderr, "error: failed to create project directory '%s'\n", project_name);
            return 1;
        }
    }
    else
    {
        // initialize in current directory - get directory name for project name
        char *cwd = getcwd(NULL, 0);
        if (cwd)
        {
            const char *last_slash = strrchr(cwd, '/');
            if (last_slash && strlen(last_slash + 1) > 0)
                project_name = strdup(last_slash + 1);
            else
                project_name = strdup("mach-project");
            free(cwd);
        }
    }

    // create standard mach project directory structure
    char path[1024];

    // dep/ - dependency source files
    snprintf(path, sizeof(path), "%s/dep", project_dir);
    mkdir(path, 0755);

    // lib/ - library/object dependencies
    snprintf(path, sizeof(path), "%s/lib", project_dir);
    mkdir(path, 0755);

    // out/ - output directory
    snprintf(path, sizeof(path), "%s/out", project_dir);
    mkdir(path, 0755);

    // per-target subdirectories are created during builds based on target name

    // src/ - source files
    snprintf(path, sizeof(path), "%s/src", project_dir);
    mkdir(path, 0755);

    // create mach.toml with proper directory configuration
    snprintf(path, sizeof(path), "%s/mach.toml", project_dir);
    ProjectConfig *config = config_create_default(project_name);

    // directories already set in default config

    // add default target
    config->default_target = strdup("all"); // build all targets by default

    // add a default native target
    char *native_triple = LLVMGetDefaultTargetTriple();
    config_add_target(config, "native", native_triple);
    free(native_triple);

    // add default standard library dependency and copy it if available
    const char *mach_std = getenv("MACH_STD");
    if (mach_std)
    {
        // validate that MACH_STD points to a valid mach project
        if (!is_valid_mach_project(mach_std))
        {
            fprintf(stderr, "error: MACH_STD='%s' does not contain a valid mach project\n", mach_std);
            fprintf(stderr, "make sure MACH_STD points to a directory with a valid mach.toml file\n");
            config_dnit(config);
            free(config);
            return 1;
        }

        // expand tilde if necessary
        char *expanded_mach_std = NULL;
        if (mach_std[0] == '~' && (mach_std[1] == '/' || mach_std[1] == '\0'))
        {
            const char *home = getenv("HOME");
            if (home)
            {
                size_t len        = strlen(home) + strlen(mach_std) + 1;
                expanded_mach_std = malloc(len);
                snprintf(expanded_mach_std, len, "%s%s", home, mach_std + 1);
                mach_std = expanded_mach_std;
            }
        }

        // automatically add and copy std dependency
        printf("adding default standard library dependency from MACH_STD...\n");

        char *dep_dir = config_resolve_dep_dir(config, project_dir);
        if (dep_dir)
        {
            char dest_path[1024];
            snprintf(dest_path, sizeof(dest_path), "%s/std", dep_dir);

            if (copy_directory(mach_std, dest_path))
            {
                printf("standard library copied to dep/std\n");
                printf("note: import as 'std.*'\n");
                // register std dependency spec
                DepSpec *std_dep  = calloc(1, sizeof(DepSpec));
                std_dep->name     = strdup("std");
                std_dep->path     = strdup("dep/std");
                std_dep->src_dir  = strdup("src");
                DepSpec **new_arr = realloc(config->deps, (config->dep_count + 1) * sizeof(DepSpec *));
                if (new_arr)
                {
                    config->deps                      = new_arr;
                    config->deps[config->dep_count++] = std_dep;
                }
                else
                {
                    free(std_dep->name);
                    free(std_dep->path);
                    free(std_dep->src_dir);
                    free(std_dep);
                }
            }
            else
            {
                fprintf(stderr, "error: failed to copy standard library from MACH_STD\n");
                config_dnit(config);
                free(config);
                free(expanded_mach_std);
                return 1;
            }

            free(dep_dir);
        }

        free(expanded_mach_std);
    }
    else
    {
        fprintf(stderr, "error: MACH_STD environment variable not set\n");
        fprintf(stderr, "set MACH_STD to the path of your mach standard library\n");
        fprintf(stderr, "example: export MACH_STD=/path/to/mach-std\n");
        config_dnit(config);
        free(config);
        return 1;
    }

    if (!config_save(config, path))
    {
        fprintf(stderr, "error: failed to create mach.toml\n");
        config_dnit(config);
        free(config);
        return 1;
    }

    // create main.mach in src directory
    snprintf(path, sizeof(path), "%s/src/main.mach", project_dir);
    FILE *main_file = fopen(path, "w");
    if (!main_file)
    {
        fprintf(stderr, "error: failed to create src/main.mach\n");
        config_dnit(config);
        free(config);
        return 1;
    }

    fprintf(main_file, "use console: std.io.console;\n\n");
    fprintf(main_file, "fun main() u32 {\n");
    fprintf(main_file, "    console.print(\"Hello, world!\\n\");\n");
    fprintf(main_file, "    ret 0;\n");
    fprintf(main_file, "}\n");
    fclose(main_file);

    if (argc >= 3)
    {
        printf("created project '%s' with the following structure:\n", project_name);
        printf("  %s/\n", project_name);
    }
    else
    {
        printf("initialized project '%s' with the following structure:\n", project_name);
    }
    printf("  ├── dep/          # dependency source files\n");
    printf("  ├── lib/          # library/object dependencies\n");
    printf("  ├── out/          # output directory\n");
    printf("  │   └── native/   # build target\n");
    printf("  │       ├── bin/  # binary output\n");
    printf("  │       └── obj/  # object files\n");
    printf("  ├── src/          # source files\n");
    printf("  │   └── main.mach # main source file\n");
    printf("  └── mach.toml     # project configuration\n");
    if (argc >= 3)
    {
        printf("\nto build: cd %s && cmach build\n", project_name);
    }
    else
    {
        printf("\nto build: cmach build\n");
    }

    config_dnit(config);
    free(config);
    return 0;
}

static int clean_command(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    // load project config
    ProjectConfig *config = config_load_from_dir(".");
    if (!config)
    {
        fprintf(stderr, "error: no mach.toml found in current directory\n");
        return 1;
    }

    // discover dependencies for clean command

    char       *out_dir    = config_resolve_out_dir(config, ".");
    const char *output_dir = out_dir ? out_dir : ".";

    printf("cleaning build artifacts...\n");

    // remove output directory
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", output_dir);
    system(cmd);

    // remove temporary files
    system("find . -name '*.ast' -delete");
    system("find . -name '*.ll' -delete");
    system("find . -name '*.s' -delete");
    system("find . -name '*.o' -delete");

    printf("clean complete\n");

    free(out_dir);

    config_dnit(config);
    free(config);
    return 0;
}

static BuildOptions config_to_build_options(ProjectConfig *config, const char *target_name)
{
    BuildOptions options = {0};
    options.link_executable = true;
    options.opt_level       = 2;
    options.emit_object     = true;
    options.include_runtime = true;

    if (config && target_name)
    {
        TargetConfig *target = config_get_target(config, target_name);
        if (target)
        {
            options.emit_ast        = target->emit_ast;
            options.emit_ir         = target->emit_ir;
            options.emit_asm        = target->emit_asm;
            options.emit_object     = target->emit_object;
            options.build_library   = target->build_library;
            options.no_pie          = target->no_pie;
            options.opt_level       = target->opt_level;
            options.link_executable = !target->build_library;
            options.include_runtime = !target->build_library;
        }
    }

    return options;
}

static int build_command(int argc, char **argv)
{
    const char    *filename           = NULL;
    ProjectConfig *config             = NULL;
    char          *resolved_main_file = NULL;
    bool           is_project_build   = false;
    const char    *project_dir        = ".";
    char          *object_dir         = NULL;
    char          *root_module_name   = NULL;
    char          *owned_obj_path     = NULL;
    char          *precomputed_obj_path = NULL;
    const char    *runtime_module_path = NULL;

    // check if a directory path is provided
    if (argc >= 3 && argv[2][0] != '-')
    {
        // check if it's a directory
        struct stat st;
        if (stat(argv[2], &st) == 0 && S_ISDIR(st.st_mode))
        {
            project_dir = argv[2];
            // try to load config from the specified directory
            config           = config_load_from_dir(project_dir);
            is_project_build = config != NULL;
            if (config && config_has_main_file(config))
            {
                resolved_main_file = config_resolve_main_file(config, project_dir);
                filename           = resolved_main_file;
            }
        }
        else
        {
            // treat as a file path
            filename         = argv[2];
            is_project_build = false;
        }
    }

    // if no directory/file specified, try current directory
    if (!config)
    {
        config = config_load_from_dir(".");
        is_project_build = config != NULL;
        if (config && config_has_main_file(config))
        {
            resolved_main_file = config_resolve_main_file(config, ".");
            filename           = resolved_main_file;
        }
    }

    if (!config && !filename)
    {
        fprintf(stderr, "error: no input file specified and no project configuration found\n");
        print_usage(argv[0]);
        return 1;
    }

    // initialize build options from config or defaults
    // TODO: implement proper target selection, for now use default target
    const char *target_name = "native"; // default target name
    if (config && config->target_count > 0)
    {
        target_name = config->targets[0]->name; // use first target for now
    }
    BuildOptions options = config_to_build_options(config, target_name);

    if (config)
    {
        if (config_should_emit_ast(config, target_name))
            options.emit_ast = true;
        if (config_should_emit_ir(config, target_name))
            options.emit_ir = true;
        if (config_should_emit_asm(config, target_name))
            options.emit_asm = true;
        if (config_should_emit_object(config, target_name))
            options.emit_object = true;

        if (config_should_build_library(config, target_name))
        {
            options.build_library   = true;
            options.link_executable = false;
            options.include_runtime = false;
        }
        else if (!config_should_link_executable(config, target_name))
        {
            options.link_executable = false;
        }

        // if the project has no entrypoint, treat it as a library build
        if (!config_has_main_file(config))
        {
            options.build_library   = true;
            options.link_executable = false;
            options.include_runtime = false;
        }
    }

    // parse command line options (these override config)
    int start_idx = is_project_build ? 2 : 3;
    for (int i = start_idx; i < argc; i++)
    {
        if (strcmp(argv[i], "-o") == 0)
        {
            if (i + 1 < argc)
            {
                options.output_file = argv[++i];
            }
            else
            {
                fprintf(stderr, "error: -o requires a filename\n");
                return 1;
            }
        }
        else if (strncmp(argv[i], "-O", 2) == 0)
        {
            options.opt_level = atoi(argv[i] + 2);
            if (options.opt_level < 0 || options.opt_level > 3)
            {
                fprintf(stderr, "error: invalid optimization level\n");
                return 1;
            }
        }
        else if (strcmp(argv[i], "--emit-ast") == 0)
        {
            options.emit_ast = true;
        }
        else if (strcmp(argv[i], "--emit-ir") == 0)
        {
            options.emit_ir = true;
        }
        else if (strcmp(argv[i], "--emit-asm") == 0)
        {
            options.emit_asm = true;
        }
        else if (strcmp(argv[i], "--emit-obj") == 0)
        {
            options.emit_object     = true;
            options.link_executable = false; // don't link when just emitting object
        }
        else if (strcmp(argv[i], "--lib") == 0)
        {
            options.build_library   = true;
            options.link_executable = false;
            options.include_runtime = false;
        }
        else if (strcmp(argv[i], "--no-link") == 0)
        {
            options.link_executable = false;
        }
        else if (strcmp(argv[i], "--no-pie") == 0)
        {
            options.no_pie = true;
        }
        else if (strcmp(argv[i], "--no-runtime") == 0)
        {
            options.include_runtime = false;
        }
        else if (strcmp(argv[i], "--link") == 0)
        {
            if (i + 1 < argc)
            {
                // add object file to link list
                if (options.link_count >= options.link_capacity)
                {
                    options.link_capacity = options.link_capacity ? options.link_capacity * 2 : 4;
                    options.link_objects  = realloc(options.link_objects, options.link_capacity * sizeof(char *));
                }
                options.link_objects[options.link_count++] = argv[++i];
            }
            else
            {
                fprintf(stderr, "error: --link requires an object file\n");
                return 1;
            }
        }
        else
        {
            fprintf(stderr, "error: unknown option '%s'\n", argv[i]);
            return 1;
        }
    }

    bool has_entry_file = filename != NULL;

    // determine output file
    char *default_output = NULL;
    if (!options.output_file)
    {
        if (is_project_build && config)
        {
            // choose default output based on target type
            if (config_should_build_library(config, target_name) || !config_should_link_executable(config, target_name))
                default_output = get_library_output_path(config, project_dir, target_name);
            else
                default_output = get_executable_path(config, project_dir, target_name);
            if (default_output)
            {
                options.output_file = default_output;

                // ensure output directory exists
                char *output_dir = get_directory(default_output);
                char  cmd[1024];
                snprintf(cmd, sizeof(cmd), "mkdir -p %s", output_dir);
                system(cmd);
                free(output_dir);
            }
        }

        // fallback to traditional behavior
        if (!options.output_file)
        {
            if (options.build_library && has_entry_file && filename)
            {
                default_output      = create_output_filename(filename, ".so");
                options.output_file = default_output;
            }
            else if (options.link_executable && has_entry_file && filename)
            {
                default_output      = get_base_filename(filename);
                options.output_file = default_output;
            }
            else if (options.emit_object && !options.emit_ir && !options.emit_asm && has_entry_file && filename)
            {
                default_output      = create_output_filename(filename, ".o");
                options.output_file = default_output;
            }
            else if (options.emit_ir && !options.emit_asm && !options.emit_object && has_entry_file && filename)
            {
                default_output      = create_output_filename(filename, ".ll");
                options.output_file = default_output;
            }
            else if (options.emit_asm && !options.emit_ir && !options.emit_object && has_entry_file && filename)
            {
                default_output      = create_output_filename(filename, ".s");
                options.output_file = default_output;
            }
        }
    }

    if (!options.link_executable)
    {
        options.include_runtime = false;
    }

    if (!has_entry_file && !options.build_library)
    {
        fprintf(stderr, "error: no input file specified; provide a source file or configure a library target\n");
        if (config)
        {
            config_dnit(config);
            free(config);
        }
        return 1;
    }

    char   *source             = NULL;
    bool    lexer_initialized  = false;
    bool    parser_initialized = false;
    Lexer   lexer;
    Parser  parser;
    AstNode *program = NULL;

    if (has_entry_file)
    {
        source = read_file(filename);
        if (!source)
        {
            free(default_output);
            return 1;
        }

        lexer_init(&lexer, source);
        lexer_initialized = true;

        parser_init(&parser, &lexer);
        parser_initialized = true;

        printf("parsing '%s'...\n", filename);
        fflush(stdout);

        program = parser_parse_program(&parser);

        if (parser.had_error)
        {
            fprintf(stderr, "parsing failed with %d error(s):\n", parser.errors.count);
            parser_error_list_print(&parser.errors, &lexer, filename);

            ast_node_dnit(program);
            free(program);
            parser_dnit(&parser);
            lexer_dnit(&lexer);
            free(source);
            free(default_output);
            return 1;
        }

        if (options.emit_ast)
        {
            char *ast_file = create_output_filename(filename, ".ast");

            printf("writing abstract syntax tree to '%s'...\n", ast_file);
            fflush(stdout);

            if (!ast_emit(program, ast_file))
            {
                printf("failed to write AST\n");
            }

            free(ast_file);
        }
    }
    else
    {
        program = malloc(sizeof(AstNode));
        if (!program)
        {
            free(default_output);
            return 1;
        }
        ast_node_init(program, AST_PROGRAM);
        program->program.stmts = malloc(sizeof(AstList));
        if (!program->program.stmts)
        {
            free(program);
            free(default_output);
            return 1;
        }
        ast_list_init(program->program.stmts);
    }

    // initialize semantic analyzer with module manager
    SemanticAnalyzer analyzer;
    semantic_analyzer_init(&analyzer);

    // set configuration for dependency resolution
    if (config)
    {
        // discover dependencies before setting up module manager
        module_manager_set_config(&analyzer.module_manager, config, project_dir);
    }

    // add search paths for modules
    char *base_dir = NULL;
    if (has_entry_file)
    {
        base_dir = get_directory(filename);
    }
    else if (config)
    {
        base_dir = config_resolve_src_dir(config, project_dir);
    }
    if (!base_dir)
        base_dir = strdup(".");

    module_manager_add_search_path(&analyzer.module_manager, base_dir);
    module_manager_add_search_path(&analyzer.module_manager, ".");

    // add dependency directory from config
    if (config)
    {
        char *dep_dir = config_resolve_dep_dir(config, project_dir);
        if (dep_dir)
        {
            module_manager_add_search_path(&analyzer.module_manager, dep_dir);
            free(dep_dir);
        }
    }

    // add standard library path from config or environment
    const char *stdlib_path = NULL;
    if (config && config->stdlib_path)
    {
        stdlib_path = config->stdlib_path;
    }
    else
    {
        stdlib_path = getenv("MACH_STDLIB");
    }

    if (stdlib_path)
    {
        module_manager_add_search_path(&analyzer.module_manager, stdlib_path);
    }

    printf("analyzing program...\n");
    fflush(stdout);

    // perform semantic analysis (includes module resolution)
    bool semantic_success = semantic_analyze(&analyzer, program);

    if (!semantic_success)
    {
        if (analyzer.errors.count > 0)
        {
            fprintf(stderr, "semantic analysis failed with %d error(s):\n", analyzer.errors.count);
            if (has_entry_file)
                semantic_print_errors(&analyzer, &lexer, filename);
        }

        if (analyzer.module_manager.had_error)
        {
            fprintf(stderr, "module loading failed with %d error(s):\n", analyzer.module_manager.errors.count);
            module_error_list_print(&analyzer.module_manager.errors);
        }

        free(base_dir);
        semantic_analyzer_dnit(&analyzer);
        ast_node_dnit(program);
        free(program);
        if (parser_initialized)
            parser_dnit(&parser);
        if (lexer_initialized)
            lexer_dnit(&lexer);
        free(source);
        free(default_output);
        return 1;
    }

    if (config)
        object_dir = config_resolve_obj_dir(config, project_dir, target_name);
    if (!object_dir)
        object_dir = strdup("bin/obj");

    if (!object_dir || !ensure_directory(object_dir))
    {
        fprintf(stderr, "error: failed to prepare object directory\n");

        free(object_dir);
        free(base_dir);
        semantic_analyzer_dnit(&analyzer);
        ast_node_dnit(program);
        free(program);
        parser_dnit(&parser);
        lexer_dnit(&lexer);
        free(source);
        free(default_output);
        return 1;
    }

    if (options.include_runtime)
    {
        if (config && config_has_runtime_module(config))
        {
            runtime_module_path = config->runtime_module;
        }
        else if (config)
        {
            const char *inferred = NULL;
            for (int i = 0; i < config->dep_count && !inferred; i++)
            {
                DepSpec *d = config->deps[i];
                if (d && d->is_runtime && d->name)
                {
                    // use <dep>.runtime
                    // small stack buffer is fine for typical names
                    char buf[256];
                    snprintf(buf, sizeof(buf), "%s.runtime", d->name);
                    config_set_runtime_module(config, buf);
                    inferred = config->runtime_module;
                }
            }
            if (!inferred && config_has_dep(config, "std"))
            {
                config_set_runtime_module(config, "std.runtime");
                inferred = config->runtime_module;
            }
            runtime_module_path = inferred;
        }
        if (!runtime_module_path)
        {
            fprintf(stderr, "error: no runtime module configured; set 'runtime.runtime' in mach.toml or pass --no-runtime\n");

            free(object_dir);
            free(base_dir);
            semantic_analyzer_dnit(&analyzer);
            ast_node_dnit(program);
            free(program);
            if (parser_initialized)
                parser_dnit(&parser);
            if (lexer_initialized)
                lexer_dnit(&lexer);
            free(source);
            free(default_output);
            return 1;
        }

        if (runtime_module_path && !ensure_runtime_module(&analyzer, runtime_module_path))
        {
            fprintf(stderr, "error: failed to prepare runtime module '%s'\n", runtime_module_path);

            free(object_dir);
            free(base_dir);
            semantic_analyzer_dnit(&analyzer);
            ast_node_dnit(program);
            free(program);
            parser_dnit(&parser);
            lexer_dnit(&lexer);
            free(source);
            free(default_output);
            return 1;
        }
    }

    if (config && object_dir)
    {
        const char *preferred_name = NULL;
        char       *fallback_name  = NULL;

        if (config->target_name && config->target_name[0] != '\0')
        {
            preferred_name = config->target_name;
        }
        else if (config->name && config->name[0] != '\0')
        {
            preferred_name = config->name;
        }
        else if (root_module_name && root_module_name[0] != '\0')
        {
            preferred_name = root_module_name;
        }
        else
        {
            if (has_entry_file && filename)
            {
                fallback_name  = get_base_filename(filename);
                preferred_name = fallback_name;
            }
            else
            {
                preferred_name = "module";
            }
        }

        if (preferred_name)
        {
            precomputed_obj_path = module_make_object_path(object_dir, preferred_name);
            if (!precomputed_obj_path)
            {
                fprintf(stderr, "error: failed to prepare project object output path\n");

                free(fallback_name);
                free(object_dir);
                free(base_dir);
                semantic_analyzer_dnit(&analyzer);
                ast_node_dnit(program);
                free(program);
                if (parser_initialized)
                    parser_dnit(&parser);
                if (lexer_initialized)
                    lexer_dnit(&lexer);
                free(source);
                free(default_output);
                return 1;
            }
        }

        free(fallback_name);
    }

    if (config && options.build_library)
    {
        if (!prepare_library_modules(&analyzer, config, project_dir))
        {
            fprintf(stderr, "error: failed to prepare library modules\n");
            if (analyzer.errors.count > 0)
            {
                fprintf(stderr, "semantic errors during library preparation:\n");
                semantic_print_errors(&analyzer, NULL, NULL);
            }
            if (analyzer.module_manager.had_error)
            {
                fprintf(stderr, "module loading errors during library preparation:\n");
                module_error_list_print(&analyzer.module_manager.errors);
            }

            free(precomputed_obj_path);
            free(object_dir);
            free(base_dir);
            semantic_analyzer_dnit(&analyzer);
            ast_node_dnit(program);
            free(program);
            if (parser_initialized)
                parser_dnit(&parser);
            if (lexer_initialized)
                lexer_dnit(&lexer);
            free(source);
            free(default_output);
            return 1;
        }
    }

    if (!module_manager_compile_dependencies(&analyzer.module_manager, object_dir, options.opt_level, options.no_pie))
    {
        fprintf(stderr, "dependency compilation failed\n");
        if (analyzer.module_manager.had_error)
        {
            module_error_list_print(&analyzer.module_manager.errors);
        }

        free(precomputed_obj_path);
        free(object_dir);
        free(base_dir);
        semantic_analyzer_dnit(&analyzer);
        ast_node_dnit(program);
        free(program);
        if (parser_initialized)
            parser_dnit(&parser);
        if (lexer_initialized)
            lexer_dnit(&lexer);
        free(source);
        free(default_output);
        return 1;
    }

    bool            emit_success       = true;
    CodegenContext  codegen;
    bool            codegen_initialized = false;

    if (has_entry_file)
    {
        printf("generating code...\n");
        fflush(stdout);

        char *display_name = get_filename(filename);
        codegen_context_init(&codegen, display_name ? display_name : filename, options.no_pie);
        codegen_initialized = true;
        codegen.opt_level   = options.opt_level;
        if (display_name && strcmp(display_name, "runtime.mach") == 0)
        {
            codegen.is_runtime = true;
        }

        if (!root_module_name)
        {
            if (config && config->name)
                root_module_name = strdup(config->name);
            else
                root_module_name = get_base_filename(filename);
        }

        if (root_module_name)
        {
            char *root_package = module_sanitize_name(root_module_name);
            if (root_package)
                codegen.package_name = root_package;
        }

        free(display_name);

    // inform codegen whether runtime is in play (affects main mangling)
    codegen.use_runtime = options.include_runtime;
    bool codegen_success = codegen_generate(&codegen, program, &analyzer);

        if (!codegen_success)
        {
            fprintf(stderr, "code generation failed:\n");
            codegen_print_errors(&codegen);

            codegen_context_dnit(&codegen);
            free(base_dir);
            free(object_dir);
            free(precomputed_obj_path);
            free(root_module_name);
            semantic_analyzer_dnit(&analyzer);
            ast_node_dnit(program);
            free(program);
            if (parser_initialized)
                parser_dnit(&parser);
            if (lexer_initialized)
                lexer_dnit(&lexer);
            free(source);
            free(default_output);
            return 1;
        }

        if (options.emit_ir)
        {
            char *ir_file = create_output_filename(filename, ".ll");

            printf("writing llvm ir to '%s'...\n", ir_file);
            fflush(stdout);

            if (!codegen_emit_llvm_ir(&codegen, ir_file))
            {
                printf("failed to write LLVM IR\n");
                emit_success = false;
            }

            free(ir_file);
        }

        if (options.emit_asm)
        {
            char *asm_file = create_output_filename(filename, ".s");

            printf("writing assembly to '%s'...\n", asm_file);
            fflush(stdout);

            if (!codegen_emit_assembly(&codegen, asm_file))
            {
                printf("failed to write assembly\n");
                emit_success = false;
            }

            free(asm_file);
        }

        if (options.emit_object || options.link_executable || options.build_library)
        {
            const char *obj_file = NULL;
            if (options.emit_object && !options.link_executable && !options.build_library && options.output_file && ends_with(options.output_file, ".o"))
            {
                obj_file = options.output_file;
            }
            else
            {
                if (precomputed_obj_path)
                {
                    owned_obj_path       = precomputed_obj_path;
                    precomputed_obj_path = NULL;
                    obj_file             = owned_obj_path;
                }
                else
                {
                    const char *object_name  = NULL;
                    char       *fallback_name = NULL;

                    if (root_module_name)
                        object_name = root_module_name;
                    else
                        object_name = fallback_name = get_base_filename(filename);

                    owned_obj_path = module_make_object_path(object_dir, object_name ? object_name : "module");
                    free(fallback_name);
                    obj_file = owned_obj_path;
                }
            }

            if (!obj_file)
            {
                fprintf(stderr, "error: failed to determine object file path\n");
                emit_success = false;
            }
            else
            {
                if (options.emit_object)
                {
                    printf("writing object file to '%s'...\n", obj_file);
                    fflush(stdout);
                }

                if (!codegen_emit_object(&codegen, obj_file))
                {
                    if (options.emit_object)
                        printf("failed to write object file\n");
                    else
                        fprintf(stderr, "error: failed to generate object file\n");
                    emit_success = false;
                }
            }

            // decide whether to link an executable
            bool want_link_exe = (!options.build_library) && (config ? config_should_link_executable(config, target_name) : options.link_executable);
            if (!want_link_exe && is_project_build && config && config_has_main_file(config))
                want_link_exe = true;
            if (want_link_exe && emit_success)
            {
                const char *output_exe = options.output_file ? options.output_file : (default_output ? default_output : config_default_executable_name(config));

                printf("linking executable '%s'...\n", output_exe);
                fflush(stdout);

                char **dep_objects = NULL;
                int    dep_count   = 0;
                if (!module_manager_get_link_objects(&analyzer.module_manager, &dep_objects, &dep_count))
                {
                    fprintf(stderr, "error: failed to collect dependency object files\n");
                    emit_success = false;
                }
                else
                {
                    size_t cmd_size = 1024 + (dep_count * 256) + (options.link_count * 256);
                    char  *link_cmd = malloc(cmd_size);

                    snprintf(link_cmd, cmd_size, options.no_pie ? "cc -no-pie -o %s %s" : "cc -pie -o %s %s", output_exe, obj_file);

                    for (int i = 0; i < dep_count; i++)
                    {
                        strcat(link_cmd, " ");
                        strcat(link_cmd, dep_objects[i]);
                    }

                    for (int i = 0; i < options.link_count; i++)
                    {
                        strcat(link_cmd, " ");
                        strcat(link_cmd, options.link_objects[i]);
                    }

                    if (system(link_cmd) != 0)
                    {
                        printf("failed to link executable\n");
                        emit_success = false;
                    }
                    else if (!options.emit_object && owned_obj_path)
                    {
                        unlink(owned_obj_path);
                    }

                    free(link_cmd);

                    for (int i = 0; i < dep_count; i++)
                    {
                        free(dep_objects[i]);
                    }
                    free(dep_objects);
                }
            }
            else if (options.build_library && emit_success)
            {
                bool        shared     = config_is_shared_library(config, target_name);
                char       *auto_name  = NULL;
                const char *output_lib = options.output_file ? options.output_file : (default_output ? default_output : (auto_name = get_library_output_path(config, project_dir, target_name)));

                printf("linking library '%s'...\n", output_lib);
                fflush(stdout);

                char **dep_objects = NULL;
                int    dep_count   = 0;
                if (!module_manager_get_link_objects(&analyzer.module_manager, &dep_objects, &dep_count))
                {
                    fprintf(stderr, "error: failed to collect dependency object files\n");
                    emit_success = false;
                }
                else
                {
                    size_t cmd_size = 2048 + (dep_count * 256) + (options.link_count * 256);
                    char  *link_cmd = malloc(cmd_size);

                    if (shared)
                        snprintf(link_cmd, cmd_size, "cc -shared -fPIC -o %s %s", output_lib, obj_file);
                    else
                        snprintf(link_cmd, cmd_size, "ar rcs %s %s", output_lib, obj_file);

                    for (int i = 0; i < dep_count; i++)
                    {
                        strcat(link_cmd, " ");
                        strcat(link_cmd, dep_objects[i]);
                    }

                    for (int i = 0; i < options.link_count; i++)
                    {
                        strcat(link_cmd, " ");
                        strcat(link_cmd, options.link_objects[i]);
                    }

                    if (system(link_cmd) != 0)
                    {
                        printf("failed to link library\n");
                        emit_success = false;
                    }
                    else if (!options.emit_object && owned_obj_path)
                    {
                        unlink(owned_obj_path);
                    }
                    if (auto_name)
                        free(auto_name);

                    free(link_cmd);

                    for (int i = 0; i < dep_count; i++)
                    {
                        free(dep_objects[i]);
                    }
                    free(dep_objects);
                }
            }
        }
    }
    else
    {
        if (options.emit_ast || options.emit_ir || options.emit_asm)
        {
            fprintf(stderr, "warning: entrypoint-less build ignores --emit-* outputs\n");
        }

        if (options.link_executable)
        {
            fprintf(stderr, "error: cannot link executable without an entry file\n");
            emit_success = false;
        }
        else if (options.build_library && (options.output_file || default_output))
        {
            bool        shared     = config_is_shared_library(config, target_name);
            char       *auto_name  = NULL;
            const char *output_lib = options.output_file ? options.output_file : (default_output ? default_output : (auto_name = get_library_output_path(config, project_dir, target_name)));

            char **dep_objects = NULL;
            int    dep_count   = 0;
            if (!module_manager_get_link_objects(&analyzer.module_manager, &dep_objects, &dep_count))
            {
                fprintf(stderr, "error: failed to collect dependency object files\n");
                emit_success = false;
            }
            else if (dep_count == 0)
            {
                fprintf(stderr, "error: no objects generated for library output\n");
                emit_success = false;
                free(dep_objects);
            }
            else
            {
                size_t cmd_size = 2048 + (dep_count * 256) + (options.link_count * 256);
                char  *link_cmd = malloc(cmd_size);

                if (shared)
                    snprintf(link_cmd, cmd_size, "cc -shared -fPIC -o %s %s", output_lib, dep_objects[0]);
                else
                    snprintf(link_cmd, cmd_size, "ar rcs %s %s", output_lib, dep_objects[0]);

                for (int i = 1; i < dep_count; i++)
                {
                    strcat(link_cmd, " ");
                    strcat(link_cmd, dep_objects[i]);
                }

                for (int i = 0; i < options.link_count; i++)
                {
                    strcat(link_cmd, " ");
                    strcat(link_cmd, options.link_objects[i]);
                }

                if (system(link_cmd) != 0)
                {
                    printf("failed to link library\n");
                    emit_success = false;
                }
                if (auto_name)
                    free(auto_name);

                free(link_cmd);

                for (int i = 0; i < dep_count; i++)
                {
                    free(dep_objects[i]);
                }
                free(dep_objects);
            }
        }
    }

    // cleanup
    if (codegen_initialized)
        codegen_context_dnit(&codegen);
    free(base_dir);
    semantic_analyzer_dnit(&analyzer);
    ast_node_dnit(program);
    free(program);
    if (parser_initialized)
        parser_dnit(&parser);
    if (lexer_initialized)
        lexer_dnit(&lexer);
    free(source);
    free(default_output);
    free(options.link_objects); // cleanup link objects array
    free(resolved_main_file);
    free(owned_obj_path);
    free(precomputed_obj_path);
    free(object_dir);
    free(root_module_name);

    if (config)
    {
        config_dnit(config);
        free(config);
    }

    return emit_success ? 0 : 1;
}

static int run_command(int argc, char **argv)
{
    // first build the project
    int build_result = build_command(argc, argv);
    if (build_result != 0)
    {
        return build_result;
    }

    // load project config to get output path
    ProjectConfig *config = config_load_from_dir(".");
    if (!config)
    {
        fprintf(stderr, "error: no mach.toml found in current directory\n");
        return 1;
    }

    // determine executable path
    const char *target_name     = config->target_count > 0 ? config->targets[0]->name : "native";
    char       *executable_path = get_executable_path(config, ".", target_name);
    if (!executable_path)
    {
        fprintf(stderr, "error: could not determine executable path\n");
        config_dnit(config);
        free(config);
        return 1;
    }

    // check if executable exists
    if (access(executable_path, X_OK) != 0)
    {
        fprintf(stderr, "error: executable '%s' not found or not executable\n", executable_path);
        free(executable_path);
        config_dnit(config);
        free(config);
        return 1;
    }

    printf("running '%s'...\n", executable_path);

    // run the executable
    int result = system(executable_path);

    free(executable_path);
    config_dnit(config);
    free(config);

    return result;
}

// utility function to check if a directory contains a valid mach project
static bool is_valid_mach_project(const char *path)
{
    if (!path)
        return false;

    // expand tilde if necessary
    char *expanded_path = NULL;
    if (path[0] == '~' && (path[1] == '/' || path[1] == '\0'))
    {
        const char *home = getenv("HOME");
        if (home)
        {
            size_t len    = strlen(home) + strlen(path) + 1;
            expanded_path = malloc(len);
            snprintf(expanded_path, len, "%s%s", home, path + 1);
            path = expanded_path;
        }
    }

    char config_path[1024];
    snprintf(config_path, sizeof(config_path), "%s/mach.toml", path);

    // check if mach.toml exists and can be loaded
    ProjectConfig *config   = config_load(config_path);
    bool           is_valid = false;
    if (config)
    {
        config_dnit(config);
        free(config);
        is_valid = true;
    }

    free(expanded_path);
    return is_valid;
}

// utility function to copy directory recursively
static bool copy_directory(const char *src, const char *dest)
{
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "cp -r \"%s\" \"%s\"", src, dest);
    return system(cmd) == 0;
}

static int dep_command(int argc, char **argv)
{
    if (argc < 3)
    {
        print_dep_usage();
        return 1;
    }

    const char *subcommand = argv[2];

    if (strcmp(subcommand, "add") == 0)
    {
        return dep_add_command(argc - 1, argv + 1); // shift args
    }
    else if (strcmp(subcommand, "remove") == 0)
    {
        return dep_remove_command(argc - 1, argv + 1); // shift args
    }
    else if (strcmp(subcommand, "list") == 0)
    {
        return dep_list_command(argc - 1, argv + 1); // shift args
    }
    else
    {
        fprintf(stderr, "error: unknown dep subcommand '%s'\n", subcommand);
        print_dep_usage();
        return 1;
    }
}

static int dep_add_command(int argc, char **argv)
{
    (void)argc; // suppress unused parameter warning
    (void)argv; // suppress unused parameter warning

    printf("The 'dep add' command has been removed.\n");
    printf("Dependencies are now managed using git submodules.\n");
    printf("\n");
    printf("To add a dependency:\n");
    printf("  1. Add it as a git submodule in your 'dep' directory:\n");
    printf("     git submodule add <repository-url> dep/<name>\n");
    printf("  2. Ensure the dependency has a 'mach.toml' configuration file\n");
    printf("  3. The compiler will automatically discover and make it available as 'dep.<name>.*'\n");
    printf("\n");
    printf("Example:\n");
    printf("  git submodule add https://github.com/mach-std/std.git dep/std\n");
    printf("  # Now you can use: use std.io;\n");

    return 0;
}

static int dep_remove_command(int argc, char **argv)
{
    (void)argc; // suppress unused parameter warning
    (void)argv; // suppress unused parameter warning

    printf("The 'dep remove' command has been removed.\n");
    printf("Dependencies are now managed using git submodules.\n");
    printf("\n");
    printf("To remove a dependency:\n");
    printf("  1. Remove the git submodule:\n");
    printf("     git submodule deinit dep/<name>\n");
    printf("     git rm dep/<name>\n");
    printf("  2. Commit the changes:\n");
    printf("     git commit -m \"Remove <name> dependency\"\n");
    printf("\n");
    printf("Example:\n");
    printf("  git submodule deinit dep/std\n");
    printf("  git rm dep/std\n");
    printf("  git commit -m \"Remove std dependency\"\n");

    return 0;
}

static int dep_list_command(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    // load project config
    ProjectConfig *config = config_load_from_dir(".");
    if (!config)
    {
        fprintf(stderr, "error: no mach.toml found in current directory\n");
        return 1;
    }

    printf("dependencies:\n");
    if (config->dep_count == 0)
    {
        printf("  (none)\n");
        printf("  add in [deps] section of mach.toml, e.g.:\n");
        printf("    [deps]\n    std = { path = \"dep/std\" }\n");
    }
    else
    {
        for (int i = 0; i < config->dep_count; i++)
        {
            DepSpec *d = config->deps[i];
            printf("  %s -> %s (src=%s)%s\n", d->name, d->path, d->src_dir ? d->src_dir : "src", d->is_runtime ? " [runtime]" : "");
        }
    }

    if (config->lib_dep_count > 0)
    {
        printf("\nlibrary dependencies:\n");
        for (int i = 0; i < config->lib_dep_count; i++)
        {
            printf("  %s\n", config->lib_dependencies[i]);
        }
    }

    config_dnit(config);
    free(config);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        print_usage(argv[0]);
        return 1;
    }

    const char *command = argv[1];

    if (strcmp(command, "help") == 0)
    {
        print_usage(argv[0]);
        return 0;
    }
    else if (strcmp(command, "init") == 0)
    {
        return init_command(argc, argv);
    }
    else if (strcmp(command, "build") == 0)
    {
        return build_command(argc, argv);
    }
    else if (strcmp(command, "run") == 0)
    {
        return run_command(argc, argv);
    }
    else if (strcmp(command, "clean") == 0)
    {
        return clean_command(argc, argv);
    }
    else if (strcmp(command, "dep") == 0)
    {
        return dep_command(argc, argv);
    }
    else if (strcmp(command, "examine") == 0)
    {
        return examine_command(argc, argv);
    }
    else
    {
        fprintf(stderr, "error: unknown command '%s'\n", command);
        print_usage(argv[0]);
        return 1;
    }
}


static bool str_ends_with(const char *s, const char *suffix)
{
    size_t ls = strlen(s), lf = strlen(suffix);
    return lf <= ls && strcmp(s + ls - lf, suffix) == 0;
}

static int examine_command(int argc, char **argv)
{
    if (argc < 3)
    {
        fprintf(stderr, "usage: %s examine <file.mach>\n", argv[0]);
        return 1;
    }
    const char *file_path = argv[2];
    ProjectConfig *config = config_load_from_dir(".");
    if (!config)
        config = config_create_default("examine");

    // load source
    FILE *f = fopen(file_path, "r");
    if (!f)
    {
        fprintf(stderr, "error: cannot open '%s'\n", file_path);
        config_dnit(config); free(config);
        return 1;
    }
    fseek(f, 0, SEEK_END); long size = ftell(f); fseek(f, 0, SEEK_SET);
    char *source = malloc(size + 1); fread(source,1,size,f); source[size]='\0'; fclose(f);

    Lexer lexer; lexer_init(&lexer, source);
    Parser parser; parser_init(&parser, &lexer);
    AstNode *program = parser_parse_program(&parser);
    if (!program)
    {
        fprintf(stderr, "parse failed\n");
        parser_dnit(&parser); lexer_dnit(&lexer); free(source); config_dnit(config); free(config); return 1;
    }
    SemanticAnalyzer analyzer; semantic_analyzer_init(&analyzer);
    module_manager_set_config(&analyzer.module_manager, config, ".");
    semantic_analyze(&analyzer, program);

    // derive module path heuristic
    const char *mod = file_path;
    const char *dep_pos = strstr(file_path, "/dep/");
    char module_buf[512];
    if (dep_pos)
    {
        dep_pos += 5; // after dep/
        const char *slash = strchr(dep_pos, '/');
        if (slash)
        {
            size_t pkg_len = (size_t)(slash - dep_pos);
            const char *rel = slash + 1;
            const char *srcm = strstr(rel, "/src/");
            if (srcm) rel = srcm + 5;
            char rel_copy[256]; strncpy(rel_copy, rel, sizeof(rel_copy)-1); rel_copy[255]='\0';
            for (char *p = rel_copy; *p; ++p) if (*p=='/') *p='.';
            if (str_ends_with(rel_copy, ".mach")) rel_copy[strlen(rel_copy)-5]='\0';
            snprintf(module_buf, sizeof(module_buf), "dep.%.*s.%s", (int)pkg_len, dep_pos, rel_copy);
            mod = module_buf;
        }
    }
    printf("module: %s\n", mod);
    printf("symbols:\n");
    if (analyzer.symbol_table.global_scope)
    {
        for (Symbol *s = analyzer.symbol_table.global_scope->symbols; s; s = s->next)
        {
            const char *k = "?";
            switch (s->kind)
            {
            case SYMBOL_FUNC: k="fun"; break;
            case SYMBOL_VAR: k="var"; break;
            case SYMBOL_VAL: k="val"; break;
            case SYMBOL_TYPE: k="type"; break;
            case SYMBOL_MODULE: k="module"; break;
            default: break;
            }
            printf("  %s %s\n", k, s->name ? s->name : "<anon>");
        }
    }
    if (analyzer.has_errors)
    {
        printf("errors:\n");
        semantic_print_errors(&analyzer, &lexer, file_path);
    }
    semantic_analyzer_dnit(&analyzer);
    ast_node_dnit(program); free(program);
    parser_dnit(&parser); lexer_dnit(&lexer);
    free(source);
    config_dnit(config); free(config);
    return 0;
}
