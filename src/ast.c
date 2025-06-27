#include "ast.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void node_init(Node *node, NodeKind kind)
{
    memset(node, 0, sizeof(Node));
    node->kind = kind;
}

void node_dnit(Node *node)
{
    if (!node)
        return;

    switch (node->kind)
    {
    case NODE_ERROR:
        free(node->error.message);
        break;

    case NODE_IDENTIFIER:
    case NODE_LIT_STRING:
        free(node->str_value);
        break;

    case NODE_EXPR_BINARY:
    case NODE_EXPR_INDEX:
    case NODE_EXPR_MEMBER:
    case NODE_EXPR_CAST:
        node_dnit(node->binary.left);
        free(node->binary.left);
        node_dnit(node->binary.right);
        free(node->binary.right);
        break;

    case NODE_EXPR_UNARY:
        node_dnit(node->unary.target);
        free(node->unary.target);
        break;

    case NODE_EXPR_CALL:
        node_dnit(node->call.target);
        free(node->call.target);
        if (node->call.args)
        {
            for (int i = 0; node->call.args[i]; i++)
            {
                node_dnit(node->call.args[i]);
                free(node->call.args[i]);
            }
            free(node->call.args);
        }
        break;

    case NODE_STMT_FUNCTION:
    case NODE_TYPE_FUNCTION:
        if (node->function.name)
        {
            node_dnit(node->function.name);
            free(node->function.name);
        }
        if (node->function.params)
        {
            for (int i = 0; node->function.params[i]; i++)
            {
                node_dnit(node->function.params[i]);
                free(node->function.params[i]);
            }
            free(node->function.params);
        }
        if (node->function.return_type)
        {
            node_dnit(node->function.return_type);
            free(node->function.return_type);
        }
        if (node->function.body)
        {
            node_dnit(node->function.body);
            free(node->function.body);
        }
        break;

    case NODE_STMT_IF:
    case NODE_STMT_OR:
    case NODE_STMT_FOR:
        if (node->conditional.condition)
        {
            node_dnit(node->conditional.condition);
            free(node->conditional.condition);
        }
        if (node->conditional.body)
        {
            node_dnit(node->conditional.body);
            free(node->conditional.body);
        }
        break;

    case NODE_STMT_VAL:
    case NODE_STMT_VAR:
    case NODE_STMT_DEF:
        if (node->decl.name)
        {
            node_dnit(node->decl.name);
            free(node->decl.name);
        }
        if (node->decl.type)
        {
            node_dnit(node->decl.type);
            free(node->decl.type);
        }
        if (node->decl.init)
        {
            node_dnit(node->decl.init);
            free(node->decl.init);
        }
        break;

    case NODE_TYPE_STRUCT:
    case NODE_TYPE_UNION:
    case NODE_STMT_STRUCT:
    case NODE_STMT_UNION:
        if (node->composite.fields)
        {
            for (int i = 0; node->composite.fields[i]; i++)
            {
                node_dnit(node->composite.fields[i]);
                free(node->composite.fields[i]);
            }
            free(node->composite.fields);
        }
        break;

    case NODE_PROGRAM:
    case NODE_BLOCK:
        if (node->children)
        {
            for (int i = 0; node->children[i]; i++)
            {
                node_dnit(node->children[i]);
                free(node->children[i]);
            }
            free(node->children);
        }
        break;

    case NODE_TYPE_ARRAY:
        node_dnit(node->binary.left);
        free(node->binary.left);
        node_dnit(node->binary.right);
        free(node->binary.right);
        break;

    case NODE_TYPE_POINTER:
    case NODE_STMT_USE:
    case NODE_STMT_EXTERNAL:
    case NODE_STMT_RETURN:
    case NODE_STMT_EXPRESSION:
        if (node->single)
        {
            node_dnit(node->single);
            free(node->single);
        }
        break;

    case NODE_LIT_INT:
    case NODE_LIT_FLOAT:
    case NODE_LIT_CHAR:
    case NODE_STMT_BREAK:
    case NODE_STMT_CONTINUE:
        break;
    default:
        fprintf(stderr, "error: unknown node kind %d\n", node->kind);
        break;
    }

    memset(node, 0, sizeof(Node));
}

// node creation helpers
Node *node_identifier(const char *name)
{
    Node *node = malloc(sizeof(Node));
    if (node == NULL)
    {
        return NULL;
    }

    node_init(node, NODE_IDENTIFIER);
    node->str_value = strdup(name);
    return node;
}

Node *node_int(unsigned long long value)
{
    Node *node = malloc(sizeof(Node));
    if (node == NULL)
    {
        return NULL;
    }

    node_init(node, NODE_LIT_INT);
    node->int_value = value;
    return node;
}

