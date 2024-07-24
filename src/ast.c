#include <stdlib.h>
#include <stdio.h>
#include "ast.h"

Node *node_new(NodeType type)
{
    Node *node = calloc(1, sizeof(Node));
    if (!node)
    {
        fprintf(stderr, "fatal: failed to allocate memory for node\n");
        exit(1);
    }

    node->child = NULL;
    node->type = type;

    return node;
}

void node_free(Node *node)
{
    if (node == NULL)
    {
        return;
    }

    if (node->child == NULL)
    {
        node_free(node->child);
    }
    
    switch (node->type)
    {
    case NODE_BLOCK:
        node_list_free(&node->data.node_block.statements);
        break;
    case NODE_IDENTIFIER:
        node_free(node->data.node_identifier.member);
        break;
    case NODE_EXPR_IDENTIFIER:
        node_free(node->data.expr_identifier.identifier);
        break;
    case NODE_EXPR_LIT_CHAR:
        break;
    case NODE_EXPR_LIT_NUMBER:
        break;
    case NODE_EXPR_LIT_STRING:
        break;
    case NODE_EXPR_CALL:
        node_free(node->data.expr_call.target);
        node_list_free(&node->data.expr_call.arguments);
        break;
    case NODE_EXPR_INDEX:
        node_free(node->data.expr_index.target);
        node_free(node->data.expr_index.index);
        break;
    case NODE_EXPR_DEF_STR:
        node_list_free(&node->data.expr_def_str.initializers);
        break;
    case NODE_EXPR_DEF_UNI:
        node_free(node->data.expr_def_uni.initializer);
        break;
    case NODE_EXPR_DEF_ARRAY:
        node_free(node->data.expr_def_array.type);
        node_free(node->data.expr_def_array.size);
        node_list_free(&node->data.expr_def_array.elements);
        break;
    case NODE_EXPR_UNARY:
        node_free(node->data.expr_unary.rhs);
        break;
    case NODE_EXPR_BINARY:
        node_free(node->data.expr_binary.rhs);
        node_free(node->data.expr_binary.lhs);
        break;
    case NODE_TYPE_ARRAY:
        node_free(node->data.type_array.size);
        node_free(node->data.type_array.type);
        break;
    case NODE_TYPE_REF:
        node_free(node->data.type_ref.target);
        break;
    case NODE_TYPE_FUN:
        node_free(node->data.type_fun.return_type);
        node_list_free(&node->data.type_fun.parameter_types);
        break;
    case NODE_TYPE_STR:
        node_list_free(&node->data.type_str.fields);
        break;
    case NODE_TYPE_UNI:
        node_list_free(&node->data.type_uni.fields);
        break;
    case NODE_FIELD:
        node_free(node->data.node_field.identifier);
        node_free(node->data.node_field.type);
        break;
    case NODE_STMT_USE:
        node_free(node->data.stmt_use.path);
        break;
    case NODE_STMT_IF:
        node_free(node->data.stmt_if.condition);
        node_free(node->data.stmt_if.body);
        break;
    case NODE_STMT_OR:
        node_free(node->data.stmt_or.condition);
        node_free(node->data.stmt_or.body);
        break;
    case NODE_STMT_FOR:
        node_free(node->data.stmt_for.condition);
        node_free(node->data.stmt_for.body);
        break;
    case NODE_STMT_BRK:
        break;
    case NODE_STMT_CNT:
        break;
    case NODE_STMT_RET:
        node_free(node->data.stmt_ret.value);
        break;
    case NODE_STMT_DECL_TYPE:
        node_free(node->data.stmt_decl_type.identifier);
        node_free(node->data.stmt_decl_type.type);
        break;
    case NODE_STMT_DECL_VAL:
        node_free(node->data.stmt_decl_val.identifier);
        node_free(node->data.stmt_decl_val.type);
        node_free(node->data.stmt_decl_val.initializer);
        break;
    case NODE_STMT_DECL_VAR:
        node_free(node->data.stmt_decl_var.identifier);
        node_free(node->data.stmt_decl_var.type);
        node_free(node->data.stmt_decl_var.initializer);
        break;
    case NODE_STMT_DECL_FUN:
        node_free(node->data.stmt_decl_fun.identifier);
        node_free(node->data.stmt_decl_fun.return_type);
        node_list_free(&node->data.stmt_decl_fun.parameters);
        node_free(node->data.stmt_decl_fun.body);
        break;
    case NODE_STMT_DECL_STR:
        node_free(node->data.stmt_decl_str.identifier);
        node_list_free(&node->data.stmt_decl_str.fields);
        break;
    case NODE_STMT_DECL_UNI:
        node_free(node->data.stmt_decl_uni.identifier);
        node_list_free(&node->data.stmt_decl_uni.fields);
        break;
    case NODE_STMT_EXPR:
        node_free(node->data.stmt_expr.expression);
        break;
    default:
        break;
    }
}

