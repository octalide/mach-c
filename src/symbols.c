#include "symbols.h"

#include <stdlib.h>
#include <string.h>

SymbolTable *symbol_table_new()
{
    SymbolTable *table = calloc(sizeof(SymbolTable), 1);
    table->symbols = NULL;
    table->parent = NULL;

    return table;
}

void symbol_table_free(SymbolTable *table)
{
    if (table == NULL)
    {
        return;
    }

    if (table->symbols != NULL)
    {
        for (size_t i = 0; table->symbols[i] != NULL; i++)
        {
            symbol_free(table->symbols[i]);
        }
    }

    free(table->symbols);
    table->symbols = NULL;

    free(table);
}

Symbol *symbol_new(char *name)
{
    Symbol *symbol = calloc(sizeof(Symbol), 1);
    symbol->name = name;

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

Symbol *symbol_table_get(SymbolTable *table, char *name)
{
    for (size_t i = 0; table->symbols[i] != NULL; i++)
    {
        if (strcmp(table->symbols[i]->name, name) == 0)
        {
            return table->symbols[i];
        }
    }

    if (table->parent != NULL)
    {
        return symbol_table_get(table->parent, name);
    }

    return NULL;
}

void symbol_table_add(SymbolTable *table, Symbol *symbol)
{
    size_t i = 0;
    while (table->symbols[i] != NULL)
    {
        i++;
    }

    table->symbols = realloc(table->symbols, sizeof(Symbol *) * (i + 2));
    table->symbols[i] = symbol;
    table->symbols[i + 1] = NULL;
}