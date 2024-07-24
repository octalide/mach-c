#include <stdlib.h>
#include <stdio.h>

#include "semantic_analyzer.h"

void analyzer_init(SemanticAnalyzer *analyzer)
{
    analyzer->symbol_table = symbol_table_new(256);
}

void semantic_analyzer_free(SemanticAnalyzer *analyzer)
{
    symbol_table_free(analyzer->symbol_table);
}

void analyzer_analyze(SemanticAnalyzer *analyzer, Node *ast)
{
    // TODO
    (void)analyzer;
    (void)ast;
}
