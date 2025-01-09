#include "scope.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Symbol *symbol_new()
{
    Symbol *symbol = calloc(sizeof(Symbol), 1);
    symbol->name = NULL;

    return symbol;
}

void symbol_free(Symbol *symbol)
{
    if (symbol == NULL)
    {
        return;
    }

    free(symbol->name);
    symbol->name = NULL;

    free(symbol);
}

Scope *scope_new()
{
    Scope *scope = calloc(sizeof(Scope), 1);
    scope->symbols = calloc(sizeof(Symbol *), 2);
    scope->symbols[0] = NULL;
    
    scope->parent = NULL;

    return scope;
}

void scope_free(Scope *scope)
{
    if (scope == NULL)
    {
        return;
    }

    if (scope->symbols != NULL)
    {
        for (size_t i = 0; scope->symbols[i] != NULL; i++)
        {
            symbol_free(scope->symbols[i]);
            scope->symbols[i] = NULL;
        }
    }

    free(scope->symbols);
    scope->symbols = NULL;

    if (scope->name != NULL)
    {
        free(scope->name);
        scope->name = NULL;
    }

    // DO NOT free parent
    // not owned by this object
    scope->parent = NULL;

    free(scope);
}

Symbol *scope_get(Scope *scope, char *name)
{
    Scope *current = scope;
    while (current != NULL)
    {
        if (scope->symbols != NULL)
        {
            for (size_t i = 0; scope->symbols[i] != NULL; i++)
            {
                if (strcmp(scope->symbols[i]->name, name) == 0)
                {
                    return scope->symbols[i];
                }
            }
        }

        current = current->parent;
    }

    return NULL;
}

void scope_add(Scope *scope, Symbol *symbol)
{
    if (scope == NULL)
    {
        return;
    }

    size_t i = 0;
    while (scope->symbols[i] != NULL)
    {
        i++;
    }

    scope->symbols = realloc(scope->symbols, sizeof(Symbol *) * (i + 2));
    scope->symbols[i] = symbol;
    scope->symbols[i + 1] = NULL;
}

void scope_add_scope(Scope *scope, Scope *child)
{
    if (scope == NULL)
    {
        return;
    }

    size_t i = 0;
    while (scope->symbols[i] != NULL)
    {
        i++;
    }

    scope->symbols = realloc(scope->symbols, sizeof(Symbol *) * (i + 2));
    scope->symbols[i] = symbol_new();
    scope->symbols[i]->kind = SYMBOL_SCOPE;
    scope->symbols[i]->name = strdup(child->name);
    scope->symbols[i]->data.scope = child;
    scope->symbols[i + 1] = NULL;
}

void scope_print(Scope *scope)
{
    if (scope == NULL || scope->symbols == NULL)
    {
        return;
    }

    for (size_t i = 0; scope->symbols[i] != NULL; i++)
    {
        switch (scope->symbols[i]->kind)
        {
        case SYMBOL_VAL:
            printf("  val %s: %s\n", scope->symbols[i]->name, type_describe(scope->symbols[i]->data.val.type));
            break;
        case SYMBOL_VAR:
            printf("  var %s: %s\n", scope->symbols[i]->name, type_describe(scope->symbols[i]->data.var.type));
            break;
        case SYMBOL_DEF:
            printf("  def %s: %s\n", scope->symbols[i]->name, type_describe(scope->symbols[i]->data.def.type));
            break;
        case SYMBOL_STR:
            printf("  str %s: %s\n", scope->symbols[i]->name, type_describe(scope->symbols[i]->data.str.type));
            break;
        case SYMBOL_UNI:
            printf("  uni %s: %s\n", scope->symbols[i]->name, type_describe(scope->symbols[i]->data.uni.type));
            break;
        case SYMBOL_FUN:
            printf("  fun %s: %s\n", scope->symbols[i]->name, type_describe(scope->symbols[i]->data.fun.type));
            break;
        case SYMBOL_EXT:
            printf("  ext %s: %s\n", scope->symbols[i]->name, type_describe(scope->symbols[i]->data.ext.type));
            break;
        case SYMBOL_USE:
            printf("  use %s: %s\n", scope->symbols[i]->name, scope->symbols[i]->data.use.module);
            break;
        case SYMBOL_ERR:
            printf("  err %s: %s\n", scope->symbols[i]->name, scope->symbols[i]->data.err.message);
            break;
        default:
            printf("  %s: unknown symbol kind\n", scope->symbols[i]->name);
            break;
        }
    }
}
