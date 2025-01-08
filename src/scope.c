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

void scope_print(Scope *scope)
{
    if (scope == NULL || scope->symbols == NULL)
    {
        return;
    }

    for (size_t i = 0; scope->symbols[i] != NULL; i++)
    {
        printf("  %s: %s\n", scope->symbols[i]->name, type_describe(scope->symbols[i]->type));
    }
}
