#ifndef SYMBOL_H
#define SYMBOL_H

#include "type.h"
#include <stdbool.h>

// forward declarations
typedef struct AstNode AstNode;

typedef enum SymbolKind
{
    SYMBOL_VAR,    // variable
    SYMBOL_VAL,    // constant value
    SYMBOL_FUNC,   // function
    SYMBOL_TYPE,   // type definition
    SYMBOL_FIELD,  // struct/union field
    SYMBOL_PARAM,  // function parameter
    SYMBOL_MODULE, // module
} SymbolKind;

typedef struct Symbol
{
    SymbolKind     kind;
    char          *name;
    Type          *type;
    AstNode       *decl; // declaration node
    struct Symbol *next; // for linked list in scope

    union
    {
        // SYMBOL_VAR, SYMBOL_VAL
        struct
        {
            bool is_global;
            bool is_const; // true for val
        } var;

        // SYMBOL_FUNC
        struct
        {
            bool is_external;
            bool is_defined; // false for forward declarations
        } func;

        // SYMBOL_TYPE
        struct
        {
            bool is_alias; // true for 'def' aliases
        } type_def;

        // SYMBOL_FIELD
        struct
        {
            size_t offset; // offset within struct/union
        } field;

        // SYMBOL_PARAM
        struct
        {
            size_t index; // parameter index
        } param;

        // SYMBOL_MODULE
        struct
        {
            char         *path;  // module path
            struct Scope *scope; // module scope
        } module;
    };
} Symbol;

typedef struct Scope
{
    struct Scope *parent;    // parent scope
    Symbol       *symbols;   // linked list of symbols
    bool          is_module; // true for module scope
    char         *name;      // scope name (for debugging)
} Scope;

typedef struct SymbolTable
{
    Scope *current_scope;
    Scope *global_scope;
    Scope *module_scope; // current module scope
} SymbolTable;

// symbol table operations
void symbol_table_init(SymbolTable *table);
void symbol_table_dnit(SymbolTable *table);

// scope operations
Scope *scope_create(Scope *parent, const char *name);
void   scope_destroy(Scope *scope);
void   scope_enter(SymbolTable *table, Scope *scope);
void   scope_exit(SymbolTable *table);
Scope *scope_push(SymbolTable *table, const char *name);
void   scope_pop(SymbolTable *table);

// symbol operations
Symbol *symbol_create(SymbolKind kind, const char *name, Type *type, AstNode *decl);
void    symbol_destroy(Symbol *symbol);
void    symbol_add(Scope *scope, Symbol *symbol);
Symbol *symbol_lookup(SymbolTable *table, const char *name);
Symbol *symbol_lookup_scope(Scope *scope, const char *name);
Symbol *symbol_lookup_module(SymbolTable *table, const char *module, const char *name);

// field operations for structs/unions
Symbol *symbol_add_field(Symbol *composite_symbol, const char *field_name, Type *field_type, AstNode *decl);
Symbol *symbol_find_field(Symbol *composite_symbol, const char *field_name);
size_t  symbol_calculate_struct_layout(Symbol *struct_symbol);
size_t  symbol_calculate_union_layout(Symbol *union_symbol);

// module operations
Symbol *symbol_create_module(const char *name, const char *path);
void    symbol_add_module(SymbolTable *table, Symbol *module_symbol);
Symbol *symbol_find_module(SymbolTable *table, const char *name);

// debug
void symbol_print(Symbol *symbol, int indent);
void scope_print(Scope *scope, int indent);

#endif
