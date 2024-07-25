#ifndef ANALYZER_H
#define ANALYZER_H

#include <stdbool.h>

#include "ast.h"
#include "symbol_table.h"

typedef struct AnalyzerError
{
    const char *message;
    Node *node;
} AnalyzerError;

typedef struct Analyzer {
    SymbolTable *symbol_table;

    AnalyzerError **errors;

    bool opt_verbose;
} Analyzer;

typedef struct AnalyzerVisitorContext
{
    Analyzer *analyzer;
} AnalyzerVisitorContext;

void analyzer_init(Analyzer *analyzer);
void analyzer_free(Analyzer *analyzer);

void analyzer_add_error(Analyzer *analyzer, Node *node, const char *message);
void analyzer_add_errorf(Analyzer *analyzer, Node *node, const char *fmt, ...);
bool analyzer_has_errors(Analyzer *analyzer);
void analyzer_print_errors(Analyzer *analyzer);

// NOTE: commented functions are unused portions of the visitor pattern
// bool analyzer_cb_visit_node_block(void *context, Node *node);
// bool analyzer_cb_visit_node_identifier(void *context, Node *node);
bool analyzer_cb_visit_expr_identifier(void *context, Node *node);
bool analyzer_cb_visit_expr_member(void *context, Node *node);
bool analyzer_cb_visit_expr_lit_char(void *context, Node *node);
bool analyzer_cb_visit_expr_lit_number(void *context, Node *node);
bool analyzer_cb_visit_expr_lit_string(void *context, Node *node);
bool analyzer_cb_visit_expr_call(void *context, Node *node);
bool analyzer_cb_visit_expr_index(void *context, Node *node);
bool analyzer_cb_visit_expr_def_str(void *context, Node *node);
bool analyzer_cb_visit_expr_def_uni(void *context, Node *node);
bool analyzer_cb_visit_expr_def_array(void *context, Node *node);
bool analyzer_cb_visit_expr_unary(void *context, Node *node);
bool analyzer_cb_visit_expr_binary(void *context, Node *node);
bool analyzer_cb_visit_type_array(void *context, Node *node);
bool analyzer_cb_visit_type_ref(void *context, Node *node);
bool analyzer_cb_visit_type_fun(void *context, Node *node);
bool analyzer_cb_visit_type_str(void *context, Node *node);
bool analyzer_cb_visit_type_uni(void *context, Node *node);
bool analyzer_cb_visit_node_field(void *context, Node *node);
bool analyzer_cb_visit_stmt_use(void *context, Node *node);
bool analyzer_cb_visit_stmt_if(void *context, Node *node);
bool analyzer_cb_visit_stmt_or(void *context, Node *node);
bool analyzer_cb_visit_stmt_for(void *context, Node *node);
bool analyzer_cb_visit_stmt_ret(void *context, Node *node);
bool analyzer_cb_visit_stmt_decl_type(void *context, Node *node);
bool analyzer_cb_visit_stmt_decl_val(void *context, Node *node);
bool analyzer_cb_visit_stmt_decl_var(void *context, Node *node);
bool analyzer_cb_visit_stmt_decl_fun(void *context, Node *node);
bool analyzer_cb_visit_stmt_decl_str(void *context, Node *node);
bool analyzer_cb_visit_stmt_decl_uni(void *context, Node *node);
bool analyzer_cb_visit_stmt_expr(void *context, Node *node);
bool analyzer_cb_visit_node_stmt(void *context, Node *node);
bool analyzer_cb_visit_node(void *context, Node *node);

void analyzer_analyze(Analyzer *analyzer, Node *ast);

#endif // ANALYZER_H
