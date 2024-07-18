#ifndef NODE_H
#define NODE_H

#include "token.h"

typedef enum
{
    NODE_BASE,

    NODE_PROGRAM,
    NODE_IDENTIFIER,
    NODE_COMMENT,

    NODE_STMT,
    NODE_EXPR,

    NODE_BLOCK,

    NODE_STMT_USE,
    NODE_STMT_FUN_PARAM,
    NODE_STMT_FUN,
    NODE_STMT_STR_FIELD,
    NODE_STMT_STR,
    NODE_STMT_VAL,
    NODE_STMT_VAR,
    NODE_STMT_IF,
    NODE_STMT_FOR,
    NODE_STMT_BRK,
    NODE_STMT_CNT,
    NODE_STMT_RET,
    NODE_STMT_ASSIGN,
    NODE_STMT_EXPR,

    NODE_EXPR_IDENTIFIER,
    NODE_EXPR_LITERAL,
    NODE_EXPR_TYPE,
    NODE_EXPR_UNARY,
    NODE_EXPR_BINARY,
    NODE_EXPR_ASSIGN,
    NODE_EXPR_CALL,
    NODE_EXPR_INDEX,
} NodeType;

typedef struct
{
    Token text;
} base_comment;

typedef struct
{
    struct Node *statements;
    int count;
} base_program;

typedef struct
{
    struct Node *statements;
    int count;
} base_block;

typedef struct
{
    Token name;
} expr_identifier;

typedef struct
{
    Token value;
} expr_literal;

typedef struct
{
    expr_identifier *identifier;
    bool is_reference;
} expr_type;

typedef struct
{
    expr_identifier *identifier;
    struct Node *size;
} expr_type_array;

typedef struct
{
    Token op;
    struct Node *operand;
} expr_unary;

typedef struct
{
    Token op;
    struct Node *left;
    struct Node *right;
} expr_binary;

typedef struct
{
    struct Node *target;
    struct Node *value;
} expr_assign;

typedef struct
{
    struct Node *callee;
    struct Node *arguments;
    int count;
} expr_call;

typedef struct
{
    struct Node *target;
    struct Node *index;
} expr_index;

typedef struct
{
    struct NodeStmtUse *stmt_use;
} stmt_use;

typedef struct Node
{
    NodeType type;
    struct Node *next;

    union
    {
        base_comment base_comment;
        base_program base_program;
        base_block base_block;

        expr_identifier expr_identifier;
        expr_literal expr_literal;
        expr_type expr_type;
        expr_type_array expr_type_array;
        expr_unary expr_unary;
        expr_binary expr_binary;
        expr_assign expr_assign;
        expr_call expr_call;
        expr_index expr_index;

        struct NodeStmtUse stmt_use;
        struct NodeStmtFunParam stmt_fun_param;
        struct NodeStmtFun stmt_fun;
        struct NodeStmtStrField stmt_str_field;
        struct NodeStmtStr stmt_str;
        struct NodeStmtVal stmt_val;
        struct NodeStmtVar stmt_var;
        struct NodeStmtIf stmt_if;
        struct NodeStmtFor stmt_for;
        struct NodeStmtBrk stmt_brk;
        struct NodeStmtCnt stmt_cnt;
        struct NodeStmtRet stmt_ret;
        struct NodeStmtExpr stmt_expr;
    } data;
} Node;

typedef struct NodeIdentifier
{
    Node base;
    Token name;
} NodeIdentifier;

typedef struct NodeComment
{
    Node base;
    Token comment;
} NodeComment;

typedef struct NodeStmt
{
    Node base;
} NodeStmt;

typedef struct NodeExpr
{
    Node base;
} NodeExpr;

typedef struct NodeProgram
{
    Node base;
    NodeStmt *statements;
    int count;
} NodeProgram;

typedef struct NodeExprType
{
    Node base;
    NodeIdentifier *identifier;
    bool is_reference;
} NodeExprType;

typedef struct NodeBlock
{
    Node base;
    NodeStmt *statements;
    int count;
} NodeBlock;

typedef struct NodeStmtUse
{
    Node base;
    NodeIdentifier *parts;
    int count;
} NodeStmtUse;

typedef struct NodeStmtFunParam
{
    Node base;
    NodeIdentifier *identifier;
    NodeExprType *type;
} NodeStmtFunParam;

typedef struct NodeStmtFun
{
    Node base;
    NodeIdentifier *identifier;
    NodeStmtFunParam *params;
    int count;
    NodeExprType *return_type;
    NodeBlock *body;
} NodeStmtFun;

typedef struct NodeStmtStrField
{
    Node base;
    NodeIdentifier *identifier;
    NodeExprType *type;
} NodeStmtStrField;

