#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "ast.h"

#include <stdbool.h>
#include <stddef.h>

// type system
typedef enum TypeKind
{
    TYPE_VOID,
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_POINTER,
    TYPE_ARRAY,
    TYPE_FUNCTION,
    TYPE_ERROR // for error recovery
} TypeKind;

typedef struct Type
{
    TypeKind kind;
    bool     is_unsigned; // for integer types
    size_t   bit_width;   // 8, 16, 32, 64 for integers and floats

    // for composite types
    struct Type  *base_type;   // pointer/array element type
    struct Type  *return_type; // function return type
    struct Type **param_types; // function parameter types
    size_t        param_count;
    bool          is_variadic; // function is variadic
    size_t        array_size;  // array size (0 for unknown/dynamic)
} Type;

// symbol table entry
typedef enum SymbolType
{
    SYMBOL_TYPE_VARIABLE,
    SYMBOL_TYPE_FUNCTION,
    SYMBOL_TYPE_TYPE
} SymbolType;

typedef struct SemanticSymbol
{
    char      *name;
    SymbolType symbol_type;
    Type      *type;
    bool       is_mutable; // false for val declarations
    size_t     scope_level;
    Node      *declaration; // reference to AST node
} SemanticSymbol;

// semantic analyzer
typedef struct SemanticAnalyzer
{
    // symbol table
    SemanticSymbol *symbols;
    size_t          symbol_count;
    size_t          symbol_capacity;
    size_t          current_scope_level;

    // current context
    Type *current_function_return_type;
    bool  in_loop;

    // error tracking
    bool   has_error;
    char   error_messages[4096]; // buffer for multiple errors
    size_t error_buffer_pos;
} SemanticAnalyzer;

// core functions
bool semantic_init(SemanticAnalyzer *analyzer);
void semantic_dnit(SemanticAnalyzer *analyzer);
bool semantic_analyze(SemanticAnalyzer *analyzer, Node *program);

// type system functions
Type       *type_create_void(void);
Type       *type_create_int(size_t bit_width, bool is_unsigned);
Type       *type_create_float(size_t bit_width);
Type       *type_create_pointer(Type *base_type);
Type       *type_create_array(Type *element_type, size_t size);
Type       *type_create_function(Type *return_type, Type **param_types, size_t param_count, bool is_variadic);
void        type_destroy(Type *type);
Type       *type_clone(Type *type);
bool        type_equals(Type *a, Type *b);
bool        type_is_assignable(Type *from, Type *to);
const char *type_to_string(Type *type);

// type checking
Type *check_expression_type(SemanticAnalyzer *analyzer, Node *expr);
bool  check_statement(SemanticAnalyzer *analyzer, Node *stmt);

// symbol table
void            push_scope(SemanticAnalyzer *analyzer);
void            pop_scope(SemanticAnalyzer *analyzer);
SemanticSymbol *add_symbol(SemanticAnalyzer *analyzer, const char *name, SymbolType symbol_type, Type *type, bool is_mutable, Node *declaration);
SemanticSymbol *find_symbol(SemanticAnalyzer *analyzer, const char *name);

// error handling
void semantic_error(SemanticAnalyzer *analyzer, const char *message);
void semantic_error_node(SemanticAnalyzer *analyzer, Node *node, const char *message);

#endif
