#include "node.h"
#include "token.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

Node *node_new(NodeKind kind, Token *token)
{
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    node->token = NULL;

    // perform an annoying deepcopy
    if (token != NULL)
    {
        node->token = token_new(token->kind, token->pos, token->raw);
        switch (token->kind)
        {
        case TOKEN_ERROR:
            node->token->value.error = strdup(token->value.error);
            break;
        case TOKEN_INT:
            node->token->value.i = token->value.i;
            break;
        case TOKEN_FLOAT:
            node->token->value.f = token->value.f;
            break;
        default:
            break;
        }
    }

    node->parent = NULL;
    node->next = NULL;

    return node;
}

void node_free(Node *node)
{
    if (node == NULL)
    {
        return;
    }

    // NOTE: do not directly free parent node from child. assume parent will be
    //   freed by its owner which is likely at another point in the tree
    node->parent = NULL;

    token_free(node->token);
    node->token = NULL;

    switch (node->kind)
    {
    case NODE_ERROR:
        free(node->error.message);
        node->error.message = NULL;
        break;
    case NODE_PROGRAM:
        node_list_free(node->program.statements);
        node->program.statements = NULL;
        break;
    case NODE_IDENTIFIER:
    case NODE_LIT_INT:
    case NODE_LIT_FLOAT:
    case NODE_LIT_CHAR:
    case NODE_LIT_STRING:
        break;

    case NODE_EXPR_MEMBER:
        node_free(node->expr_member.target);
        node->expr_member.target = NULL;
        node_free(node->expr_member.identifier);
        node->expr_member.identifier = NULL;
        break;
    case NODE_EXPR_CALL:
        node_free(node->expr_call.target);
        node->expr_call.target = NULL;
        node_list_free(node->expr_call.arguments);
        node->expr_call.arguments = NULL;
        break;
    case NODE_EXPR_INDEX:
        node_free(node->expr_index.target);
        node->expr_index.target = NULL;
        node_free(node->expr_index.index);
        node->expr_index.index = NULL;
        break;
    case NODE_EXPR_CAST:
        node_free(node->expr_cast.target);
        node->expr_cast.target = NULL;
        node_free(node->expr_cast.type);
        node->expr_cast.type = NULL;
        break;
    case NODE_EXPR_NEW:
        node_free(node->expr_new.type);
        node->expr_new.type = NULL;
        node_list_free(node->expr_new.initializers);
        node->expr_new.initializers = NULL;
        break;
    case NODE_EXPR_UNARY:
        node_free(node->expr_unary.right);
        node->expr_unary.right = NULL;
        break;
    case NODE_EXPR_BINARY:
        node_free(node->expr_binary.left);
        node->expr_binary.left = NULL;
        node_free(node->expr_binary.right);
        node->expr_binary.right = NULL;
        break;

    case NODE_TYPE_ARRAY:
        node_free(node->type_arr.size);
        node->type_arr.size = NULL;
        node_free(node->type_arr.type);
        node->type_arr.type = NULL;
        break;
    case NODE_TYPE_POINTER:
        node_free(node->type_ptr.type);
        node->type_ptr.type = NULL;
        break;
    case NODE_TYPE_FUN:
        node_list_free(node->type_fun.parameters);
        node->type_fun.parameters = NULL;
        node_free(node->type_fun.return_type);
        node->type_fun.return_type = NULL;
        break;
    case NODE_TYPE_STR:
        node_list_free(node->type_str.fields);
        node->type_str.fields = NULL;
        break;
    case NODE_TYPE_UNI:
        node_list_free(node->type_uni.fields);
        node->type_uni.fields = NULL;
        break;
    case NODE_FIELD:
        node_free(node->field.identifier);
        node->field.identifier = NULL;
        node_free(node->field.type);
        node->field.type = NULL;
        break;

    case NODE_STMT_VAL:
        node_free(node->stmt_val.identifier);
        node->stmt_val.identifier = NULL;
        node_free(node->stmt_val.type);
        node->stmt_val.type = NULL;
        node_free(node->stmt_val.initializer);
        node->stmt_val.initializer = NULL;
        break;
    case NODE_STMT_VAR:
        node_free(node->stmt_var.identifier);
        node->stmt_var.identifier = NULL;
        node_free(node->stmt_var.type);
        node->stmt_var.type = NULL;
        node_free(node->stmt_var.initializer);
        node->stmt_var.initializer = NULL;
        break;
    case NODE_STMT_DEF:
        node_free(node->stmt_def.identifier);
        node->stmt_def.identifier = NULL;
        node_free(node->stmt_def.type);
        node->stmt_def.type = NULL;
        break;
    case NODE_STMT_USE:
        node_free(node->stmt_use.path);
        node->stmt_use.path = NULL;
        break;
    case NODE_STMT_FUN:
        node_free(node->stmt_fun.identifier);
        node->stmt_fun.identifier = NULL;
        node_list_free(node->stmt_fun.parameters);
        node->stmt_fun.parameters = NULL;
        node_free(node->stmt_fun.return_type);
        node->stmt_fun.return_type = NULL;
        node_free(node->stmt_fun.body);
        node->stmt_fun.body = NULL;
        break;
    case NODE_STMT_EXT:
        node_free(node->stmt_ext.identifier);
        node->stmt_ext.identifier = NULL;
        node_free(node->stmt_ext.type);
        node->stmt_ext.type = NULL;
        break;
    case NODE_STMT_IF:
        node_free(node->stmt_if.condition);
        node->stmt_if.condition = NULL;
        node_free(node->stmt_if.body);
        node->stmt_if.body = NULL;
        break;
    case NODE_STMT_OR:
        node_free(node->stmt_or.condition);
        node->stmt_or.condition = NULL;
        node_free(node->stmt_or.body);
        node->stmt_or.body = NULL;
        break;
    case NODE_STMT_FOR:
        node_free(node->stmt_for.condition);
        node->stmt_for.condition = NULL;
        node_free(node->stmt_for.body);
        node->stmt_for.body = NULL;
        break;
    case NODE_STMT_BRK:
    case NODE_STMT_CNT:
        break;
    case NODE_STMT_RET:
        node_free(node->stmt_ret.value);
        node->stmt_ret.value = NULL;
        break;
    case NODE_STMT_ASM:
        node_free(node->stmt_asm.code);
        node->stmt_asm.code = NULL;
        break;
    case NODE_STMT_BLOCK:
        node_list_free(node->stmt_block.statements);
        node->stmt_block.statements = NULL;
        break;
    case NODE_STMT_EXPR:
        node_free(node->stmt_expr.expression);
        node->stmt_expr.expression = NULL;
        break;
    default:
        break;
    }

    free(node);
}

