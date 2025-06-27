#ifndef CODEGEN_H
#define CODEGEN_H

#include "ast.h"
#include "semantic.h"
#include <llvm-c/Analysis.h>
#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
#include <stdbool.h>

// forward declarations
typedef struct CodeGenerator CodeGenerator;
typedef struct CodegenValue  CodegenValue;

// value wrapper for expressions
struct CodegenValue
{
    LLVMValueRef value;
    Type        *type;
    bool         is_lvalue;
};

// code generator state
struct CodeGenerator
{
    LLVMContextRef context;
    LLVMModuleRef  module;
    LLVMBuilderRef builder;

    // current function context
    LLVMValueRef      current_function;
    LLVMBasicBlockRef loop_continue;
    LLVMBasicBlockRef loop_break;
    void             *current_scope; // current function scope

    // symbol to LLVM value mapping
    Symbol      **symbols;
    LLVMValueRef *values;

    // type cache
    Type       **types;
    LLVMTypeRef *llvm_types;

    // semantic analyzer reference
    SemanticAnalyzer *analyzer;
};

// core functions
void codegen_init(CodeGenerator *gen, SemanticAnalyzer *analyzer, const char *module_name);
void codegen_dnit(CodeGenerator *gen);
bool codegen_generate(CodeGenerator *gen);
bool codegen_emit_ir(CodeGenerator *gen, const char *filename);
bool codegen_emit_object(CodeGenerator *gen, const char *filename);

#endif
