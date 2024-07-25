#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "analyzer.h"

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

void analyzer_analyze(Analyzer *analyzer, Node *ast)
{
    if (ast == NULL)
    {
        return;
    }

    (void)analyzer;
}
