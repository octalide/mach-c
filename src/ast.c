#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void ast_node_init(AstNode *node, AstKind kind)
{
    memset(node, 0, sizeof(AstNode));
    node->kind = kind;
}

void ast_node_dnit(AstNode *node)
{
    if (!node)
        return;

    switch (node->kind)
    {
    case AST_PROGRAM:
        if (node->program.decls)
        {
            ast_list_dnit(node->program.decls);
            free(node->program.decls);
        }
        break;
    case AST_MODULE:
        free(node->module.name);
        if (node->module.decls)
        {
            ast_list_dnit(node->module.decls);
            free(node->module.decls);
        }
        break;

    case AST_USE_DECL:
        free(node->use_decl.module_path);
        free(node->use_decl.alias);
        break;

    case AST_EXT_DECL:
        free(node->ext_decl.name);
        if (node->ext_decl.type)
        {
            ast_node_dnit(node->ext_decl.type);
            free(node->ext_decl.type);
        }
        break;

    case AST_DEF_DECL:
        free(node->def_decl.name);
        if (node->def_decl.type)
        {
            ast_node_dnit(node->def_decl.type);
            free(node->def_decl.type);
        }
        break;

    case AST_VAL_DECL:
    case AST_VAR_DECL:
        free(node->var_decl.name);
        if (node->var_decl.type)
        {
            ast_node_dnit(node->var_decl.type);
            free(node->var_decl.type);
        }
        if (node->var_decl.init)
        {
            ast_node_dnit(node->var_decl.init);
            free(node->var_decl.init);
        }
        break;

    case AST_FUN_DECL:
        free(node->fun_decl.name);
        if (node->fun_decl.params)
        {
            ast_list_dnit(node->fun_decl.params);
            free(node->fun_decl.params);
        }
        if (node->fun_decl.return_type)
        {
            ast_node_dnit(node->fun_decl.return_type);
            free(node->fun_decl.return_type);
        }
        if (node->fun_decl.body)
        {
            ast_node_dnit(node->fun_decl.body);
            free(node->fun_decl.body);
        }
        break;

    case AST_STR_DECL:
        free(node->str_decl.name);
        if (node->str_decl.fields)
        {
            ast_list_dnit(node->str_decl.fields);
            free(node->str_decl.fields);
        }
        break;

    case AST_UNI_DECL:
        free(node->uni_decl.name);
        if (node->uni_decl.fields)
        {
            ast_list_dnit(node->uni_decl.fields);
            free(node->uni_decl.fields);
        }
        break;

    case AST_FIELD_DECL:
        free(node->field_decl.name);
        if (node->field_decl.type)
        {
            ast_node_dnit(node->field_decl.type);
            free(node->field_decl.type);
        }
        break;

    case AST_PARAM_DECL:
        free(node->param_decl.name);
        if (node->param_decl.type)
        {
            ast_node_dnit(node->param_decl.type);
            free(node->param_decl.type);
        }
        break;

    case AST_BLOCK_STMT:
        if (node->block_stmt.stmts)
        {
            ast_list_dnit(node->block_stmt.stmts);
            free(node->block_stmt.stmts);
        }
        break;

    case AST_EXPR_STMT:
        if (node->expr_stmt.expr)
        {
            ast_node_dnit(node->expr_stmt.expr);
            free(node->expr_stmt.expr);
        }
        break;

    case AST_RET_STMT:
        if (node->ret_stmt.expr)
        {
            ast_node_dnit(node->ret_stmt.expr);
            free(node->ret_stmt.expr);
        }
        break;

    case AST_IF_STMT:
        if (node->if_stmt.cond)
        {
            ast_node_dnit(node->if_stmt.cond);
            free(node->if_stmt.cond);
        }
        if (node->if_stmt.then_stmt)
        {
            ast_node_dnit(node->if_stmt.then_stmt);
            free(node->if_stmt.then_stmt);
        }
        if (node->if_stmt.else_stmt)
        {
            ast_node_dnit(node->if_stmt.else_stmt);
            free(node->if_stmt.else_stmt);
        }
        break;

    case AST_FOR_STMT:
        if (node->for_stmt.cond)
        {
            ast_node_dnit(node->for_stmt.cond);
            free(node->for_stmt.cond);
        }
        if (node->for_stmt.body)
        {
            ast_node_dnit(node->for_stmt.body);
            free(node->for_stmt.body);
        }
        break;

    case AST_BRK_STMT:
    case AST_CNT_STMT:
        break;

    case AST_BINARY_EXPR:
        if (node->binary_expr.left)
        {
            ast_node_dnit(node->binary_expr.left);
            free(node->binary_expr.left);
        }
        if (node->binary_expr.right)
        {
            ast_node_dnit(node->binary_expr.right);
            free(node->binary_expr.right);
        }
        break;

    case AST_UNARY_EXPR:
        if (node->unary_expr.expr)
        {
            ast_node_dnit(node->unary_expr.expr);
            free(node->unary_expr.expr);
        }
        break;

    case AST_CALL_EXPR:
        if (node->call_expr.func)
        {
            ast_node_dnit(node->call_expr.func);
            free(node->call_expr.func);
        }
        if (node->call_expr.args)
        {
            ast_list_dnit(node->call_expr.args);
            free(node->call_expr.args);
        }
        break;

    case AST_INDEX_EXPR:
        if (node->index_expr.array)
        {
            ast_node_dnit(node->index_expr.array);
            free(node->index_expr.array);
        }
        if (node->index_expr.index)
        {
            ast_node_dnit(node->index_expr.index);
            free(node->index_expr.index);
        }
        break;

    case AST_FIELD_EXPR:
        if (node->field_expr.object)
        {
            ast_node_dnit(node->field_expr.object);
            free(node->field_expr.object);
        }
        free(node->field_expr.field);
        break;

    case AST_CAST_EXPR:
        if (node->cast_expr.expr)
        {
            ast_node_dnit(node->cast_expr.expr);
            free(node->cast_expr.expr);
        }
        if (node->cast_expr.type)
        {
            ast_node_dnit(node->cast_expr.type);
            free(node->cast_expr.type);
        }
        break;

    case AST_IDENT_EXPR:
        free(node->ident_expr.name);
        break;

    case AST_LIT_EXPR:
        if (node->lit_expr.kind == TOKEN_LIT_STRING)
        {
            free(node->lit_expr.string_val);
        }
        break;

    case AST_ARRAY_EXPR:
        if (node->array_expr.type)
        {
            ast_node_dnit(node->array_expr.type);
            free(node->array_expr.type);
        }
        if (node->array_expr.elems)
        {
            ast_list_dnit(node->array_expr.elems);
            free(node->array_expr.elems);
        }
        break;

    case AST_STRUCT_EXPR:
        if (node->struct_expr.type)
        {
            ast_node_dnit(node->struct_expr.type);
            free(node->struct_expr.type);
        }
        if (node->struct_expr.fields)
        {
            ast_list_dnit(node->struct_expr.fields);
            free(node->struct_expr.fields);
        }
        break;

    case AST_TYPE_NAME:
        free(node->type_name.name);
        break;

    case AST_TYPE_PTR:
        if (node->type_ptr.base)
        {
            ast_node_dnit(node->type_ptr.base);
            free(node->type_ptr.base);
        }
        break;

    case AST_TYPE_ARRAY:
        if (node->type_array.elem_type)
        {
            ast_node_dnit(node->type_array.elem_type);
            free(node->type_array.elem_type);
        }
        if (node->type_array.size)
        {
            ast_node_dnit(node->type_array.size);
            free(node->type_array.size);
        }
        break;

    case AST_TYPE_FUN:
        if (node->type_fun.params)
        {
            ast_list_dnit(node->type_fun.params);
            free(node->type_fun.params);
        }
        if (node->type_fun.return_type)
        {
            ast_node_dnit(node->type_fun.return_type);
            free(node->type_fun.return_type);
        }
        break;

    case AST_TYPE_STR:
        free(node->type_str.name);
        if (node->type_str.fields)
        {
            ast_list_dnit(node->type_str.fields);
            free(node->type_str.fields);
        }
        break;

    case AST_TYPE_UNI:
        free(node->type_uni.name);
        if (node->type_uni.fields)
        {
            ast_list_dnit(node->type_uni.fields);
            free(node->type_uni.fields);
        }
        break;
    }
}

