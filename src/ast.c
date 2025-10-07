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

    // free token if present
    if (node->token)
    {
        token_dnit(node->token);
        free(node->token);
        node->token = NULL;
    }

    switch (node->kind)
    {
    case AST_PROGRAM:
        if (node->program.stmts)
        {
            ast_list_dnit(node->program.stmts);
            free(node->program.stmts);
        }
        break;
    case AST_MODULE:
        free(node->module.name);
        if (node->module.stmts)
        {
            ast_list_dnit(node->module.stmts);
            free(node->module.stmts);
        }
        break;

    case AST_STMT_USE:
        free(node->use_stmt.module_path);
        break;

    case AST_STMT_EXT:
        free(node->ext_stmt.name);
        free(node->ext_stmt.convention);
        free(node->ext_stmt.symbol);
        if (node->ext_stmt.type)
        {
            ast_node_dnit(node->ext_stmt.type);
            free(node->ext_stmt.type);
        }
        break;

    case AST_STMT_DEF:
        free(node->def_stmt.name);
        if (node->def_stmt.type)
        {
            ast_node_dnit(node->def_stmt.type);
            free(node->def_stmt.type);
        }
        break;

    case AST_STMT_VAL:
    case AST_STMT_VAR:
        free(node->var_stmt.name);
        if (node->var_stmt.type)
        {
            ast_node_dnit(node->var_stmt.type);
            free(node->var_stmt.type);
        }
        if (node->var_stmt.init)
        {
            ast_node_dnit(node->var_stmt.init);
            free(node->var_stmt.init);
        }
        break;

    case AST_STMT_FUN:
        free(node->fun_stmt.name);
        if (node->fun_stmt.params)
        {
            ast_list_dnit(node->fun_stmt.params);
            free(node->fun_stmt.params);
        }
        if (node->fun_stmt.return_type)
        {
            ast_node_dnit(node->fun_stmt.return_type);
            free(node->fun_stmt.return_type);
        }
        if (node->fun_stmt.body)
        {
            ast_node_dnit(node->fun_stmt.body);
            free(node->fun_stmt.body);
        }
        break;

    case AST_STMT_STR:
        free(node->str_stmt.name);
        if (node->str_stmt.fields)
        {
            ast_list_dnit(node->str_stmt.fields);
            free(node->str_stmt.fields);
        }
        break;

    case AST_STMT_UNI:
        free(node->uni_stmt.name);
        if (node->uni_stmt.fields)
        {
            ast_list_dnit(node->uni_stmt.fields);
            free(node->uni_stmt.fields);
        }
        break;

    case AST_STMT_FIELD:
        free(node->field_stmt.name);
        if (node->field_stmt.type)
        {
            ast_node_dnit(node->field_stmt.type);
            free(node->field_stmt.type);
        }
        break;

    case AST_STMT_PARAM:
        free(node->param_stmt.name);
        if (node->param_stmt.type)
        {
            ast_node_dnit(node->param_stmt.type);
            free(node->param_stmt.type);
        }
        break;

    case AST_STMT_BLOCK:
        if (node->block_stmt.stmts)
        {
            ast_list_dnit(node->block_stmt.stmts);
            free(node->block_stmt.stmts);
        }
        break;

    case AST_STMT_EXPR:
        if (node->expr_stmt.expr)
        {
            ast_node_dnit(node->expr_stmt.expr);
            free(node->expr_stmt.expr);
        }
        break;

    case AST_STMT_RET:
        if (node->ret_stmt.expr)
        {
            ast_node_dnit(node->ret_stmt.expr);
            free(node->ret_stmt.expr);
        }
        break;

    case AST_STMT_IF:
        if (node->cond_stmt.cond)
        {
            ast_node_dnit(node->cond_stmt.cond);
            free(node->cond_stmt.cond);
        }
        if (node->cond_stmt.body)
        {
            ast_node_dnit(node->cond_stmt.body);
            free(node->cond_stmt.body);
        }
        if (node->cond_stmt.stmt_or)
        {
            ast_node_dnit(node->cond_stmt.stmt_or);
            free(node->cond_stmt.stmt_or);
        }
        break;
    case AST_STMT_OR:
        if (node->cond_stmt.cond)
        {
            ast_node_dnit(node->cond_stmt.cond);
            free(node->cond_stmt.cond);
        }
        if (node->cond_stmt.body)
        {
            ast_node_dnit(node->cond_stmt.body);
            free(node->cond_stmt.body);
        }
        if (node->cond_stmt.stmt_or)
        {
            ast_node_dnit(node->cond_stmt.stmt_or);
            free(node->cond_stmt.stmt_or);
        }
        break;
    case AST_STMT_FOR:
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

    case AST_STMT_BRK:
    case AST_STMT_CNT:
        break;
    case AST_STMT_ASM:
        free(node->asm_stmt.code);
        free(node->asm_stmt.constraints);
        break;

    case AST_EXPR_BINARY:
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

    case AST_EXPR_UNARY:
        if (node->unary_expr.expr)
        {
            ast_node_dnit(node->unary_expr.expr);
            free(node->unary_expr.expr);
        }
        break;

    case AST_EXPR_CALL:
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

    case AST_EXPR_INDEX:
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

    case AST_EXPR_FIELD:
        if (node->field_expr.object)
        {
            ast_node_dnit(node->field_expr.object);
            free(node->field_expr.object);
        }
        free(node->field_expr.field);
        break;

    case AST_EXPR_CAST:
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

    case AST_EXPR_IDENT:
        free(node->ident_expr.name);
        break;

    case AST_EXPR_LIT:
        if (node->lit_expr.kind == TOKEN_LIT_STRING)
        {
            free(node->lit_expr.string_val);
        }
        break;

    case AST_EXPR_ARRAY:
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

    case AST_EXPR_STRUCT:
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
        for (int i = 0; i < node->program.stmts->count; i++)
        {
            ast_print(node->program.stmts->items[i], indent + 1);
        }
        break;
    case AST_MODULE:
        printf("MODULE %s\n", node->module.name);
        for (int i = 0; i < node->module.stmts->count; i++)
        {
            ast_print(node->module.stmts->items[i], indent + 1);
        }
        break;
    case AST_STMT_USE:
        printf("USE %s\n", node->use_stmt.module_path);
        break;

    case AST_STMT_EXT:
        printf("EXT %s:\n", node->ext_stmt.name);
        ast_print(node->ext_stmt.type, indent + 1);
        break;

    case AST_STMT_DEF:
        printf("DEF %s:\n", node->def_stmt.name);
        ast_print(node->def_stmt.type, indent + 1);
        break;

    case AST_STMT_VAL:
        printf("VAL %s:\n", node->var_stmt.name);
        if (node->var_stmt.type)
        {
            ast_print(node->var_stmt.type, indent + 1);
        }
        if (node->var_stmt.init)
        {
            print_indent(indent + 1);
            printf("= \n");
            ast_print(node->var_stmt.init, indent + 2);
        }
        break;

    case AST_STMT_VAR:
        printf("VAR %s:\n", node->var_stmt.name);
        if (node->var_stmt.type)
        {
            ast_print(node->var_stmt.type, indent + 1);
        }
        if (node->var_stmt.init)
        {
            print_indent(indent + 1);
            printf("= \n");
            ast_print(node->var_stmt.init, indent + 2);
        }
        break;

    case AST_STMT_FUN:
        printf("FUN %s\n", node->fun_stmt.name);
        print_indent(indent + 1);
        printf("params:\n");
        for (int i = 0; i < node->fun_stmt.params->count; i++)
        {
            ast_print(node->fun_stmt.params->items[i], indent + 2);
        }
        if (node->fun_stmt.return_type)
        {
            print_indent(indent + 1);
            printf("returns:\n");
            ast_print(node->fun_stmt.return_type, indent + 2);
        }
        if (node->fun_stmt.body)
        {
            print_indent(indent + 1);
            printf("body:\n");
            ast_print(node->fun_stmt.body, indent + 2);
        }
        break;

    case AST_STMT_STR:
        printf("STR %s\n", node->str_stmt.name);
        for (int i = 0; i < node->str_stmt.fields->count; i++)
        {
            ast_print(node->str_stmt.fields->items[i], indent + 1);
        }
        break;
    case AST_STMT_ASM:
        printf("ASM %s\n", node->asm_stmt.code ? node->asm_stmt.code : "");
        break;

    case AST_STMT_UNI:
        printf("UNI %s\n", node->uni_stmt.name);
        for (int i = 0; i < node->uni_stmt.fields->count; i++)
        {
            ast_print(node->uni_stmt.fields->items[i], indent + 1);
        }
        break;

    case AST_STMT_FIELD:
        printf("FIELD %s:\n", node->field_stmt.name);
        ast_print(node->field_stmt.type, indent + 1);
        break;

    case AST_STMT_PARAM:
        printf("PARAM %s:\n", node->param_stmt.name);
        ast_print(node->param_stmt.type, indent + 1);
        break;

    case AST_STMT_BLOCK:
        printf("BLOCK\n");
        for (int i = 0; i < node->block_stmt.stmts->count; i++)
        {
            ast_print(node->block_stmt.stmts->items[i], indent + 1);
        }
        break;

    case AST_STMT_EXPR:
        printf("EXPR_STMT\n");
        ast_print(node->expr_stmt.expr, indent + 1);
        break;

    case AST_STMT_RET:
        printf("RET\n");
        if (node->ret_stmt.expr)
        {
            ast_print(node->ret_stmt.expr, indent + 1);
        }
        break;

    case AST_STMT_IF:
        printf("IF\n");
        print_indent(indent + 1);
        printf("cond:\n");
        ast_print(node->cond_stmt.cond, indent + 2);
        print_indent(indent + 1);
        printf("then:\n");
        ast_print(node->cond_stmt.body, indent + 2);
        if (node->cond_stmt.stmt_or)
        {
            print_indent(indent + 1);
            printf("else:\n");
            ast_print(node->cond_stmt.stmt_or, indent + 2);
        }
        break;

    case AST_STMT_OR:
        printf("OR\n");
        print_indent(indent + 1);
        printf("cond:\n");
        ast_print(node->cond_stmt.cond, indent + 2);
        print_indent(indent + 1);
        printf("then:\n");
        ast_print(node->cond_stmt.body, indent + 2);
        if (node->cond_stmt.stmt_or)
        {
            print_indent(indent + 1);
            printf("or:\n");
            ast_print(node->cond_stmt.stmt_or, indent + 2);
        }
        break;

    case AST_STMT_FOR:
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

    case AST_STMT_BRK:
        printf("BRK\n");
        break;

    case AST_STMT_CNT:
        printf("CNT\n");
        break;

    case AST_EXPR_BINARY:
        printf("BINARY %s\n", token_kind_to_string(node->binary_expr.op));
        ast_print(node->binary_expr.left, indent + 1);
        ast_print(node->binary_expr.right, indent + 1);
        break;

    case AST_EXPR_UNARY:
        printf("UNARY %s\n", token_kind_to_string(node->unary_expr.op));
        ast_print(node->unary_expr.expr, indent + 1);
        break;

    case AST_EXPR_CALL:
        printf("CALL\n");
        ast_print(node->call_expr.func, indent + 1);
        print_indent(indent + 1);
        printf("args:\n");
        for (int i = 0; i < node->call_expr.args->count; i++)
        {
            ast_print(node->call_expr.args->items[i], indent + 2);
        }
        break;

    case AST_EXPR_INDEX:
        printf("INDEX\n");
        ast_print(node->index_expr.array, indent + 1);
        ast_print(node->index_expr.index, indent + 1);
        break;

    case AST_EXPR_FIELD:
        printf("FIELD .%s\n", node->field_expr.field);
        ast_print(node->field_expr.object, indent + 1);
        break;

    case AST_EXPR_CAST:
        printf("CAST\n");
        ast_print(node->cast_expr.expr, indent + 1);
        print_indent(indent + 1);
        printf("to:\n");
        ast_print(node->cast_expr.type, indent + 2);
        break;

    case AST_EXPR_IDENT:
        printf("IDENT %s\n", node->ident_expr.name);
        break;

    case AST_EXPR_LIT:
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

    case AST_EXPR_ARRAY:
        printf("ARRAY\n");
        ast_print(node->array_expr.type, indent + 1);
        for (int i = 0; i < node->array_expr.elems->count; i++)
        {
            ast_print(node->array_expr.elems->items[i], indent + 1);
        }
        break;

    case AST_EXPR_STRUCT:
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

const char *ast_node_kind_to_string(AstKind kind)
{
    switch (kind)
    {
    case AST_PROGRAM:
        return "PROGRAM";
    case AST_MODULE:
        return "MODULE";
    case AST_STMT_USE:
        return "USE";
    case AST_STMT_EXT:
        return "EXT";
    case AST_STMT_DEF:
        return "DEF";
    case AST_STMT_VAL:
        return "VAL";
    case AST_STMT_VAR:
        return "VAR";
    case AST_STMT_FUN:
        return "FUN";
    case AST_STMT_STR:
        return "STR";
    case AST_STMT_UNI:
        return "UNI";
    case AST_STMT_FIELD:
        return "FIELD";
    case AST_STMT_PARAM:
        return "PARAM";
    case AST_STMT_BLOCK:
        return "BLOCK";
    case AST_STMT_EXPR:
        return "EXPR_STMT";
    case AST_STMT_ASM:
        return "ASM";
    case AST_STMT_RET:
        return "RET";
    case AST_STMT_IF:
        return "IF";
    case AST_STMT_OR:
        return "OR";
    case AST_STMT_FOR:
        return "FOR";
    case AST_STMT_BRK:
        return "BRK";
    case AST_STMT_CNT:
        return "CNT";

    case AST_EXPR_BINARY:
        return "BINARY_EXPR";
    case AST_EXPR_UNARY:
        return "UNARY_EXPR";
    case AST_EXPR_CALL:
        return "CALL_EXPR";
    case AST_EXPR_INDEX:
        return "INDEX_EXPR";
    case AST_EXPR_FIELD:
        return "FIELD_EXPR";
    case AST_EXPR_CAST:
        return "CAST_EXPR";
    case AST_EXPR_IDENT:
        return "IDENT_EXPR";
    case AST_EXPR_LIT:
        return "LIT_EXPR";
    case AST_EXPR_ARRAY:
        return "ARRAY_EXPR";
    case AST_EXPR_STRUCT:
        return "STRUCT_LIT";

    case AST_TYPE_NAME:
        return "TYPE_NAME";
    case AST_TYPE_PTR:
        return "TYPE_PTR";
    case AST_TYPE_ARRAY:
        return "TYPE_ARRAY";
    case AST_TYPE_FUN:
        return "TYPE_FUN";
    case AST_TYPE_STR:
        return "TYPE_STR";
    case AST_TYPE_UNI:
        return "TYPE_UNI";

    default:
        return "UNKNOWN";
    }
}

// helper for printing to file
static void print_indent_to_file(FILE *file, int indent)
{
    for (int i = 0; i < indent; i++)
    {
        fprintf(file, "  ");
    }
}

static void ast_print_to_file(AstNode *node, FILE *file, int indent)
{
    if (!node)
    {
        print_indent_to_file(file, indent);
        fprintf(file, "(null)\n");
        return;
    }

    print_indent_to_file(file, indent);

    switch (node->kind)
    {
    case AST_PROGRAM:
        fprintf(file, "PROGRAM\n");
        for (int i = 0; i < node->program.stmts->count; i++)
        {
            ast_print_to_file(node->program.stmts->items[i], file, indent + 1);
        }
        break;
    case AST_MODULE:
        fprintf(file, "MODULE %s\n", node->module.name);
        for (int i = 0; i < node->module.stmts->count; i++)
        {
            ast_print_to_file(node->module.stmts->items[i], file, indent + 1);
        }
        break;
    case AST_STMT_USE:
        fprintf(file, "USE %s\n", node->use_stmt.module_path);
        break;
    case AST_STMT_EXT:
        fprintf(file, "EXT %s:\n", node->ext_stmt.name);
        ast_print_to_file(node->ext_stmt.type, file, indent + 1);
        break;
    case AST_STMT_DEF:
        fprintf(file, "DEF %s:\n", node->def_stmt.name);
        ast_print_to_file(node->def_stmt.type, file, indent + 1);
        break;
    case AST_STMT_VAL:
        fprintf(file, "VAL %s:\n", node->var_stmt.name);
        if (node->var_stmt.type)
        {
            ast_print_to_file(node->var_stmt.type, file, indent + 1);
        }
        if (node->var_stmt.init)
        {
            print_indent_to_file(file, indent + 1);
            fprintf(file, "= \n");
            ast_print_to_file(node->var_stmt.init, file, indent + 2);
        }
        break;
    case AST_STMT_VAR:
        fprintf(file, "VAR %s:\n", node->var_stmt.name);
        if (node->var_stmt.type)
        {
            ast_print_to_file(node->var_stmt.type, file, indent + 1);
        }
        if (node->var_stmt.init)
        {
            print_indent_to_file(file, indent + 1);
            fprintf(file, "= \n");
            ast_print_to_file(node->var_stmt.init, file, indent + 2);
        }
        break;
    case AST_STMT_FUN:
        fprintf(file, "FUN %s\n", node->fun_stmt.name);
        print_indent_to_file(file, indent + 1);
        fprintf(file, "params:\n");
        for (int i = 0; i < node->fun_stmt.params->count; i++)
        {
            ast_print_to_file(node->fun_stmt.params->items[i], file, indent + 2);
        }
        if (node->fun_stmt.return_type)
        {
            print_indent_to_file(file, indent + 1);
            fprintf(file, "returns:\n");
            ast_print_to_file(node->fun_stmt.return_type, file, indent + 2);
        }
        if (node->fun_stmt.body)
        {
            print_indent_to_file(file, indent + 1);
            fprintf(file, "body:\n");
            ast_print_to_file(node->fun_stmt.body, file, indent + 2);
        }
        break;
    case AST_STMT_STR:
        fprintf(file, "STR %s\n", node->str_stmt.name);
        for (int i = 0; i < node->str_stmt.fields->count; i++)
        {
            ast_print_to_file(node->str_stmt.fields->items[i], file, indent + 1);
        }
        break;
    case AST_STMT_UNI:
        fprintf(file, "UNI %s\n", node->uni_stmt.name);
        for (int i = 0; i < node->uni_stmt.fields->count; i++)
        {
            ast_print_to_file(node->uni_stmt.fields->items[i], file, indent + 1);
        }
        break;
    case AST_STMT_FIELD:
        fprintf(file, "FIELD %s:\n", node->field_stmt.name);
        ast_print_to_file(node->field_stmt.type, file, indent + 1);
        break;
    case AST_STMT_PARAM:
        fprintf(file, "PARAM %s:\n", node->param_stmt.name);
        ast_print_to_file(node->param_stmt.type, file, indent + 1);
        break;
    case AST_STMT_BLOCK:
        fprintf(file, "BLOCK\n");
        for (int i = 0; i < node->block_stmt.stmts->count; i++)
        {
            ast_print_to_file(node->block_stmt.stmts->items[i], file, indent + 1);
        }
        break;
    case AST_STMT_EXPR:
        fprintf(file, "EXPR_STMT\n");
        ast_print_to_file(node->expr_stmt.expr, file, indent + 1);
        break;
    case AST_STMT_ASM:
        fprintf(file, "ASM %s\n", node->asm_stmt.code ? node->asm_stmt.code : "");
        break;
    case AST_STMT_RET:
        fprintf(file, "RET\n");
        if (node->ret_stmt.expr)
        {
            ast_print_to_file(node->ret_stmt.expr, file, indent + 1);
        }
        break;
    case AST_STMT_IF:
        fprintf(file, "IF\n");
        print_indent_to_file(file, indent + 1);
        fprintf(file, "cond:\n");
        ast_print_to_file(node->cond_stmt.cond, file, indent + 2);
        print_indent_to_file(file, indent + 1);
        fprintf(file, "then:\n");
        ast_print_to_file(node->cond_stmt.body, file, indent + 2);
        if (node->cond_stmt.stmt_or)
        {
            print_indent_to_file(file, indent + 1);
            fprintf(file, "else:\n");
            ast_print_to_file(node->cond_stmt.stmt_or, file, indent + 2);
        }
        break;
    case AST_STMT_OR:
        fprintf(file, "OR\n");
        print_indent_to_file(file, indent + 1);
        fprintf(file, "cond:\n");
        ast_print_to_file(node->cond_stmt.cond, file, indent + 2);
        print_indent_to_file(file, indent + 1);
        fprintf(file, "then:\n");
        ast_print_to_file(node->cond_stmt.body, file, indent + 2);
        if (node->cond_stmt.stmt_or)
        {
            print_indent_to_file(file, indent + 1);
            fprintf(file, "or:\n");
            ast_print_to_file(node->cond_stmt.stmt_or, file, indent + 2);
        }
        break;
    case AST_STMT_FOR:
        fprintf(file, "FOR\n");
        if (node->for_stmt.cond)
        {
            print_indent_to_file(file, indent + 1);
            fprintf(file, "cond:\n");
            ast_print_to_file(node->for_stmt.cond, file, indent + 2);
        }
        print_indent_to_file(file, indent + 1);
        fprintf(file, "body:\n");
        ast_print_to_file(node->for_stmt.body, file, indent + 2);
        break;
    case AST_STMT_BRK:
        fprintf(file, "BRK\n");
        break;
    case AST_STMT_CNT:
        fprintf(file, "CNT\n");
        break;
    case AST_EXPR_BINARY:
        fprintf(file, "BINARY %s\n", token_kind_to_string(node->binary_expr.op));
        ast_print_to_file(node->binary_expr.left, file, indent + 1);
        ast_print_to_file(node->binary_expr.right, file, indent + 1);
        break;
    case AST_EXPR_UNARY:
        fprintf(file, "UNARY %s\n", token_kind_to_string(node->unary_expr.op));
        ast_print_to_file(node->unary_expr.expr, file, indent + 1);
        break;
    case AST_EXPR_CALL:
        fprintf(file, "CALL\n");
        ast_print_to_file(node->call_expr.func, file, indent + 1);
        print_indent_to_file(file, indent + 1);
        fprintf(file, "args:\n");
        for (int i = 0; i < node->call_expr.args->count; i++)
        {
            ast_print_to_file(node->call_expr.args->items[i], file, indent + 2);
        }
        break;
    case AST_EXPR_INDEX:
        fprintf(file, "INDEX\n");
        ast_print_to_file(node->index_expr.array, file, indent + 1);
        ast_print_to_file(node->index_expr.index, file, indent + 1);
        break;
    case AST_EXPR_FIELD:
        fprintf(file, "FIELD .%s\n", node->field_expr.field);
        ast_print_to_file(node->field_expr.object, file, indent + 1);
        break;
    case AST_EXPR_CAST:
        fprintf(file, "CAST\n");
        ast_print_to_file(node->cast_expr.expr, file, indent + 1);
        print_indent_to_file(file, indent + 1);
        fprintf(file, "to:\n");
        ast_print_to_file(node->cast_expr.type, file, indent + 2);
        break;
    case AST_EXPR_IDENT:
        fprintf(file, "IDENT %s\n", node->ident_expr.name);
        break;
    case AST_EXPR_LIT:
        fprintf(file, "LIT ");
        switch (node->lit_expr.kind)
        {
        case TOKEN_LIT_INT:
            fprintf(file, "INT %llu\n", node->lit_expr.int_val);
            break;
        case TOKEN_LIT_FLOAT:
            fprintf(file, "FLOAT %f\n", node->lit_expr.float_val);
            break;
        case TOKEN_LIT_CHAR:
            fprintf(file, "CHAR '%c'\n", node->lit_expr.char_val);
            break;
        case TOKEN_LIT_STRING:
            fprintf(file, "STRING \"%s\"\n", node->lit_expr.string_val);
            break;
        default:
            fprintf(file, "???\n");
        }
        break;
    case AST_EXPR_ARRAY:
        fprintf(file, "ARRAY\n");
        ast_print_to_file(node->array_expr.type, file, indent + 1);
        for (int i = 0; i < node->array_expr.elems->count; i++)
        {
            ast_print_to_file(node->array_expr.elems->items[i], file, indent + 1);
        }
        break;
    case AST_EXPR_STRUCT:
        fprintf(file, "STRUCT_LIT\n");
        ast_print_to_file(node->struct_expr.type, file, indent + 1);
        for (int i = 0; i < node->struct_expr.fields->count; i++)
        {
            ast_print_to_file(node->struct_expr.fields->items[i], file, indent + 1);
        }
        break;
    case AST_TYPE_NAME:
        fprintf(file, "TYPE %s\n", node->type_name.name);
        break;
    case AST_TYPE_PTR:
        fprintf(file, "TYPE_PTR\n");
        ast_print_to_file(node->type_ptr.base, file, indent + 1);
        break;
    case AST_TYPE_ARRAY:
        fprintf(file, "TYPE_ARRAY\n");
        if (node->type_array.size)
        {
            print_indent_to_file(file, indent + 1);
            fprintf(file, "size:\n");
            ast_print_to_file(node->type_array.size, file, indent + 2);
        }
        else
        {
            print_indent_to_file(file, indent + 1);
            fprintf(file, "size: unbound\n");
        }
        print_indent_to_file(file, indent + 1);
        fprintf(file, "elem:\n");
        ast_print_to_file(node->type_array.elem_type, file, indent + 2);
        break;
    case AST_TYPE_FUN:
        fprintf(file, "TYPE_FUN\n");
        print_indent_to_file(file, indent + 1);
        fprintf(file, "params:\n");
        for (int i = 0; i < node->type_fun.params->count; i++)
        {
            ast_print_to_file(node->type_fun.params->items[i], file, indent + 2);
        }
        if (node->type_fun.return_type)
        {
            print_indent_to_file(file, indent + 1);
            fprintf(file, "returns:\n");
            ast_print_to_file(node->type_fun.return_type, file, indent + 2);
        }
        break;
    case AST_TYPE_STR:
        fprintf(file, "TYPE_STR");
        if (node->type_str.name)
        {
            fprintf(file, " %s", node->type_str.name);
        }
        fprintf(file, "\n");
        for (int i = 0; i < node->type_str.fields->count; i++)
        {
            ast_print_to_file(node->type_str.fields->items[i], file, indent + 1);
        }
        break;
    case AST_TYPE_UNI:
        fprintf(file, "TYPE_UNI");
        if (node->type_uni.name)
        {
            fprintf(file, " %s", node->type_uni.name);
        }
        fprintf(file, "\n");
        for (int i = 0; i < node->type_uni.fields->count; i++)
        {
            ast_print_to_file(node->type_uni.fields->items[i], file, indent + 1);
        }
        break;
    }
}

bool ast_emit(AstNode *node, const char *file_path)
{
    if (!node || !file_path)
        return false;

    FILE *file = fopen(file_path, "w");
    if (!file)
    {
        perror("failed to open AST file for writing");
        return false;
    }

    ast_print_to_file(node, file, 0);

    fclose(file);
    return true;
}
