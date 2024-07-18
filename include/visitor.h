#ifndef VISITOR_H
#define VISITOR_H

#include "parser.h"

typedef struct Visitor
{
    void (*visit_node)(struct Visitor *visitor, Node *node);
    void (*visit_program)(struct Visitor *visitor, Node *node);
    void (*visit_expr_identifier)(struct Visitor *visitor, Node *node);
    void (*visit_expr_literal)(struct Visitor *visitor, Node *node);
    void (*visit_expr_array)(struct Visitor *visitor, Node *node);
    void (*visit_expr_type)(struct Visitor *visitor, Node *node);
    void (*visit_expr_type_array)(struct Visitor *visitor, Node *node);
    void (*visit_expr_type_ref)(struct Visitor *visitor, Node *node);
    void (*visit_expr_type_member)(struct Visitor *visitor, Node *node);
    void (*visit_expr_unary)(struct Visitor *visitor, Node *node);
    void (*visit_expr_binary)(struct Visitor *visitor, Node *node);
    void (*visit_expr_call)(struct Visitor *visitor, Node *node);
    void (*visit_expr_index)(struct Visitor *visitor, Node *node);
    void (*visit_expr_assign)(struct Visitor *visitor, Node *node);
    void (*visit_stmt_block)(struct Visitor *visitor, Node *node);
    void (*visit_stmt_use)(struct Visitor *visitor, Node *node);
    void (*visit_stmt_fun)(struct Visitor *visitor, Node *node);
    void (*visit_stmt_str)(struct Visitor *visitor, Node *node);
    void (*visit_stmt_val)(struct Visitor *visitor, Node *node);
    void (*visit_stmt_var)(struct Visitor *visitor, Node *node);
    void (*visit_stmt_if)(struct Visitor *visitor, Node *node);
    void (*visit_stmt_or)(struct Visitor *visitor, Node *node);
    void (*visit_stmt_for)(struct Visitor *visitor, Node *node);
    void (*visit_stmt_brk)(struct Visitor *visitor, Node *node);
    void (*visit_stmt_cnt)(struct Visitor *visitor, Node *node);
    void (*visit_stmt_ret)(struct Visitor *visitor, Node *node);
    void (*visit_stmt_expr)(struct Visitor *visitor, Node *node);
} Visitor;

#endif // VISITOR_H
