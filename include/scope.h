#ifndef SCOPE_H
#define SCOPE_H

#include "type.h"

struct Scope;

typedef struct Scope
{
    struct Scope *parent;
    Symbol **symbols;
} Scope;

void scope_init(Scope *scope);
void scope_free(Scope *scope);

void scope_add_symbol(Scope *scope, Symbol *symbol);
Symbol *scope_find_symbol(Scope *scope, char *name);
Symbol *scope_find_symbol_recursive(Scope *scope, char *name);

Scope *scope_down(Scope *scope);
Scope *scope_up(Scope *scope);

#endif // SCOPE_H
