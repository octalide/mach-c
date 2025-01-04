#include "ast.h"

#include <stdio.h>
#include <stdlib.h>

Node *node_new(NodeKind kind)
{
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;

    switch (kind)
    {
    case NODE_ERROR:
        node->data.error = calloc(1, sizeof(NodeError));
        break;
    case NODE_PROGRAM:
        node->data.program = calloc(1, sizeof(NodeProgram));
        node->data.program->modules = node_list_new();
        break;
    case NODE_MODULE:
        node->data.module = calloc(1, sizeof(NodeModule));
        node->data.module->files = node_list_new();
        break;
    case NODE_FILE:
        node->data.file = calloc(1, sizeof(NodeFile));
        node->data.file->statements = node_list_new();
        break;
    case NODE_IDENTIFIER:
        node->data.identifier = calloc(1, sizeof(NodeIdentifier));
        break;
    case NODE_LIT_INT:
        node->data.lit_int = calloc(1, sizeof(NodeLitInt));
        node->data.lit_int->value = 0;
        break;
    case NODE_LIT_FLOAT:
        node->data.lit_float = calloc(1, sizeof(NodeLitFloat));
        node->data.lit_float->value = 0;
        break;
    case NODE_LIT_CHAR:
        node->data.lit_char = calloc(1, sizeof(NodeLitChar));
        node->data.lit_char->value = 0;
        break;
    case NODE_LIT_STRING:
        node->data.lit_string = calloc(1, sizeof(NodeLitString));
        break;
    case NODE_EXPR_MEMBER:
        node->data.expr_member = calloc(1, sizeof(NodeExprMember));
        break;
    case NODE_EXPR_CALL:
        node->data.expr_call = calloc(1, sizeof(NodeExprCall));
        node->data.expr_call->arguments = node_list_new();
        break;
    case NODE_EXPR_INDEX:
        node->data.expr_index = calloc(1, sizeof(NodeExprIndex));
        break;
    case NODE_EXPR_CAST:
        node->data.expr_cast = calloc(1, sizeof(NodeExprCast));
        break;
    case NODE_EXPR_UNARY:
        node->data.expr_unary = calloc(1, sizeof(NodeExprUnary));
        break;
    case NODE_EXPR_BINARY:
        node->data.expr_binary = calloc(1, sizeof(NodeExprBinary));
        break;
    case NODE_TYPE_ARRAY:
        node->data.type_array = calloc(1, sizeof(NodeTypeArray));
        break;
    case NODE_TYPE_POINTER:
        node->data.type_pointer = calloc(1, sizeof(NodeTypePointer));
        break;
    case NODE_TYPE_FUN:
        node->data.type_function = calloc(1, sizeof(NodeTypeFunction));
        node->data.type_function->parameters = node_list_new();
        break;
    case NODE_TYPE_STR:
        node->data.type_struct = calloc(1, sizeof(NodeTypeStruct));
        node->data.type_struct->fields = node_list_new();
        break;
    case NODE_TYPE_UNI:
        node->data.type_union = calloc(1, sizeof(NodeTypeUnion));
        node->data.type_union->fields = node_list_new();
        break;
    case NODE_FIELD:
        node->data.field = calloc(1, sizeof(NodeField));
        break;
    case NODE_STMT_VAL:
        node->data.stmt_val = calloc(1, sizeof(NodeStmtVal));
        break;
    case NODE_STMT_VAR:
        node->data.stmt_var = calloc(1, sizeof(NodeStmtVar));
        break;
    case NODE_STMT_MOD:
        node->data.stmt_mod = calloc(1, sizeof(NodeStmtMod));
        break;
    case NODE_STMT_DEF:
        node->data.stmt_def = calloc(1, sizeof(NodeStmtDef));
        break;
    case NODE_STMT_USE:
        node->data.stmt_use = calloc(1, sizeof(NodeStmtUse));
        break;
    case NODE_STMT_STR:
        node->data.stmt_str = calloc(1, sizeof(NodeStmtStruct));
        node->data.stmt_str->fields = node_list_new();
        break;
    case NODE_STMT_UNI:
        node->data.stmt_uni = calloc(1, sizeof(NodeStmtUnion));
        node->data.stmt_uni->fields = node_list_new();
        break;
    case NODE_STMT_FUN:
        node->data.stmt_fun = calloc(1, sizeof(NodeStmtFun));
        node->data.stmt_fun->parameters = node_list_new();
        break;
    case NODE_STMT_EXT:
        node->data.stmt_ext = calloc(1, sizeof(NodeStmtExt));
        break;
    case NODE_STMT_IF:
        node->data.stmt_if = calloc(1, sizeof(NodeStmtIf));
        break;
    case NODE_STMT_OR:
        node->data.stmt_or = calloc(1, sizeof(NodeStmtOr));
        break;
    case NODE_STMT_FOR:
        node->data.stmt_for = calloc(1, sizeof(NodeStmtFor));
        break;
    case NODE_STMT_BRK:
        node->data.stmt_break = calloc(1, sizeof(NodeStmtBreak));
        break;
    case NODE_STMT_CNT:
        node->data.stmt_continue = calloc(1, sizeof(NodeStmtContinue));
        break;
    case NODE_STMT_RET:
        node->data.stmt_return = calloc(1, sizeof(NodeStmtReturn));
        break;
    case NODE_STMT_ASM:
        node->data.stmt_asm = calloc(1, sizeof(NodeStmtAsm));
        break;
    case NODE_STMT_BLOCK:
        node->data.stmt_block = calloc(1, sizeof(NodeStmtBlock));
        node->data.stmt_block->statements = node_list_new();
        break;
    case NODE_STMT_EXPR:
        node->data.stmt_expr = calloc(1, sizeof(NodeStmtExpr));
        break;
    default:
        break;
    }

    return node;
}

