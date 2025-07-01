#include "symbol.h"
#include <stdlib.h>
#include <string.h>

static unsigned int hash_symbol_name(const char *name, int capacity)
{
    unsigned int hash = 5381;
    for (int i = 0; name[i]; i++)
    {
        hash = ((hash << 5) + hash) + name[i];
    }
    return hash % capacity;
}

void symbol_table_init(SymbolTable *table, SymbolTable *parent)
{
    table->capacity = 16;
    table->count    = 0;
    table->symbols  = calloc(table->capacity, sizeof(Symbol *));
    table->parent   = parent;
}

void symbol_table_dnit(SymbolTable *table)
{
    for (int i = 0; i < table->capacity; i++)
    {
        Symbol *symbol = table->symbols[i];
        while (symbol)
        {
            Symbol *next = symbol->next;
            // don't free parameter or variable symbols - they need to persist for codegen
            if (symbol->kind != SYMBOL_PARAM && symbol->kind != SYMBOL_VAR)
            {
                free(symbol->name);
                free(symbol);
            }
            symbol = next;
        }
    }
    free(table->symbols);
}

Symbol *symbol_table_declare(SymbolTable *table, const char *name, SymbolKind kind, Type *type, AstNode *decl)
{
    if (symbol_table_lookup_current_scope(table, name))
    {
        return NULL;
    }
    unsigned int index    = hash_symbol_name(name, table->capacity);
    Symbol      *symbol   = malloc(sizeof(Symbol));
    symbol->kind          = kind;
    symbol->name          = strdup(name);
    symbol->type          = type;
    symbol->decl_node     = decl;
    symbol->is_val        = false;
    symbol->module        = NULL;
    symbol->next          = table->symbols[index];
    table->symbols[index] = symbol;
    table->count++;
    return symbol;
}

Symbol *symbol_table_lookup(SymbolTable *table, const char *name)
{
    SymbolTable *current = table;
    while (current)
    {
        Symbol *symbol = symbol_table_lookup_current_scope(current, name);
        if (symbol)
            return symbol;
        current = current->parent;
    }
    return NULL;
}

Symbol *symbol_table_lookup_current_scope(SymbolTable *table, const char *name)
{
    unsigned int index  = hash_symbol_name(name, table->capacity);
    Symbol      *symbol = table->symbols[index];
    while (symbol)
    {
        if (strcmp(symbol->name, name) == 0)
            return symbol;
        symbol = symbol->next;
    }
    return NULL;
}

void symbol_table_iterate(SymbolTable *table, void (*callback)(Symbol *symbol, void *data), void *data)
{
    if (!table || !callback)
        return;
    for (int i = 0; i < table->capacity; i++)
    {
        Symbol *symbol = table->symbols[i];
        while (symbol)
        {
            callback(symbol, data);
            symbol = symbol->next;
        }
    }
}
