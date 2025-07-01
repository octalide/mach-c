#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "ast.h"
#include "module.h"
#include "symbol.h"
#include "type.h"
#include <stdbool.h>

// forward declarations
typedef struct Lexer Lexer;

typedef struct SemanticError
{
    Token *token;
    char  *message;
} SemanticError;

typedef struct SemanticErrorList
{
    SemanticError *errors;
    int            count;
    int            capacity;
} SemanticErrorList;

typedef struct SemanticAnalyzer
{
    SymbolTable       symbol_table;
    ModuleManager     module_manager;   // for cross-module analysis
    SemanticErrorList errors;           // error tracking
    AstNode          *current_function; // for return type checking
    int               loop_depth;       // for break/continue checking
    bool              has_errors;
} SemanticAnalyzer;

// semantic analyzer operations
void semantic_analyzer_init(SemanticAnalyzer *analyzer);
void semantic_analyzer_dnit(SemanticAnalyzer *analyzer);

// main analysis entry point
bool semantic_analyze(SemanticAnalyzer *analyzer, AstNode *root);

// convenience function to print all semantic errors
void semantic_print_errors(SemanticAnalyzer *analyzer, Lexer *lexer, const char *file_path);

// statement analysis
bool semantic_analyze_stmt(SemanticAnalyzer *analyzer, AstNode *stmt);
bool semantic_analyze_use_stmt(SemanticAnalyzer *analyzer, AstNode *stmt);
bool semantic_analyze_imported_module(SemanticAnalyzer *analyzer, AstNode *use_stmt);
bool semantic_analyze_ext_stmt(SemanticAnalyzer *analyzer, AstNode *stmt);
bool semantic_analyze_def_stmt(SemanticAnalyzer *analyzer, AstNode *stmt);
bool semantic_analyze_var_stmt(SemanticAnalyzer *analyzer, AstNode *stmt);
bool semantic_analyze_fun_stmt(SemanticAnalyzer *analyzer, AstNode *stmt);
bool semantic_analyze_str_stmt(SemanticAnalyzer *analyzer, AstNode *stmt);
bool semantic_analyze_uni_stmt(SemanticAnalyzer *analyzer, AstNode *stmt);
bool semantic_analyze_if_stmt(SemanticAnalyzer *analyzer, AstNode *stmt);
bool semantic_analyze_or_stmt(SemanticAnalyzer *analyzer, AstNode *stmt);
bool semantic_analyze_for_stmt(SemanticAnalyzer *analyzer, AstNode *stmt);
bool semantic_analyze_ret_stmt(SemanticAnalyzer *analyzer, AstNode *stmt);
bool semantic_analyze_block_stmt(SemanticAnalyzer *analyzer, AstNode *stmt);

// expression analysis
Type *semantic_analyze_expr(SemanticAnalyzer *analyzer, AstNode *expr);
Type *semantic_analyze_expr_with_hint(SemanticAnalyzer *analyzer, AstNode *expr, Type *expected_type);
Type *semantic_analyze_binary_expr(SemanticAnalyzer *analyzer, AstNode *expr);
Type *semantic_analyze_unary_expr(SemanticAnalyzer *analyzer, AstNode *expr);
Type *semantic_analyze_call_expr(SemanticAnalyzer *analyzer, AstNode *expr);
Type *semantic_analyze_index_expr(SemanticAnalyzer *analyzer, AstNode *expr);
Type *semantic_analyze_field_expr(SemanticAnalyzer *analyzer, AstNode *expr);
Type *semantic_analyze_cast_expr(SemanticAnalyzer *analyzer, AstNode *expr);
Type *semantic_analyze_ident_expr(SemanticAnalyzer *analyzer, AstNode *expr);
Type *semantic_analyze_lit_expr(SemanticAnalyzer *analyzer, AstNode *expr);
Type *semantic_analyze_lit_expr_with_hint(SemanticAnalyzer *analyzer, AstNode *expr, Type *expected_type);
Type *semantic_analyze_array_expr(SemanticAnalyzer *analyzer, AstNode *expr);
Type *semantic_analyze_struct_expr(SemanticAnalyzer *analyzer, AstNode *expr);

// type checking utilities
bool semantic_check_assignment(SemanticAnalyzer *analyzer, Type *target, Type *source, AstNode *node);
bool semantic_check_binary_op(SemanticAnalyzer *analyzer, TokenKind op, Type *left, Type *right, AstNode *node);
bool semantic_check_unary_op(SemanticAnalyzer *analyzer, TokenKind op, Type *operand, AstNode *node);
bool semantic_check_function_call(SemanticAnalyzer *analyzer, Type *func_type, AstList *args, AstNode *node);

// error handling
void semantic_error_list_init(SemanticErrorList *list);
void semantic_error_list_dnit(SemanticErrorList *list);
void semantic_error_list_add(SemanticErrorList *list, Token *token, const char *message);
void semantic_error_list_print(SemanticErrorList *list, Lexer *lexer, const char *file_path);
void semantic_error(SemanticAnalyzer *analyzer, AstNode *node, const char *fmt, ...);
void semantic_warning(SemanticAnalyzer *analyzer, AstNode *node, const char *fmt, ...);

#endif
