#include "ast.h"
#include "codegen.h"
#include "ioutil.h"
#include "lexer.h"
#include "parser.h"
#include "semantic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void print_usage(const char *program_name)
{
    printf("Usage: %s <command> <file> [output]\n", program_name);
    printf("Commands:\n");
    printf("  parse     Parse and display AST for the given file\n");
    printf("  compile   Compile to object file\n");
    printf("  emit-ir   Emit LLVM IR\n");
    printf("  build     Compile and link to executable\n");
}

static void print_node(Node *node, int indent);
static void print_indent(int indent);

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        print_usage(argv[0]);
        return 1;
    }

    const char *command  = argv[1];
    const char *filename = argv[2];
    const char *output   = argc > 3 ? argv[3] : NULL;

    if (strcmp(command, "parse") != 0 && strcmp(command, "compile") != 0 && strcmp(command, "emit-ir") != 0 && strcmp(command, "build") != 0)
    {
        fprintf(stderr, "Error: Unknown command '%s'\n", command);
        print_usage(argv[0]);
        return 1;
    }

    // read source file
    char *source = read_file((char *)filename);
    if (!source)
    {
        fprintf(stderr, "Error: Could not read file '%s'\n", filename);
        return 1;
    }

    // initialize lexer
    Lexer lexer;
    lexer_init(&lexer, source);

    // initialize parser
    Parser parser;
    parser_init(&parser, &lexer);

    // parse the program
    Node *program = parser_parse_program(&parser);

    if (parser.has_error || !program)
    {
        fprintf(stderr, "Parse error occurred\n");
        parser_dnit(&parser);
        lexer_dnit(&lexer);
        free(source);
        return 1;
    }

    // handle parse command
    if (strcmp(command, "parse") == 0)
    {
        printf("AST for %s:\n", filename);
        print_node(program, 0);
        node_dnit(program);
        free(program);
        parser_dnit(&parser);
        lexer_dnit(&lexer);
        free(source);
        return 0;
    }

    // semantic analysis required for other commands
    SemanticAnalyzer analyzer;
    semantic_init(&analyzer, program, &lexer, filename);

    if (!semantic_analyze(&analyzer))
    {
        semantic_error_print_all(&analyzer);
        semantic_dnit(&analyzer);
        node_dnit(program);
        free(program);
        parser_dnit(&parser);
        lexer_dnit(&lexer);
        free(source);
        return 1;
    }

    // initialize code generator
    CodeGenerator codegen;
    codegen_init(&codegen, &analyzer, filename);

    if (!codegen_generate(&codegen))
    {
        fprintf(stderr, "Code generation failed\n");
        codegen_dnit(&codegen);
        semantic_dnit(&analyzer);
        node_dnit(program);
        free(program);
        parser_dnit(&parser);
        lexer_dnit(&lexer);
        free(source);
        return 1;
    }

    // handle output commands
    int ret = 0;

    if (strcmp(command, "emit-ir") == 0)
    {
        const char *ir_file = output ? output : "output.ll";
        if (!codegen_emit_ir(&codegen, ir_file))
        {
            fprintf(stderr, "Failed to write IR file\n");
            ret = 1;
        }
        else
        {
            printf("LLVM IR written to: %s\n", ir_file);
        }
    }
    else if (strcmp(command, "compile") == 0)
    {
        const char *obj_file = output ? output : "output.o";
        if (!codegen_emit_object(&codegen, obj_file))
        {
            fprintf(stderr, "Failed to write object file\n");
            ret = 1;
        }
        else
        {
            printf("Object file written to: %s\n", obj_file);
        }
    }
    else if (strcmp(command, "build") == 0)
    {
        // write to temporary object file
        char temp_obj[256];
        snprintf(temp_obj, sizeof(temp_obj), "/tmp/mach_%d.o", getpid());

        if (!codegen_emit_object(&codegen, temp_obj))
        {
            fprintf(stderr, "Failed to write object file\n");
            ret = 1;
        }
        else
        {
            // link with clang
            const char *exe_file = output ? output : "a.out";
            char        link_cmd[1024];
            snprintf(link_cmd, sizeof(link_cmd), "clang %s -o %s", temp_obj, exe_file);

            if (system(link_cmd) != 0)
            {
                fprintf(stderr, "Failed to link executable\n");
                ret = 1;
            }
            else
            {
                printf("Executable written to: %s\n", exe_file);
            }

            // cleanup temp file
            unlink(temp_obj);
        }
    }

    // cleanup
    codegen_dnit(&codegen);
    semantic_dnit(&analyzer);
    node_dnit(program);
    free(program);
    parser_dnit(&parser);
    lexer_dnit(&lexer);
    free(source);

    return ret;
}

static void print_indent(int indent)
{
    for (int i = 0; i < indent; i++)
    {
        printf("  ");
    }
}

