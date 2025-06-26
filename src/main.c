#include "ast.h"
#include "codegen.h"
#include "ioutil.h"
#include "lexer.h"
#include "parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

    // check if file exists
    if (!file_exists((char *)filename))
    {
        fprintf(stderr, "Error: File '%s' does not exist\n", filename);
        return 1;
    }

    // read the file
    char *source = read_file((char *)filename);
    if (source == NULL)
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

    if (parser.has_error)
    {
        fprintf(stderr, "Parse error occurred\n");
        free(source);
        parser_dnit(&parser);
        lexer_dnit(&lexer);
        return 1;
    }

    if (program == NULL)
    {
        fprintf(stderr, "Failed to parse program\n");
        free(source);
        parser_dnit(&parser);
        lexer_dnit(&lexer);
        return 1;
    }

    // handle command
    if (strcmp(command, "parse") == 0)
    {
        // output AST
        printf("AST for %s:\n", filename);
        print_node(program, 0);
    }
    else if (strcmp(command, "compile") == 0 || strcmp(command, "emit-ir") == 0 || strcmp(command, "build") == 0)
    {
        // initialize codegen
        CodeGen codegen;
        if (!codegen_init(&codegen, "mach_program"))
        {
            fprintf(stderr, "Failed to initialize code generator\n");
            node_dnit(program);
            free(program);
            parser_dnit(&parser);
            lexer_dnit(&lexer);
            free(source);
            return 1;
        }

        // generate code
        if (!codegen_generate(&codegen, program))
        {
            fprintf(stderr, "Code generation failed\n");
            codegen_dnit(&codegen);
            node_dnit(program);
            free(program);
            parser_dnit(&parser);
            lexer_dnit(&lexer);
            free(source);
            return 1;
        }

        // output based on command
        if (strcmp(command, "compile") == 0)
        {
            const char *obj_file = output ? output : "output.o";
            if (!codegen_write_object(&codegen, obj_file))
            {
                fprintf(stderr, "Failed to write object file\n");
                codegen_dnit(&codegen);
                node_dnit(program);
                free(program);
                parser_dnit(&parser);
                lexer_dnit(&lexer);
                free(source);
                return 1;
            }
            printf("Object file written to: %s\n", obj_file);
        }
        else if (strcmp(command, "emit-ir") == 0)
        {
            const char *ir_file = output ? output : "output.ll";
            if (!codegen_write_ir(&codegen, ir_file))
            {
                fprintf(stderr, "Failed to write IR file\n");
                codegen_dnit(&codegen);
                node_dnit(program);
                free(program);
                parser_dnit(&parser);
                lexer_dnit(&lexer);
                free(source);
                return 1;
            }
            printf("LLVM IR written to: %s\n", ir_file);
        }
        else if (strcmp(command, "build") == 0)
        {
            // compile to object file first
            const char *obj_file = "temp.o";
            if (!codegen_write_object(&codegen, obj_file))
            {
                fprintf(stderr, "Failed to write object file\n");
                codegen_dnit(&codegen);
                node_dnit(program);
                free(program);
                parser_dnit(&parser);
                lexer_dnit(&lexer);
                free(source);
                return 1;
            }

            // determine output executable name
            const char *exe_file = output ? output : "a.out";

            // link with clang
            char link_cmd[1024];
            snprintf(link_cmd, sizeof(link_cmd), "clang %s -o %s", obj_file, exe_file);

            int link_result = system(link_cmd);
            if (link_result != 0)
            {
                fprintf(stderr, "Failed to link executable\n");
                remove(obj_file);
                codegen_dnit(&codegen);
                node_dnit(program);
                free(program);
                parser_dnit(&parser);
                lexer_dnit(&lexer);
                free(source);
                return 1;
            }

            // cleanup temporary object file
            remove(obj_file);
            printf("Executable written to: %s\n", exe_file);
        }

        codegen_dnit(&codegen);
    }

    // cleanup
    node_dnit(program);
    free(program);
    parser_dnit(&parser);
    lexer_dnit(&lexer);
    free(source);

    return 0;
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
    if (node == NULL)
    {
        print_indent(indent);
        printf("(null)\n");
        return;
    }

    print_indent(indent);
    printf("%s", node_kind_name(node->kind));

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
    default:
        break;
    }

    printf("\n");

    // print children based on node type
    switch (node->kind)
    {
    case NODE_PROGRAM:
    case NODE_BLOCK:
        if (node->children)
        {
            for (size_t i = 0; i < node_list_count(node->children); i++)
            {
                print_node(node->children[i], indent + 1);
            }
        }
        break;

    case NODE_EXPR_BINARY:
    case NODE_EXPR_INDEX:
    case NODE_EXPR_MEMBER:
    case NODE_EXPR_CAST:
    case NODE_TYPE_ARRAY:
        print_node(node->binary.left, indent + 1);
        print_node(node->binary.right, indent + 1);
        break;

    case NODE_EXPR_UNARY:
    case NODE_TYPE_POINTER:
    case NODE_STMT_RETURN:
    case NODE_STMT_EXPRESSION:
        print_node(node->single, indent + 1);
        break;

    case NODE_EXPR_CALL:
        print_node(node->call.target, indent + 1);
        if (node->call.args)
        {
            for (size_t i = 0; i < node_list_count(node->call.args); i++)
            {
                print_node(node->call.args[i], indent + 1);
            }
        }
        break;

    case NODE_STMT_VAL:
    case NODE_STMT_VAR:
    case NODE_STMT_DEF:
    case NODE_STMT_EXTERNAL:
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
    case NODE_TYPE_FUNCTION:
        if (node->function.name)
        {
            print_indent(indent + 1);
            printf("name:\n");
            print_node(node->function.name, indent + 2);
        }
        if (node->function.params)
        {
            print_indent(indent + 1);
            printf("parameters:\n");
            for (size_t i = 0; i < node_list_count(node->function.params); i++)
            {
                print_node(node->function.params[i], indent + 2);
            }
        }
        if (node->function.return_type)
        {
            print_indent(indent + 1);
            printf("return_type:\n");
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
            print_indent(indent + 1);
            printf("fields:\n");
            for (size_t i = 0; i < node_list_count(node->composite.fields); i++)
            {
                print_node(node->composite.fields[i], indent + 2);
            }
        }
        break;

    case NODE_STMT_USE:
        if (node->binary.left)
        {
            print_indent(indent + 1);
            printf("alias:\n");
            print_node(node->binary.left, indent + 2);
        }
        print_indent(indent + 1);
        printf("module:\n");
        print_node(node->binary.right, indent + 2);
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
        print_indent(indent + 1);
        printf("body:\n");
        print_node(node->conditional.body, indent + 2);
        break;

    default:
        break;
    }
}
