#include "analysis.h"

#include <stdio.h>
#include <string.h>

Type *analyze_type(Scope *scope, Node *node)
{
    // The only nodes that can be used in a type definition are:
    // - NODE_IDENTIFIER (if the target root is a module)
    // - NODE_EXPR_MEMBER
    // - NODE_TYPE_ARRAY
    // - NODE_TYPE_POINTER
    // - NODE_TYPE_FUN
    // - NODE_TYPE_STR
    // - NODE_TYPE_UNI

    switch (node->kind)
    {
    case NODE_IDENTIFIER:
        Symbol *sym_identifier = scope_get(scope, node->data.identifier->name);
        if (sym_identifier == NULL)
        {
            printf("error: undefined type: %s\n", node->data.identifier->name);
            return NULL;
        }
    case NODE_EXPR_MEMBER:
        if (node->data.expr_member->member->kind != NODE_IDENTIFIER)
        {
            printf("error: expected identifier in member expression\n");
            return NULL;
        }

        if (node->data.expr_member->target->kind != NODE_IDENTIFIER)
        {
            printf("error: expected identifier target in member expression\n");
            return NULL;
        }

        Symbol *sym_target = scope_get(scope, node->data.expr_member->target->data.identifier->name);
        if (sym_target == NULL)
        {
            printf("error: undefined symbol: %s\n", node->data.expr_member->target->data.identifier->name);
            return NULL;
        }

        if (sym_target->kind != SYMBOL_USE)
        {
            printf("error: expected module symbol: %s\n", node->data.expr_member->target->data.identifier->name);
            return NULL;
        }

        

    case NODE_TYPE_ARRAY:
        return type_new(TYPE_ARRAY);
    case NODE_TYPE_POINTER:
        return type_new(TYPE_POINTER);
    case NODE_TYPE_FUN:
        return type_new(TYPE_FUNCTION);
    case NODE_TYPE_STR:
        return type_new(TYPE_STRUCT);
    case NODE_TYPE_UNI:
        return type_new(TYPE_UNION);
    default:
        printf("error: invalid node kind: %s\n", node_kind_to_string(node->kind));
        return NULL;
    }

    return 0;
}

int analyze_node(Scope *scope, Node *node)
{
    switch (node->kind)
    {
    case NODE_STMT_VAL:
        Symbol *sym_val = symbol_new();
        sym_val->kind = SYMBOL_VAL;
        sym_val->name = strdup(node->data.stmt_val->identifier->data.identifier->name);
        sym_val->data.val.type = type_new(TYPE_U32);
        scope_add(scope, sym_val);
        break;
    case NODE_STMT_DEF:
        Symbol *sym_def = symbol_new();
        sym_def->kind = SYMBOL_DEF;
        sym_def->name = strdup(node->data.stmt_def->identifier->data.identifier->name);
        sym_def->data.def.type = type_new(TYPE_U32);
        scope_add(scope, sym_def);
        break;
    default:
        printf("error: invalid node kind: %s\n", node_kind_to_string(node->kind));
        break;
    }

    return 0;
}

int analyze_project(Project *project)
{
    if (project->program == NULL)
    {
        printf("error: project program is NULL\n");
        return 1;
    }

    int errors = 0;

    for (size_t i = 0; project->program->data.program->modules[i]; i++)
    {
        Node *node_mod = project->program->data.program->modules[i];

        Scope *scope_mod = scope_new();
        scope_mod->parent = project->scope_project;
        scope_mod->name = strdup(node_mod->data.module->name);

        for (size_t j = 0; node_mod->data.module->files[j]; j++)
        {
            Node *node_file = node_mod->data.module->files[j];
            
            Scope *scope_file = scope_new();
            scope_file->parent = scope_mod;

            for (size_t k = 0; node_file->data.file->statements[k]; k++)
            {
                errors += analyze_node(scope_file, node_file->data.file->statements[k]);
            }
        }
    }

    return errors;
}
