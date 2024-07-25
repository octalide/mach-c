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

void analyzer_init(Analyzer *analyzer);
void analyzer_free(Analyzer *analyzer);

void analyzer_add_error(Analyzer *analyzer, Node *node, const char *message);
void analyzer_add_errorf(Analyzer *analyzer, Node *node, const char *fmt, ...);
bool analyzer_has_errors(Analyzer *analyzer);
void analyzer_print_errors(Analyzer *analyzer);

void analyzer_analyze(Analyzer *analyzer, Node *ast);

#endif // ANALYZER_H
