#ifndef AST_H
#define AST_H

#include "token.h"
#include <stdbool.h>

// forward declarations
typedef struct Type   Type;
typedef struct Symbol Symbol;

typedef enum AstKind
{
    AST_PROGRAM,
    AST_MODULE,

    // declarations
    AST_USE_DECL,
    AST_EXT_DECL,
    AST_DEF_DECL,
    AST_VAL_DECL,
    AST_VAR_DECL,
    AST_FUN_DECL,
    AST_STR_DECL,
    AST_UNI_DECL,
    AST_FIELD_DECL,
    AST_PARAM_DECL,

    // statements
    AST_BLOCK_STMT,
    AST_EXPR_STMT,
    AST_RET_STMT,
    AST_IF_STMT,
    AST_FOR_STMT,
    AST_BRK_STMT,
    AST_CNT_STMT,

    // expressions
    AST_BINARY_EXPR,
    AST_UNARY_EXPR,
    AST_CALL_EXPR,
    AST_INDEX_EXPR,
    AST_FIELD_EXPR,
    AST_CAST_EXPR,
    AST_IDENT_EXPR,
    AST_LIT_EXPR,
    AST_ARRAY_EXPR,
    AST_STRUCT_EXPR,

    // types
    AST_TYPE_NAME,
    AST_TYPE_PTR,
    AST_TYPE_ARRAY,
    AST_TYPE_FUN,
    AST_TYPE_STR,
    AST_TYPE_UNI,
} AstKind;

typedef struct AstNode AstNode;
typedef struct AstList AstList;

// generic list for child nodes
struct AstList
{
    AstNode **items;
    int       count;
    int       capacity;
};

// base AST node
struct AstNode
{
    AstKind kind;
    Token  *token;  // source token for error reporting
    Type   *type;   // resolved type (filled during semantic analysis)
    Symbol *symbol; // symbol table entry (if applicable)

    union
    {
        // program root
        struct
        {
            AstList *decls;
        } program;

        // module declaration
        struct
        {
            char    *name;  // module name
            AstList *decls; // declarations in this module
        } module;

        // use declaration
        struct
        {
            char   *module_path;
            char   *alias;      // null if unaliased
            Symbol *module_sym; // filled during semantic analysis
        } use_decl;

        // external declaration
        struct
        {
            char    *name;
            AstNode *type;
        } ext_decl;

        // type definition
        struct
        {
            char    *name;
            AstNode *type;
        } def_decl;

        // value/variable declaration
        struct
        {
            char    *name;
            AstNode *type; // explicit type or null
            AstNode *init; // initializer expression
            bool     is_val;
        } var_decl;

        // function declaration
        struct
        {
            char    *name;
            AstList *params;
            AstNode *return_type; // null for no return
            AstNode *body;        // null for external functions
        } fun_decl;

        // struct declaration
        struct
        {
            char    *name;
            AstList *fields;
        } str_decl;

        // union declaration
        struct
        {
            char    *name;
            AstList *fields;
        } uni_decl;

        // field declaration
        struct
        {
            char    *name;
            AstNode *type;
        } field_decl;

        // parameter declaration
        struct
        {
            char    *name;
            AstNode *type;
        } param_decl;

        // block statement
        struct
        {
            AstList *stmts;
        } block_stmt;

        // expression statement
        struct
        {
            AstNode *expr;
        } expr_stmt;

        // return statement
        struct
        {
            AstNode *expr; // null for void return
        } ret_stmt;

        // if statement
        struct
        {
            AstNode *cond;
            AstNode *then_stmt;
            AstNode *else_stmt; // can be another if (or chain)
        } if_stmt;

        // for loop
        struct
        {
            AstNode *cond; // null for infinite loop
            AstNode *body;
        } for_stmt;

        // binary expression
        struct
        {
            AstNode  *left;
            AstNode  *right;
            TokenKind op;
        } binary_expr;

        // unary expression
        struct
        {
            AstNode  *expr;
            TokenKind op;
        } unary_expr;

        // function call
        struct
        {
            AstNode *func;
            AstList *args;
        } call_expr;

        // array indexing
        struct
        {
            AstNode *array;
            AstNode *index;
        } index_expr;

        // field access
        struct
        {
            AstNode *object;
            char    *field;
        } field_expr;

        // type cast
        struct
        {
            AstNode *expr;
            AstNode *type;
        } cast_expr;

        // identifier
        struct
        {
            char *name;
        } ident_expr;

        // literal
        struct
        {
            TokenKind kind; // TOKEN_LIT_INT, etc.
            union
            {
                unsigned long long int_val;
                double             float_val;
                char               char_val;
                char              *string_val;
            };
        } lit_expr;

        // array literal
        struct
        {
            AstNode *type;  // element type
            AstList *elems; // elements
        } array_expr;

        // struct literal
        struct
        {
            AstNode *type;   // struct type
            AstList *fields; // field initializers
        } struct_expr;

        // type expressions
        struct
        {
            char *name;
        } type_name;

        struct
        {
            AstNode *base;
        } type_ptr;

        struct
        {
            AstNode *elem_type;
            AstNode *size; // null for unbound arrays [_]
        } type_array;

        struct
        {
            AstList *params;
            AstNode *return_type; // null for no return
        } type_fun;

        struct
        {
            char    *name; // can be null for anonymous
            AstList *fields;
        } type_str;

        struct
        {
            char    *name; // can be null for anonymous
            AstList *fields;
        } type_uni;
    };
};

// ast node operations
void ast_node_init(AstNode *node, AstKind kind);
void ast_node_dnit(AstNode *node);

// list operations
void ast_list_init(AstList *list);
void ast_list_dnit(AstList *list);
void ast_list_append(AstList *list, AstNode *node);

// pretty printing for debugging
void ast_print(AstNode *node, int indent);

#endif
