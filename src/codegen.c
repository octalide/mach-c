#include "codegen.h"

#include <llvm-c/Analysis.h>
#include <llvm-c/IRReader.h>
#include <llvm-c/Object.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// forward declarations
static LLVMValueRef codegen_node(CodeGen *codegen, Node *node);
static LLVMTypeRef  codegen_type(CodeGen *codegen, Node *type_node);
static LLVMValueRef codegen_function(CodeGen *codegen, Node *func_node);
static LLVMValueRef codegen_expression(CodeGen *codegen, Node *expr_node);
static void         codegen_statement(CodeGen *codegen, Node *stmt_node);

// symbol table helpers
static void         add_symbol(CodeGen *codegen, const char *name, LLVMValueRef value);
static LLVMValueRef get_symbol(CodeGen *codegen, const char *name);

bool codegen_init(CodeGen *codegen, const char *module_name)
{
    memset(codegen, 0, sizeof(CodeGen));

    // initialize LLVM
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();

    // create context and module
    codegen->context = LLVMContextCreate();
    if (!codegen->context)
    {
        codegen_error(codegen, "Failed to create LLVM context");
        return false;
    }

    codegen->module = LLVMModuleCreateWithNameInContext(module_name, codegen->context);
    if (!codegen->module)
    {
        codegen_error(codegen, "Failed to create LLVM module");
        return false;
    }

    // create builder
    codegen->builder = LLVMCreateBuilderInContext(codegen->context);
    if (!codegen->builder)
    {
        codegen_error(codegen, "Failed to create LLVM builder");
        return false;
    }

    // get native target
    char *error_msg = NULL;
    char *triple    = LLVMGetDefaultTargetTriple();
    if (LLVMGetTargetFromTriple(triple, &codegen->target, &error_msg) != 0)
    {
        printf("Failed to get target: %s\n", error_msg);
        LLVMDisposeMessage(error_msg);
        LLVMDisposeMessage(triple);
        return false;
    }

    // create target machine
    codegen->target_machine = LLVMCreateTargetMachine(codegen->target, triple, "generic", "", LLVMCodeGenLevelDefault, LLVMRelocDefault, LLVMCodeModelDefault);

    LLVMDisposeMessage(triple);

    if (!codegen->target_machine)
    {
        codegen_error(codegen, "Failed to create target machine");
        return false;
    }

    // initialize symbol table
    codegen->symbol_capacity = 64;
    codegen->symbols         = malloc(sizeof(LLVMValueRef) * codegen->symbol_capacity);
    codegen->symbol_names    = malloc(sizeof(char *) * codegen->symbol_capacity);

    if (!codegen->symbols || !codegen->symbol_names)
    {
        codegen_error(codegen, "Failed to allocate symbol table");
        return false;
    }

    return true;
}

void codegen_dnit(CodeGen *codegen)
{
    if (!codegen)
        return;

    // cleanup symbol table
    if (codegen->symbol_names)
    {
        for (size_t i = 0; i < codegen->symbol_count; i++)
        {
            free(codegen->symbol_names[i]);
        }
        free(codegen->symbol_names);
    }
    free(codegen->symbols);

    // cleanup LLVM objects
    if (codegen->target_machine)
        LLVMDisposeTargetMachine(codegen->target_machine);
    if (codegen->builder)
        LLVMDisposeBuilder(codegen->builder);
    if (codegen->module)
        LLVMDisposeModule(codegen->module);
    if (codegen->context)
        LLVMContextDispose(codegen->context);

    memset(codegen, 0, sizeof(CodeGen));
}

bool codegen_generate(CodeGen *codegen, Node *program)
{
    if (!codegen || !program)
    {
        codegen_error(codegen, "Invalid parameters");
        return false;
    }

    if (program->kind != NODE_PROGRAM)
    {
        codegen_error(codegen, "Expected program node");
        return false;
    }

    // generate code for all top-level statements
    if (program->children)
    {
        for (size_t i = 0; i < node_list_count(program->children); i++)
        {
            Node *stmt = program->children[i];
            codegen_node(codegen, stmt);

            if (codegen->has_error)
            {
                return false;
            }
        }
    }

    // verify module
    char *error_msg = NULL;
    if (LLVMVerifyModule(codegen->module, LLVMReturnStatusAction, &error_msg) != 0)
    {
        printf("Module verification failed: %s\n", error_msg);
        LLVMDisposeMessage(error_msg);
        return false;
    }

    return true;
}

