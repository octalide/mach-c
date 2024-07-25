#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "analyzer.h"
#include "visitor.h"

void analyzer_init(Analyzer *analyzer)
{
    analyzer->symbol_table = symbol_table_new(256);
    analyzer->errors = calloc(1, sizeof(AnalyzerError *));
    analyzer->errors[0] = NULL;
}

void analyzer_free(Analyzer *analyzer)
{
    if (analyzer == NULL)
    {
        return;
    }

    if (analyzer->symbol_table != NULL)
    {
        symbol_table_free(analyzer->symbol_table);
        free(analyzer->symbol_table);
    }

    if (analyzer->errors != NULL)
    {
        for (int i = 0; analyzer->errors[i] != NULL; i++)
        {
            free(analyzer->errors[i]);
        }

        free(analyzer->errors);
    }
}

void analyzer_add_error(Analyzer *analyzer, Node *node, const char *message)
{
    AnalyzerError *error = (AnalyzerError *)malloc(sizeof(AnalyzerError));
    error->message = strdup(message);
    error->node = node;

    int i = 0;
    while (analyzer->errors[i] != NULL)
    {
        i++;
    }

    analyzer->errors = realloc(analyzer->errors, (i + 2) * sizeof(AnalyzerError *));
    analyzer->errors[i] = error;
    analyzer->errors[i + 1] = NULL;
}

void analyzer_add_errorf(Analyzer *analyzer, Node *node, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    char buffer[1024];
    vsnprintf(buffer, 1024, fmt, args);

    analyzer_add_error(analyzer, node, buffer);

    va_end(args);
}

bool analyzer_has_errors(Analyzer *analyzer)
{
    return analyzer->errors[0] != NULL;
}

void analyzer_print_errors(Analyzer *analyzer)
{
    int i = 0;
    while (analyzer->errors[i] != NULL)
    {
        printf("error: %s\n", analyzer->errors[i]->message);
        i++;
    }
}

// bool analyzer_cb_visit_node_identifier(void *context, Node *node);
// bool analyzer_cb_visit_node_identifier(void *context, Node *node);

void analyzer_cb_visit_expr_identifier(void *context, Node *node)
{
    AnalyzerVisitorContext *ctx = (AnalyzerVisitorContext *)context;
    Analyzer *analyzer = ctx->analyzer;
}

void analyzer_analyze(Analyzer *analyzer, Node *ast)
{
    if (ast == NULL)
    {
        return;
    }

    Visitor visitor;
    visitor_init(&visitor);

    visitor.cb_visit_node_block = NULL;

    AnalyzerVisitorContext context;
    context.analyzer = analyzer;

    // NOTE: commented functions are unused portions of the visitor pattern
    // visitor.cb_visit_node_block = analyzer_cb_visit_node_block;
    // visitor.cb_visit_node_identifier = analyzer_cb_visit_node_identifier;
    visitor.cb_visit_expr_identifier = analyzer_cb_visit_expr_identifier;
    visitor.cb_visit_expr_member = analyzer_cb_visit_expr_member;
    visitor.cb_visit_expr_lit_char = analyzer_cb_visit_expr_lit_char;
    visitor.cb_visit_expr_lit_number = analyzer_cb_visit_expr_lit_number;
    visitor.cb_visit_expr_lit_string = analyzer_cb_visit_expr_lit_string;
    visitor.cb_visit_expr_call = analyzer_cb_visit_expr_call;
    visitor.cb_visit_expr_index = analyzer_cb_visit_expr_index;
    visitor.cb_visit_expr_def_str = analyzer_cb_visit_expr_def_str;
    visitor.cb_visit_expr_def_uni = analyzer_cb_visit_expr_def_uni;
    visitor.cb_visit_expr_def_array = analyzer_cb_visit_expr_def_array;
    visitor.cb_visit_expr_unary = analyzer_cb_visit_expr_unary;
    visitor.cb_visit_expr_binary = analyzer_cb_visit_expr_binary;
    visitor.cb_visit_type_array = analyzer_cb_visit_type_array;
    visitor.cb_visit_type_ref = analyzer_cb_visit_type_ref;
    visitor.cb_visit_type_fun = analyzer_cb_visit_type_fun;
    visitor.cb_visit_type_str = analyzer_cb_visit_type_str;
    visitor.cb_visit_type_uni = analyzer_cb_visit_type_uni;
    visitor.cb_visit_node_field = analyzer_cb_visit_node_field;
    visitor.cb_visit_stmt_use = analyzer_cb_visit_stmt_use;
    visitor.cb_visit_stmt_if = analyzer_cb_visit_stmt_if;
    visitor.cb_visit_stmt_or = analyzer_cb_visit_stmt_or;
    visitor.cb_visit_stmt_for = analyzer_cb_visit_stmt_for;
    visitor.cb_visit_stmt_ret = analyzer_cb_visit_stmt_ret;
    visitor.cb_visit_stmt_decl_type = analyzer_cb_visit_stmt_decl_type;
    visitor.cb_visit_stmt_decl_val = analyzer_cb_visit_stmt_decl_val;
    visitor.cb_visit_stmt_decl_var = analyzer_cb_visit_stmt_decl_var;
    visitor.cb_visit_stmt_decl_fun = analyzer_cb_visit_stmt_decl_fun;
    visitor.cb_visit_stmt_decl_str = analyzer_cb_visit_stmt_decl_str;
    visitor.cb_visit_stmt_decl_uni = analyzer_cb_visit_stmt_decl_uni;
    visitor.cb_visit_stmt_expr = analyzer_cb_visit_stmt_expr;
    visitor.cb_visit_node_stmt = analyzer_cb_visit_node_stmt;
    visitor.cb_visit_node = analyzer_cb_visit_node;

    visit_node(&visitor, ast);
}