typedef struct NodeStmtStr
{
    Node base;
    NodeIdentifier *identifier;
    NodeStmtStrField *fields;
    int count;
} NodeStmtStr;

typedef struct NodeStmtVal
{
    Node base;
    NodeIdentifier *identifier;
    NodeExprType *type;
    NodeExpr *initializer;
} NodeStmtVal;

typedef struct NodeStmtVar
{
    Node base;
    NodeIdentifier *identifier;
    NodeExprType *type;
    NodeExpr *initializer;
} NodeStmtVar;

typedef struct NodeStmtIf
{
    Node base;
    NodeExpr *condition;
    NodeBlock *body;
    struct NodeStmtIf *branch;
} NodeStmtIf;

typedef struct NodeStmtFor
{
    Node base;
    NodeExpr *condition;
    NodeBlock *body;
} NodeStmtFor;

typedef struct NodeStmtBrk
{
    Node base;
} NodeStmtBrk;

typedef struct NodeStmtCnt
{
    Node base;
} NodeStmtCnt;

typedef struct NodeStmtRet
{
    Node base;
    NodeExpr *value;
} NodeStmtRet;

typedef struct NodeStmtExpr
{
    Node base;
    NodeExpr *expression;
} NodeStmtExpr;

typedef struct NodeExprIdentifier
{
    Node base;
    NodeIdentifier *identifier;
} NodeExprIdentifier;

typedef struct NodeExprLiteral
{
    Node base;
    Token value;
} NodeExprLiteral;

typedef struct NodeExprUnary
{
    Node base;
    Token op;
    NodeExpr *operand;
} NodeExprUnary;

typedef struct NodeExprBinary
{
    Node base;
    Token op;
    NodeExpr *left;
    NodeExpr *right;
} NodeExprBinary;

typedef struct NodeExprAssign
{
    Node base;
    NodeExprIdentifier *target;
    NodeExpr *value;
} NodeExprAssign;

typedef struct NodeExprCall
{
    Node base;
    NodeExpr *callee;
    NodeExpr *arguments;
    int count;
} NodeExprCall;

typedef struct NodeExprIndex
{
    Node base;
    NodeExpr *target;
    NodeExpr *index;
} NodeExprIndex;

Node *new_node(NodeType type);
NodeProgram *new_node_program(NodeStmt *statements, int count);
NodeIdentifier *new_node_identifier(Token name);
NodeComment *new_node_comment(Token comment);
NodeStmt *new_node_stmt();
NodeExpr *new_node_expr();
NodeBlock *new_node_block(NodeStmt *statements, int count);
NodeStmtUse *new_node_stmt_use(NodeIdentifier *parts, int count);
NodeStmtFunParam *new_node_stmt_fun_param(NodeIdentifier *identifier, NodeExprType *type);
NodeStmtFun *new_node_stmt_fun(NodeIdentifier *identifier, NodeStmtFunParam *params, int count, NodeExprType *return_type, NodeBlock *body);
NodeStmtStrField *new_node_stmt_str_field(NodeIdentifier *identifier, NodeExprType *type);
NodeStmtStr *new_node_stmt_str(NodeIdentifier *identifier, NodeStmtStrField *fields, int count);
NodeStmtVal *new_node_stmt_val(NodeIdentifier *identifier, NodeExprType *type, NodeExpr *initializer);
NodeStmtVar *new_node_stmt_var(NodeIdentifier *identifier, NodeExprType *type, NodeExpr *initializer);
NodeStmtIf *new_node_stmt_if(NodeExpr *condition, NodeBlock *body, NodeStmtIf *branch);
NodeStmtFor *new_node_stmt_for(NodeExpr *condition, NodeBlock *body);
NodeStmtBrk *new_node_stmt_brk();
NodeStmtCnt *new_node_stmt_cnt();
NodeStmtRet *new_node_stmt_ret(NodeExpr *value);
NodeStmtExpr *new_node_stmt_expr(NodeExpr *expression);

NodeExprLiteral *new_node_expr_literal(Token value);
NodeExprIdentifier *new_node_expr_identifier(NodeIdentifier *identifier);
NodeExprType *new_node_expr_type(NodeIdentifier *identifier, bool is_reference);
NodeExprUnary *new_node_expr_unary(Token op, NodeExpr *operand);
NodeExprBinary *new_node_expr_binary(Token op, NodeExpr *left, NodeExpr *right);
NodeExprAssign *new_node_expr_assign(NodeIdentifier *target, NodeExpr *value);
NodeExprCall *new_node_expr_call(NodeExpr *callee, NodeExpr *arguments, int count);
NodeExprIndex *new_node_expr_index(NodeExpr *target, NodeExpr *index);

char *node_type_string(NodeType type);

#endif // NODE_H
