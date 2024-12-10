#ifndef AST_H
#define AST_H

#include "operator.h"

#include <stdbool.h>

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

typedef struct NodeError
{
    char *message;
} NodeError;

typedef struct NodeProgram
{
    struct Node **statements;
} NodeProgram;

typedef struct NodeIdentifier
{
    char *name;
} NodeIdentifier;

typedef struct NodeLitInt
{
    unsigned long long value;
} NodeLitInt;

typedef struct NodeLitFloat
{
    double value;
} NodeLitFloat;

typedef struct NodeLitChar
{
    char value;
} NodeLitChar;

typedef struct NodeLitString
{
    char *value;
} NodeLitString;

typedef struct NodeExprMember
{
    struct Node *target;
    struct Node *member;
} NodeExprMember;

typedef struct NodeExprCall
{
    struct Node *target;
    struct Node **arguments;
} NodeExprCall;

typedef struct NodeExprIndex
{
    struct Node *target;
    struct Node *index;
} NodeExprIndex;

typedef struct NodeExprCast
{
    struct Node *target;
    struct Node *type;
} NodeExprCast;

typedef struct NodeExprNew
{
    struct Node **initializers;
} NodeExprNew;

typedef struct NodeExprUnary
{
    Operator op;
    struct Node *target;
} NodeExprUnary;

typedef struct NodeExprBinary
{
    Operator op;
    struct Node *left;
    struct Node *right;
} NodeExprBinary;

typedef struct NodeTypeArray
{
    struct Node *size;
    struct Node *type;
} NodeTypeArray;

typedef struct NodeTypePointer
{
    struct Node *type;
} NodeTypePointer;

typedef struct NodeTypeFunction
{
    struct Node **parameters;
    struct Node *return_type;
} NodeTypeFunction;

typedef struct NodeTypeStruct
{
    struct Node **fields;
} NodeTypeStruct;

typedef struct NodeTypeUnion
{
    struct Node **fields;
} NodeTypeUnion;

typedef struct NodeField
{
    struct Node *identifier;
    struct Node *type;
} NodeField;

typedef struct NodeStmtVal
{
    struct Node *identifier;
    struct Node *type;
    struct Node *initializer;
} NodeStmtVal;

typedef struct NodeStmtVar
{
    struct Node *identifier;
    struct Node *type;
    struct Node *initializer;
} NodeStmtVar;

typedef struct NodeStmtDef
{
    struct Node *identifier;
    struct Node *type;
} NodeStmtDef;

typedef struct NodeStmtUse
{
    struct Node *alias;
    struct Node **module_parts;
} NodeStmtUse;

typedef struct NodeStmtFun
{
    struct Node *identifier;
    struct Node **parameters;
    struct Node *return_type;
    struct Node *body;
} NodeStmtFun;

typedef struct NodeStmtExt
{
    struct Node *identifier;
    struct Node *type;
} NodeStmtExt;

typedef struct NodeStmtIf
{
    struct Node *condition;
    struct Node *body;
} NodeStmtIf;

typedef struct NodeStmtOr
{
    struct Node *condition;
    struct Node *body;
} NodeStmtOr;

typedef struct NodeStmtFor
{
    struct Node *condition;
    struct Node *body;
} NodeStmtFor;

typedef struct NodeStmtBreak
{
} NodeStmtBreak;

typedef struct NodeStmtContinue
{
} NodeStmtContinue;

typedef struct NodeStmtReturn
{
    struct Node *value;
} NodeStmtReturn;

typedef struct NodeStmtAsm
{
    struct Node *code;
} NodeStmtAsm;

typedef struct NodeStmtBlock
{
    struct Node **statements;
} NodeStmtBlock;

typedef struct NodeStmtExpr
{
    struct Node *expression;
} NodeStmtExpr;

typedef struct Node
{
    NodeKind kind;
    Token *token;

    struct Node *parent;

    union data
    {
        NodeError *error;
        NodeProgram *program;
        NodeIdentifier *identifier;
        NodeLitInt *lit_int;
        NodeLitFloat *lit_float;
        NodeLitChar *lit_char;
        NodeLitString *lit_string;
        NodeExprMember *expr_member;
        NodeExprCall *expr_call;
        NodeExprIndex *expr_index;
        NodeExprCast *expr_cast;
        NodeExprNew *expr_new;
        NodeExprUnary *expr_unary;
        NodeExprBinary *expr_binary;
        NodeTypeArray *type_array;
        NodeTypePointer *type_pointer;
        NodeTypeFunction *type_function;
        NodeTypeStruct *type_struct;
        NodeTypeUnion *type_union;
        NodeField *field;
        NodeStmtVal *stmt_val;
        NodeStmtVar *stmt_var;
        NodeStmtDef *stmt_def;
        NodeStmtUse *stmt_use;
        NodeStmtFun *stmt_fun;
        NodeStmtExt *stmt_ext;
        NodeStmtIf *stmt_if;
        NodeStmtOr *stmt_or;
        NodeStmtFor *stmt_for;
        NodeStmtBreak *stmt_break;
        NodeStmtContinue *stmt_continue;
        NodeStmtReturn *stmt_return;
        NodeStmtAsm *stmt_asm;
        NodeStmtBlock *stmt_block;
        NodeStmtExpr *stmt_expr;
    } data;
} Node;

Node *node_new(NodeKind kind);
void node_free(Node *node);

char *node_kind_to_string(NodeKind kind);

Node **node_list_new();
int node_list_count(Node **list);
void node_list_add(Node ***list, Node *node);
void node_list_free(Node **list);

void node_walk(void *context, Node *node, void (*callback)(void *context, Node *node, int depth));

#endif
