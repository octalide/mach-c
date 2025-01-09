#ifndef SCOPE_H
#define SCOPE_H

#include "type.h"

typedef enum SymbolKind
{
    SYMBOL_ERR,

    SYMBOL_VAL,
    SYMBOL_VAR,
    SYMBOL_DEF,
    SYMBOL_STR,
    SYMBOL_UNI,
    SYMBOL_FUN,
    SYMBOL_EXT,
    SYMBOL_USE,
} SymbolKind;

typedef struct SymbolErr
{
    char *message;
} SymbolErr;

typedef struct SymbolVal
{
    Type *type;
} SymbolVal;

typedef struct SymbolVar
{
    Type *type;
} SymbolVar;

typedef struct SymbolDef
{
    Type *type;
} SymbolDef;

typedef struct SymbolStr
{
    Type *type;
} SymbolStr;

typedef struct SymbolUni
{
    Type *type;
} SymbolUni;

typedef struct SymbolFun
{
    Type *type;
} SymbolFun;

typedef struct SymbolExt
{
    Type *type;
} SymbolExt;

typedef struct SymbolUse
{
    char *module;
} SymbolUse;

typedef struct Symbol {
    SymbolKind kind;

    char *name;

    union {
        SymbolErr err;
        SymbolVal val;
        SymbolVar var;
        SymbolDef def;
        SymbolStr str;
        SymbolUni uni;
        SymbolFun fun;
        SymbolExt ext;
        SymbolUse use;
    } data;
} Symbol;

typedef struct Scope {
    // name is used to prefix module scopes with their name
    char *name;

    Symbol **symbols;

    struct Scope *parent;
} Scope;

Symbol *symbol_new();
void symbol_free(Symbol *symbol);

Scope *scope_new();
void scope_free(Scope *scope);

Symbol *scope_get(Scope *scope, char *name);
void scope_add(Scope *scope, Symbol *symbol);

void scope_add_scope(Scope *target, Scope *source);

void scope_print(Scope *scope);

#endif
