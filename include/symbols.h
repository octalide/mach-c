#ifndef SYMBOLS_H
#define SYMBOLS_H

#include "type.h"
#include "ast.h"

typedef struct Symbol {
    char *name;
    Type *type;
    Node *location;
} Symbol;

typedef struct SymbolTable {
    Symbol **symbols;

    struct SymbolTable *parent;
} SymbolTable;

SymbolTable *symbol_table_new();
void symbol_table_free(SymbolTable *table);

Symbol *symbol_new();
void symbol_free(Symbol *symbol);

Symbol *symbol_table_get(SymbolTable *table, char *name);
void symbol_table_add(SymbolTable *table, Symbol *symbol);

void symbol_table_print(SymbolTable *table);

#endif