Node *node_float(double value)
{
    Node *node = malloc(sizeof(Node));
    if (node == NULL)
    {
        return NULL;
    }

    node_init(node, NODE_LIT_FLOAT);
    node->float_value = value;
    return node;
}

Node *node_char(char value)
{
    Node *node = malloc(sizeof(Node));
    if (node == NULL)
    {
        return NULL;
    }

    node_init(node, NODE_LIT_CHAR);
    node->char_value = value;
    return node;
}

Node *node_string(const char *value)
{
    Node *node = malloc(sizeof(Node));
    if (node == NULL)
    {
        return NULL;
    }

    node_init(node, NODE_LIT_STRING);
    node->str_value = strdup(value);
    return node;
}

Node *node_binary(Operator op, Node *left, Node *right)
{
    Node *node = malloc(sizeof(Node));
    if (node == NULL)
    {
        return NULL;
    }

    node_init(node, NODE_EXPR_BINARY);
    node->binary.op    = op;
    node->binary.left  = left;
    node->binary.right = right;
    return node;
}

Node *node_unary(Operator op, Node *target)
{
    Node *node = malloc(sizeof(Node));
    if (node == NULL)
    {
        return NULL;
    }

    node_init(node, NODE_EXPR_UNARY);
    node->unary.op     = op;
    node->unary.target = target;
    return node;
}

Node *node_call(Node *target, Node **args)
{
    Node *node = malloc(sizeof(Node));
    if (node == NULL)
    {
        return NULL;
    }

    node_init(node, NODE_EXPR_CALL);
    node->call.target = target;
    node->call.args   = args;
    return node;
}

Node *node_block(Node **statements)
{
    Node *node = malloc(sizeof(Node));
    if (node == NULL)
    {
        return NULL;
    }

    node_init(node, NODE_BLOCK);
    node->children = statements;
    return node;
}

// list utilities
Node **node_list_new(void)
{
    Node **list = malloc(sizeof(Node *));
    if (list == NULL)
    {
        return NULL;
    }

    list[0] = NULL;
    return list;
}

void node_list_add(Node ***list, Node *node)
{
    if (list == NULL || *list == NULL)
    {
        return;
    }

    size_t count    = node_list_count(*list);
    Node **new_list = realloc(*list, (count + 2) * sizeof(Node *));
    if (new_list == NULL)
    {
        return;
    }

    new_list[count]     = node;
    new_list[count + 1] = NULL;
    *list               = new_list;
}

size_t node_list_count(Node **list)
{
    if (list == NULL)
    {
        return 0;
    }

    size_t count = 0;
    while (list[count] != NULL)
    {
        count++;
    }
    return count;
}

const char *node_kind_name(NodeKind kind)
{
    switch (kind)
    {
    case NODE_ERROR:
        return "error";
    case NODE_IDENTIFIER:
        return "identifier";
    case NODE_PROGRAM:
        return "program";
    case NODE_BLOCK:
        return "block";

    case NODE_LIT_INT:
        return "int_literal";
    case NODE_LIT_FLOAT:
        return "float_literal";
    case NODE_LIT_CHAR:
        return "char_literal";
    case NODE_LIT_STRING:
        return "string_literal";

    case NODE_EXPR_CALL:
        return "call_expression";
    case NODE_EXPR_INDEX:
        return "index_expression";
    case NODE_EXPR_MEMBER:
        return "member_expression";
    case NODE_EXPR_CAST:
        return "cast_expression";
    case NODE_EXPR_UNARY:
        return "unary_expression";
    case NODE_EXPR_BINARY:
        return "binary_expression";

    case NODE_TYPE_ARRAY:
        return "array_type";
    case NODE_TYPE_POINTER:
        return "pointer_type";
    case NODE_TYPE_FUNCTION:
        return "function_type";
    case NODE_TYPE_STRUCT:
        return "struct_type";
    case NODE_TYPE_UNION:
        return "union_type";

    case NODE_STMT_VAL:
        return "val_statement";
    case NODE_STMT_VAR:
        return "var_statement";
    case NODE_STMT_DEF:
        return "def_statement";
    case NODE_STMT_FUNCTION:
        return "function_statement";
    case NODE_STMT_STRUCT:
        return "struct_statement";
    case NODE_STMT_UNION:
        return "union_statement";
    case NODE_STMT_USE:
        return "use_statement";
    case NODE_STMT_EXTERNAL:
        return "external_statement";
    case NODE_STMT_IF:
        return "if_statement";
    case NODE_STMT_OR:
        return "or_statement";
    case NODE_STMT_FOR:
        return "for_statement";
    case NODE_STMT_BREAK:
        return "break_statement";
    case NODE_STMT_CONTINUE:
        return "continue_statement";
    case NODE_STMT_RETURN:
        return "return_statement";
    case NODE_STMT_EXPRESSION:
        return "expression_statement";

    case NODE_COUNT:
        return "count";
    }

    return "unknown";
}
