#ifndef SYMBOLS_H
#define SYMBOLS_H

typedef struct Symbol {
    char *name;
} Symbol;

typedef struct SymbolTable {
    Symbol **symbols;

    struct SymbolTable *parent;
} SymbolTable;

SymbolTable *symbol_table_new();
void symbol_table_free(SymbolTable *table);

Symbol *symbol_new(char *name);
void symbol_free(Symbol *symbol);

Symbol *symbol_table_get(SymbolTable *table, char *name);
void symbol_table_add(SymbolTable *table, Symbol *symbol);

#endif
