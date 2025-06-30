#ifndef SYMBOL_H
#define SYMBOL_H

#include "ast.h"
#include "module.h"
#include "type.h"
#include <stdbool.h>

typedef enum SymbolKind
{
    SYMBOL_VAR,
    SYMBOL_FUNC,
    SYMBOL_TYPE,
    SYMBOL_FIELD,
    SYMBOL_PARAM,
    SYMBOL_MODULE,
} SymbolKind;

typedef struct Symbol
{
    SymbolKind     kind;
    char          *name;
    Type          *type;
    AstNode       *decl_node;
    bool           is_val;
    Module        *module;
    struct Symbol *next;
} Symbol;

typedef struct SymbolTable
{
    Symbol            **symbols;
    int                 capacity;
    int                 count;
    struct SymbolTable *parent;
} SymbolTable;

void    symbol_table_init(SymbolTable *table, SymbolTable *parent);
void    symbol_table_dnit(SymbolTable *table);
Symbol *symbol_table_declare(SymbolTable *table, const char *name, SymbolKind kind, Type *type, AstNode *decl);
Symbol *symbol_table_lookup(SymbolTable *table, const char *name);
Symbol *symbol_table_lookup_current_scope(SymbolTable *table, const char *name);
void    symbol_table_iterate(SymbolTable *table, void (*callback)(Symbol *symbol, void *data), void *data);

#endif
