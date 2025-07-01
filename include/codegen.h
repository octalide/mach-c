#ifndef CODEGEN_H
#define CODEGEN_H

#include "ast.h"
#include "module.h"
#include "type.h"
#include <stdio.h>

// forward declarations
typedef struct LLVMOpaqueContext    *LLVMContextRef;
typedef struct LLVMOpaqueModule     *LLVMModuleRef;
typedef struct LLVMOpaqueBuilder    *LLVMBuilderRef;
typedef struct LLVMOpaqueType       *LLVMTypeRef;
typedef struct LLVMOpaqueValue      *LLVMValueRef;
typedef struct LLVMOpaqueBasicBlock *LLVMBasicBlockRef;

// code generation context
typedef struct CodegenContext
{
    LLVMContextRef llvm_context;
    LLVMModuleRef  llvm_module;
    LLVMBuilderRef llvm_builder;
    ModuleManager *module_manager;

    // type caching
    LLVMTypeRef *cached_types;
    Type       **type_keys;
    int          type_cache_count;
    int          type_cache_capacity;

    // value tracking for variables and functions
    LLVMValueRef *values;
    Symbol      **value_keys;
    int           value_count;
    int           value_capacity;

    // current function context
    LLVMValueRef      current_function;
    Type             *current_function_type;
    LLVMBasicBlockRef exit_block;   // for return statements
    LLVMValueRef      return_value; // return value alloca

    // loop context for break/continue
    LLVMBasicBlockRef *loop_exit_blocks;
    LLVMBasicBlockRef *loop_cont_blocks;
    int                loop_depth;
    int                loop_capacity;
} CodegenContext;

// context lifecycle
void codegen_context_init(CodegenContext *ctx, const char *module_name, ModuleManager *manager);
void codegen_context_dnit(CodegenContext *ctx);

// main codegen entry points
bool codegen_program(CodegenContext *ctx, AstNode *program);
bool codegen_module(CodegenContext *ctx, Module *module);

// output generation
bool codegen_write_ir(CodegenContext *ctx, const char *filename);
bool codegen_write_bitcode(CodegenContext *ctx, const char *filename);
bool codegen_write_object(CodegenContext *ctx, const char *filename);
bool codegen_link_executable(const char *object_file, const char *exe_file);

// declaration codegen
bool codegen_declaration(CodegenContext *ctx, AstNode *decl);
bool codegen_use_decl(CodegenContext *ctx, AstNode *decl);
bool codegen_ext_decl(CodegenContext *ctx, AstNode *decl);
bool codegen_def_decl(CodegenContext *ctx, AstNode *decl);
bool codegen_var_decl(CodegenContext *ctx, AstNode *decl);
bool codegen_fun_decl(CodegenContext *ctx, AstNode *decl);
bool codegen_str_decl(CodegenContext *ctx, AstNode *decl);
bool codegen_uni_decl(CodegenContext *ctx, AstNode *decl);

// statement codegen
bool codegen_statement(CodegenContext *ctx, AstNode *stmt);
bool codegen_block_stmt(CodegenContext *ctx, AstNode *stmt);
bool codegen_expr_stmt(CodegenContext *ctx, AstNode *stmt);
bool codegen_ret_stmt(CodegenContext *ctx, AstNode *stmt);
bool codegen_if_stmt(CodegenContext *ctx, AstNode *stmt);
bool codegen_for_stmt(CodegenContext *ctx, AstNode *stmt);
bool codegen_brk_stmt(CodegenContext *ctx, AstNode *stmt);
bool codegen_cnt_stmt(CodegenContext *ctx, AstNode *stmt);

// expression codegen
LLVMValueRef codegen_expression(CodegenContext *ctx, AstNode *expr);
LLVMValueRef codegen_binary_expr(CodegenContext *ctx, AstNode *expr);
LLVMValueRef codegen_unary_expr(CodegenContext *ctx, AstNode *expr);
LLVMValueRef codegen_call_expr(CodegenContext *ctx, AstNode *expr);
LLVMValueRef codegen_index_expr(CodegenContext *ctx, AstNode *expr);
LLVMValueRef codegen_field_expr(CodegenContext *ctx, AstNode *expr);
LLVMValueRef codegen_cast_expr(CodegenContext *ctx, AstNode *expr);
LLVMValueRef codegen_ident_expr(CodegenContext *ctx, AstNode *expr);
LLVMValueRef codegen_lit_expr(CodegenContext *ctx, AstNode *expr);
LLVMValueRef codegen_array_expr(CodegenContext *ctx, AstNode *expr);
LLVMValueRef codegen_struct_expr(CodegenContext *ctx, AstNode *expr);

// builtin function handling
LLVMValueRef codegen_builtin_call(CodegenContext *ctx, AstNode *expr);

// type conversion
LLVMTypeRef codegen_type(CodegenContext *ctx, Type *type);

// address computation for lvalues
LLVMValueRef codegen_lvalue_address(CodegenContext *ctx, AstNode *expr);

// helper functions
LLVMValueRef codegen_get_value(CodegenContext *ctx, Symbol *symbol);
void         codegen_set_value(CodegenContext *ctx, Symbol *symbol, LLVMValueRef value);
LLVMTypeRef  codegen_get_cached_type(CodegenContext *ctx, Type *type);
void         codegen_cache_type(CodegenContext *ctx, Type *type, LLVMTypeRef llvm_type);

// error handling
void codegen_error(CodegenContext *ctx, const char *format, ...);

#endif
