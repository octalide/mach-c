#include <stdlib.h>
#include <stdio.h>
#include "ast.h"

Node *new_node(NodeType type)
{
    Node *node = malloc(sizeof(Node));
    if (!node)
    {
        fprintf(stderr, "fatal: failed to allocate memory for node\n");
        exit(1);
    }

    memset(node, 0, sizeof(Node));
    node->type = type;

    return node;
}

void free_node(Node *node)
{
    if (!node)
    {
        return;
    }

    switch (node->type)
    {
    case NODE_PROGRAM:
        if (node->data.base_program.statements)
        {
            if (node->data.base_program.statements->next)
            {
                free_node(node->data.base_program.statements->next);
            }

            free_node(node->data.base_program.statements);
        }
        break;
    case NODE_EXPR_IDENTIFIER:
        break;
    case NODE_EXPR_LITERAL:
        break;
    case NODE_EXPR_TYPE:
        free_node(node->data.expr_type.identifier);
        break;
    case NODE_EXPR_TYPE_REF:
        free_node(node->data.expr_type_ref.type);
        break;
    case NODE_EXPR_UNARY:
        free_node(node->data.expr_unary.right);
        break;
    case NODE_EXPR_BINARY:
        free_node(node->data.expr_binary.left);
        free_node(node->data.expr_binary.right);
        break;
    case NODE_EXPR_CALL:
        free_node(node->data.expr_call.target);
        if (node->data.expr_call.arguments)
        {
            if (node->data.expr_call.arguments->next)
            {
                free_node(node->data.expr_call.arguments->next);
            }

            free_node(node->data.expr_call.arguments);
        }
        break;
    case NODE_EXPR_INDEX:
        free_node(node->data.expr_index.target);
        free_node(node->data.expr_index.index);
        break;
    case NODE_EXPR_ASSIGN:
        free_node(node->data.expr_assign.target);
        free_node(node->data.expr_assign.value);
        break;
    case NODE_STMT_BLOCK:
        if (node->data.stmt_block.statements)
        {
            if (node->data.stmt_block.statements->next)
            {
                free_node(node->data.stmt_block.statements->next);
            }

            free_node(node->data.stmt_block.statements);
        }
        break;
    case NODE_STMT_USE:
        free_node(node->data.stmt_use.module);
        break;
    case NODE_STMT_FUN:
        free_node(node->data.stmt_fun.identifier);
        for (int i = 0; i < node->data.stmt_fun.parameter_count; i++)
        {
            free(node->data.stmt_fun.parameters[i].name);
            free_node(node->data.stmt_fun.parameters[i].type);
        }
        free(node->data.stmt_fun.parameters);
        break;
    case NODE_STMT_STR:
        free_node(node->data.stmt_str.identifier);
        for (int i = 0; i < node->data.stmt_str.field_count; i++)
        {
            free(node->data.stmt_str.fields[i].name);
            free_node(node->data.stmt_str.fields[i].type);
        }
        free(node->data.stmt_str.fields);
        break;
    case NODE_STMT_VAL:
        free_node(node->data.stmt_val.identifier);
        free_node(node->data.stmt_val.type);
        free_node(node->data.stmt_val.initializer);
        break;
    case NODE_STMT_VAR:
        free_node(node->data.stmt_var.identifier);
        free_node(node->data.stmt_var.type);
        free_node(node->data.stmt_var.initializer);
        break;
    case NODE_STMT_IF:
        free_node(node->data.stmt_if.condition);
        free_node(node->data.stmt_if.body);
        break;
    case NODE_STMT_OR:
        free_node(node->data.stmt_or.condition);
        free_node(node->data.stmt_or.body);
        break;
    case NODE_STMT_FOR:
        free_node(node->data.stmt_for.condition);
        free_node(node->data.stmt_for.body);
        break;
    case NODE_STMT_BRK:
        break;
    case NODE_STMT_CNT:
        break;
    case NODE_STMT_RET:
        free_node(node->data.stmt_ret.value);
        break;
    case NODE_STMT_EXPR:
        free_node(node->data.stmt_expr.expression);
        break;
    default:
        break;
    }

    free(node);
}

char *node_type_string(NodeType type)
{
    switch (type)
    {
    case NODE_PROGRAM:
        return "PROGRAM";
    case NODE_EXPR_IDENTIFIER:
        return "EXPR_IDENTIFIER";
    case NODE_EXPR_LITERAL:
        return "EXPR_LITERAL";
    case NODE_EXPR_ARRAY:
        return "EXPR_ARRAY";
    case NODE_EXPR_TYPE:
        return "EXPR_TYPE";
    case NODE_EXPR_TYPE_ARRAY:
        return "EXPR_TYPE_ARRAY";
    case NODE_EXPR_TYPE_REF:
        return "EXPR_TYPE_REF";
    case NODE_EXPR_TYPE_MEMBER:
        return "EXPR_TYPE_MEMBER";
    case NODE_EXPR_UNARY:
        return "EXPR_UNARY";
    case NODE_EXPR_BINARY:
        return "EXPR_BINARY";
    case NODE_EXPR_CALL:
        return "EXPR_CALL";
    case NODE_EXPR_INDEX:
        return "EXPR_INDEX";
    case NODE_EXPR_ASSIGN:
        return "EXPR_ASSIGN";
    case NODE_STMT_BLOCK:
        return "STMT_BLOCK";
    case NODE_STMT_USE:
        return "STMT_USE";
    case NODE_STMT_FUN:
        return "STMT_FUN";
    case NODE_STMT_STR:
        return "STMT_STR";
    case NODE_STMT_VAL:
        return "STMT_VAL";
    case NODE_STMT_VAR:
        return "STMT_VAR";
    case NODE_STMT_IF:
        return "STMT_IF";
    case NODE_STMT_OR:
        return "STMT_OR";
    case NODE_STMT_FOR:
        return "STMT_FOR";
    case NODE_STMT_BRK:
        return "STMT_BRK";
    case NODE_STMT_CNT:
        return "STMT_CNT";
    case NODE_STMT_RET:
        return "STMT_RET";
    case NODE_STMT_EXPR:
        return "STMT_EXPR";
    default:
        return "UNKNOWN";
    }
}
