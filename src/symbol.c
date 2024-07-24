#include <stdlib.h>
#include <string.h>

#include "symbol_table.h"

SymbolTable *symbol_table_new(int size)
{
    SymbolTable *table = malloc(sizeof(SymbolTable));
    table->symbols = calloc(size, sizeof(Symbol *));
    table->size = size;
    table->scope_level = 0;
    return table;
}

void symbol_table_free(SymbolTable *table)
{
    for (int i = 0; i < table->size; ++i)
    {
        Symbol *symbol = table->symbols[i];
        while (symbol)
        {
            Symbol *next = symbol->next;
            free(symbol->name);
            free(symbol);
            symbol = next;
        }
    }
    free(table->symbols);
}

void symbol_table_enter_scope(SymbolTable *table)
{
    table->scope_level++;
}

void symbol_table_exit_scope(SymbolTable *table)
{
    table->scope_level--;
    for (int i = 0; i < table->size; ++i)
    {
        Symbol *symbol = table->symbols[i];
        Symbol *prev = NULL;
        while (symbol)
        {
            if (symbol->scope_level > table->scope_level)
            {
                if (prev)
                {
                    prev->next = symbol->next;
                    free(symbol->name);
                    free(symbol);
                    symbol = prev->next;
                }
                else
                {
                    table->symbols[i] = symbol->next;
                    free(symbol->name);
                    free(symbol);
                    symbol = table->symbols[i];
                }
            }
            else
            {
                prev = symbol;
                symbol = symbol->next;
            }
        }
    }
}

Symbol *symbol_table_insert(SymbolTable *table, const char *name, NodeType type)
{
    int hash = abs((int)name[0]) % table->size;
    Symbol *symbol = malloc(sizeof(Symbol));
    symbol->name = strdup(name);
    symbol->type = type;
    symbol->scope_level = table->scope_level;
    symbol->next = table->symbols[hash];
    table->symbols[hash] = symbol;
    return symbol;
}

Symbol *symbol_table_lookup(SymbolTable *table, const char *name)
{
    int hash = abs((int)name[0]) % table->size;
    Symbol *symbol = table->symbols[hash];
    while (symbol)
    {
        if (strcmp(symbol->name, name) == 0)
        {
            return symbol;
        }
        symbol = symbol->next;
    }
    return NULL;
}
