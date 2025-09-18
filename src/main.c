#include "ast.h"
#include "codegen.h"
#include "config.h"
#include "lexer.h"
#include "module.h"
#include "parser.h"
#include "semantic.h"

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

static char *get_default_target_triple(void)
{
    // initialize LLVM targets to get target info
    LLVMInitializeAllTargetInfos();

    // get the default target triple from LLVM
    char *triple = LLVMGetDefaultTargetTriple();
    if (!triple)
    {
        // fallback to a reasonable default
        return strdup("x86_64-unknown-linux-gnu");
    }

    // copy the string since LLVM owns the original
    char *result = strdup(triple);
    LLVMDisposeMessage(triple);

    return result;
}

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

    // get the default target triple before creating directories
    char *target_triple = get_default_target_triple();

    // dep/ - dependency source files
    snprintf(path, sizeof(path), "%s/dep", project_dir);
    mkdir(path, 0755);

    // lib/ - library/object dependencies
    snprintf(path, sizeof(path), "%s/lib", project_dir);
    mkdir(path, 0755);

    // out/ - output directory
    snprintf(path, sizeof(path), "%s/out", project_dir);
    mkdir(path, 0755);

    // out/<target>/ - target directory
    snprintf(path, sizeof(path), "%s/out/%s", project_dir, target_triple);
    mkdir(path, 0755);

    // out/<target>/bin/ - binary output
    snprintf(path, sizeof(path), "%s/out/%s/bin", project_dir, target_triple);
    mkdir(path, 0755);

    // out/<target>/obj/ - object files
    snprintf(path, sizeof(path), "%s/out/%s/obj", project_dir, target_triple);
    mkdir(path, 0755);

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
                printf("note: import as 'dep.std.*'\n");
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

    fprintf(main_file, "use console: dep.std.io.console;\n\n");
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
    printf("  │   └── %s/   # target triple\n", target_triple);
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
        }
        else
        {
            // fallback to default values
            options.link_executable = true;
            options.opt_level       = 2;
            options.emit_object     = true;
        }
    }
    else
    {
        options.link_executable = true;
        options.opt_level       = 2;
        options.emit_object     = true;
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

    // check if a directory path is provided
    if (argc >= 3 && argv[2][0] != '-')
    {
        // check if it's a directory
        struct stat st;
        if (stat(argv[2], &st) == 0 && S_ISDIR(st.st_mode))
        {
            project_dir = argv[2];
            // try to load config from the specified directory
            config = config_load_from_dir(project_dir);
            if (config && config_has_main_file(config))
            {
                // discover dependencies in the project

                is_project_build   = true;
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
    if (!filename && !config)
    {
        config = config_load_from_dir(".");
        if (config && config_has_main_file(config))
        {
            // discover dependencies in the project

            is_project_build   = true;
            resolved_main_file = config_resolve_main_file(config, ".");
            filename           = resolved_main_file;
        }
    }

    if (!filename)
    {
        fprintf(stderr, "error: no input file specified and no project configuration found\n");
        print_usage(argv[0]);
        if (config)
        {
            config_dnit(config);
            free(config);
        }
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
        }
        else if (strcmp(argv[i], "--no-link") == 0)
        {
            options.link_executable = false;
        }
        else if (strcmp(argv[i], "--no-pie") == 0)
        {
            options.no_pie = true;
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

    // determine output file
    char *default_output = NULL;
    if (!options.output_file)
    {
        if (is_project_build && config)
        {
            // use project config output
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
            if (options.build_library)
            {
                default_output      = create_output_filename(filename, ".so");
                options.output_file = default_output;
            }
            else if (options.link_executable)
            {
                default_output      = get_base_filename(filename);
                options.output_file = default_output;
            }
            else if (options.emit_object && !options.emit_ir && !options.emit_asm)
            {
                default_output      = create_output_filename(filename, ".o");
                options.output_file = default_output;
            }
            else if (options.emit_ir && !options.emit_asm && !options.emit_object)
            {
                default_output      = create_output_filename(filename, ".ll");
                options.output_file = default_output;
            }
            else if (options.emit_asm && !options.emit_ir && !options.emit_object)
            {
                default_output      = create_output_filename(filename, ".s");
                options.output_file = default_output;
            }
        }
    }

    // read source file
    char *source = read_file(filename);
    if (!source)
    {
        free(default_output);
        return 1;
    }

    // initialize lexer
    Lexer lexer;
    lexer_init(&lexer, source);

    // initialize parser
    Parser parser;
    parser_init(&parser, &lexer);

    printf("parsing '%s'...\n", filename);
    fflush(stdout);

    // parse program
    AstNode *program = parser_parse_program(&parser);

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
    char *base_dir = get_directory(filename);
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
        parser_dnit(&parser);
        lexer_dnit(&lexer);
        free(source);
        free(default_output);
        return 1;
    }

    // ensure output directory exists for dependency compilation
    if (system("mkdir -p bin/obj") != 0)
    {
        fprintf(stderr, "warning: failed to create bin/obj directory\n");
    }

    // compile module dependencies
    if (!module_manager_compile_dependencies(&analyzer.module_manager, "bin/obj", options.opt_level, options.no_pie))
    {
        fprintf(stderr, "dependency compilation failed\n");
        if (analyzer.module_manager.had_error)
        {
            module_error_list_print(&analyzer.module_manager.errors);
        }

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

    // initialize codegen context
    printf("generating code...\n");
    fflush(stdout);

    CodegenContext codegen;
    codegen_context_init(&codegen, get_filename(filename), options.no_pie);
    codegen.opt_level = options.opt_level;
    if (config && config->name)
    {
        codegen.package_name = strdup(config->name);
    }

    // check if we're compiling the runtime module
    char *base_filename = get_filename(filename);
    if (base_filename && strcmp(base_filename, "runtime.mach") == 0)
    {
        codegen.is_runtime = true;
    }
    free(base_filename);

    // generate code
    bool codegen_success = codegen_generate(&codegen, program, &analyzer);

    if (!codegen_success)
    {
        fprintf(stderr, "code generation failed:\n");
        codegen_print_errors(&codegen);

        codegen_context_dnit(&codegen);
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

    // emit requested outputs
    bool emit_success = true;

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
        char *obj_file = (options.output_file && options.emit_object && !options.link_executable && !options.build_library) ? strdup(options.output_file) : create_output_filename(filename, ".o");

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

        if (options.link_executable && emit_success)
        {
            const char *output_exe = options.output_file ? options.output_file : default_output;

            printf("linking executable '%s'...\n", output_exe);
            fflush(stdout);

            // compile runtime if not already compiled
            char *runtime_obj = NULL;
            if (base_dir)
            {
                size_t runtime_path_len = strlen(base_dir) + strlen("/runtime/runtime.mach") + 1;
                char  *runtime_path     = malloc(runtime_path_len);
                snprintf(runtime_path, runtime_path_len, "%s/runtime/runtime.mach", base_dir);

                if (access(runtime_path, F_OK) == 0)
                {
                    printf("compiling runtime...\n");
                    fflush(stdout);

                    // compile runtime to object file
                    runtime_obj = create_output_filename(runtime_path, ".o");

                    // recursive call to compile runtime
                    size_t runtime_cmd_len = strlen(argv[0]) + strlen(runtime_path) + strlen(runtime_obj) + 64;
                    char  *runtime_cmd     = malloc(runtime_cmd_len);
                    snprintf(runtime_cmd, runtime_cmd_len, "%s build %s -o %s --emit-obj --no-link", argv[0], runtime_path, runtime_obj);

                    printf("runtime command: %s\n", runtime_cmd);
                    fflush(stdout);

                    if (system(runtime_cmd) != 0)
                    {
                        fprintf(stderr, "error: failed to compile runtime\n");
                        emit_success = false;
                    }

                    free(runtime_cmd);
                }

                free(runtime_path);
            }

            // get dependency object files
            char **dep_objects = NULL;
            int    dep_count   = 0;
            if (!module_manager_get_link_objects(&analyzer.module_manager, &dep_objects, &dep_count))
            {
                printf("failed to collect dependency object files\n");
                emit_success = false;
            }
            else
            {
                // construct linking command with all object files
                size_t cmd_size = 1024 + (dep_count * 256) + (options.link_count * 256) + 256; // extra space for runtime
                char  *link_cmd = malloc(cmd_size);

                if (options.no_pie)
                {
                    snprintf(link_cmd, cmd_size, "cc -no-pie -o %s %s", output_exe, obj_file);
                }
                else
                {
                    snprintf(link_cmd, cmd_size, "cc -pie -o %s %s", output_exe, obj_file);
                }

                // append dependency object files
                for (int i = 0; i < dep_count; i++)
                {
                    strcat(link_cmd, " ");
                    strcat(link_cmd, dep_objects[i]);
                }

                // append runtime object file if available
                if (runtime_obj)
                {
                    strcat(link_cmd, " ");
                    strcat(link_cmd, runtime_obj);
                }

                // append user-specified link objects
                for (int i = 0; i < options.link_count; i++)
                {
                    strcat(link_cmd, " ");
                    strcat(link_cmd, options.link_objects[i]);
                }

                if (system(link_cmd) == 0)
                {
                    // remove temporary object file
                    if (!options.emit_object)
                    {
                        unlink(obj_file);
                    }

                    // remove runtime object file (it's always temporary)
                    if (runtime_obj)
                    {
                        unlink(runtime_obj);
                    }
                }
                else
                {
                    printf("failed to link executable\n");
                    emit_success = false;
                }

                free(link_cmd);

                // cleanup dependency object file list
                for (int i = 0; i < dep_count; i++)
                {
                    free(dep_objects[i]);
                }
                free(dep_objects);
            }

            // cleanup runtime object file name
            if (runtime_obj)
            {
                free(runtime_obj);
            }
        }
        else if (options.build_library && emit_success)
        {
            const char *output_lib = options.output_file ? options.output_file : default_output;

            printf("linking library '%s'...\n", output_lib);
            fflush(stdout);

            // get dependency object files
            char **dep_objects = NULL;
            int    dep_count   = 0;
            if (!module_manager_get_link_objects(&analyzer.module_manager, &dep_objects, &dep_count))
            {
                printf("failed to collect dependency object files\n");
                emit_success = false;
            }
            else
            {
                // construct linking command for shared library
                size_t cmd_size = 1024 + (dep_count * 256) + (options.link_count * 256);
                char  *link_cmd = malloc(cmd_size);

                snprintf(link_cmd, cmd_size, "cc -shared -fPIC -o %s %s", output_lib, obj_file);

                // append dependency object files
                for (int i = 0; i < dep_count; i++)
                {
                    strcat(link_cmd, " ");
                    strcat(link_cmd, dep_objects[i]);
                }

                // append user-specified link objects
                for (int i = 0; i < options.link_count; i++)
                {
                    strcat(link_cmd, " ");
                    strcat(link_cmd, options.link_objects[i]);
                }

                if (system(link_cmd) == 0)
                {
                    // remove temporary object file
                    if (!options.emit_object)
                    {
                        unlink(obj_file);
                    }
                }
                else
                {
                    printf("failed to link library\n");
                    emit_success = false;
                }

                free(link_cmd);

                // cleanup dependency object file list
                for (int i = 0; i < dep_count; i++)
                {
                    free(dep_objects[i]);
                }
                free(dep_objects);
            }
        }

        if (obj_file != options.output_file)
            free(obj_file);
    }

    // cleanup
    codegen_context_dnit(&codegen);
    free(base_dir);
    semantic_analyzer_dnit(&analyzer);
    ast_node_dnit(program);
    free(program);
    parser_dnit(&parser);
    lexer_dnit(&lexer);
    free(source);
    free(default_output);
    free(options.link_objects); // cleanup link objects array
    free(resolved_main_file);

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
    printf("  # Now you can use: use dep.std.io;\n");

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
    analyzer.module_manager.config = config;
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
