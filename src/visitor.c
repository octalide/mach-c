#include <stdlib.h>

#include "visitor.h"

void visitor_init(Visitor *visitor)
{
    visitor->cb_visit_node_block = NULL;
    visitor->cb_visit_node_identifier = NULL;
    visitor->cb_visit_expr_identifier = NULL;
    visitor->cb_visit_expr_member = NULL;
    visitor->cb_visit_expr_lit_char = NULL;
    visitor->cb_visit_expr_lit_number = NULL;
    visitor->cb_visit_expr_lit_string = NULL;
    visitor->cb_visit_expr_call = NULL;
    visitor->cb_visit_expr_index = NULL;
    visitor->cb_visit_expr_def_str = NULL;
    visitor->cb_visit_expr_def_uni = NULL;
    visitor->cb_visit_expr_def_array = NULL;
    visitor->cb_visit_expr_unary = NULL;
    visitor->cb_visit_expr_binary = NULL;
    visitor->cb_visit_type_array = NULL;
    visitor->cb_visit_type_ref = NULL;
    visitor->cb_visit_type_fun = NULL;
    visitor->cb_visit_type_str = NULL;
    visitor->cb_visit_type_uni = NULL;
    visitor->cb_visit_node_field = NULL;
    visitor->cb_visit_stmt_use = NULL;
    visitor->cb_visit_stmt_if = NULL;
    visitor->cb_visit_stmt_or = NULL;
    visitor->cb_visit_stmt_for = NULL;
    visitor->cb_visit_stmt_ret = NULL;
    visitor->cb_visit_stmt_decl_type = NULL;
    visitor->cb_visit_stmt_decl_val = NULL;
    visitor->cb_visit_stmt_decl_var = NULL;
    visitor->cb_visit_stmt_decl_fun = NULL;
    visitor->cb_visit_stmt_decl_str = NULL;
    visitor->cb_visit_stmt_decl_uni = NULL;
    visitor->cb_visit_stmt_expr = NULL;
    visitor->cb_visit_node_stmt = NULL;
    visitor->cb_visit_node = NULL;
    visitor->context = NULL;
}

void visit_node_block(Visitor *visitor, Node *node)
{
    if (visitor->cb_visit_node_block != NULL)
    {
        if (!visitor->cb_visit_node_block(visitor->context, node))
        {
            return;
        }
    }

    for (int i = 0; node->data.node_block.statements[i] != NULL; i++)
    {
        visit_node(visitor, node->data.node_block.statements[i]);
    }
}

void visit_node_identifier(Visitor *visitor, Node *node)
{
    if (visitor->cb_visit_node_identifier != NULL)
    {
        if (!visitor->cb_visit_node_identifier(visitor->context, node))
        {
            return;
        }
    }

    if (node->data.node_identifier.member != NULL)
    {
        visit_node(visitor, node->data.node_identifier.member);
    }
}

void visit_expr_identifier(Visitor *visitor, Node *node)
{
    if (visitor->cb_visit_expr_identifier != NULL)
    {
        if (!visitor->cb_visit_expr_identifier(visitor->context, node))
        {
            return;
        }
    }

    if (node->data.expr_identifier.identifier != NULL)
    {
        visit_node(visitor, node->data.expr_identifier.identifier);
    }
}

void visit_expr_member(Visitor *visitor, Node *node)
{
    if (visitor->cb_visit_expr_member != NULL)
    {
        if (!visitor->cb_visit_expr_member(visitor->context, node))
        {
            return;
        }
    }

    if (node->data.expr_member.target != NULL)
    {
        visit_node(visitor, node->data.expr_member.target);
    }

    if (node->data.expr_member.member != NULL)
    {
        visit_node(visitor, node->data.expr_member.member);
    }
}

void visit_expr_lit_char(Visitor *visitor, Node *node)
{
    if (visitor->cb_visit_expr_lit_char != NULL)
    {
        if (!visitor->cb_visit_expr_lit_char(visitor->context, node))
        {
            return;
        }
    }
}

void visit_expr_lit_number(Visitor *visitor, Node *node)
{
    if (visitor->cb_visit_expr_lit_number != NULL)
    {
        if (!visitor->cb_visit_expr_lit_number(visitor->context, node))
        {
            return;
        }
    }
}

