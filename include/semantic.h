#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "ast.h"
#include <stdbool.h>

// forward declarations
typedef struct Type             Type;
typedef struct Symbol           Symbol;
typedef struct Scope            Scope;
typedef struct SemanticAnalyzer SemanticAnalyzer;

// type kinds
typedef enum TypeKind
{
    TYPE_VOID,
    TYPE_BOOL,
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_POINTER,
    TYPE_ARRAY,
    TYPE_FUNCTION,
    TYPE_STRUCT,
    TYPE_UNION,
    TYPE_ALIAS,
    TYPE_ERROR
} TypeKind;

// type structure
struct Type
{
    TypeKind kind;
    unsigned size;      // size in bytes
    unsigned align;     // alignment requirement
    bool     is_signed; // for numeric types
    bool     is_const;  // immutability

    union
    {
        // pointer type
        struct
        {
            Type *base;
        } pointer;

        // array type
        struct
        {
            Type  *element;
            size_t size; // 0 for dynamic arrays [_]
        } array;

        // function type
        struct
        {
            Type **params;
            Type  *return_type;
            bool   is_variadic;
        } function;

        // struct/union type
        struct
        {
            Symbol **fields;
            char    *name;
        } composite;

        // alias type
        struct
        {
            Type *base;
            char *name;
        } alias;
    };
};

// symbol kinds
typedef enum SymbolKind
{
    SYMBOL_VARIABLE,
    SYMBOL_CONSTANT,
    SYMBOL_FUNCTION,
    SYMBOL_TYPE,
    SYMBOL_STRUCT,
    SYMBOL_UNION,
    SYMBOL_FIELD
} SymbolKind;

// symbol structure
struct Symbol
{
    char      *name;
    SymbolKind kind;
    Type      *type;
    Node      *node;        // ast node reference
    Scope     *scope;       // owning scope
    bool       is_external; // external declaration
    size_t     offset;      // for struct fields
};

// scope structure
struct Scope
{
    Scope   *parent;
    Symbol **symbols; // null-terminated array
    int      level;   // nesting level
};

// semantic analyzer
struct SemanticAnalyzer
{
    Scope *global;  // global scope
    Scope *current; // current scope
    Type **types;   // registered types
    Node  *ast;     // ast root
    bool   has_errors;
};

// core functions
void semantic_init(SemanticAnalyzer *analyzer, Node *ast);
void semantic_dnit(SemanticAnalyzer *analyzer);
bool semantic_analyze(SemanticAnalyzer *analyzer);

// type management
Type  *type_new(TypeKind kind);
void   type_free(Type *type);
Type  *type_builtin(const char *name);
Type  *type_pointer(Type *base);
Type  *type_array(Type *element, size_t size);
Type  *type_function(Type *return_type, Type **params, bool is_variadic);
Type  *type_struct(const char *name);
Type  *type_union(const char *name);
Type  *type_alias(const char *name, Type *base);
bool   type_equal(Type *a, Type *b);
bool   type_compatible(Type *a, Type *b);
size_t type_sizeof(Type *type);
size_t type_alignof(Type *type);

// scope management
Scope  *scope_new(Scope *parent);
void    scope_free(Scope *scope);
Symbol *scope_lookup(Scope *scope, const char *name);
Symbol *scope_define(Scope *scope, const char *name, SymbolKind kind, Type *type);

// symbol management
Symbol *symbol_new(const char *name, SymbolKind kind, Type *type);
void    symbol_free(Symbol *symbol);

#endif