static void print_node(Node *node, int indent)
{
    if (!node)
    {
        print_indent(indent);
        printf("(null)\n");
        return;
    }

    print_indent(indent);
    printf("%s", node_kind_name(node->kind));

    // print value info
    switch (node->kind)
    {
    case NODE_IDENTIFIER:
        printf(" '%s'", node->str_value);
        break;
    case NODE_LIT_INT:
        printf(" %llu", node->int_value);
        break;
    case NODE_LIT_FLOAT:
        printf(" %g", node->float_value);
        break;
    case NODE_LIT_CHAR:
        printf(" '%c'", node->char_value);
        break;
    case NODE_LIT_STRING:
        printf(" \"%s\"", node->str_value);
        break;
    case NODE_EXPR_BINARY:
        printf(" op=%s", op_to_string(node->binary.op));
        break;
    case NODE_EXPR_UNARY:
        printf(" op=%s", op_to_string(node->unary.op));
        break;
    default:
        break;
    }

    printf("\n");

    // print children
    switch (node->kind)
    {
    case NODE_PROGRAM:
    case NODE_BLOCK:
        if (node->children)
        {
            for (size_t i = 0; node->children[i]; i++)
            {
                print_node(node->children[i], indent + 1);
            }
        }
        break;

    case NODE_EXPR_BINARY:
        print_node(node->binary.left, indent + 1);
        print_node(node->binary.right, indent + 1);
        break;

    case NODE_EXPR_UNARY:
        print_node(node->unary.target, indent + 1);
        break;

    case NODE_EXPR_CALL:
        print_node(node->call.target, indent + 1);
        if (node->call.args)
        {
            print_indent(indent + 1);
            printf("arguments:\n");
            for (size_t i = 0; node->call.args[i]; i++)
            {
                print_node(node->call.args[i], indent + 2);
            }
        }
        break;

    case NODE_EXPR_INDEX:
    case NODE_EXPR_MEMBER:
    case NODE_EXPR_CAST:
        if (node->children)
        {
            print_node(node->children[0], indent + 1);
            print_node(node->children[1], indent + 1);
        }
        break;

    case NODE_TYPE_POINTER:
    case NODE_TYPE_ARRAY:
        print_node(node->single, indent + 1);
        break;

    case NODE_TYPE_FUNCTION:
        if (node->function.return_type)
        {
            print_indent(indent + 1);
            printf("return:\n");
            print_node(node->function.return_type, indent + 2);
        }
        if (node->function.params)
        {
            print_indent(indent + 1);
            printf("params:\n");
            for (size_t i = 0; node->function.params[i]; i++)
            {
                print_node(node->function.params[i], indent + 2);
            }
        }
        break;

    case NODE_STMT_VAL:
    case NODE_STMT_VAR:
    case NODE_STMT_DEF:
        print_node(node->decl.name, indent + 1);
        if (node->decl.type)
        {
            print_node(node->decl.type, indent + 1);
        }
        if (node->decl.init)
        {
            print_node(node->decl.init, indent + 1);
        }
        break;

    case NODE_STMT_FUNCTION:
        print_node(node->function.name, indent + 1);
        if (node->function.params)
        {
            print_indent(indent + 1);
            printf("params:\n");
            for (size_t i = 0; node->function.params[i]; i++)
            {
                print_node(node->function.params[i], indent + 2);
            }
        }
        if (node->function.return_type)
        {
            print_indent(indent + 1);
            printf("return:\n");
            print_node(node->function.return_type, indent + 2);
        }
        if (node->function.body)
        {
            print_indent(indent + 1);
            printf("body:\n");
            print_node(node->function.body, indent + 2);
        }
        break;

    case NODE_STMT_STRUCT:
    case NODE_STMT_UNION:
        if (node->composite.fields)
        {
            for (size_t i = 0; node->composite.fields[i]; i++)
            {
                print_node(node->composite.fields[i], indent + 1);
            }
        }
        break;

    case NODE_STMT_IF:
    case NODE_STMT_OR:
    case NODE_STMT_FOR:
        if (node->conditional.condition)
        {
            print_indent(indent + 1);
            printf("condition:\n");
            print_node(node->conditional.condition, indent + 2);
        }
        if (node->conditional.body)
        {
            print_indent(indent + 1);
            printf("body:\n");
            print_node(node->conditional.body, indent + 2);
        }
        break;

    case NODE_STMT_RETURN:
    case NODE_STMT_EXPRESSION:
    case NODE_STMT_EXTERNAL:
        if (node->single)
        {
            print_node(node->single, indent + 1);
        }
        break;

    case NODE_STMT_USE:
        if (node->children)
        {
            print_node(node->children[0], indent + 1);
            if (node->children[1])
            {
                print_node(node->children[1], indent + 1);
            }
        }
        break;

    default:
        break;
    }
}
