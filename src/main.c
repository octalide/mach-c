#include "ast.h"
#include "codegen.h"
#include "lexer.h"
#include "module.h"
#include "parser.h"
#include "semantic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
    fprintf(stderr, "  build <file> [options]    build a mach source file\n");
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

static int build_command(int argc, char **argv)
{
    if (argc < 3)
    {
        fprintf(stderr, "error: 'build' command requires a filename\n");
        print_usage(argv[0]);
        return 1;
    }

    const char  *filename   = argv[2];
    BuildOptions options    = {0};
    options.link_executable = true;
    options.build_library   = false;
    options.opt_level       = 2;
    options.link_objects    = NULL;
    options.link_count      = 0;
    options.link_capacity   = 0;

    // parse command line options
    for (int i = 3; i < argc; i++)
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

    // add standard library path if available
    const char *stdlib_path = getenv("MACH_STDLIB");
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
                    size_t runtime_cmd_len = strlen(runtime_path) + strlen(runtime_obj) + 64;
                    char  *runtime_cmd     = malloc(runtime_cmd_len);
                    snprintf(runtime_cmd, runtime_cmd_len, "%s build %s -o %s --emit-obj --no-link", argv[0], runtime_path, runtime_obj);

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

    return emit_success ? 0 : 1;
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
    else if (strcmp(command, "build") == 0)
    {
        return build_command(argc, argv);
    }
    else
    {
        fprintf(stderr, "error: unknown command '%s'\n", command);
        print_usage(argv[0]);
        return 1;
    }
}