bool codegen_write_object(CodeGen *codegen, const char *filename)
{
    if (!codegen || !filename)
    {
        return false;
    }

    char *error_msg = NULL;
    if (LLVMTargetMachineEmitToFile(codegen->target_machine, codegen->module, (char *)filename, LLVMObjectFile, &error_msg) != 0)
    {
        printf("Failed to emit object file: %s\n", error_msg);
        LLVMDisposeMessage(error_msg);
        return false;
    }

    return true;
}

bool codegen_write_ir(CodeGen *codegen, const char *filename)
{
    if (!codegen || !filename)
    {
        return false;
    }

    char *error_msg = NULL;
    if (LLVMPrintModuleToFile(codegen->module, filename, &error_msg) != 0)
    {
        printf("Failed to write IR file: %s\n", error_msg);
        LLVMDisposeMessage(error_msg);
        return false;
    }

    return true;
}

void codegen_error(CodeGen *codegen, const char *message)
{
    if (codegen)
    {
        codegen->has_error = true;
    }
    fprintf(stderr, "Codegen error: %s\n", message);
}

// symbol table helpers
static void add_symbol(CodeGen *codegen, const char *name, LLVMValueRef value)
{
    if (codegen->symbol_count >= codegen->symbol_capacity)
    {
        codegen->symbol_capacity *= 2;
        codegen->symbols      = realloc(codegen->symbols, sizeof(LLVMValueRef) * codegen->symbol_capacity);
        codegen->symbol_names = realloc(codegen->symbol_names, sizeof(char *) * codegen->symbol_capacity);
    }

    codegen->symbols[codegen->symbol_count]      = value;
    codegen->symbol_names[codegen->symbol_count] = strdup(name);
    codegen->symbol_count++;
}

static LLVMValueRef get_symbol(CodeGen *codegen, const char *name)
{
    for (size_t i = 0; i < codegen->symbol_count; i++)
    {
        if (strcmp(codegen->symbol_names[i], name) == 0)
        {
            return codegen->symbols[i];
        }
    }
    return NULL;
}

// type conversion
static LLVMTypeRef codegen_type(CodeGen *codegen, Node *type_node)
{
    if (!type_node)
    {
        return LLVMVoidTypeInContext(codegen->context);
    }

    switch (type_node->kind)
    {
    case NODE_IDENTIFIER:
        // basic types
        if (strcmp(type_node->str_value, "i8") == 0)
            return LLVMInt8TypeInContext(codegen->context);
        if (strcmp(type_node->str_value, "i16") == 0)
            return LLVMInt16TypeInContext(codegen->context);
        if (strcmp(type_node->str_value, "i32") == 0)
            return LLVMInt32TypeInContext(codegen->context);
        if (strcmp(type_node->str_value, "i64") == 0)
            return LLVMInt64TypeInContext(codegen->context);
        if (strcmp(type_node->str_value, "u8") == 0)
            return LLVMInt8TypeInContext(codegen->context);
        if (strcmp(type_node->str_value, "u16") == 0)
            return LLVMInt16TypeInContext(codegen->context);
        if (strcmp(type_node->str_value, "u32") == 0)
            return LLVMInt32TypeInContext(codegen->context);
        if (strcmp(type_node->str_value, "u64") == 0)
            return LLVMInt64TypeInContext(codegen->context);
        if (strcmp(type_node->str_value, "f32") == 0)
            return LLVMFloatTypeInContext(codegen->context);
        if (strcmp(type_node->str_value, "f64") == 0)
            return LLVMDoubleTypeInContext(codegen->context);

        // unknown type, default to i32
        codegen_error(codegen, "Unknown type");
        return LLVMInt32TypeInContext(codegen->context);

    case NODE_TYPE_POINTER:
    {
        LLVMTypeRef base_type = codegen_type(codegen, type_node->single);
        return LLVMPointerType(base_type, 0);
    }

    case NODE_TYPE_ARRAY:
    {
        LLVMTypeRef element_type = codegen_type(codegen, type_node->binary.right);
        // for now, treat all arrays as pointers
        return LLVMPointerType(element_type, 0);
    }

    default:
        codegen_error(codegen, "Unsupported type");
        return LLVMInt32TypeInContext(codegen->context);
    }
}

