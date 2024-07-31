#ifndef NODE_H
#define NODE_H

#include "token.h"

typedef enum NodeKind
{
    NODE_ERROR,      // error
    NODE_PROGRAM,    // root node
    NODE_IDENTIFIER, // identifier

    // literals
    NODE_LIT_INT,    // literal integer
    NODE_LIT_FLOAT,  // literal float
    NODE_LIT_CHAR,   // literal character
    NODE_LIT_STRING, // literal string

    // expressions
    NODE_EXPR_MEMBER, // postfix member access
    NODE_EXPR_CALL,   // postfix function call
    NODE_EXPR_INDEX,  // postfix array index
    NODE_EXPR_CAST,   // postfix type cast
    NODE_EXPR_NEW,    // object definition (struct, union, array)
    NODE_EXPR_UNARY,  // unary expression
    NODE_EXPR_BINARY, // binary expression

    // type declarations
    NODE_TYPE_ARRAY,   // array type
    NODE_TYPE_POINTER, // pointer type
    NODE_TYPE_FUN,     // function type
    NODE_TYPE_STR,     // struct type
    NODE_TYPE_UNI,     // union type
    NODE_FIELD,        // type field (struct/union members, function params)

    // keyworded statements
    NODE_STMT_VAL, // value declaration
    NODE_STMT_VAR, // variable declaration
    NODE_STMT_DEF, // type definition
    NODE_STMT_USE, // value declaration
    NODE_STMT_FUN, // function declaration
    NODE_STMT_EXT, // external type declaration
    NODE_STMT_IF,  // if statement
    NODE_STMT_OR,  // or statement
    NODE_STMT_FOR, // for statement
    NODE_STMT_BRK, // break statement
    NODE_STMT_CNT, // continue statement
    NODE_STMT_RET, // return statement
    NODE_STMT_ASM, // inline assembly block

    NODE_STMT_BLOCK, // block statement
    NODE_STMT_EXPR,  // expression statement
} NodeKind;

typedef struct Node
{
    NodeKind kind;
    Token *token;

    struct Node *parent; // parent node (scope or owner, not linked list)
    struct Node *next;   // used for linked lists

    union
    {
        struct
        {
            char *message;
        } error;
        struct
        {
            struct Node *statements;
        } program;
        // identifier

        // lit_int
        // lit_float
        // lit_char
        // lit_string

        struct expr_member
        {
            struct Node *target;
            struct Node *identifier;
        } expr_member;
        struct expr_call
        {
            struct Node *target;
            struct Node *arguments;
        } expr_call;
        struct expr_index
        {
            struct Node *target;
            struct Node *index;
        } expr_index;
        struct expr_cast
        {
            struct Node *target;
            struct Node *type;
        } expr_cast;
        struct expr_new
        {
            struct Node *type;
            struct Node *initializers;
        } expr_new;
        struct expr_unary
        {
            struct Node *right;
        } expr_unary;
        struct expr_binary
        {
            struct Node *left;
            struct Node *right;
        } expr_binary;

        struct type_arr
        {
            struct Node *size;
            struct Node *type;
        } type_arr;
        struct type_ptr
        {
            struct Node *type;
        } type_ptr;
        struct type_fun
        {
            struct Node *parameters;
            struct Node *return_type;
        } type_fun;
        struct type_str
        {
            struct Node *fields;
        } type_str;
        struct type_uni
        {
            struct Node *fields;
        } type_uni;
        struct field
        {
            struct Node *identifier;
            struct Node *type;
        } field;

        struct stmt_val
        {
            struct Node *identifier;
            struct Node *type;
            struct Node *initializer;
        } stmt_val;
        struct stmt_var
        {
            struct Node *identifier;
            struct Node *type;
            struct Node *initializer;
        } stmt_var;
        struct stmt_def
        {
            struct Node *identifier;
            struct Node *type;
        } stmt_def;
        struct stmt_use
        {
            struct Node *path;
        } stmt_use;
        struct stmt_fun
        {
            struct Node *identifier;
            struct Node *parameters;
            struct Node *return_type;
            struct Node *body;
        } stmt_fun;
        struct stmt_ext
        {
            struct Node *identifier;
            struct Node *type;
        } stmt_ext;
        struct stmt_if
        {
            struct Node *condition;
            struct Node *body;
        } stmt_if;
        struct stmt_or
        {
            struct Node *condition;
            struct Node *body;
        } stmt_or;
        struct stmt_for
        {
            struct Node *condition;
            struct Node *body;
        } stmt_for;
        // stmt_brk
        // stmt_cnt
        struct stmt_ret
        {
            struct Node *value;
        } stmt_ret;
        struct stmt_asm
        {
            struct Node *code;
        } stmt_asm;
        struct stmt_block
        {
            struct Node *statements;
        } stmt_block;
        struct stmt_expr
        {
            struct Node *expression;
        } stmt_expr;
    };
} Node;

Node *node_new(NodeKind kind, Token *token);
void node_free(Node *node);
void node_list_free(Node *node);

Node *node_find_parent(Node *node, NodeKind kind);

Node *node_list_add(Node *list, Node *node);
int node_list_len(Node *list);

char *node_kind_string(NodeKind kind);
void node_print(Node *node, char *prefix, int indent);

#endif // NODE_H