void node_list_append(Node ***list, Node *node)
{
    if (!node)
    {
        return;
    }

    int length = node_list_length(*list);
    
    Node **new_list = realloc(*list, sizeof(Node *) * (length + 2));
    if (!new_list)
    {
        fprintf(stderr, "fatal: failed to allocate memory for node list\n");
        *list = NULL;
        exit(1);
    }

    new_list[length] = node;
    new_list[length + 1] = NULL;

    *list = new_list;
}

int node_list_length(Node **list)
{
    if (!list)
    {
        return 0;
    }

    int length = 0;
    while (list[length])
    {
        length++;
    }

    return length;
}

void node_list_free(Node ***list)
{
    for (int i = 0; i < node_list_length(*list); i++)
    {
        node_free((*list)[i]);
    }

    free(*list);
}

char *node_type_string(NodeType type)
{
    switch (type)
    {
    case NODE_BLOCK:
        return "BLOCK";
    case NODE_EXPR_IDENTIFIER:
        return "EXPR_IDENTIFIER";
    case NODE_EXPR_LIT_CHAR:
        return "EXPR_LIT_CHAR";
    case NODE_EXPR_LIT_NUMBER:
        return "EXPR_LIT_NUMBER";
    case NODE_EXPR_LIT_STRING:
        return "EXPR_LIT_STRING";
    case NODE_EXPR_CALL:
        return "EXPR_CALL";
    case NODE_EXPR_INDEX:
        return "EXPR_INDEX";
    case NODE_EXPR_DEF_STR:
        return "EXPR_DEF_STR";
    case NODE_EXPR_DEF_UNI:
        return "EXPR_DEF_UNI";
    case NODE_EXPR_DEF_ARRAY:
        return "EXPR_DEF_ARRAY";
    case NODE_EXPR_UNARY:
        return "EXPR_UNARY";
    case NODE_EXPR_BINARY:
        return "EXPR_BINARY";
    case NODE_TYPE_ARRAY:
        return "TYPE_ARRAY";
    case NODE_TYPE_REF:
        return "TYPE_REF";
    case NODE_TYPE_FUN:
        return "TYPE_FUN";
    case NODE_TYPE_STR:
        return "TYPE_STR";
    case NODE_TYPE_UNI:
        return "TYPE_UNI";
    case NODE_FIELD:
        return "FIELD";
    case NODE_STMT_USE:
        return "STMT_USE";
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
    case NODE_STMT_DECL_TYPE:
        return "STMT_DECL_TYPE";
    case NODE_STMT_DECL_VAL:
        return "STMT_DECL_VAL";
    case NODE_STMT_DECL_VAR:
        return "STMT_DECL_VAR";
    case NODE_STMT_DECL_FUN:
        return "STMT_DECL_FUN";
    case NODE_STMT_DECL_STR:
        return "STMT_DECL_STR";
    case NODE_STMT_DECL_UNI:
        return "STMT_DECL_UNI";
    case NODE_STMT_EXPR:
        return "STMT_EXPR";
    default:
        return "UNKNOWN";
    }
}
