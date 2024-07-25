#ifndef VISITOR_H
#define VISITOR_H

#include <stdbool.h>

#include "ast.h"

typedef struct Visitor
{
    // user defined context
    void *context;

    bool (*cb_visit_node_block)(void *context, Node *node);
    bool (*cb_visit_node_identifier)(void *context, Node *node);
    bool (*cb_visit_expr_identifier)(void *context, Node *node);
    bool (*cb_visit_expr_member)(void *context, Node *node);
    bool (*cb_visit_expr_lit_char)(void *context, Node *node);
    bool (*cb_visit_expr_lit_number)(void *context, Node *node);
    bool (*cb_visit_expr_lit_string)(void *context, Node *node);
    bool (*cb_visit_expr_call)(void *context, Node *node);
    bool (*cb_visit_expr_index)(void *context, Node *node);
    bool (*cb_visit_expr_def_str)(void *context, Node *node);
    bool (*cb_visit_expr_def_uni)(void *context, Node *node);
    bool (*cb_visit_expr_def_array)(void *context, Node *node);
    bool (*cb_visit_expr_unary)(void *context, Node *node);
    bool (*cb_visit_expr_binary)(void *context, Node *node);
    bool (*cb_visit_type_array)(void *context, Node *node);
    bool (*cb_visit_type_ref)(void *context, Node *node);
    bool (*cb_visit_type_fun)(void *context, Node *node);
    bool (*cb_visit_type_str)(void *context, Node *node);
    bool (*cb_visit_type_uni)(void *context, Node *node);
    bool (*cb_visit_node_field)(void *context, Node *node);
    bool (*cb_visit_stmt_use)(void *context, Node *node);
    bool (*cb_visit_stmt_if)(void *context, Node *node);
    bool (*cb_visit_stmt_or)(void *context, Node *node);
    bool (*cb_visit_stmt_for)(void *context, Node *node);
    bool (*cb_visit_stmt_ret)(void *context, Node *node);
    bool (*cb_visit_stmt_decl_type)(void *context, Node *node);
    bool (*cb_visit_stmt_decl_val)(void *context, Node *node);
    bool (*cb_visit_stmt_decl_var)(void *context, Node *node);
    bool (*cb_visit_stmt_decl_fun)(void *context, Node *node);
    bool (*cb_visit_stmt_decl_str)(void *context, Node *node);
    bool (*cb_visit_stmt_decl_uni)(void *context, Node *node);
    bool (*cb_visit_stmt_expr)(void *context, Node *node);
    bool (*cb_visit_node_stmt)(void *context, Node *node);
    bool (*cb_visit_node)(void *context, Node *node);
} Visitor;

void visitor_init(Visitor *visitor);

void visit_node_block(Visitor *visitor, Node *node);
void visit_node_identifier(Visitor *visitor, Node *node);
void visit_expr_identifier(Visitor *visitor, Node *node);
void visit_expr_member(Visitor *visitor, Node *node);
void visit_expr_lit_char(Visitor *visitor, Node *node);
void visit_expr_lit_number(Visitor *visitor, Node *node);
void visit_expr_lit_string(Visitor *visitor, Node *node);
void visit_expr_call(Visitor *visitor, Node *node);
void visit_expr_index(Visitor *visitor, Node *node);
void visit_expr_def_str(Visitor *visitor, Node *node);
void visit_expr_def_uni(Visitor *visitor, Node *node);
void visit_expr_def_array(Visitor *visitor, Node *node);
void visit_expr_unary(Visitor *visitor, Node *node);
void visit_expr_binary(Visitor *visitor, Node *node);
void visit_type_array(Visitor *visitor, Node *node);
void visit_type_ref(Visitor *visitor, Node *node);
void visit_type_fun(Visitor *visitor, Node *node);
void visit_type_str(Visitor *visitor, Node *node);
void visit_type_uni(Visitor *visitor, Node *node);
void visit_node_field(Visitor *visitor, Node *node);
void visit_stmt_use(Visitor *visitor, Node *node);
void visit_stmt_if(Visitor *visitor, Node *node);
void visit_stmt_or(Visitor *visitor, Node *node);
void visit_stmt_for(Visitor *visitor, Node *node);
void visit_stmt_ret(Visitor *visitor, Node *node);
void visit_stmt_decl_type(Visitor *visitor, Node *node);
void visit_stmt_decl_val(Visitor *visitor, Node *node);
void visit_stmt_decl_var(Visitor *visitor, Node *node);
void visit_stmt_decl_fun(Visitor *visitor, Node *node);
void visit_stmt_decl_str(Visitor *visitor, Node *node);
void visit_stmt_decl_uni(Visitor *visitor, Node *node);
void visit_stmt_expr(Visitor *visitor, Node *node);
void visit_node(Visitor *visitor, Node *node);

#endif // VISITOR_H
