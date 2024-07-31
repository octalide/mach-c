#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "builtin.h"
#include "analyzer.h"

Analyzer *analyzer_new(Target target)
{
    Analyzer *analyzer = calloc(1, sizeof(Analyzer));
    analyzer->target = target;
    analyzer->scope = NULL;

    analyzer->errors = calloc(1, sizeof(AnalyzerError *));
    analyzer->errors[0] = NULL;

    analyzer->resolve_all = true;
    analyzer->opt_verbose = false;

    return analyzer;
}

void analyzer_free(Analyzer *analyzer)
{
    if (analyzer == NULL)
    {
        return;
    }

    scope_free(analyzer->scope);
    analyzer->scope = NULL;

    for (int i = 0; analyzer->errors[i] != NULL; i++)
    {
        free(analyzer->errors[i]);
        analyzer->errors[i] = NULL;
    }

    free(analyzer->errors);
    analyzer->errors = NULL;

    free(analyzer);
}

void analyzer_scope_down(Analyzer *analyzer)
{
    if (analyzer->scope == NULL)
    {
        return;
    }

    analyzer->scope = scope_down(analyzer->scope);
}

void analyzer_scope_up(Analyzer *analyzer)
{
    if (analyzer->scope == NULL)
    {
        return;
    }

    analyzer->scope = scope_up(analyzer->scope);
}

void analyzer_scope_add_symbol(Analyzer *analyzer, Symbol *symbol)
{
    if (analyzer->scope == NULL)
    {
        return;
    }

    scope_add_symbol(analyzer->scope, symbol);
}

// add builtin symbols into current scope
void analyzer_add_builtins(Analyzer *analyzer)
{
    for (int i = 0; i < BI_UNKNOWN; i++)
    {
        Type *type = builtin_type(BUILTIN_INFO_MAP[i].type, analyzer->target);
        if (type == NULL)
        {
            fprintf(stderr, "fatal: failed to create builtin type '%d'\n", BUILTIN_INFO_MAP[i].type);
            exit(1);
        }

        Symbol *symbol = malloc(sizeof(Symbol));
        if (symbol == NULL)
        {
            fprintf(stderr, "fatal: failed to add builtin symbol '%s'\n", BUILTIN_INFO_MAP[i].name);
            exit(1);
        }

        symbol_init(symbol);
        symbol->name = strdup(BUILTIN_INFO_MAP[i].name);
        symbol->type = type;
    }
}

void analyzer_add_error(Analyzer *analyzer, Node *node, char *message)
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

void analyzer_add_errorf(Analyzer *analyzer, Node *node, char *fmt, ...)
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

void analyzer_analyze(Analyzer *analyzer, Node *ast)
{
    if (ast == NULL)
    {
        return;
    }

    (void)analyzer;
}
