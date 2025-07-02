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
    bool        no_pie; // disable PIE
    const char *output_file;
    int         opt_level;
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
    fprintf(stderr, "  --emit-ast    emit abstract syntax tree (.ast file)\n");
    fprintf(stderr, "  --emit-ir     emit llvm ir (.ll file)\n");
    fprintf(stderr, "  --emit-asm    emit assembly (.s file)\n");
    fprintf(stderr, "  --emit-obj    emit object file (.o file)\n");
    fprintf(stderr, "  --no-link     don't create executable (just compile)\n");
    fprintf(stderr, "  --no-pie      disable position independent executable\n");
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
    options.opt_level       = 2;

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
            options.emit_object = true;
        }
        else if (strcmp(argv[i], "--no-link") == 0)
        {
            options.link_executable = false;
        }
        else if (strcmp(argv[i], "--no-pie") == 0)
        {
            options.no_pie = true;
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
        if (options.link_executable)
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

    // initialize codegen context
    printf("generating code...\n");
    fflush(stdout);

    CodegenContext codegen;
    codegen_context_init(&codegen, get_filename(filename), options.no_pie);
    codegen.opt_level = options.opt_level;

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

    if (options.emit_object || options.link_executable)
    {
        char *obj_file = options.output_file && options.emit_object && !options.link_executable ? strdup(options.output_file) : create_output_filename(filename, ".o");

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

            // simple linking using system cc
            char link_cmd[1024];
            if (options.no_pie)
            {
                snprintf(link_cmd, sizeof(link_cmd), "cc -no-pie -o %s %s", output_exe, obj_file);
            }
            else
            {
                snprintf(link_cmd, sizeof(link_cmd), "cc -pie -o %s %s", output_exe, obj_file);
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
                printf("failed to link executable\n");
                emit_success = false;
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
