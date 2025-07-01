#include "ast.h"
#include "lexer.h"
#include "module.h"
#include "parser.h"
#include "semantic.h"
// #include "codegen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct BuildOptions
{
    bool        emit_ir;
    bool        emit_object;
    bool        emit_bitcode;
    bool        link_executable;
    const char *output_file;
} BuildOptions;

static void print_usage(const char *program_name)
{
    fprintf(stderr, "Usage: %s <command> [options]\n", program_name);
    fprintf(stderr, "Commands:\n");
    fprintf(stderr, "  build <file> [options]    Build a Mach source file\n");
    fprintf(stderr, "  help                      Show this help message\n");
    fprintf(stderr, "\nBuild options:\n");
    fprintf(stderr, "  -o <file>     Set output file name\n");
    fprintf(stderr, "  --emit-ir     Emit LLVM IR (.ll file)\n");
    fprintf(stderr, "  --emit-obj    Emit object file (.o file)\n");
    fprintf(stderr, "  --emit-bc     Emit LLVM bitcode (.bc file)\n");
    fprintf(stderr, "  --no-link     Don't create executable (just compile)\n");
}

static char *read_file(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (!file)
    {
        fprintf(stderr, "error: Could not open file '%s'\n", path);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *buffer = malloc(size + 1);
    if (!buffer)
    {
        fprintf(stderr, "error: Could not allocate memory for file\n");
        fclose(file);
        return NULL;
    }

    size_t read  = fread(buffer, 1, size, file);
    buffer[read] = '\0';
    fclose(file);

    return buffer;
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
    options.link_executable = true; // default to creating executable

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
        else if (strcmp(argv[i], "--emit-ir") == 0)
        {
            options.emit_ir = true;
        }
        else if (strcmp(argv[i], "--emit-obj") == 0)
        {
            options.emit_object = true;
        }
        else if (strcmp(argv[i], "--emit-bc") == 0)
        {
            options.emit_bitcode = true;
        }
        else if (strcmp(argv[i], "--no-link") == 0)
        {
            options.link_executable = false;
        }
        else
        {
            fprintf(stderr, "error: Unknown option '%s'\n", argv[i]);
            return 1;
        }
    }

    // create default output filename if not specified
    char *default_output = NULL;
    if (!options.output_file)
    {
        if (options.link_executable)
        {
            default_output      = get_base_filename(filename);
            options.output_file = default_output;
        }
        else if (options.emit_object)
        {
            default_output      = create_output_filename(filename, ".o");
            options.output_file = default_output;
        }
        else if (options.emit_ir)
        {
            default_output      = create_output_filename(filename, ".ll");
            options.output_file = default_output;
        }
        else if (options.emit_bitcode)
        {
            default_output      = create_output_filename(filename, ".bc");
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

    printf("parsing `%s`... ", filename);

    // parse program
    AstNode *program = parser_parse_program(&parser);

    // check for parse errors
    if (parser.had_error)
    {
        fprintf(stderr, "\nparsing failed with %d error(s):\n", parser.errors.count);
        parser_error_list_print(&parser.errors, &lexer, filename);

        // cleanup
        ast_node_dnit(program);
        free(program);
        parser_dnit(&parser);
        lexer_dnit(&lexer);
        free(source);
        free(default_output);

        return 1;
    }

    printf("done\n");

    // initialize module manager and resolve dependencies
    ModuleManager module_manager;
    module_manager_init(&module_manager);

    // get base directory from input file
    char       *base_dir   = get_base_filename(filename);
    const char *last_slash = strrchr(filename, '/');
    if (last_slash)
    {
        free(base_dir);
        size_t dir_len = last_slash - filename;
        base_dir       = malloc(dir_len + 1);
        strncpy(base_dir, filename, dir_len);
        base_dir[dir_len] = '\0';
    }
    else
    {
        free(base_dir);
        base_dir = strdup(".");
    }

    if (!module_manager_resolve_dependencies(&module_manager, program, base_dir))
    {
        if (module_manager.had_error && module_manager.errors.count > 0)
        {
            fprintf(stderr, "\nmodule loading failed with %d error(s):\n", module_manager.errors.count);
            module_error_list_print(&module_manager.errors);
        }
        else
        {
            fprintf(stderr, "\nerror: failed to resolve dependencies for program\n");
        }

        // cleanup
        free(base_dir);
        module_manager_dnit(&module_manager);
        ast_node_dnit(program);
        free(program);
        parser_dnit(&parser);
        lexer_dnit(&lexer);
        free(source);
        free(default_output);

        return 1;
    }

    free(base_dir);

    // perform semantic analysis
    printf("performing semantic analysis... ");

    SemanticAnalyzer semantic_analyzer;
    semantic_analyzer_init(&semantic_analyzer);

    bool semantic_success = semantic_analyze(&semantic_analyzer, program);

    if (!semantic_success)
    {
        printf("failed\n");

        if (semantic_analyzer.errors.count > 0)
        {
            fprintf(stderr, "\nsemantic analysis failed with %d error(s):\n", semantic_analyzer.errors.count);
            semantic_error_list_print(&semantic_analyzer.errors, &lexer, filename);
        }
        else
        {
            fprintf(stderr, "\nerror: semantic analysis failed\n");
        }

        // cleanup
        semantic_analyzer_dnit(&semantic_analyzer);
        module_manager_dnit(&module_manager);
        ast_node_dnit(program);
        free(program);
        parser_dnit(&parser);
        lexer_dnit(&lexer);
        free(source);
        free(default_output);

        return 1;
    }

    printf("done\n");

    // cleanup semantic analyzer
    semantic_analyzer_dnit(&semantic_analyzer);

    module_manager_dnit(&module_manager);
    ast_node_dnit(program);
    free(program);
    parser_dnit(&parser);
    lexer_dnit(&lexer);
    free(source);
    free(default_output);

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
    else if (strcmp(command, "build") == 0)
    {
        if (argc < 3)
        {
            fprintf(stderr, "error: 'build' command requires a filename\n");
            print_usage(argv[0]);
            return 1;
        }
        return build_command(argc, argv);
    }
    else
    {
        fprintf(stderr, "error: Unknown command '%s'\n", command);
        print_usage(argv[0]);
        return 1;
    }
}
