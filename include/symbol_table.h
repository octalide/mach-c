#ifndef SYMBOL_H
#define SYMBOL_H

#include "ast.h"

typedef struct Symbol
{
    char *name;
    NodeType type;
    int scope_level;
    struct Symbol *next;
} Symbol;

typedef struct SymbolTable
{
    Symbol **symbols;
    int size;
    int scope_level;
} SymbolTable;

SymbolTable *symbol_table_new(int size);
void symbol_table_free(SymbolTable *table);

void symbol_table_enter_scope(SymbolTable *table);
void symbol_table_exit_scope(SymbolTable *table);

Symbol *symbol_table_insert(SymbolTable *table, const char *name, NodeType type);
Symbol *symbol_table_lookup(SymbolTable *table, const char *name);

#endif // SYMBOL_TABLE_H
