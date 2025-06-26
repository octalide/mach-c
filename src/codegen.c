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
static void         add_symbol(CodeGen *codegen, const char *name, LLVMValueRef value, SymbolKind kind);
static LLVMValueRef get_symbol(CodeGen *codegen, const char *name);
static SymbolKind   get_symbol_kind(CodeGen *codegen, const char *name);

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
    codegen->symbols         = malloc(sizeof(Symbol) * codegen->symbol_capacity);

    if (!codegen->symbols)
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
    if (codegen->symbols)
    {
        for (size_t i = 0; i < codegen->symbol_count; i++)
        {
            free(codegen->symbols[i].name);
        }
        free(codegen->symbols);
    }

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
static void add_symbol(CodeGen *codegen, const char *name, LLVMValueRef value, SymbolKind kind)
{
    if (codegen->symbol_count >= codegen->symbol_capacity)
    {
        codegen->symbol_capacity *= 2;
        codegen->symbols = realloc(codegen->symbols, sizeof(Symbol) * codegen->symbol_capacity);
    }

    codegen->symbols[codegen->symbol_count].name  = strdup(name);
    codegen->symbols[codegen->symbol_count].value = value;
    codegen->symbols[codegen->symbol_count].kind  = kind;
    codegen->symbol_count++;
}

static LLVMValueRef get_symbol(CodeGen *codegen, const char *name)
{
    for (size_t i = 0; i < codegen->symbol_count; i++)
    {
        if (strcmp(codegen->symbols[i].name, name) == 0)
        {
            return codegen->symbols[i].value;
        }
    }
    return NULL;
}

