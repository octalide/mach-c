#ifndef NODE_H
#define NODE_H

#include "token.h"

typedef enum
{
    NODE_PROGRAM,

    NODE_EXPR_IDENTIFIER,
    NODE_EXPR_LITERAL,
    NODE_EXPR_ARRAY,
    NODE_EXPR_TYPE,
    NODE_EXPR_TYPE_ARRAY,
    NODE_EXPR_TYPE_REF,
    NODE_EXPR_TYPE_MEMBER,
    NODE_EXPR_UNARY,
    NODE_EXPR_BINARY,
    NODE_EXPR_CALL,
    NODE_EXPR_INDEX,
    NODE_EXPR_ASSIGN,

    NODE_STMT_BLOCK,
    NODE_STMT_USE,
    NODE_STMT_FUN,
    NODE_STMT_STR,
    NODE_STMT_VAL,
    NODE_STMT_VAR,
    NODE_STMT_IF,
    NODE_STMT_OR,
    NODE_STMT_FOR,
    NODE_STMT_BRK,
    NODE_STMT_CNT,
    NODE_STMT_RET,
    NODE_STMT_EXPR,
} NodeType;

typedef struct base_program
{
    struct Node *statements;
    int statement_count;
} base_program;

typedef struct expr_identifier
{
    const char *name;
} expr_identifier;

typedef struct expr_literal
{
    Token value;
} expr_literal;

typedef struct expr_array
{
    struct Node *elements;
    int element_count;
} expr_array;

typedef struct expr_type
{
    struct Node *identifier;
} expr_type;

typedef struct expr_type_ref
{
    struct Node *type;
} expr_type_ref;

typedef struct expr_type_array
{
    struct Node *size;
    struct Node *type;
} expr_type_array;

typedef struct expr_type_member
{
    struct Node *target;
    struct Node *member;
} expr_type_member;

typedef struct expr_unary
{
    TokenType operator;
    struct Node *right;
} expr_unary;

typedef struct expr_binary
{
    TokenType operator;
    struct Node *left;
    struct Node *right;
} expr_binary;

typedef struct expr_call
{
    struct Node *target;
    struct Node *arguments;
    int argument_count;
} expr_call;

typedef struct expr_index
{
    struct Node *target;
    struct Node *index;
} expr_index;

typedef struct expr_assign
{
    struct Node *target;
    struct Node *value;
} expr_assign;

typedef struct stmt_block
{
    struct Node *statements;
    int statement_count;
} stmt_block;

typedef struct stmt_use
{
    struct Node *module;
} stmt_use;

typedef struct fun_parameter
{
    const char *name;
    struct Node *type;
} fun_parameter;

typedef struct stmt_fun
{
    struct Node *identifier;
    struct fun_parameter *parameters;
    int parameter_count;
    struct Node *body;
    struct Node *return_type;
} stmt_fun;

typedef struct str_field
{
    const char *name;
    struct Node *type;
} str_field;

typedef struct stmt_str
{
    struct Node *identifier;
    struct str_field *fields;
    int field_count;
} stmt_str;

typedef struct stmt_val
{
    struct Node *identifier;
    struct Node *type;
    struct Node *initializer;
} stmt_val;

typedef struct stmt_var
{
    struct Node *identifier;
    struct Node *type;
    struct Node *initializer;
} stmt_var;

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

typedef struct stmt_ret
{
    struct Node *value;
} stmt_ret;

typedef struct stmt_expr
{
    struct Node *expression;
} stmt_expr;

typedef struct Node
{
    NodeType type;
    struct Node *next;

    union data
    {
        base_program base_program;

        expr_identifier expr_identifier;
        expr_literal expr_literal;
        expr_array expr_array;
        expr_type expr_type;
        expr_type_array expr_type_array;
        expr_type_ref expr_type_ref;
        expr_type_member expr_type_member;
        expr_unary expr_unary;
        expr_binary expr_binary;
        expr_call expr_call;
        expr_index expr_index;
        expr_assign expr_assign;

        stmt_block stmt_block;
        stmt_use stmt_use;
        stmt_fun stmt_fun;
        stmt_str stmt_str;
        stmt_val stmt_val;
        stmt_var stmt_var;
        stmt_if stmt_if;
        stmt_or stmt_or;
        stmt_for stmt_for;
        stmt_ret stmt_ret;
        stmt_expr stmt_expr;
    } data;
} Node;

struct Node *new_node(NodeType type);
void free_node(struct Node *node);

char *node_type_string(NodeType type);

#endif // NODE_H