void node_free(Node *node)
{
    if (node == NULL)
    {
        return;
    }

    switch (node->kind)
    {
    case NODE_ERROR:
        free(node->data.error->message);
        node->data.error->message = NULL;
       
        free(node->data.error);
        node->data.error = NULL;
        
        break;
    case NODE_PROGRAM:
        node_list_free(node->data.program->modules);
        node->data.program->modules = NULL;
        free(node->data.program->name);
        node->data.program->name = NULL;
        free(node->data.program);
        node->data.program = NULL;
        break;
    case NODE_MODULE:
        free(node->data.module->name);
        node->data.module->name = NULL;
        node_list_free(node->data.module->files);
        node->data.module->files = NULL;
        free(node->data.module);
        node->data.module = NULL;
        break;
    case NODE_FILE:
        free(node->data.file->path);
        node->data.file->path = NULL;
        // parser_free(node->data.file->parser); // NOTE: not owned by node
        node->data.file->parser = NULL;
        node_list_free(node->data.file->statements);
        node->data.file->statements = NULL;
        free(node->data.file);
        node->data.file = NULL;
        break;
    case NODE_IDENTIFIER:
        free(node->data.identifier->name);
        node->data.identifier->name = NULL;
        free(node->data.identifier);
        node->data.identifier = NULL;
        break;
    case NODE_LIT_INT:
        free(node->data.lit_int);
        node->data.lit_int = NULL;
        break;
    case NODE_LIT_FLOAT:
        free(node->data.lit_float);
        node->data.lit_float = NULL;
        break;
    case NODE_LIT_CHAR:
        free(node->data.lit_char);
        node->data.lit_char = NULL;
        break;
    case NODE_LIT_STRING:
        free(node->data.lit_string->value);
        node->data.lit_string->value = NULL;
        free(node->data.lit_string);
        node->data.lit_string = NULL;
        break;
    case NODE_EXPR_MEMBER:
        node_free(node->data.expr_member->target);
        node->data.expr_member->target = NULL;
        node_free(node->data.expr_member->member);
        node->data.expr_member->member = NULL;
        free(node->data.expr_member);
        node->data.expr_member = NULL;
        break;
    case NODE_EXPR_CALL:
        node_free(node->data.expr_call->target);
        node->data.expr_call->target = NULL;
        node_list_free(node->data.expr_call->arguments);
        node->data.expr_call->arguments = NULL;
        free(node->data.expr_call);
        node->data.expr_call = NULL;
        break;
    case NODE_EXPR_INDEX:
        node_free(node->data.expr_index->target);
        node->data.expr_index->target = NULL;
        node_free(node->data.expr_index->index);
        node->data.expr_index->index = NULL;
        free(node->data.expr_index);
        node->data.expr_index = NULL;
        break;
    case NODE_EXPR_CAST:
        node_free(node->data.expr_cast->target);
        node->data.expr_cast->target = NULL;
        node_free(node->data.expr_cast->type);
        node->data.expr_cast->type = NULL;
        free(node->data.expr_cast);
        node->data.expr_cast = NULL;
        break;
    case NODE_EXPR_UNARY:
        node_free(node->data.expr_unary->target);
        node->data.expr_unary->target = NULL;
        free(node->data.expr_unary);
        node->data.expr_unary = NULL;
        break;
    case NODE_EXPR_BINARY:
        node_free(node->data.expr_binary->left);
        node->data.expr_binary->left = NULL;
        node_free(node->data.expr_binary->right);
        node->data.expr_binary->right = NULL;
        free(node->data.expr_binary);
        node->data.expr_binary = NULL;
        break;
    case NODE_TYPE_ARRAY:
        node_free(node->data.type_array->type);
        node->data.type_array->type = NULL;
        node_free(node->data.type_array->size);
        node->data.type_array->size = NULL;
        free(node->data.type_array);
        node->data.type_array = NULL;
        break;
    case NODE_TYPE_POINTER:
        node_free(node->data.type_pointer->type);
        node->data.type_pointer->type = NULL;
        free(node->data.type_pointer);
        node->data.type_pointer = NULL;
        break;
    case NODE_TYPE_FUN:
        node_free(node->data.type_function->return_type);
        node->data.type_function->return_type = NULL;
        node_list_free(node->data.type_function->parameters);
        node->data.type_function->parameters = NULL;
        free(node->data.type_function);
        node->data.type_function = NULL;
        break;
    case NODE_TYPE_STR:
        node_list_free(node->data.type_struct->fields);
        node->data.type_struct->fields = NULL;
        free(node->data.type_struct);
        node->data.type_struct = NULL;
        break;
    case NODE_TYPE_UNI:
        node_list_free(node->data.type_union->fields);
        node->data.type_union->fields = NULL;
        free(node->data.type_union);
        node->data.type_union = NULL;
        break;
    case NODE_FIELD:
        node_free(node->data.field->identifier);
        node->data.field->identifier = NULL;
        node_free(node->data.field->type);
        node->data.field->type = NULL;
        free(node->data.field);
        node->data.field = NULL;
        break;
    case NODE_STMT_VAL:
        node_free(node->data.stmt_val->identifier);
        node->data.stmt_val->identifier = NULL;
        node_free(node->data.stmt_val->type);
        node->data.stmt_val->type = NULL;
        node_free(node->data.stmt_val->initializer);
        node->data.stmt_val->initializer = NULL;
        free(node->data.stmt_val);
        node->data.stmt_val = NULL;
        break;
    case NODE_STMT_VAR:
        node_free(node->data.stmt_var->identifier);
        node->data.stmt_var->identifier = NULL;
        node_free(node->data.stmt_var->type);
        node->data.stmt_var->type = NULL;
        node_free(node->data.stmt_var->initializer);
        node->data.stmt_var->initializer = NULL;
        free(node->data.stmt_var);
        node->data.stmt_var = NULL;
        break;
    case NODE_STMT_DEF:
        node_free(node->data.stmt_def->identifier);
        node->data.stmt_def->identifier = NULL;
        node_free(node->data.stmt_def->type);
        node->data.stmt_def->type = NULL;
        free(node->data.stmt_def);
        node->data.stmt_def = NULL;
        break;
    case NODE_STMT_MOD:
        node_free(node->data.stmt_mod->module_path);
        node->data.stmt_mod->module_path = NULL;
        free(node->data.stmt_mod);
        node->data.stmt_mod = NULL;
        break;
    case NODE_STMT_USE:
        node_free(node->data.stmt_use->alias);
        node->data.stmt_use->alias = NULL;
        node_free(node->data.stmt_use->module_path);
        node->data.stmt_use->module_path = NULL;
        free(node->data.stmt_use);
        node->data.stmt_use = NULL;
        break;
    case NODE_STMT_STR:
        node_free(node->data.stmt_str->identifier);
        node->data.stmt_str->identifier = NULL;
        node_list_free(node->data.stmt_str->fields);
        node->data.stmt_str->fields = NULL;
        free(node->data.stmt_str);
        node->data.stmt_str = NULL;
        break;
    case NODE_STMT_UNI:
        node_free(node->data.stmt_uni->identifier);
        node->data.stmt_uni->identifier = NULL;
        node_list_free(node->data.stmt_uni->fields);
        node->data.stmt_uni->fields = NULL;
        free(node->data.stmt_uni);
        node->data.stmt_uni = NULL;
        break;
    case NODE_STMT_FUN:
        node_free(node->data.stmt_fun->identifier);
        node->data.stmt_fun->identifier = NULL;
        node_free(node->data.stmt_fun->return_type);
        node->data.stmt_fun->return_type = NULL;
        node_list_free(node->data.stmt_fun->parameters);
        node->data.stmt_fun->parameters = NULL;
        node_free(node->data.stmt_fun->body);
        node->data.stmt_fun->body = NULL;
        free(node->data.stmt_fun);
        node->data.stmt_fun = NULL;
        break;
    case NODE_STMT_EXT:
        node_free(node->data.stmt_ext->identifier);
        node->data.stmt_ext->identifier = NULL;
        node_free(node->data.stmt_ext->type);
        node->data.stmt_ext->type = NULL;
        free(node->data.stmt_ext);
        node->data.stmt_ext = NULL;
        break;
    case NODE_STMT_IF:
        node_free(node->data.stmt_if->condition);
        node->data.stmt_if->condition = NULL;
        node_free(node->data.stmt_if->body);
        node->data.stmt_if->body = NULL;
        free(node->data.stmt_if);
        node->data.stmt_if = NULL;
        break;
    case NODE_STMT_OR:
        node_free(node->data.stmt_or->condition);
        node->data.stmt_or->condition = NULL;
        node_free(node->data.stmt_or->body);
        node->data.stmt_or->body = NULL;
        free(node->data.stmt_or);
        node->data.stmt_or = NULL;
        break;
    case NODE_STMT_FOR:
        node_free(node->data.stmt_for->condition);
        node->data.stmt_for->condition = NULL;
        node_free(node->data.stmt_for->body);
        node->data.stmt_for->body = NULL;
        free(node->data.stmt_for);
        node->data.stmt_for = NULL;
        break;
    case NODE_STMT_BRK:
        free(node->data.stmt_break);
        node->data.stmt_break = NULL;
        break;
    case NODE_STMT_CNT:
        free(node->data.stmt_continue);
        node->data.stmt_continue = NULL;
        break;
    case NODE_STMT_RET:
        node_free(node->data.stmt_return->value);
        node->data.stmt_return->value = NULL;
        free(node->data.stmt_return);
        node->data.stmt_return = NULL;
        break;
    case NODE_STMT_ASM:
        node_free(node->data.stmt_asm->code);
        node->data.stmt_asm->code = NULL;
        free(node->data.stmt_asm);
        node->data.stmt_asm = NULL;
        break;
    case NODE_STMT_BLOCK:
        node_list_free(node->data.stmt_block->statements);
        node->data.stmt_block->statements = NULL;
        free(node->data.stmt_block);
        node->data.stmt_block = NULL;
        break;
    case NODE_STMT_EXPR:
        node_free(node->data.stmt_expr->expression);
        node->data.stmt_expr->expression = NULL;
        free(node->data.stmt_expr);
        node->data.stmt_expr = NULL;
        break;
    }

    token_free(node->token);
    node->token = NULL;

    free(node);
}