void visit_expr_lit_string(Visitor *visitor, Node *node)
{
    if (visitor->cb_visit_expr_lit_string != NULL)
    {
        if (!visitor->cb_visit_expr_lit_string(visitor->context, node))
        {
            return;
        }
    }
}

void visit_expr_call(Visitor *visitor, Node *node)
{
    if (visitor->cb_visit_expr_call != NULL)
    {
        if (!visitor->cb_visit_expr_call(visitor->context, node))
        {
            return;
        }
    }

    if (node->data.expr_call.target != NULL)
    {
        visit_node(visitor, node->data.expr_call.target);
    }

    for (int i = 0; node->data.expr_call.arguments[i] != NULL; i++)
    {
        visit_node(visitor, node->data.expr_call.arguments[i]);
    }
}

void visit_expr_index(Visitor *visitor, Node *node)
{
    if (visitor->cb_visit_expr_index != NULL)
    {
        if (!visitor->cb_visit_expr_index(visitor->context, node))
        {
            return;
        }
    }

    if (node->data.expr_index.target != NULL)
    {
        visit_node(visitor, node->data.expr_index.target);
    }

    if (node->data.expr_index.index != NULL)
    {
        visit_node(visitor, node->data.expr_index.index);
    }
}

void visit_expr_def_str(Visitor *visitor, Node *node)
{
    if (visitor->cb_visit_expr_def_str != NULL)
    {
        if (!visitor->cb_visit_expr_def_str(visitor->context, node))
        {
            return;
        }
    }

    for (int i = 0; node->data.expr_def_str.initializers[i] != NULL; i++)
    {
        visit_node(visitor, node->data.expr_def_str.initializers[i]);
    }
}

void visit_expr_def_uni(Visitor *visitor, Node *node)
{
    if (visitor->cb_visit_expr_def_uni != NULL)
    {
        if (!visitor->cb_visit_expr_def_uni(visitor->context, node))
        {
            return;
        }
    }

    if (node->data.expr_def_uni.initializer != NULL)
    {
        visit_node(visitor, node->data.expr_def_uni.initializer);
    }
}

void visit_expr_def_array(Visitor *visitor, Node *node)
{
    if (visitor->cb_visit_expr_def_array != NULL)
    {
        if (!visitor->cb_visit_expr_def_array(visitor->context, node))
        {
            return;
        }
    }

    if (node->data.expr_def_array.size != NULL)
    {
        visit_node(visitor, node->data.expr_def_array.size);
    }

    if (node->data.expr_def_array.type != NULL)
    {
        visit_node(visitor, node->data.expr_def_array.type);
    }

    for (int i = 0; node->data.expr_def_array.elements[i] != NULL; i++)
    {
        visit_node(visitor, node->data.expr_def_array.elements[i]);
    }
}

void visit_expr_unary(Visitor *visitor, Node *node)
{
    if (visitor->cb_visit_expr_unary != NULL)
    {
        if (!visitor->cb_visit_expr_unary(visitor->context, node))
        {
            return;
        }
    }

    if (node->data.expr_unary.rhs != NULL)
    {
        visit_node(visitor, node->data.expr_unary.rhs);
    }
}

void visit_expr_binary(Visitor *visitor, Node *node)
{
    if (visitor->cb_visit_expr_binary != NULL)
    {
        if (!visitor->cb_visit_expr_binary(visitor->context, node))
        {
            return;
        }
    }

    if (node->data.expr_binary.lhs != NULL)
    {
        visit_node(visitor, node->data.expr_binary.lhs);
    }

    if (node->data.expr_binary.rhs != NULL)
    {
        visit_node(visitor, node->data.expr_binary.rhs);
    }
}

void visit_type_array(Visitor *visitor, Node *node)
{
    if (visitor->cb_visit_type_array != NULL)
    {
        if (!visitor->cb_visit_type_array(visitor->context, node))
        {
            return;
        }
    }

    if (node->data.type_array.size != NULL)
    {
        visit_node(visitor, node->data.type_array.size);
    }

    if (node->data.type_array.type != NULL)
    {
        visit_node(visitor, node->data.type_array.type);
    }
}

