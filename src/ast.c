#include <stdlib.h>
#include <stdio.h>
#include "ast.h"

Node *new_node(NodeType type)
{
    Node *node = malloc(sizeof(Node));
    node->type = type;

    return node;
}

NodeProgram *new_node_program(NodeStmt *statements, int count)
{
    NodeProgram *node = malloc(sizeof(NodeProgram));
    node->base.type = NODE_PROGRAM;
    node->statements = statements;
    node->count = count;

    return node;
}

NodeIdentifier *new_node_identifier(Token name)
{
    NodeIdentifier *node = malloc(sizeof(NodeIdentifier));
    node->base.type = NODE_IDENTIFIER;
    node->name = name;

    return node;
}

NodeComment *new_node_comment(Token comment)
{
    NodeComment *node = malloc(sizeof(NodeComment));
    node->base.type = NODE_COMMENT;
    node->comment = comment;

    return node;
}

NodeStmt *new_node_stmt()
{
    NodeStmt *node = malloc(sizeof(NodeStmt));
    node->base.type = NODE_STMT;

    return node;
}

NodeExpr *new_node_expr()
{
    NodeExpr *node = malloc(sizeof(NodeExpr));
    node->base.type = NODE_EXPR;

    return node;
}

NodeBlock *new_node_block(NodeStmt *statements, int count)
{
    NodeBlock *node = malloc(sizeof(NodeBlock));
    node->base.type = NODE_BLOCK;
    node->statements = statements;
    node->count = count;

    return node;
}

NodeStmtUse *new_node_stmt_use(NodeIdentifier *parts, int count)
{
    NodeStmtUse *node = malloc(sizeof(NodeStmtUse));
    node->base.type = NODE_STMT_USE;
    node->parts = parts;
    node->count = count;

    return node;
}

NodeStmtFunParam *new_node_stmt_fun_param(NodeIdentifier *identifier, NodeExprType *type)
{
    NodeStmtFunParam *node = malloc(sizeof(NodeStmtFunParam));
    node->base.type = NODE_STMT_FUN_PARAM;
    node->identifier = identifier;
    node->type = type;

    return node;
}

NodeStmtFun *new_node_stmt_fun(NodeIdentifier *identifier, NodeStmtFunParam *params, int count, NodeExprType *return_type, NodeBlock *body)
{
    NodeStmtFun *node = malloc(sizeof(NodeStmtFun));
    node->base.type = NODE_STMT_FUN;
    node->identifier = identifier;
    node->params = params;
    node->count = count;
    node->return_type = return_type;
    node->body = body;

    return node;
}

NodeStmtStrField *new_node_stmt_str_field(NodeIdentifier *identifier, NodeExprType *type)
{
    NodeStmtStrField *node = malloc(sizeof(NodeStmtStrField));
    node->base.type = NODE_STMT_STR_FIELD;
    node->identifier = identifier;
    node->type = type;

    return node;
}

NodeStmtStr *new_node_stmt_str(NodeIdentifier *identifier, NodeStmtStrField *fields, int count)
{
    NodeStmtStr *node = malloc(sizeof(NodeStmtStr));
    node->base.type = NODE_STMT_STR;
    node->identifier = identifier;
    node->fields = fields;
    node->count = count;

    return node;
}

NodeStmtVal *new_node_stmt_val(NodeIdentifier *identifier, NodeExprType *type, NodeExpr *initializer)
{
    NodeStmtVal *node = malloc(sizeof(NodeStmtVal));
    node->base.type = NODE_STMT_VAL;
    node->identifier = identifier;
    node->type = type;
    node->initializer = initializer;

    return node;
}

NodeStmtVar *new_node_stmt_var(NodeIdentifier *identifier, NodeExprType *type, NodeExpr *initializer)
{
    NodeStmtVar *node = malloc(sizeof(NodeStmtVar));
    node->base.type = NODE_STMT_VAR;
    node->identifier = identifier;
    node->type = type;
    node->initializer = initializer;

    return node;
}

NodeStmtIf *new_node_stmt_if(NodeExpr *condition, NodeBlock *body, NodeStmtIf *branch)
{
    NodeStmtIf *node = malloc(sizeof(NodeStmtIf));
    node->base.type = NODE_STMT_IF;
    node->condition = condition;
    node->body = body;
    node->branch = branch;

    return node;
}

NodeStmtFor *new_node_stmt_for(NodeExpr *condition, NodeBlock *body)
{
    NodeStmtFor *node = malloc(sizeof(NodeStmtFor));
    node->base.type = NODE_STMT_FOR;
    node->condition = condition;
    node->body = body;

    return node;
}

NodeStmtBrk *new_node_stmt_brk()
{
    NodeStmtBrk *node = malloc(sizeof(NodeStmtBrk));
    node->base.type = NODE_STMT_BRK;

    return node;
}

NodeStmtCnt *new_node_stmt_cnt()
{
    NodeStmtCnt *node = malloc(sizeof(NodeStmtCnt));
    node->base.type = NODE_STMT_CNT;

    return node;
}

