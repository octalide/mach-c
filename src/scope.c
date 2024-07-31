#include "scope.h"

#include <string.h>

void scope_init(Scope *scope)
{
    scope->parent = NULL;

    scope->symbols = calloc(1, sizeof(Symbol *));
    scope->symbols[0] = NULL;
}

void scope_free(Scope *scope)
{
    if (scope == NULL)
    {
        return;
    }

    if (scope->symbols)
    {
        for (int i = 0; scope->symbols[i] != NULL; i++)
        {
            symbol_free(scope->symbols[i]);
        }

        free(scope->symbols);
    }
}

void scope_add_symbol(Scope *scope, Symbol *symbol)
{
    if (scope == NULL || symbol == NULL)
    {
        return;
    }

    for (int i = 0; scope->symbols[i] != NULL; i++)
    {
        if (strcmp(scope->symbols[i]->name, symbol->name) == 0)
        {
            return;
        }
    }

    int count = 0;
    for (int i = 0; scope->symbols[i] != NULL; i++)
    {
        count++;
    }

    scope->symbols = realloc(scope->symbols, sizeof(Symbol *) * (count + 2));
    scope->symbols[count] = symbol;
    scope->symbols[count + 1] = NULL;
}

Symbol *scope_find_symbol(Scope *scope, char *name)
{
    if (scope == NULL || name == NULL)
    {
        return NULL;
    }

    for (int i = 0; scope->symbols[i] != NULL; i++)
    {
        if (strcmp(scope->symbols[i]->name, name) == 0)
        {
            return scope->symbols[i];
        }
    }

    return NULL;
}

Symbol *scope_find_symbol_recursive(Scope *scope, char *name)
{
    if (scope == NULL || name == NULL)
    {
        return NULL;
    }

    Symbol *symbol = scope_find_symbol(scope, name);
    if (symbol != NULL)
    {
        return symbol;
    }

    if (scope->parent != NULL)
    {
        return scope_find_symbol_recursive(scope->parent, name);
    }

    return NULL;
}

Scope *scope_down(Scope *scope)
{
    if (scope == NULL)
    {
        return NULL;
    }

    Scope *child = malloc(sizeof(Scope));
    scope_init(child);

    child->parent = scope;

    return child;
}

Scope *scope_up(Scope *scope)
{
    if (scope == NULL)
    {
        return NULL;
    }

    Scope *parent = scope->parent;
    scope_free(scope);
    return parent;
}