void ast_list_init(AstList *list)
{
    list->items    = NULL;
    list->count    = 0;
    list->capacity = 0;
}

void ast_list_dnit(AstList *list)
{
    if (!list)
        return;

    for (int i = 0; i < list->count; i++)
    {
        if (list->items[i])
        {
            ast_node_dnit(list->items[i]);
            free(list->items[i]);
        }
    }
    free(list->items);
}

void ast_list_append(AstList *list, AstNode *node)
{
    if (list->count >= list->capacity)
    {
        list->capacity = list->capacity ? list->capacity * 2 : 8;
        list->items    = realloc(list->items, sizeof(AstNode *) * list->capacity);
    }
    list->items[list->count++] = node;
}

// helper for printing
static void print_indent(int indent)
{
    for (int i = 0; i < indent; i++)
    {
        printf("  ");
    }
}

void ast_print(AstNode *node, int indent)
{
    if (!node)
    {
        print_indent(indent);
        printf("(null)\n");
        return;
    }

    print_indent(indent);

    switch (node->kind)
    {
    case AST_PROGRAM:
        printf("PROGRAM\n");
        for (int i = 0; i < node->program.decls->count; i++)
        {
            ast_print(node->program.decls->items[i], indent + 1);
        }
        break;
    case AST_MODULE:
        printf("MODULE %s\n", node->module.name);
        for (int i = 0; i < node->module.decls->count; i++)
        {
            ast_print(node->module.decls->items[i], indent + 1);
        }
        break;
    case AST_USE_DECL:
        printf("USE %s", node->use_decl.module_path);
        if (node->use_decl.alias)
        {
            printf(" as %s", node->use_decl.alias);
        }
        printf("\n");
        break;

    case AST_EXT_DECL:
        printf("EXT %s:\n", node->ext_decl.name);
        ast_print(node->ext_decl.type, indent + 1);
        break;

    case AST_DEF_DECL:
        printf("DEF %s:\n", node->def_decl.name);
        ast_print(node->def_decl.type, indent + 1);
        break;

    case AST_VAL_DECL:
        printf("VAL %s:\n", node->var_decl.name);
        if (node->var_decl.type)
        {
            ast_print(node->var_decl.type, indent + 1);
        }
        if (node->var_decl.init)
        {
            print_indent(indent + 1);
            printf("= \n");
            ast_print(node->var_decl.init, indent + 2);
        }
        break;

    case AST_VAR_DECL:
        printf("VAR %s:\n", node->var_decl.name);
        if (node->var_decl.type)
        {
            ast_print(node->var_decl.type, indent + 1);
        }
        if (node->var_decl.init)
        {
            print_indent(indent + 1);
            printf("= \n");
            ast_print(node->var_decl.init, indent + 2);
        }
        break;

    case AST_FUN_DECL:
        printf("FUN %s\n", node->fun_decl.name);
        print_indent(indent + 1);
        printf("params:\n");
        for (int i = 0; i < node->fun_decl.params->count; i++)
        {
            ast_print(node->fun_decl.params->items[i], indent + 2);
        }
        if (node->fun_decl.return_type)
        {
            print_indent(indent + 1);
            printf("returns:\n");
            ast_print(node->fun_decl.return_type, indent + 2);
        }
        if (node->fun_decl.body)
        {
            print_indent(indent + 1);
            printf("body:\n");
            ast_print(node->fun_decl.body, indent + 2);
        }
        break;

    case AST_STR_DECL:
        printf("STR %s\n", node->str_decl.name);
        for (int i = 0; i < node->str_decl.fields->count; i++)
        {
            ast_print(node->str_decl.fields->items[i], indent + 1);
        }
        break;

    case AST_UNI_DECL:
        printf("UNI %s\n", node->uni_decl.name);
        for (int i = 0; i < node->uni_decl.fields->count; i++)
        {
            ast_print(node->uni_decl.fields->items[i], indent + 1);
        }
        break;

    case AST_FIELD_DECL:
        printf("FIELD %s:\n", node->field_decl.name);
        ast_print(node->field_decl.type, indent + 1);
        break;

    case AST_PARAM_DECL:
        printf("PARAM %s:\n", node->param_decl.name);
        ast_print(node->param_decl.type, indent + 1);
        break;

    case AST_BLOCK_STMT:
        printf("BLOCK\n");
        for (int i = 0; i < node->block_stmt.stmts->count; i++)
        {
            ast_print(node->block_stmt.stmts->items[i], indent + 1);
        }
        break;

    case AST_EXPR_STMT:
        printf("EXPR_STMT\n");
        ast_print(node->expr_stmt.expr, indent + 1);
        break;

    case AST_RET_STMT:
        printf("RET\n");
        if (node->ret_stmt.expr)
        {
            ast_print(node->ret_stmt.expr, indent + 1);
        }
        break;

    case AST_IF_STMT:
        printf("IF\n");
        print_indent(indent + 1);
        printf("cond:\n");
        ast_print(node->if_stmt.cond, indent + 2);
        print_indent(indent + 1);
        printf("then:\n");
        ast_print(node->if_stmt.then_stmt, indent + 2);
        if (node->if_stmt.else_stmt)
        {
            print_indent(indent + 1);
            printf("else:\n");
            ast_print(node->if_stmt.else_stmt, indent + 2);
        }
        break;

    case AST_FOR_STMT:
        printf("FOR\n");
        if (node->for_stmt.cond)
        {
            print_indent(indent + 1);
            printf("cond:\n");
            ast_print(node->for_stmt.cond, indent + 2);
        }
        print_indent(indent + 1);
        printf("body:\n");
        ast_print(node->for_stmt.body, indent + 2);
        break;

    case AST_BRK_STMT:
        printf("BRK\n");
        break;

    case AST_CNT_STMT:
        printf("CNT\n");
        break;

    case AST_BINARY_EXPR:
        printf("BINARY %s\n", token_kind_to_string(node->binary_expr.op));
        ast_print(node->binary_expr.left, indent + 1);
        ast_print(node->binary_expr.right, indent + 1);
        break;

    case AST_UNARY_EXPR:
        printf("UNARY %s\n", token_kind_to_string(node->unary_expr.op));
        ast_print(node->unary_expr.expr, indent + 1);
        break;

    case AST_CALL_EXPR:
        printf("CALL\n");
        ast_print(node->call_expr.func, indent + 1);
        print_indent(indent + 1);
        printf("args:\n");
        for (int i = 0; i < node->call_expr.args->count; i++)
        {
            ast_print(node->call_expr.args->items[i], indent + 2);
        }
        break;

    case AST_INDEX_EXPR:
        printf("INDEX\n");
        ast_print(node->index_expr.array, indent + 1);
        ast_print(node->index_expr.index, indent + 1);
        break;

    case AST_FIELD_EXPR:
        printf("FIELD .%s\n", node->field_expr.field);
        ast_print(node->field_expr.object, indent + 1);
        break;

    case AST_CAST_EXPR:
        printf("CAST\n");
        ast_print(node->cast_expr.expr, indent + 1);
        print_indent(indent + 1);
        printf("to:\n");
        ast_print(node->cast_expr.type, indent + 2);
        break;

    case AST_IDENT_EXPR:
        printf("IDENT %s\n", node->ident_expr.name);
        break;

    case AST_LIT_EXPR:
        printf("LIT ");
        switch (node->lit_expr.kind)
        {
        case TOKEN_LIT_INT:
            printf("INT %llu\n", node->lit_expr.int_val);
            break;
        case TOKEN_LIT_FLOAT:
            printf("FLOAT %f\n", node->lit_expr.float_val);
            break;
        case TOKEN_LIT_CHAR:
            printf("CHAR '%c'\n", node->lit_expr.char_val);
            break;
        case TOKEN_LIT_STRING:
            printf("STRING \"%s\"\n", node->lit_expr.string_val);
            break;
        default:
            printf("???\n");
        }
        break;

    case AST_ARRAY_EXPR:
        printf("ARRAY\n");
        ast_print(node->array_expr.type, indent + 1);
        for (int i = 0; i < node->array_expr.elems->count; i++)
        {
            ast_print(node->array_expr.elems->items[i], indent + 1);
        }
        break;

    case AST_STRUCT_EXPR:
        printf("STRUCT_LIT\n");
        ast_print(node->struct_expr.type, indent + 1);
        for (int i = 0; i < node->struct_expr.fields->count; i++)
        {
            ast_print(node->struct_expr.fields->items[i], indent + 1);
        }
        break;

    case AST_TYPE_NAME:
        printf("TYPE %s\n", node->type_name.name);
        break;

    case AST_TYPE_PTR:
        printf("TYPE_PTR\n");
        ast_print(node->type_ptr.base, indent + 1);
        break;

    case AST_TYPE_ARRAY:
        printf("TYPE_ARRAY\n");
        if (node->type_array.size)
        {
            print_indent(indent + 1);
            printf("size:\n");
            ast_print(node->type_array.size, indent + 2);
        }
        else
        {
            print_indent(indent + 1);
            printf("size: unbound\n");
        }
        print_indent(indent + 1);
        printf("elem:\n");
        ast_print(node->type_array.elem_type, indent + 2);
        break;

    case AST_TYPE_FUN:
        printf("TYPE_FUN\n");
        print_indent(indent + 1);
        printf("params:\n");
        for (int i = 0; i < node->type_fun.params->count; i++)
        {
            ast_print(node->type_fun.params->items[i], indent + 2);
        }
        if (node->type_fun.return_type)
        {
            print_indent(indent + 1);
            printf("returns:\n");
            ast_print(node->type_fun.return_type, indent + 2);
        }
        break;

    case AST_TYPE_STR:
        printf("TYPE_STR");
        if (node->type_str.name)
        {
            printf(" %s", node->type_str.name);
        }
        printf("\n");
        for (int i = 0; i < node->type_str.fields->count; i++)
        {
            ast_print(node->type_str.fields->items[i], indent + 1);
        }
        break;

    case AST_TYPE_UNI:
        printf("TYPE_UNI");
        if (node->type_uni.name)
        {
            printf(" %s", node->type_uni.name);
        }
        printf("\n");
        for (int i = 0; i < node->type_uni.fields->count; i++)
        {
            ast_print(node->type_uni.fields->items[i], indent + 1);
        }
        break;
    }
}
