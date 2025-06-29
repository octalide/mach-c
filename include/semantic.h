#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "ast.h"
#include "lexer.h"
#include "module.h"
#include <stdbool.h>

// forward declarations
typedef struct Type             Type;
typedef struct Symbol           Symbol;
typedef struct SymbolTable      SymbolTable;
typedef struct SemanticAnalyzer SemanticAnalyzer;

typedef enum TypeKind
{
    TYPE_UNKNOWN,
    TYPE_PTR,   // untyped pointer (mach's equivalent to void*)
    TYPE_INT,   // i8, i16, i32, i64, u8, u16, u32, u64
    TYPE_FLOAT, // f16, f32, f64
    TYPE_ARRAY, // [size]type or [_]type
    TYPE_FUNCTION,
    TYPE_STRUCT,
    TYPE_UNION,
    TYPE_ALIAS, // type alias from def declaration
} TypeKind;

struct Type
{
    TypeKind kind;
    unsigned size;      // size in bytes
    unsigned alignment; // alignment in bytes
    bool     is_signed; // for integer types

    union
    {
        // for TYPE_PTR
        struct
        {
            Type *base; // null for untyped ptr
        } ptr;

        // for TYPE_ARRAY
        struct
        {
            Type *elem_type;
            int   size; // -1 for unbound arrays [_]
        } array;

        // for TYPE_FUNCTION
        struct
        {
            Type **param_types;
            int    param_count;
            Type  *return_type; // null for no return value
        } function;

        // for TYPE_STRUCT/TYPE_UNION
        struct
        {
            Symbol **fields;
            int      field_count;
            char    *name; // can be null for anonymous
        } composite;

        // for TYPE_ALIAS
        struct
        {
            Type *target;
            char *name;
        } alias;
    };
};

typedef enum SymbolKind
{
    SYMBOL_VAR,    // variable or value
    SYMBOL_FUNC,   // function
    SYMBOL_TYPE,   // type definition
    SYMBOL_FIELD,  // struct/union field
    SYMBOL_PARAM,  // function parameter
    SYMBOL_MODULE, // imported module
} SymbolKind;

struct Symbol
{
    SymbolKind kind;
    char      *name;
    Type      *type;
    AstNode   *decl_node; // declaration AST node
    bool       is_val;    // true for val, false for var
    Module    *module;    // for module symbols
    Symbol    *next;      // for hash table chaining
};

struct SymbolTable
{
    Symbol     **symbols;
    int          capacity;
    int          count;
    SymbolTable *parent; // for nested scopes
};

typedef struct SemanticError
{
    AstNode *node;
    char    *message;
} SemanticError;

typedef struct SemanticErrorList
{
    SemanticError *errors;
    int            count;
    int            capacity;
} SemanticErrorList;

struct SemanticAnalyzer
{
    SymbolTable      *global_scope;
    SymbolTable      *current_scope;
    ModuleManager    *module_manager;
    Type            **builtin_types;
    int               builtin_count;
    Type             *current_function_return_type; // for return statement checking
    SemanticErrorList errors;
    bool              had_error;
};

// semantic analyzer lifecycle
void semantic_analyzer_init(SemanticAnalyzer *analyzer, ModuleManager *manager);
void semantic_analyzer_dnit(SemanticAnalyzer *analyzer);

// main analysis functions
bool semantic_analyze_program(SemanticAnalyzer *analyzer, AstNode *program);
bool semantic_analyze_module(SemanticAnalyzer *analyzer, Module *module);

// declaration analysis
bool semantic_analyze_declaration(SemanticAnalyzer *analyzer, AstNode *decl);
bool semantic_analyze_use_decl(SemanticAnalyzer *analyzer, AstNode *decl);
bool semantic_analyze_ext_decl(SemanticAnalyzer *analyzer, AstNode *decl);
bool semantic_analyze_def_decl(SemanticAnalyzer *analyzer, AstNode *decl);
bool semantic_analyze_var_decl(SemanticAnalyzer *analyzer, AstNode *decl);
bool semantic_analyze_fun_decl(SemanticAnalyzer *analyzer, AstNode *decl);
bool semantic_analyze_str_decl(SemanticAnalyzer *analyzer, AstNode *decl);
bool semantic_analyze_uni_decl(SemanticAnalyzer *analyzer, AstNode *decl);

// statement analysis
bool semantic_analyze_statement(SemanticAnalyzer *analyzer, AstNode *stmt);
bool semantic_analyze_block_stmt(SemanticAnalyzer *analyzer, AstNode *stmt);
bool semantic_analyze_ret_stmt(SemanticAnalyzer *analyzer, AstNode *stmt);
bool semantic_analyze_if_stmt(SemanticAnalyzer *analyzer, AstNode *stmt);

// expression analysis
Type *semantic_analyze_expression(SemanticAnalyzer *analyzer, AstNode *expr);
Type *semantic_analyze_binary_expr(SemanticAnalyzer *analyzer, AstNode *expr);
Type *semantic_analyze_unary_expr(SemanticAnalyzer *analyzer, AstNode *expr);
Type *semantic_analyze_call_expr(SemanticAnalyzer *analyzer, AstNode *expr);
Type *semantic_analyze_ident_expr(SemanticAnalyzer *analyzer, AstNode *expr);

// type analysis
Type *semantic_analyze_type(SemanticAnalyzer *analyzer, AstNode *type_node);
Type *semantic_get_builtin_type(SemanticAnalyzer *analyzer, const char *name);

// symbol table operations
void symbol_table_init(SymbolTable *table, SymbolTable *parent);
void symbol_table_dnit(SymbolTable *table);
void symbol_table_push_scope(SemanticAnalyzer *analyzer);
void symbol_table_pop_scope(SemanticAnalyzer *analyzer);

Symbol *symbol_table_declare(SymbolTable *table, const char *name, SymbolKind kind, Type *type, AstNode *decl);
Symbol *symbol_table_lookup(SymbolTable *table, const char *name);
Symbol *symbol_table_lookup_current_scope(SymbolTable *table, const char *name);

// symbol table iteration (for imports)
void symbol_table_iterate(SymbolTable *table, void (*callback)(Symbol *symbol, void *data), void *data);

// type operations
Type *type_create_builtin(TypeKind kind, unsigned size, unsigned alignment, bool is_signed);
Type *type_create_ptr(Type *base);
Type *type_create_array(Type *elem_type, int size);
Type *type_create_function(Type **param_types, int param_count, Type *return_type);
Type *type_create_struct(const char *name);
Type *type_create_union(const char *name);
Type *type_create_alias(const char *name, Type *target);

void  type_dnit(Type *type);
bool  type_equals(Type *a, Type *b);
bool  type_is_assignable(Type *target, Type *source);
Type *type_get_common_type(Type *a, Type *b);

// error reporting
void semantic_error(SemanticAnalyzer *analyzer, AstNode *node, const char *format, ...);

// error list operations
void semantic_error_list_init(SemanticErrorList *list);
void semantic_error_list_dnit(SemanticErrorList *list);
void semantic_error_list_add(SemanticErrorList *list, AstNode *node, const char *message);
void semantic_error_list_print(SemanticErrorList *list, Lexer *lexer, const char *file_path);

#endif
