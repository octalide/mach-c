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
    char  *file_path; // file path for this error
} SemanticError;

typedef struct SemanticErrorList
{
    SemanticError *errors;
    int            count;
    int            capacity;
} SemanticErrorList;

typedef struct GenericBinding
{
    const char *name;
    Type       *type;
} GenericBinding;

typedef enum InstantiationKind
{
    INSTANTIATION_FUNCTION,
    INSTANTIATION_STRUCT,
    INSTANTIATION_UNION,
} InstantiationKind;

typedef struct InstantiationRequest
{
    InstantiationKind            kind;
    Symbol                      *generic_symbol;
    Type                       **type_args;
    size_t                       type_arg_count;
    AstNode                     *call_site;
    char                        *unique_id;
    struct InstantiationRequest *next;
} InstantiationRequest;

typedef struct InstantiationQueue
{
    InstantiationRequest *head;
    InstantiationRequest *tail;
    size_t                count;
} InstantiationQueue;

typedef struct SemanticAnalyzer
{
    SymbolTable        symbol_table;
    ModuleManager      module_manager;
    SemanticErrorList  errors;
    AstNode           *current_function;
    AstNode           *program_root;
    int                loop_depth;
    bool               has_errors;
    bool               has_fatal_error;
    const char        *current_module_name;
    GenericBinding    *generic_bindings;
    size_t             generic_binding_count;
    size_t             generic_binding_capacity;
    InstantiationQueue instantiation_queue;
} SemanticAnalyzer;

// semantic analyzer operations
void semantic_analyzer_init(SemanticAnalyzer *analyzer);
void semantic_analyzer_dnit(SemanticAnalyzer *analyzer);
void semantic_analyzer_set_module(SemanticAnalyzer *analyzer, const char *module_name);

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
void semantic_error_list_add(SemanticErrorList *list, Token *token, const char *message, const char *file_path);
void semantic_error_list_print(SemanticErrorList *list, Lexer *lexer, const char *file_path);
void semantic_error(SemanticAnalyzer *analyzer, AstNode *node, const char *fmt, ...);
void semantic_warning(SemanticAnalyzer *analyzer, AstNode *node, const char *fmt, ...);
void semantic_mark_fatal(SemanticAnalyzer *analyzer);
bool semantic_has_fatal_error(SemanticAnalyzer *analyzer);

#endif
