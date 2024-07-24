#ifndef SEMANTIC_ANALYZER_H
#define SEMANTIC_ANALYZER_H

#include "ast.h"
#include "symbol_table.h"

typedef struct SemanticAnalyzer {
    SymbolTable *symbol_table;
} SemanticAnalyzer;

void analyzer_init(SemanticAnalyzer *analyzer);
void semantic_analyzer_free(SemanticAnalyzer *analyzer);

void analyzer_analyze(SemanticAnalyzer *analyzer, Node *ast);

#endif // SEMANTIC_ANALYZER_H