NodeStmtRet *new_node_stmt_ret(NodeExpr *value)
{
    NodeStmtRet *node = malloc(sizeof(NodeStmtRet));
    node->base.type = NODE_STMT_RET;
    node->value = value;

    return node;
}

NodeStmtExpr *new_node_stmt_expr(NodeExpr *expression)
{
    NodeStmtExpr *node = malloc(sizeof(NodeStmtExpr));
    node->base.type = NODE_STMT_EXPR;
    node->expression = expression;

    return node;
}

NodeExprIdentifier *new_node_expr_identifier(NodeIdentifier *identifier)
{
    NodeExprIdentifier *node = malloc(sizeof(NodeExprIdentifier));
    node->base.type = NODE_EXPR_IDENTIFIER;
    node->identifier = identifier;

    return node;
}

NodeExprLiteral *new_node_expr_literal(Token value)
{
    NodeExprLiteral *node = malloc(sizeof(NodeExprLiteral));
    node->base.type = NODE_EXPR_LITERAL;
    node->value = value;

    return node;
}

NodeExprType *new_node_expr_type(NodeIdentifier *identifier, bool is_reference)
{
    NodeExprType *node = malloc(sizeof(NodeExprType));
    node->base.type = NODE_EXPR_TYPE;
    node->identifier = identifier;
    node->is_reference = is_reference;

    return node;
}

NodeExprUnary *new_node_expr_unary(Token op, NodeExpr *operand)
{
    NodeExprUnary *node = malloc(sizeof(NodeExprUnary));
    node->base.type = NODE_EXPR_UNARY;
    node->op = op;
    node->operand = operand;

    return node;
}

NodeExprBinary *new_node_expr_binary(Token op, NodeExpr *left, NodeExpr *right)
{
    NodeExprBinary *node = malloc(sizeof(NodeExprBinary));
    node->base.type = NODE_EXPR_BINARY;
    node->op = op;
    node->left = left;
    node->right = right;

    return node;
}

NodeExprAssign *new_node_expr_assign(NodeExprIdentifier *target, NodeExpr *value)
{
    NodeExprAssign *node = malloc(sizeof(NodeExprAssign));
    node->base.type = NODE_EXPR_ASSIGN;
    node->target = target;
    node->value = value;

    return node;
}

NodeExprCall *new_node_expr_call(NodeExpr *callee, NodeExpr *arguments, int count)
{
    NodeExprCall *node = malloc(sizeof(NodeExprCall));
    node->base.type = NODE_EXPR_CALL;
    node->callee = callee;
    node->arguments = arguments;
    node->count = count;

    return node;
}

NodeExprIndex *new_node_expr_index(NodeExpr *target, NodeExpr *index)
{
    NodeExprIndex *node = malloc(sizeof(NodeExprIndex));
    node->base.type = NODE_EXPR_INDEX;
    node->target = target;
    node->index = index;

    return node;
}

char *node_type_string(NodeType type)
{
    switch (type)
    {
    case NODE_PROGRAM:
        return "PROGRAM";
    case NODE_IDENTIFIER:
        return "IDENTIFIER";
    case NODE_COMMENT:
        return "COMMENT";
    case NODE_STMT:
        return "STMT";
    case NODE_EXPR:
        return "EXPR";
    case NODE_BLOCK:
        return "BLOCK";
    case NODE_STMT_USE:
        return "STMT_USE";
    case NODE_STMT_FUN_PARAM:
        return "STMT_FUN_PARAM";
    case NODE_STMT_FUN:
        return "STMT_FUN";
    case NODE_STMT_STR_FIELD:
        return "STMT_STR_FIELD";
    case NODE_STMT_STR:
        return "STMT_STR";
    case NODE_STMT_VAL:
        return "STMT_VAL";
    case NODE_STMT_VAR:
        return "STMT_VAR";
    case NODE_STMT_IF:
        return "STMT_IF";
    case NODE_STMT_FOR:
        return "STMT_FOR";
    case NODE_STMT_BRK:
        return "STMT_BRK";
    case NODE_STMT_CNT:
        return "STMT_CNT";
    case NODE_STMT_RET:
        return "STMT_RET";
    case NODE_STMT_ASSIGN:
        return "STMT_ASSIGN";
    case NODE_STMT_EXPR:
        return "STMT_EXPR";
    case NODE_EXPR_LITERAL:
        return "EXPR_LITERAL";
    case NODE_EXPR_IDENTIFIER:
        return "EXPR_IDENTIFIER";
    case NODE_EXPR_ASSIGN:
        return "EXPR_ASSIGN";
    case NODE_EXPR_TYPE:
        return "EXPR_TYPE";
    case NODE_EXPR_UNARY:
        return "EXPR_UNARY";
    case NODE_EXPR_BINARY:
        return "EXPR_BINARY";
    case NODE_EXPR_CALL:
        return "EXPR_CALL";
    case NODE_EXPR_INDEX:
        return "EXPR_INDEX";
    default:
        return "UNKNOWN";
    }
}