void visit_type_ref(Visitor *visitor, Node *node)
{
    if (visitor->cb_visit_type_ref != NULL)
    {
        if (!visitor->cb_visit_type_ref(visitor->context, node))
        {
            return;
        }
    }

    if (node->data.type_ref.target != NULL)
    {
        visit_node(visitor, node->data.type_ref.target);
    }
}

void visit_type_fun(Visitor *visitor, Node *node)
{
    if (visitor->cb_visit_type_fun != NULL)
    {
        if (!visitor->cb_visit_type_fun(visitor->context, node))
        {
            return;
        }
    }

    for (int i = 0; node->data.type_fun.parameter_types[i] != NULL; i++)
    {
        visit_node(visitor, node->data.type_fun.parameter_types[i]);
    }

    if (node->data.type_fun.return_type != NULL)
    {
        visit_node(visitor, node->data.type_fun.return_type);
    }
}

void visit_type_str(Visitor *visitor, Node *node)
{
    if (visitor->cb_visit_type_str != NULL)
    {
        if (!visitor->cb_visit_type_str(visitor->context, node))
        {
            return;
        }
    }

    for (int i = 0; node->data.type_str.fields[i] != NULL; i++)
    {
        visit_node(visitor, node->data.type_str.fields[i]);
    }
}

void visit_type_uni(Visitor *visitor, Node *node)
{
    if (visitor->cb_visit_type_uni != NULL)
    {
        if (!visitor->cb_visit_type_uni(visitor->context, node))
        {
            return;
        }
    }

    for (int i = 0; node->data.type_uni.fields[i] != NULL; i++)
    {
        visit_node(visitor, node->data.type_uni.fields[i]);
    }
}

void visit_node_field(Visitor *visitor, Node *node)
{
    if (visitor->cb_visit_node_field != NULL)
    {
        if (!visitor->cb_visit_node_field(visitor->context, node))
        {
            return;
        }
    }

    if (node->data.node_field.identifier != NULL)
    {
        visit_node(visitor, node->data.node_field.identifier);
    }

    if (node->data.node_field.type != NULL)
    {
        visit_node(visitor, node->data.node_field.type);
    }
}

void visit_stmt_use(Visitor *visitor, Node *node)
{
    if (visitor->cb_visit_stmt_use != NULL)
    {
        if (!visitor->cb_visit_stmt_use(visitor->context, node))
        {
            return;
        }
    }

    if (node->data.stmt_use.identifier != NULL)
    {
        visit_node(visitor, node->data.stmt_use.identifier);
    }

    if (node->data.stmt_use.path != NULL)
    {
        visit_node(visitor, node->data.stmt_use.path);
    }
}

void visit_stmt_if(Visitor *visitor, Node *node)
{
    if (visitor->cb_visit_stmt_if != NULL)
    {
        if (!visitor->cb_visit_stmt_if(visitor->context, node))
        {
            return;
        }
    }

    if (node->data.stmt_if.condition != NULL)
    {
        visit_node(visitor, node->data.stmt_if.condition);
    }

    if (node->data.stmt_if.body != NULL)
    {
        visit_node(visitor, node->data.stmt_if.body);
    }
}

void visit_stmt_or(Visitor *visitor, Node *node)
{
    if (visitor->cb_visit_stmt_or != NULL)
    {
        if (!visitor->cb_visit_stmt_or(visitor->context, node))
        {
            return;
        }
    }

    if (node->data.stmt_or.condition != NULL)
    {
        visit_node(visitor, node->data.stmt_or.condition);
    }

    if (node->data.stmt_or.body != NULL)
    {
        visit_node(visitor, node->data.stmt_or.body);
    }
}

void visit_stmt_for(Visitor *visitor, Node *node)
{
    if (visitor->cb_visit_stmt_for != NULL)
    {
        if (!visitor->cb_visit_stmt_for(visitor->context, node))
        {
            return;
        }
    }

    if (node->data.stmt_for.condition != NULL)
    {
        visit_node(visitor, node->data.stmt_for.condition);
    }

    if (node->data.stmt_for.body != NULL)
    {
        visit_node(visitor, node->data.stmt_for.body);
    }
}

