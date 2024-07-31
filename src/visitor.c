#include <stdlib.h>
#include <stdio.h>

#include "visitor.h"

Visitor *visitor_new(NodeKind kind)
{
    Visitor *visitor = malloc(sizeof(Visitor));
    visitor->context = NULL;
    visitor->callback = NULL;
    visitor->kind = kind;

    return visitor;
}

void visitor_free(Visitor *visitor)
{
    if (visitor == NULL)
    {
        return;
    }

    visitor->context = NULL;
    visitor->callback = NULL;

    free(visitor);
    visitor = NULL;
}

void visitor_visit(Visitor *visitor, Node *node)
{
    Node *current = node;
    while (current != NULL)
    {
        if (visitor->kind == (NodeKind)-1 || current->kind == visitor->kind)
        {
            if (visitor->callback != NULL)
            {
                visitor->callback(visitor->context, current);
            }
        }

        switch (current->kind)
        {
        case NODE_ERROR:
            break;
        case NODE_PROGRAM:
            visitor_visit(visitor, current->program.statements);
            break;
        case NODE_IDENTIFIER:
            break;
        case NODE_LIT_INT:
            break;
        case NODE_LIT_FLOAT:
            break;
        case NODE_LIT_CHAR:
            break;
        case NODE_LIT_STRING:
            break;
        case NODE_EXPR_MEMBER:
            visitor_visit(visitor, current->expr_member.target);
            visitor_visit(visitor, current->expr_member.identifier);
            break;
        case NODE_EXPR_CALL:
            visitor_visit(visitor, current->expr_call.target);
            visitor_visit(visitor, current->expr_call.arguments);
            break;
        case NODE_EXPR_INDEX:
            visitor_visit(visitor, current->expr_index.target);
            visitor_visit(visitor, current->expr_index.index);
            break;
        case NODE_EXPR_CAST:
            visitor_visit(visitor, current->expr_cast.target);
            visitor_visit(visitor, current->expr_cast.type);
            break;
        case NODE_EXPR_NEW:
            visitor_visit(visitor, current->expr_new.type);
            visitor_visit(visitor, current->expr_new.initializers);
            break;
        case NODE_EXPR_UNARY:
            visitor_visit(visitor, current->expr_unary.right);
            break;
        case NODE_EXPR_BINARY:
            visitor_visit(visitor, current->expr_binary.left);
            visitor_visit(visitor, current->expr_binary.right);
            break;
        case NODE_TYPE_ARRAY:
            visitor_visit(visitor, current->type_arr.size);
            visitor_visit(visitor, current->type_arr.type);
            break;
        case NODE_TYPE_POINTER:
            visitor_visit(visitor, current->type_ptr.type);
            break;
        case NODE_TYPE_FUN:
            visitor_visit(visitor, current->type_fun.parameters);
            visitor_visit(visitor, current->type_fun.return_type);
            break;
        case NODE_TYPE_STR:
            visitor_visit(visitor, current->type_str.fields);
            break;
        case NODE_TYPE_UNI:
            visitor_visit(visitor, current->type_uni.fields);
            break;
        case NODE_FIELD:
            visitor_visit(visitor, current->field.identifier);
            visitor_visit(visitor, current->field.type);
            break;
        case NODE_STMT_VAL:
            visitor_visit(visitor, current->stmt_val.identifier);
            visitor_visit(visitor, current->stmt_val.type);
            visitor_visit(visitor, current->stmt_val.initializer);
            break;
        case NODE_STMT_VAR:
            visitor_visit(visitor, current->stmt_var.identifier);
            visitor_visit(visitor, current->stmt_var.type);
            visitor_visit(visitor, current->stmt_var.initializer);
            break;
        case NODE_STMT_DEF:
            visitor_visit(visitor, current->stmt_def.identifier);
            visitor_visit(visitor, current->stmt_def.type);
            break;
        case NODE_STMT_USE:
            visitor_visit(visitor, current->stmt_use.path);
            break;
        case NODE_STMT_FUN:
            visitor_visit(visitor, current->stmt_fun.identifier);
            visitor_visit(visitor, current->stmt_fun.parameters);
            visitor_visit(visitor, current->stmt_fun.return_type);
            visitor_visit(visitor, current->stmt_fun.body);
            break;
        case NODE_STMT_EXT:
            visitor_visit(visitor, current->stmt_ext.identifier);
            visitor_visit(visitor, current->stmt_ext.type);
            break;
        case NODE_STMT_IF:
            visitor_visit(visitor, current->stmt_if.condition);
            visitor_visit(visitor, current->stmt_if.body);
            break;
        case NODE_STMT_OR:
            visitor_visit(visitor, current->stmt_or.condition);
            visitor_visit(visitor, current->stmt_or.body);
            break;
        case NODE_STMT_FOR:
            visitor_visit(visitor, current->stmt_for.condition);
            visitor_visit(visitor, current->stmt_for.body);
            break;
        case NODE_STMT_BRK:
            break;
        case NODE_STMT_CNT:
            break;
        case NODE_STMT_RET:
            visitor_visit(visitor, current->stmt_ret.value);
            break;
        case NODE_STMT_ASM:
            visitor_visit(visitor, current->stmt_asm.code);
            break;
        case NODE_STMT_BLOCK:
            visitor_visit(visitor, current->stmt_block.statements);
            break;
        case NODE_STMT_EXPR:
            visitor_visit(visitor, current->stmt_expr.expression);
            break;
        default:
            break;
        }

        if (current->next == NULL)
        {
            break;
        }
        
        current = current->next;
    }
}

void visitor_print_node_tree(void *context, Node *node)
{
    (void)context;
    
    Node *current = node;
    while (current != NULL)
    {
        if (current->kind == NODE_PROGRAM)
        {
            break;
        }

        printf("  ");

        current = current->parent;
    }

    printf("%s\n", node_kind_string(node->kind));
}