char *node_kind_to_string(NodeKind kind)
{
    switch (kind)
    {
    case NODE_ERROR:
        return "ERROR";
    case NODE_PROGRAM:
        return "PROGRAM";
    case NODE_MODULE:
        return "MODULE";
    case NODE_FILE:
        return "FILE";
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
    case NODE_STMT_MOD:
        return "STMT_MOD";
    case NODE_STMT_USE:
        return "STMT_USE";
    case NODE_STMT_STR:
        return "STMT_STR";
    case NODE_STMT_UNI:
        return "STMT_UNI";
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

Node **node_list_new()
{
    Node **list = calloc(1, sizeof(Node *));

    return list;
}

int node_list_count(Node **list)
{
    if (list == NULL)
    {
        return 0;
    }

    int count = 0;
    while (list[count] != NULL)
    {
        count++;
    }

    return count;
}

void node_list_add(Node ***list, Node *node)
{
    int count = node_list_count(*list);
    Node **new_list = realloc(*list, (count + 2) * sizeof(Node *));
    new_list[count + 1] = NULL;
    new_list[count] = node;
    *list = new_list;
}

void node_list_free(Node **list)
{
    int count = node_list_count(list);
    for (int i = 0; i < count - 1; i++)
    {
        node_free(list[i]);
        list[i] = NULL;
    }
    free(list);
}

void node_walk(void *context, Node *node, void (*callback)(void *context, Node *node, int depth))
{
    static int depth = 0;

    if (node == NULL)
    {
        return;
    }

    callback(context, node, depth);

    switch (node->kind)
    {
    case NODE_ERROR:
        break;
    case NODE_PROGRAM:
        depth++;
        for (int i = 0; i < node_list_count(node->data.program->modules); i++)
        {
            node_walk(context, node->data.program->modules[i], callback);
        }
        depth--;
        break;
    case NODE_MODULE:
        depth++;
        for (int i = 0; i < node_list_count(node->data.module->files); i++)
        {
            node_walk(context, node->data.module->files[i], callback);
        }
        depth--;
        break;
    case NODE_FILE:
        depth++;
        for (int i = 0; i < node_list_count(node->data.file->statements); i++)
        {
            node_walk(context, node->data.file->statements[i], callback);
        }
        depth--;
        break;
    case NODE_IDENTIFIER:
    case NODE_LIT_INT:
    case NODE_LIT_FLOAT:
    case NODE_LIT_CHAR:
    case NODE_LIT_STRING:
        break;
    case NODE_EXPR_MEMBER:
        depth++;
        node_walk(context, node->data.expr_member->target, callback);
        node_walk(context, node->data.expr_member->member, callback);
        depth--;
        break;
    case NODE_EXPR_CALL:
        depth++;
        node_walk(context, node->data.expr_call->target, callback);
        for (int i = 0; i < node_list_count(node->data.expr_call->arguments); i++)
        {
            node_walk(context, node->data.expr_call->arguments[i], callback);
        }
        depth--;
        break;
    case NODE_EXPR_INDEX:
        depth++;
        node_walk(context, node->data.expr_index->target, callback);
        node_walk(context, node->data.expr_index->index, callback);
        depth--;
        break;
    case NODE_EXPR_CAST:
        depth++;
        node_walk(context, node->data.expr_cast->target, callback);
        node_walk(context, node->data.expr_cast->type, callback);
        depth--;
        break;
    case NODE_EXPR_UNARY:
        depth++;
        node_walk(context, node->data.expr_unary->target, callback);
        depth--;
        break;
    case NODE_EXPR_BINARY:
        depth++;
        node_walk(context, node->data.expr_binary->left, callback);
        node_walk(context, node->data.expr_binary->right, callback);
        depth--;
        break;
    case NODE_TYPE_ARRAY:
        depth++;
        node_walk(context, node->data.type_array->type, callback);
        node_walk(context, node->data.type_array->size, callback);
        depth--;
        break;
    case NODE_TYPE_POINTER:
        depth++;
        node_walk(context, node->data.type_pointer->type, callback);
        depth--;
        break;
    case NODE_TYPE_FUN:
        depth++;
        node_walk(context, node->data.type_function->return_type, callback);
        for (int i = 0; i < node_list_count(node->data.type_function->parameters); i++)
        {
            node_walk(context, node->data.type_function->parameters[i], callback);
        }
        depth--;
        break;
    case NODE_TYPE_STR:
        for (int i = 0; i < node_list_count(node->data.type_struct->fields); i++)
        {
            depth++;
            node_walk(context, node->data.type_struct->fields[i], callback);
            depth--;
        }
        break;
    case NODE_TYPE_UNI:
        for (int i = 0; i < node_list_count(node->data.type_union->fields); i++)
        {
            depth++;
            node_walk(context, node->data.type_union->fields[i], callback);
            depth--;
        }
        break;
    case NODE_FIELD:
        depth++;
        node_walk(context, node->data.field->identifier, callback);
        node_walk(context, node->data.field->type, callback);
        depth--;
        break;
    case NODE_STMT_VAL:
        depth++;
        node_walk(context, node->data.stmt_val->identifier, callback);
        node_walk(context, node->data.stmt_val->type, callback);
        node_walk(context, node->data.stmt_val->initializer, callback);
        depth--;
        break;
    case NODE_STMT_VAR:
        depth++;
        node_walk(context, node->data.stmt_var->identifier, callback);
        node_walk(context, node->data.stmt_var->type, callback);
        node_walk(context, node->data.stmt_var->initializer, callback);
        depth--;
        break;
    case NODE_STMT_DEF:
        depth++;
        node_walk(context, node->data.stmt_def->identifier, callback);
        node_walk(context, node->data.stmt_def->type, callback);
        depth--;
        break;
    case NODE_STMT_MOD:
        depth++;
        node_walk(context, node->data.stmt_mod->module_path, callback);
        depth--;
        break;
    case NODE_STMT_USE:
        depth++;
        node_walk(context, node->data.stmt_use->alias, callback);
        node_walk(context, node->data.stmt_use->module_path, callback);
        depth--;
        break;
    case NODE_STMT_STR:
        depth++;
        node_walk(context, node->data.stmt_str->identifier, callback);
        for (int i = 0; i < node_list_count(node->data.stmt_str->fields); i++)
        {
            node_walk(context, node->data.stmt_str->fields[i], callback);
        }
        depth--;
        break;
    case NODE_STMT_UNI:
        depth++;
        node_walk(context, node->data.stmt_uni->identifier, callback);
        for (int i = 0; i < node_list_count(node->data.stmt_uni->fields); i++)
        {
            node_walk(context, node->data.stmt_uni->fields[i], callback);
        }
        depth--;
        break;
    case NODE_STMT_FUN:
        depth++;
        node_walk(context, node->data.stmt_fun->identifier, callback);
        node_walk(context, node->data.stmt_fun->return_type, callback);
        for (int i = 0; i < node_list_count(node->data.stmt_fun->parameters); i++)
        {
            node_walk(context, node->data.stmt_fun->parameters[i], callback);
        }
        node_walk(context, node->data.stmt_fun->body, callback);
        depth--;
        break;
    case NODE_STMT_EXT:
        depth++;
        node_walk(context, node->data.stmt_ext->identifier, callback);
        node_walk(context, node->data.stmt_ext->type, callback);
        depth--;
        break;
    case NODE_STMT_IF:
        depth++;
        node_walk(context, node->data.stmt_if->condition, callback);
        node_walk(context, node->data.stmt_if->body, callback);
        depth--;
        break;
    case NODE_STMT_OR:
        depth++;
        node_walk(context, node->data.stmt_or->condition, callback);
        node_walk(context, node->data.stmt_or->body, callback);
        depth--;
        break;
    case NODE_STMT_FOR:
        depth++;
        node_walk(context, node->data.stmt_for->condition, callback);
        node_walk(context, node->data.stmt_for->body, callback);
        depth--;
        break;
    case NODE_STMT_BRK:
    case NODE_STMT_CNT:
        break;
    case NODE_STMT_RET:
        depth++;
        node_walk(context, node->data.stmt_return->value, callback);
        depth--;
        break;
    case NODE_STMT_ASM:
        depth++;
        node_walk(context, node->data.stmt_asm->code, callback);
        depth--;
        break;
    case NODE_STMT_BLOCK:
        for (int i = 0; i < node_list_count(node->data.stmt_block->statements); i++)
        {
            depth++;
            node_walk(context, node->data.stmt_block->statements[i], callback);
            depth--;
        }
        break;
    case NODE_STMT_EXPR:
        depth++;
        node_walk(context, node->data.stmt_expr->expression, callback);
        depth--;
        break;
    }
}

Node *node_find_parent(Node *node, NodeKind kind)
{
    if (node == NULL)
    {
        return NULL;
    }

    if (node->parent == NULL)
    {
        return NULL;
    }

    if (node->parent->kind == kind)
    {
        return node->parent;
    }

    return node_find_parent(node->parent, kind);
}