void visit_stmt_ret(Visitor *visitor, Node *node)
{
    if (visitor->cb_visit_stmt_ret != NULL)
    {
        if (!visitor->cb_visit_stmt_ret(visitor->context, node))
        {
            return;
        }
    }

    if (node->data.stmt_ret.value != NULL)
    {
        visit_node(visitor, node->data.stmt_ret.value);
    }
}

void visit_stmt_decl_type(Visitor *visitor, Node *node)
{
    if (visitor->cb_visit_stmt_decl_type != NULL)
    {
        if (!visitor->cb_visit_stmt_decl_type(visitor->context, node))
        {
            return;
        }
    }

    if (node->data.stmt_decl_type.identifier != NULL)
    {
        visit_node(visitor, node->data.stmt_decl_type.identifier);
    }

    if (node->data.stmt_decl_type.type != NULL)
    {
        visit_node(visitor, node->data.stmt_decl_type.type);
    }
}

void visit_stmt_decl_val(Visitor *visitor, Node *node)
{
    if (visitor->cb_visit_stmt_decl_val != NULL)
    {
        if (!visitor->cb_visit_stmt_decl_val(visitor->context, node))
        {
            return;
        }
    }

    if (node->data.stmt_decl_val.identifier != NULL)
    {
        visit_node(visitor, node->data.stmt_decl_val.identifier);
    }

    if (node->data.stmt_decl_val.type != NULL)
    {
        visit_node(visitor, node->data.stmt_decl_val.type);
    }

    if (node->data.stmt_decl_val.initializer != NULL)
    {
        visit_node(visitor, node->data.stmt_decl_val.initializer);
    }
}

void visit_stmt_decl_var(Visitor *visitor, Node *node)
{
    if (visitor->cb_visit_stmt_decl_var != NULL)
    {
        if (!visitor->cb_visit_stmt_decl_var(visitor->context, node))
        {
            return;
        }
    }

    if (node->data.stmt_decl_var.identifier != NULL)
    {
        visit_node(visitor, node->data.stmt_decl_var.identifier);
    }

    if (node->data.stmt_decl_var.type != NULL)
    {
        visit_node(visitor, node->data.stmt_decl_var.type);
    }

    if (node->data.stmt_decl_var.initializer != NULL)
    {
        visit_node(visitor, node->data.stmt_decl_var.initializer);
    }
}

void visit_stmt_decl_fun(Visitor *visitor, Node *node)
{
    if (visitor->cb_visit_stmt_decl_fun != NULL)
    {
        if (!visitor->cb_visit_stmt_decl_fun(visitor->context, node))
        {
            return;
        }
    }

    if (node->data.stmt_decl_fun.identifier != NULL)
    {
        visit_node(visitor, node->data.stmt_decl_fun.identifier);
    }

    if (node->data.stmt_decl_fun.return_type != NULL)
    {
        visit_node(visitor, node->data.stmt_decl_fun.return_type);
    }

    if (node->data.stmt_decl_fun.body != NULL)
    {
        visit_node(visitor, node->data.stmt_decl_fun.body);
    }
}

void visit_stmt_decl_str(Visitor *visitor, Node *node)
{
    if (visitor->cb_visit_stmt_decl_str != NULL)
    {
        if (!visitor->cb_visit_stmt_decl_str(visitor->context, node))
        {
            return;
        }
    }

    if (node->data.stmt_decl_str.identifier != NULL)
    {
        visit_node(visitor, node->data.stmt_decl_str.identifier);
    }

    for (int i = 0; node->data.stmt_decl_str.fields[i] != NULL; i++)
    {
        visit_node(visitor, node->data.stmt_decl_str.fields[i]);
    }
}

void visit_stmt_decl_uni(Visitor *visitor, Node *node)
{
    if (visitor->cb_visit_stmt_decl_uni != NULL)
    {
        if (!visitor->cb_visit_stmt_decl_uni(visitor->context, node))
        {
            return;
        }
    }

    if (node->data.stmt_decl_uni.identifier != NULL)
    {
        visit_node(visitor, node->data.stmt_decl_uni.identifier);
    }

    for (int i = 0; node->data.stmt_decl_uni.fields[i] != NULL; i++)
    {
        visit_node(visitor, node->data.stmt_decl_uni.fields[i]);
    }
}

