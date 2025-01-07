#ifndef SCOPE_H
#define SCOPE_H

#include "type.h"

typedef struct Symbol {
    char *name;
    Type *type;
} Symbol;

typedef struct Scope {
    Symbol **symbols;

    struct Scope *parent;
} Scope;

Scope *scope_new();
void scope_free(Scope *table);

Symbol *symbol_new();
void symbol_free(Symbol *symbol);

Symbol *scope_get(Scope *table, char *name);
void scope_add(Scope *table, Symbol *symbol);

void scope_print(Scope *table);

#endif