static SymbolKind get_symbol_kind(CodeGen *codegen, const char *name)
{
    for (size_t i = 0; i < codegen->symbol_count; i++)
    {
        if (strcmp(codegen->symbols[i].name, name) == 0)
        {
            return codegen->symbols[i].kind;
        }
    }
    return SYMBOL_FUNCTION; // default, but should not happen
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

    // get function name
    const char *func_name = "unknown";
    if (func_node->function.name && func_node->function.name->kind == NODE_IDENTIFIER)
    {
        func_name = func_node->function.name->str_value;
    }

    // create function type
    LLVMTypeRef return_type = codegen_type(codegen, func_node->function.return_type);

    // handle parameters
    LLVMTypeRef *param_types = NULL;
    unsigned     param_count = 0;

    if (func_node->function.params)
    {
        param_count = node_list_count(func_node->function.params);
        if (param_count > 0)
        {
            param_types = malloc(sizeof(LLVMTypeRef) * param_count);
            for (unsigned i = 0; i < param_count; i++)
            {
                Node *param = func_node->function.params[i];
                if (param && param->kind == NODE_STMT_VAL && param->decl.type)
                {
                    param_types[i] = codegen_type(codegen, param->decl.type);
                }
                else
                {
                    param_types[i] = LLVMInt32TypeInContext(codegen->context); // default
                }
            }
        }
    }

    LLVMTypeRef func_type = LLVMFunctionType(return_type, param_types, param_count, func_node->function.is_variadic);

    // create function
    LLVMValueRef function = LLVMAddFunction(codegen->module, func_name, func_type);

    // cleanup param_types
    if (param_types)
    {
        free(param_types);
    }

    // create entry block
    LLVMBasicBlockRef entry_block = LLVMAppendBasicBlockInContext(codegen->context, function, "entry");
    LLVMPositionBuilderAtEnd(codegen->builder, entry_block);

    // set current function
    LLVMValueRef prev_function = codegen->current_function;
    codegen->current_function  = function;

    // create local variables for parameters
    if (func_node->function.params && param_count > 0)
    {
        for (unsigned i = 0; i < param_count; i++)
        {
            Node *param = func_node->function.params[i];
            if (param && param->kind == NODE_STMT_VAL && param->decl.name && param->decl.name->kind == NODE_IDENTIFIER)
            {
                const char  *param_name  = param->decl.name->str_value;
                LLVMValueRef param_value = LLVMGetParam(function, i);
                LLVMSetValueName(param_value, param_name);

                // for now, just add to symbol table (later we'll create allocas for mutable params)
                add_symbol(codegen, param_name, param_value, SYMBOL_PARAMETER);
            }
        }
    }

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

    add_symbol(codegen, func_name, function, SYMBOL_FUNCTION);
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

    case NODE_STMT_VAR:
    case NODE_STMT_VAL:
    {
        // handle local variable declarations
        if (!stmt_node->decl.name || stmt_node->decl.name->kind != NODE_IDENTIFIER)
        {
            codegen_error(codegen, "Invalid variable name");
            return;
        }

        const char *var_name = stmt_node->decl.name->str_value;
        LLVMTypeRef var_type = codegen_type(codegen, stmt_node->decl.type);

        if (stmt_node->kind == NODE_STMT_VAL)
        {
            // handle const (val) variables
            if (!stmt_node->decl.init)
            {
                codegen_error(codegen, "val declaration must have initializer");
                return;
            }

            // evaluate the initializer
            LLVMValueRef init_value = codegen_expression(codegen, stmt_node->decl.init);
            if (!init_value)
            {
                return;
            }

            // for const variables, store the value directly in symbol table
            add_symbol(codegen, var_name, init_value, SYMBOL_CONST);
        }
        else
        {
            // handle mutable (var) variables
            // create alloca for the variable
            LLVMValueRef alloca_inst = LLVMBuildAlloca(codegen->builder, var_type, var_name);

            // if there's an initializer, store it
            if (stmt_node->decl.init)
            {
                LLVMValueRef init_value = codegen_expression(codegen, stmt_node->decl.init);
                if (init_value)
                {
                    LLVMBuildStore(codegen->builder, init_value, alloca_inst);
                }
            }

            // add to symbol table as mutable variable
            add_symbol(codegen, var_name, alloca_inst, SYMBOL_VARIABLE);
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

    case NODE_STMT_IF:
    {
        // evaluate condition
        LLVMValueRef condition = codegen_expression(codegen, stmt_node->conditional.condition);
        if (!condition)
        {
            return;
        }

        // get current function
        LLVMValueRef function = codegen->current_function;
        if (!function)
        {
            codegen_error(codegen, "If statement outside function");
            return;
        }

        // create basic blocks
        LLVMBasicBlockRef then_block  = LLVMAppendBasicBlockInContext(codegen->context, function, "if_then");
        LLVMBasicBlockRef merge_block = LLVMAppendBasicBlockInContext(codegen->context, function, "if_merge");

        // conditional branch
        LLVMBuildCondBr(codegen->builder, condition, then_block, merge_block);

        // generate then block
        LLVMPositionBuilderAtEnd(codegen->builder, then_block);
        codegen_statement(codegen, stmt_node->conditional.body);

        // if then block doesn't end with terminator, branch to merge
        if (!LLVMGetLastInstruction(then_block) || !LLVMIsATerminatorInst(LLVMGetLastInstruction(then_block)))
        {
            LLVMBuildBr(codegen->builder, merge_block);
        }

        // continue with merge block
        LLVMPositionBuilderAtEnd(codegen->builder, merge_block);
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
            SymbolKind kind = get_symbol_kind(codegen, expr_node->str_value);

            switch (kind)
            {
            case SYMBOL_CONST:
            case SYMBOL_PARAMETER:
            case SYMBOL_FUNCTION:
                // const values, parameters, and functions - return directly
                return symbol;

            case SYMBOL_VARIABLE:
                // mutable variable (alloca) - load its value
                return LLVMBuildLoad2(codegen->builder, LLVMGetAllocatedType(symbol), symbol, expr_node->str_value);
            }
        }
        codegen_error(codegen, "Undefined symbol");
        return NULL;
    }

    case NODE_EXPR_BINARY:
    {
        // handle assignment specially since left side should not be evaluated as expression
        if (expr_node->binary.op == OP_ASSIGN)
        {
            // assignment requires special handling - left side must be an lvalue
            if (expr_node->binary.left->kind != NODE_IDENTIFIER)
            {
                codegen_error(codegen, "Left side of assignment must be a variable");
                return NULL;
            }

            const char  *var_name   = expr_node->binary.left->str_value;
            LLVMValueRef var_alloca = get_symbol(codegen, var_name);

            if (!var_alloca)
            {
                codegen_error(codegen, "Undefined variable in assignment");
                return NULL;
            }

            SymbolKind kind = get_symbol_kind(codegen, var_name);
            if (kind != SYMBOL_VARIABLE)
            {
                codegen_error(codegen, "Cannot assign to const variable or function");
                return NULL;
            }

            // evaluate right side
            LLVMValueRef value = codegen_expression(codegen, expr_node->binary.right);
            if (!value)
            {
                return NULL;
            }

            // store the value
            LLVMBuildStore(codegen->builder, value, var_alloca);

            // return the assigned value
            return value;
        }

        // handle other binary operators
        LLVMValueRef left  = codegen_expression(codegen, expr_node->binary.left);
        LLVMValueRef right = codegen_expression(codegen, expr_node->binary.right);

        if (!left || !right)
        {
            return NULL;
        }

        switch (expr_node->binary.op)
        {
        case OP_ADD:
            return LLVMBuildAdd(codegen->builder, left, right, "add_tmp");
        case OP_SUB:
            return LLVMBuildSub(codegen->builder, left, right, "sub_tmp");
        case OP_MUL:
            return LLVMBuildMul(codegen->builder, left, right, "mul_tmp");
        case OP_DIV:
            // assume signed division for now
            return LLVMBuildSDiv(codegen->builder, left, right, "div_tmp");
        case OP_MOD:
            return LLVMBuildSRem(codegen->builder, left, right, "mod_tmp");

        // comparison operators
        case OP_EQUAL:
            return LLVMBuildICmp(codegen->builder, LLVMIntEQ, left, right, "eq_tmp");
        case OP_NOT_EQUAL:
            return LLVMBuildICmp(codegen->builder, LLVMIntNE, left, right, "ne_tmp");
        case OP_LESS:
            return LLVMBuildICmp(codegen->builder, LLVMIntSLT, left, right, "lt_tmp");
        case OP_GREATER:
            return LLVMBuildICmp(codegen->builder, LLVMIntSGT, left, right, "gt_tmp");
        case OP_LESS_EQUAL:
            return LLVMBuildICmp(codegen->builder, LLVMIntSLE, left, right, "le_tmp");
        case OP_GREATER_EQUAL:
            return LLVMBuildICmp(codegen->builder, LLVMIntSGE, left, right, "ge_tmp");

        default:
            codegen_error(codegen, "Unsupported binary operator");
            return NULL;
        }
    }

    case NODE_EXPR_CALL:
    {
        // get function name
        if (!expr_node->call.target || expr_node->call.target->kind != NODE_IDENTIFIER)
        {
            codegen_error(codegen, "Invalid function call target");
            return NULL;
        }

        const char  *func_name = expr_node->call.target->str_value;
        LLVMValueRef function  = get_symbol(codegen, func_name);

        if (!function)
        {
            codegen_error(codegen, "Undefined function");
            return NULL;
        }

        // build arguments
        LLVMValueRef *args      = NULL;
        unsigned      arg_count = 0;

        if (expr_node->call.args)
        {
            arg_count = node_list_count(expr_node->call.args);
            if (arg_count > 0)
            {
                args = malloc(sizeof(LLVMValueRef) * arg_count);
                for (unsigned i = 0; i < arg_count; i++)
                {
                    args[i] = codegen_expression(codegen, expr_node->call.args[i]);
                    if (!args[i])
                    {
                        free(args);
                        return NULL;
                    }
                }
            }
        }

        // make the call
        LLVMValueRef result = LLVMBuildCall2(codegen->builder, LLVMGlobalGetValueType(function), function, args, arg_count, "call_tmp");

        if (args)
        {
            free(args);
        }

        return result;
    }

    default:
        codegen_error(codegen, "Unsupported expression");
        return NULL;
    }
}
