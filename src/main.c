#include "ast.h"
#include "codegen.h"
#include "config.h"
#include "lexer.h"
#include "module.h"
#include "parser.h"
#include "semantic.h"

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

static void print_usage(const char *program_name)
{
    fprintf(stderr, "usage: %s <command> [options]\n", program_name);
    fprintf(stderr, "commands:\n");
    fprintf(stderr, "  init <name>               initialize a new project\n");
    fprintf(stderr, "  build [file] [options]    build project or single file\n");
    fprintf(stderr, "  run [options]             build and run project\n");
    fprintf(stderr, "  clean                     clean build artifacts\n");
    fprintf(stderr, "  help                      show this help message\n");
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

static char *get_executable_path(ProjectConfig *config, const char *project_dir)
{
    if (!config || !config->target_name)
        return NULL;

    char *bin_dir = config_resolve_bin_dir(config, project_dir);
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
    if (argc < 3)
    {
        fprintf(stderr, "error: 'init' command requires a project name\n");
        print_usage(argv[0]);
        return 1;
    }

    const char *project_name = argv[2];

    // create project directory
    if (mkdir(project_name, 0755) != 0)
    {
        fprintf(stderr, "error: failed to create project directory '%s'\n", project_name);
        return 1;
    }

    // create standard mach project directory structure
    char path[1024];

    // dep/ - dependency source files
    snprintf(path, sizeof(path), "%s/dep", project_name);
    mkdir(path, 0755);

    // lib/ - library/object dependencies
    snprintf(path, sizeof(path), "%s/lib", project_name);
    mkdir(path, 0755);

    // out/ - output directory
    snprintf(path, sizeof(path), "%s/out", project_name);
    mkdir(path, 0755);

    // out/native/ - native target directory (default architecture)
    snprintf(path, sizeof(path), "%s/out/native", project_name);
    mkdir(path, 0755);

    // out/native/bin/ - binary output
    snprintf(path, sizeof(path), "%s/out/native/bin", project_name);
    mkdir(path, 0755);

    // out/native/obj/ - object files
    snprintf(path, sizeof(path), "%s/out/native/obj", project_name);
    mkdir(path, 0755);

    // src/ - source files
    snprintf(path, sizeof(path), "%s/src", project_name);
    mkdir(path, 0755);

    // create mach.toml with proper directory configuration
    snprintf(path, sizeof(path), "%s/mach.toml", project_name);
    ProjectConfig *config = config_create_default(project_name);

    // set up the directory structure
    config->src_dir       = strdup("src");
    config->dep_dir       = strdup("dep");
    config->lib_dir       = strdup("lib");
    config->out_dir       = strdup("out");
    config->bin_dir       = strdup("native/bin");
    config->obj_dir       = strdup("native/obj");
    config->target_triple = strdup("native");

    if (!config_save(config, path))
    {
        fprintf(stderr, "error: failed to create mach.toml\n");
        config_dnit(config);
        free(config);
        return 1;
    }

    // create main.mach in src directory
    snprintf(path, sizeof(path), "%s/src/main.mach", project_name);
    FILE *main_file = fopen(path, "w");
    if (!main_file)
    {
        fprintf(stderr, "error: failed to create src/main.mach\n");
        config_dnit(config);
        free(config);
        return 1;
    }

    fprintf(main_file, "import \"std.io.console\"\n\n");
    fprintf(main_file, "func main() {\n");
    fprintf(main_file, "    console.println(\"hello, world!\")\n");
    fprintf(main_file, "}\n");
    fclose(main_file);

    printf("created project '%s' with the following structure:\n", project_name);
    printf("  %s/\n", project_name);
    printf("  ├── dep/          # dependency source files\n");
    printf("  ├── lib/          # library/object dependencies\n");
    printf("  ├── out/          # output directory\n");
    printf("  │   └── native/   # native target\n");
    printf("  │       ├── bin/  # binary output\n");
    printf("  │       └── obj/  # object files\n");
    printf("  ├── src/          # source files\n");
    printf("  │   └── main.mach # main source file\n");
    printf("  └── mach.toml     # project configuration\n");
    printf("\nto build: cd %s && cmach build\n", project_name);
    printf("to run:   cd %s && cmach run\n", project_name);

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

static BuildOptions config_to_build_options(ProjectConfig *config)
{
    BuildOptions options = {0};

    if (config)
    {
        options.emit_ast        = config->emit_ast;
        options.emit_ir         = config->emit_ir;
        options.emit_asm        = config->emit_asm;
        options.emit_object     = config->emit_object;
        options.build_library   = config->build_library;
        options.no_pie          = config->no_pie;
        options.opt_level       = config->opt_level;
        options.link_executable = config_should_link_executable(config);
    }
    else
    {
        options.link_executable = true;
        options.opt_level       = 2;
    }

    return options;
}

static int build_command(int argc, char **argv)
{
    const char    *filename           = NULL;
    ProjectConfig *config             = NULL;
    char          *resolved_main_file = NULL;
    bool           is_project_build   = false;

    // try to load project config first
    config = config_load_from_dir(".");
    if (config && config_has_main_file(config))
    {
        is_project_build   = true;
        resolved_main_file = config_resolve_main_file(config, ".");
        filename           = resolved_main_file;
    }

    // check if a file was specified on command line
    if (argc >= 3 && argv[2][0] != '-')
    {
        filename         = argv[2];
        is_project_build = false; // override project build when file is specified
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
    BuildOptions options = config_to_build_options(config);

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
            default_output = get_executable_path(config, ".");
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

    // add search paths for modules
    char *base_dir = get_directory(filename);
    module_manager_add_search_path(&analyzer.module_manager, base_dir);
    module_manager_add_search_path(&analyzer.module_manager, ".");

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
    char *executable_path = get_executable_path(config, ".");
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
    else
    {
        fprintf(stderr, "error: unknown command '%s'\n", command);
        print_usage(argv[0]);
        return 1;
    }
}
