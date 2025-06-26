#ifndef AST_H
#define AST_H

#include "operator.h"
#include "token.h"

#include <stdbool.h>
#include <stddef.h>

// forward declaration
typedef struct Node Node;

typedef enum NodeKind
{
    // basic nodes
    NODE_IDENTIFIER,
    NODE_PROGRAM,
    NODE_BLOCK,

    // literals
    NODE_LIT_INT,
    NODE_LIT_FLOAT,
    NODE_LIT_CHAR,
    NODE_LIT_STRING,

    // expressions
    NODE_EXPR_CALL,
    NODE_EXPR_INDEX,
    NODE_EXPR_MEMBER,
    NODE_EXPR_CAST,
    NODE_EXPR_UNARY,
    NODE_EXPR_BINARY,

    // types
    NODE_TYPE_ARRAY,
    NODE_TYPE_POINTER,
    NODE_TYPE_FUNCTION,
    NODE_TYPE_STRUCT,
    NODE_TYPE_UNION,

    // statements
    NODE_STMT_VAL,
    NODE_STMT_VAR,
    NODE_STMT_DEF,
    NODE_STMT_FUNCTION,
    NODE_STMT_STRUCT,
    NODE_STMT_UNION,
    NODE_STMT_EXTERNAL,
    NODE_STMT_IF,
    NODE_STMT_OR,
    NODE_STMT_FOR,
    NODE_STMT_BREAK,
    NODE_STMT_CONTINUE,
    NODE_STMT_RETURN,
    NODE_STMT_EXPRESSION,

    NODE_COUNT
} NodeKind;

// core AST node structure
struct Node
{
    NodeKind kind;
    Token   *token;

    // flexible data based on node type
    union
    {
        // basic values
        char              *str_value;   // for identifiers, strings
        unsigned long long int_value;   // for integers
        double             float_value; // for floats
        char               char_value;  // for characters

        // node references
        Node  *single;   // single child node
        Node **children; // array of child nodes (null-terminated)

        // specific structures for complex nodes
        struct
        {
            Node    *left;
            Node    *right;
            Operator op;
        } binary;

        struct
        {
            Node    *target;
            Operator op;
        } unary;

        struct
        {
            Node  *target;
            Node **args;
        } call;

        struct
        {
            Node *name;
            Node *type;
            Node *init;
        } decl;

        struct
        {
            Node **params;
            Node  *return_type;
            Node  *body;
            bool   is_variadic;
        } function;

        struct
        {
            Node *condition;
            Node *body;
        } conditional;

        struct
        {
            Node **fields;
        } composite;
    };
};

// core functions
void node_init(Node *node, NodeKind kind);
void node_dnit(Node *node);

// node creation helpers
Node *node_identifier(const char *name);
Node *node_int(unsigned long long value);
Node *node_float(double value);
Node *node_char(char value);
Node *node_string(const char *value);
Node *node_binary(Operator op, Node *left, Node *right);
Node *node_unary(Operator op, Node *target);
Node *node_call(Node *target, Node **args);
Node *node_block(Node **statements);

// list utilities
Node **node_list_new(void);
void   node_list_add(Node ***list, Node *node);
size_t node_list_count(Node **list);

// utilities
const char *node_kind_name(NodeKind kind);

#endif