void visit_stmt_expr(Visitor *visitor, Node *node)
{
    if (visitor->cb_visit_stmt_expr != NULL)
    {
        if (!visitor->cb_visit_stmt_expr(visitor->context, node))
        {
            return;
        }
    }

    if (node->data.stmt_expr.expression != NULL)
    {
        visit_node(visitor, node->data.stmt_expr.expression);
    }
}

void visit_node(Visitor *visitor, Node *node)
{
    if (node == NULL)
    {
        return;
    }

    if (visitor->cb_visit_node != NULL)
    {
        if (!visitor->cb_visit_node(visitor->context, node))
        {
            return;
        }
    }

    switch (node->type)
    {
    case NODE_BLOCK:
        visit_node_block(visitor, node);
        break;
    case NODE_IDENTIFIER:
        visit_node_identifier(visitor, node);
        break;
    case NODE_EXPR_IDENTIFIER:
        visit_expr_identifier(visitor, node);
        break;
    case NODE_EXPR_MEMBER:
        visit_expr_member(visitor, node);
        break;
    case NODE_EXPR_LIT_CHAR:
        visit_expr_lit_char(visitor, node);
        break;
    case NODE_EXPR_LIT_NUMBER:
        visit_expr_lit_number(visitor, node);
        break;
    case NODE_EXPR_LIT_STRING:
        visit_expr_lit_string(visitor, node);
        break;
    case NODE_EXPR_CALL:
        visit_expr_call(visitor, node);
        break;
    case NODE_EXPR_INDEX:
        visit_expr_index(visitor, node);
        break;
    case NODE_EXPR_DEF_STR:
        visit_expr_def_str(visitor, node);
        break;
    case NODE_EXPR_DEF_UNI:
        visit_expr_def_uni(visitor, node);
        break;
    case NODE_EXPR_DEF_ARRAY:
        visit_expr_def_array(visitor, node);
        break;
    case NODE_EXPR_UNARY:
        visit_expr_unary(visitor, node);
        break;
    case NODE_EXPR_BINARY:
        visit_expr_binary(visitor, node);
        break;
    case NODE_TYPE_ARRAY:
        visit_type_array(visitor, node);
        break;
    case NODE_TYPE_REF:
        visit_type_ref(visitor, node);
        break;
    case NODE_TYPE_FUN:
        visit_type_fun(visitor, node);
        break;
    case NODE_TYPE_STR:
        visit_type_str(visitor, node);
        break;
    case NODE_TYPE_UNI:
        visit_type_uni(visitor, node);
        break;
    case NODE_FIELD:
        visit_node_field(visitor, node);
        break;
    case NODE_STMT_USE:
        visit_stmt_use(visitor, node);
        break;
    case NODE_STMT_IF:
        visit_stmt_if(visitor, node);
        break;
    case NODE_STMT_OR:
        visit_stmt_or(visitor, node);
        break;
    case NODE_STMT_FOR:
        visit_stmt_for(visitor, node);
        break;
    case NODE_STMT_RET:
        visit_stmt_ret(visitor, node);
        break;
    case NODE_STMT_DECL_TYPE:
        visit_stmt_decl_type(visitor, node);
        break;
    case NODE_STMT_DECL_VAL:
        visit_stmt_decl_val(visitor, node);
        break;
    case NODE_STMT_DECL_VAR:
        visit_stmt_decl_var(visitor, node);
        break;
    case NODE_STMT_DECL_FUN:
        visit_stmt_decl_fun(visitor, node);
        break;
    case NODE_STMT_DECL_STR:
        visit_stmt_decl_str(visitor, node);
        break;
    case NODE_STMT_DECL_UNI:
        visit_stmt_decl_uni(visitor, node);
        break;
    case NODE_STMT_EXPR:
        visit_stmt_expr(visitor, node);
        break;
    default:
        break;
    }
}
