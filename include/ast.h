#ifndef NODE_H
#define NODE_H

#include "token.h"

typedef enum
{
    NODE_BLOCK,      // statement block (program or function body)
    NODE_IDENTIFIER, // identifier

    NODE_EXPR_IDENTIFIER, // identifier expression
    NODE_EXPR_MEMBER,     // member access
    NODE_EXPR_LIT_CHAR,   // literal character
    NODE_EXPR_LIT_NUMBER, // literal number
    NODE_EXPR_LIT_STRING, // literal string
    NODE_EXPR_CALL,       // function call
    NODE_EXPR_INDEX,      // array index
    NODE_EXPR_DEF_STR,    // struct definition
    NODE_EXPR_DEF_UNI,    // union definition
    NODE_EXPR_DEF_ARRAY,  // array definition
    NODE_EXPR_UNARY,      // unary expression
    NODE_EXPR_BINARY,     // binary expression

    NODE_TYPE_ARRAY, // array type declaration
    NODE_TYPE_REF,   // reference type
    NODE_TYPE_FUN,   // function type
    NODE_TYPE_STR,   // struct type
    NODE_TYPE_UNI,   // union type
    NODE_FIELD,      // "<identifier>: <type>" pair (used in structs, unions, function args)

    NODE_STMT_USE,       // use statement
    NODE_STMT_IF,        // if statement
    NODE_STMT_OR,        // or statement
    NODE_STMT_FOR,       // for loop
    NODE_STMT_BRK,       // break statement
    NODE_STMT_CNT,       // continue statement
    NODE_STMT_RET,       // return statement
    NODE_STMT_DECL_TYPE, // type declaration
    NODE_STMT_DECL_VAL,  // value declaration
    NODE_STMT_DECL_VAR,  // variable declaration
    NODE_STMT_DECL_FUN,  // function declaration
    NODE_STMT_DECL_STR,  // struct declaration
    NODE_STMT_DECL_UNI,  // union declaration

    NODE_STMT_EXPR, // expression statement
} NodeType;

typedef struct node_block
{
    struct Node **statements;
} node_block;

typedef struct node_identifier
{
    Token value;
} node_identifier;

typedef struct expr_member
{
    struct Node *target;
    struct Node *member;
} expr_member;

typedef struct expr_identifier
{
    struct Node *identifier;
} expr_identifier;

typedef struct expr_lit_char
{
    Token value;
} expr_lit_char;

typedef struct expr_lit_number
{
    Token value;
} expr_lit_number;

typedef struct expr_lit_string
{
    Token value;
} expr_lit_string;

typedef struct expr_call
{
    struct Node *target;
    struct Node **arguments;
} expr_call;

typedef struct expr_index
{
    struct Node *target;
    struct Node *index;
} expr_index;

typedef struct expr_def_str
{
    struct Node **initializers;
} expr_def_str;

typedef struct expr_def_uni
{
    struct Node *initializer;
} expr_def_uni;

typedef struct expr_def_array
{
    struct Node *size;
    struct Node *type;
    struct Node **elements;
} expr_def_array;

typedef struct expr_unary
{
    TokenType op;
    struct Node *rhs;
} expr_unary;

typedef struct expr_binary
{
    TokenType op;
    struct Node *lhs;
    struct Node *rhs;
} expr_binary;

typedef struct type_array
{
    struct Node *size;
    struct Node *type;
} type_array;

typedef struct type_ref
{
    struct Node *target;
} type_ref;

typedef struct type_fun
{
    struct Node **parameter_types;
    struct Node *return_type;
} type_fun;

typedef struct type_str
{
    struct Node **fields;
} type_str;

typedef struct type_uni
{
    struct Node **fields;
} type_uni;

typedef struct node_field
{
    struct Node *identifier;
    struct Node *type;
} node_field;

typedef struct stmt_use
{
    struct Node *identifier;
    struct Node *path;
} stmt_use;

typedef struct stmt_if
{
    struct Node *condition;
    struct Node *body;
} stmt_if;

typedef struct stmt_or
{
    struct Node *condition;
    struct Node *body;
} stmt_or;

typedef struct stmt_for
{
    struct Node *condition;
    struct Node *body;
} stmt_for;

// these do not hold any data but are still represented as node types:
// stmt_brk
// stmt_cnt

typedef struct stmt_ret
{
    struct Node *value;
} stmt_ret;

typedef struct stmt_decl_type
{
    struct Node *identifier;
    struct Node *type;
} stmt_decl_type;

typedef struct stmt_decl_val
{
    struct Node *identifier;
    struct Node *type;
    struct Node *initializer;
} stmt_decl_val;

typedef struct stmt_decl_var
{
    struct Node *identifier;
    struct Node *type;
    struct Node *initializer;
} stmt_decl_var;

typedef struct stmt_decl_fun
{
    struct Node *identifier;
    struct Node **parameters;
    struct Node *return_type;
    struct Node *body;
} stmt_decl_fun;

typedef struct stmt_decl_str
{
    struct Node *identifier;
    struct Node **fields;
} stmt_decl_str;

typedef struct stmt_decl_uni
{
    struct Node *identifier;
    struct Node **fields;
} stmt_decl_uni;

typedef struct stmt_expr
{
    struct Node *expression;
} stmt_expr;

typedef struct Node
{
    NodeType type;
    struct Node *child;

    union data
    {
        node_block node_block;
        node_identifier node_identifier;

        expr_identifier expr_identifier;
        expr_member expr_member;
        expr_lit_char expr_lit_char;
        expr_lit_number expr_lit_number;
        expr_lit_string expr_lit_string;
        expr_call expr_call;
        expr_index expr_index;
        expr_def_str expr_def_str;
        expr_def_uni expr_def_uni;
        expr_def_array expr_def_array;
        expr_unary expr_unary;
        expr_binary expr_binary;

        type_array type_array;
        type_ref type_ref;
        type_fun type_fun;
        type_str type_str;
        type_uni type_uni;
        node_field node_field;

        stmt_use stmt_use;
        stmt_if stmt_if;
        stmt_or stmt_or;
        stmt_for stmt_for;
        stmt_ret stmt_ret;
        stmt_decl_type stmt_decl_type;
        stmt_decl_val stmt_decl_val;
        stmt_decl_var stmt_decl_var;
        stmt_decl_fun stmt_decl_fun;
        stmt_decl_str stmt_decl_str;
        stmt_decl_uni stmt_decl_uni;
        stmt_expr stmt_expr;
    } data;
} Node;

struct Node *node_new(NodeType type);
void node_free(Node *node);

void node_list_append(Node ***list, Node *node);
int node_list_length(Node **list);
void node_list_free(Node ***list);

char *node_type_string(NodeType type);

#endif // NODE_H