void node_list_free(Node *list)
{
    while (list != NULL)
    {
        Node *next = list->next;
        node_free(list);
        list = next;
    }
}

Node *node_find_parent(Node *node, NodeKind kind)
{
    Node *parent = node->parent;
    while (parent)
    {
        if (parent->kind == kind)
        {
            return parent;
        }

        parent = parent->parent;
    }

    return NULL;
}

Node *node_list_add(Node *list, Node *node)
{
    if (list == NULL)
    {
        return node;
    }

    if (node == NULL)
    {
        return list;
    }

    Node *last = list;
    while (last->next)
    {
        last = last->next;
    }

    last->next = node;
    return list;
}

int node_list_len(Node *list)
{
    int len = 0;
    Node *node = list;
    while (node)
    {
        len++;
        node = node->next;
    }
    return len;
}

char *node_kind_string(NodeKind kind)
{
    switch (kind)
    {
    case NODE_ERROR:
        return "ERROR";
    case NODE_PROGRAM:
        return "PROGRAM";
    case NODE_IDENTIFIER:
        return "IDENTIFIER";
    case NODE_LIT_INT:
        return "LIT_INT";
    case NODE_LIT_FLOAT:
        return "LIT_FLOAT";
    case NODE_LIT_CHAR:
        return "LIT_CHAR";
    case NODE_LIT_STRING:
        return "LIT_STRING";
    case NODE_EXPR_MEMBER:
        return "EXPR_MEMBER";
    case NODE_EXPR_CALL:
        return "EXPR_CALL";
    case NODE_EXPR_INDEX:
        return "EXPR_INDEX";
    case NODE_EXPR_CAST:
        return "EXPR_CAST";
    case NODE_EXPR_NEW:
        return "EXPR_NEW";
    case NODE_EXPR_UNARY:
        return "EXPR_UNARY";
    case NODE_EXPR_BINARY:
        return "EXPR_BINARY";
    case NODE_TYPE_ARRAY:
        return "TYPE_ARRAY";
    case NODE_TYPE_POINTER:
        return "TYPE_POINTER";
    case NODE_TYPE_FUN:
        return "TYPE_FUN";
    case NODE_TYPE_STR:
        return "TYPE_STR";
    case NODE_TYPE_UNI:
        return "TYPE_UNI";
    case NODE_FIELD:
        return "FIELD";
    case NODE_STMT_VAL:
        return "STMT_VAL";
    case NODE_STMT_VAR:
        return "STMT_VAR";
    case NODE_STMT_DEF:
        return "STMT_DEF";
    case NODE_STMT_USE:
        return "STMT_USE";
    case NODE_STMT_FUN:
        return "STMT_FUN";
    case NODE_STMT_EXT:
        return "STMT_EXT";
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
    case NODE_STMT_ASM:
        return "STMT_ASM";
    case NODE_STMT_BLOCK:
        return "STMT_BLOCK";
    case NODE_STMT_EXPR:
        return "STMT_EXPR";
    default:
        return "UNKNOWN";
    }
}
