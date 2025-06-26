#ifndef CODEGEN_H
#define CODEGEN_H

#include "ast.h"

#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <stdbool.h>

typedef enum SymbolKind
{
    SYMBOL_FUNCTION,  // function
    SYMBOL_PARAMETER, // function parameter (immutable)
    SYMBOL_CONST,     // val declaration (immutable)
    SYMBOL_VARIABLE   // var declaration (mutable, alloca)
} SymbolKind;

typedef struct Symbol
{
    char        *name;
    LLVMValueRef value;
    SymbolKind   kind;
} Symbol;

typedef struct CodeGen
{
    LLVMContextRef       context;
    LLVMModuleRef        module;
    LLVMBuilderRef       builder;
    LLVMTargetRef        target;
    LLVMTargetMachineRef target_machine;

    // current function being generated
    LLVMValueRef current_function;

    // symbol table for variables and functions
    Symbol *symbols;
    size_t  symbol_count;
    size_t  symbol_capacity;

    bool has_error;
} CodeGen;

// core functions
bool codegen_init(CodeGen *codegen, const char *module_name);
void codegen_dnit(CodeGen *codegen);

// main generation function
bool codegen_generate(CodeGen *codegen, Node *program);

// output functions
bool codegen_write_object(CodeGen *codegen, const char *filename);
bool codegen_write_ir(CodeGen *codegen, const char *filename);

// error handling
void codegen_error(CodeGen *codegen, const char *message);

#endif
