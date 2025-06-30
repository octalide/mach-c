#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "ast.h"
#include "lexer.h"
#include "module.h"
#include "symbol.h"
#include "type.h"
#include <stdbool.h>

// forward declarations
typedef struct SymbolTable SymbolTable;

typedef struct SemanticError
{
    AstNode *node;
    char    *message;
} SemanticError;

typedef struct SemanticErrorList
{
    SemanticError *errors;
    int            count;
    int            capacity;
} SemanticErrorList;

typedef struct SemanticAnalyzer
{
    SymbolTable      *global_scope;
    SymbolTable      *current_scope;
    ModuleManager    *module_manager;
    Type            **builtin_types;
    int               builtin_count;
    Type             *current_function_return_type;
    SemanticErrorList errors;
    bool              had_error;
    int               loop_depth;
} SemanticAnalyzer;

// analyzer lifecycle
void semantic_analyzer_init(SemanticAnalyzer *analyzer, ModuleManager *manager);
void semantic_analyzer_dnit(SemanticAnalyzer *analyzer);

// scope management
void semantic_push_scope(SemanticAnalyzer *analyzer);
void semantic_pop_scope(SemanticAnalyzer *analyzer);

// error handling
void semantic_error(SemanticAnalyzer *analyzer, AstNode *node, const char *format, ...);

// analysis functions
bool semantic_analyze_program(SemanticAnalyzer *analyzer, AstNode *program);
bool semantic_analyze_module(SemanticAnalyzer *analyzer, Module *module);
bool semantic_analyze_declaration(SemanticAnalyzer *analyzer, AstNode *decl);

// declaration analysis
bool semantic_analyze_use_decl(SemanticAnalyzer *analyzer, AstNode *decl);
bool semantic_analyze_ext_decl(SemanticAnalyzer *analyzer, AstNode *decl);
bool semantic_analyze_def_decl(SemanticAnalyzer *analyzer, AstNode *decl);
bool semantic_analyze_var_decl(SemanticAnalyzer *analyzer, AstNode *decl);
bool semantic_analyze_fun_decl(SemanticAnalyzer *analyzer, AstNode *decl);
bool semantic_analyze_str_decl(SemanticAnalyzer *analyzer, AstNode *decl);
bool semantic_analyze_uni_decl(SemanticAnalyzer *analyzer, AstNode *decl);

// statement analysis
bool semantic_analyze_statement(SemanticAnalyzer *analyzer, AstNode *stmt);
bool semantic_analyze_block_stmt(SemanticAnalyzer *analyzer, AstNode *stmt);
bool semantic_analyze_ret_stmt(SemanticAnalyzer *analyzer, AstNode *stmt);
bool semantic_analyze_if_stmt(SemanticAnalyzer *analyzer, AstNode *stmt);
bool semantic_analyze_for_stmt(SemanticAnalyzer *analyzer, AstNode *stmt);
bool semantic_analyze_brk_stmt(SemanticAnalyzer *analyzer, AstNode *stmt);
bool semantic_analyze_cnt_stmt(SemanticAnalyzer *analyzer, AstNode *stmt);

// expression analysis
Type *semantic_analyze_expression(SemanticAnalyzer *analyzer, AstNode *expr);
Type *semantic_analyze_binary_expr(SemanticAnalyzer *analyzer, AstNode *expr);
Type *semantic_analyze_unary_expr(SemanticAnalyzer *analyzer, AstNode *expr);
Type *semantic_analyze_call_expr(SemanticAnalyzer *analyzer, AstNode *expr);
Type *semantic_analyze_ident_expr(SemanticAnalyzer *analyzer, AstNode *expr);
Type *semantic_analyze_cast_expr(SemanticAnalyzer *analyzer, AstNode *expr);
Type *semantic_analyze_field_expr(SemanticAnalyzer *analyzer, AstNode *expr);
Type *semantic_analyze_index_expr(SemanticAnalyzer *analyzer, AstNode *expr);
Type *semantic_analyze_lit_expr(SemanticAnalyzer *analyzer, AstNode *expr);
Type *semantic_analyze_array_expr(SemanticAnalyzer *analyzer, AstNode *expr);
Type *semantic_analyze_struct_expr(SemanticAnalyzer *analyzer, AstNode *expr);
Type *semantic_analyze_builtin_expr(SemanticAnalyzer *analyzer, AstNode *expr);

// type analysis
Type *semantic_analyze_type(SemanticAnalyzer *analyzer, AstNode *type_node);
Type *semantic_get_builtin_type(SemanticAnalyzer *analyzer, const char *name);
bool  semantic_check_cast_validity(SemanticAnalyzer *analyzer, Type *from, Type *to, AstNode *node);

// error list management
void semantic_error_list_init(SemanticErrorList *list);
void semantic_error_list_dnit(SemanticErrorList *list);
void semantic_error_list_add(SemanticErrorList *list, AstNode *node, const char *message);
void semantic_error_list_print(SemanticErrorList *list, Lexer *lexer, const char *file_path);

#endif