// main code generation dispatch
static LLVMValueRef codegen_node(CodeGen *codegen, Node *node)
{
    if (!node)
        return NULL;

    switch (node->kind)
    {
    case NODE_STMT_FUNCTION:
        return codegen_function(codegen, node);

    case NODE_STMT_DEF:
        // type definitions - for now just ignore
        return NULL;

    case NODE_STMT_VAL:
    case NODE_STMT_VAR:
        // global variables - TODO
        return NULL;

    default:
        // treat as expression
        return codegen_expression(codegen, node);
    }
}

// function generation
static LLVMValueRef codegen_function(CodeGen *codegen, Node *func_node)
{
    if (func_node->kind != NODE_STMT_FUNCTION)
    {
        codegen_error(codegen, "Expected function node");
        return NULL;
    }

    // get function name from first parameter (hack for now)
    const char *func_name = "main"; // default name

    // create function type
    LLVMTypeRef return_type = codegen_type(codegen, func_node->function.return_type);

    // for now, create functions with no parameters
    LLVMTypeRef func_type = LLVMFunctionType(return_type, NULL, 0, false);

    // create function
    LLVMValueRef function = LLVMAddFunction(codegen->module, func_name, func_type);

    // create entry block
    LLVMBasicBlockRef entry_block = LLVMAppendBasicBlockInContext(codegen->context, function, "entry");
    LLVMPositionBuilderAtEnd(codegen->builder, entry_block);

    // set current function
    LLVMValueRef prev_function = codegen->current_function;
    codegen->current_function  = function;

    // generate function body
    bool has_terminator = false;
    if (func_node->function.body)
    {
        codegen_statement(codegen, func_node->function.body);

        // check if current block has a terminator
        LLVMBasicBlockRef current_block = LLVMGetInsertBlock(codegen->builder);
        if (current_block)
        {
            LLVMValueRef last_instr = LLVMGetLastInstruction(current_block);
            if (last_instr && LLVMIsATerminatorInst(last_instr))
            {
                has_terminator = true;
            }
        }
    }

    // add return if no terminator was generated
    if (!has_terminator)
    {
        if (return_type == LLVMVoidTypeInContext(codegen->context))
        {
            LLVMBuildRetVoid(codegen->builder);
        }
        else
        {
            // return zero for non-void functions without explicit return
            LLVMValueRef zero = LLVMConstInt(return_type, 0, false);
            LLVMBuildRet(codegen->builder, zero);
        }
    }

    // restore previous function
    codegen->current_function = prev_function;

    add_symbol(codegen, func_name, function);
    return function;
}

// statement generation
static void codegen_statement(CodeGen *codegen, Node *stmt_node)
{
    if (!stmt_node)
        return;

    switch (stmt_node->kind)
    {
    case NODE_BLOCK:
        if (stmt_node->children)
        {
            for (size_t i = 0; i < node_list_count(stmt_node->children); i++)
            {
                codegen_statement(codegen, stmt_node->children[i]);
            }
        }
        break;

    case NODE_STMT_RETURN:
    {
        if (stmt_node->single)
        {
            LLVMValueRef return_val = codegen_expression(codegen, stmt_node->single);
            LLVMBuildRet(codegen->builder, return_val);
        }
        else
        {
            LLVMBuildRetVoid(codegen->builder);
        }
    }
    break;

    case NODE_STMT_EXPRESSION:
        codegen_expression(codegen, stmt_node->single);
        break;

    default:
        // ignore other statements for now
        break;
    }
}

// expression generation
static LLVMValueRef codegen_expression(CodeGen *codegen, Node *expr_node)
{
    if (!expr_node)
        return NULL;

    switch (expr_node->kind)
    {
    case NODE_LIT_INT:
        return LLVMConstInt(LLVMInt32TypeInContext(codegen->context), expr_node->int_value, false);

    case NODE_LIT_FLOAT:
        return LLVMConstReal(LLVMDoubleTypeInContext(codegen->context), expr_node->float_value);

    case NODE_IDENTIFIER:
    {
        LLVMValueRef symbol = get_symbol(codegen, expr_node->str_value);
        if (symbol)
        {
            return symbol;
        }
        codegen_error(codegen, "Undefined symbol");
        return NULL;
    }

    default:
        codegen_error(codegen, "Unsupported expression");
        return NULL;
    }
}
