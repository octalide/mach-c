#ifndef VISITOR_H
#define VISITOR_H

#include "parser.h"

typedef struct Visitor {
    void (*visit_node)(struct Visitor *visitor, Node *node);
    void (*visit_program)(struct Visitor *visitor, NodeProgram *node);
    void (*visit_identifier)(struct Visitor *visitor, NodeIdentifier *node);
    void (*visit_comment)(struct Visitor *visitor, NodeComment *node);
    void (*visit_statement)(struct Visitor *visitor, NodeStmt *node);
    void (*visit_expression)(struct Visitor *visitor, NodeExpr *node);
    void (*visit_block)(struct Visitor *visitor, NodeBlock *node);
    void (*visit_stmt_use)(struct Visitor *visitor, NodeStmtUse *node);
    void (*visit_stmt_fun_param)(struct Visitor *visitor, NodeStmtFunParam *node);
    void (*visit_stmt_fun)(struct Visitor *visitor, NodeStmtFun *node);
    void (*visit_stmt_str_field)(struct Visitor *visitor, NodeStmtStrField *node);
    void (*visit_stmt_str)(struct Visitor *visitor, NodeStmtStr *node);
    void (*visit_stmt_val)(struct Visitor *visitor, NodeStmtVal *node);
    void (*visit_stmt_var)(struct Visitor *visitor, NodeStmtVar *node);
    void (*visit_stmt_if)(struct Visitor *visitor, NodeStmtIf *node);
    void (*visit_stmt_for)(struct Visitor *visitor, NodeStmtFor *node);
    void (*visit_stmt_brk)(struct Visitor *visitor, NodeStmtBrk *node);
    void (*visit_stmt_cnt)(struct Visitor *visitor, NodeStmtCnt *node);
    void (*visit_stmt_ret)(struct Visitor *visitor, NodeStmtRet *node);
    void (*visit_stmt_assign)(struct Visitor *visitor, NodeStmtAssign *node);
    void (*visit_stmt_expr)(struct Visitor *visitor, NodeStmtExpr *node);
    void (*visit_expr_literal)(struct Visitor *visitor, NodeExprLiteral *node);
    void (*visit_expr_type)(struct Visitor *visitor, NodeExprType *node);
    void (*visit_expr_unary)(struct Visitor *visitor, NodeExprUnary *node);
    void (*visit_expr_binary)(struct Visitor *visitor, NodeExprBinary *node);
    void (*visit_expr_call)(struct Visitor *visitor, NodeExprCall *node);
    void (*visit_expr_index)(struct Visitor *visitor, NodeExprIndex *node);
} Visitor;

void visit_node(Visitor *visitor, Node *node);
void visit_program(Visitor *visitor, NodeProgram *node);
void visit_comment(Visitor *visitor, NodeComment *node);
void visit_statement(Visitor *visitor, NodeStmt *node);
void visit_expression(Visitor *visitor, NodeExpr *node);
void visit_block(Visitor *visitor, NodeBlock *node);
void visit_stmt_use(Visitor *visitor, NodeStmtUse *node);
void visit_stmt_fun_param(Visitor *visitor, NodeStmtFunParam *node);
void visit_stmt_fun(Visitor *visitor, NodeStmtFun *node);
void visit_stmt_str_field(Visitor *visitor, NodeStmtStrField *node);
void visit_stmt_str(Visitor *visitor, NodeStmtStr *node);
void visit_stmt_val(Visitor *visitor, NodeStmtVal *node);
void visit_stmt_var(Visitor *visitor, NodeStmtVar *node);
void visit_stmt_if(Visitor *visitor, NodeStmtIf *node);
void visit_stmt_for(Visitor *visitor, NodeStmtFor *node);
void visit_stmt_brk(Visitor *visitor, NodeStmtBrk *node);
void visit_stmt_cnt(Visitor *visitor, NodeStmtCnt *node);
void visit_stmt_ret(Visitor *visitor, NodeStmtRet *node);
void visit_stmt_assign(Visitor *visitor, NodeStmtAssign *node);
void visit_stmt_expr(Visitor *visitor, NodeStmtExpr *node);
void visit_expr_literal(Visitor *visitor, NodeExprLiteral *node);
void visit_expr_type(Visitor *visitor, NodeExprType *node);
void visit_expr_unary(Visitor *visitor, NodeExprUnary *node);
void visit_expr_binary(Visitor *visitor, NodeExprBinary *node);
void visit_expr_call(Visitor *visitor, NodeExprCall *node);
void visit_expr_index(Visitor *visitor, NodeExprIndex *node);

#endif // VISITOR_H
