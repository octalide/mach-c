#ifndef ANALYZER_H
#define ANALYZER_H

#include <stdbool.h>

#include "node.h"
#include "target.h"
#include "type.h"
#include "scope.h"

typedef struct AnalyzerError
{
    char *message;
    Node *node;
} AnalyzerError;

typedef struct Analyzer {
    Target target;

    Scope *scope;

    struct AnalyzerError **errors;

    bool resolve_all;
    bool opt_verbose;
} Analyzer;

Analyzer *analyzer_new(Target target);
void analyzer_free(Analyzer *analyzer);

void analyzer_scope_down(Analyzer *analyzer);
void analyzer_scope_up(Analyzer *analyzer);
void analyzer_scope_add_symbol(Analyzer *analyzer, Symbol *symbol);

void analyzer_add_builtins(Analyzer *analyzer);

void analyzer_add_error(Analyzer *analyzer, Node *node, char *message);
void analyzer_add_errorf(Analyzer *analyzer, Node *node, char *fmt, ...);
bool analyzer_has_errors(Analyzer *analyzer);
void analyzer_print_errors(Analyzer *analyzer);

void analyzer_analyze(Analyzer *analyzer, Node *ast);

#endif // ANALYZER_H
