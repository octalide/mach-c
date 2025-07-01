#include "codegen.h"
#include "symbol.h"
#include <llvm-c/Analysis.h>
#include <llvm-c/BitWriter.h>
#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// context lifecycle
void codegen_context_init(CodegenContext *ctx, const char *module_name, ModuleManager *manager)
{
    ctx->llvm_context   = LLVMContextCreate();
    ctx->llvm_module    = LLVMModuleCreateWithNameInContext(module_name, ctx->llvm_context);
    ctx->llvm_builder   = LLVMCreateBuilderInContext(ctx->llvm_context);
    ctx->module_manager = manager;

    // initialize type cache
    ctx->cached_types        = NULL;
    ctx->type_keys           = NULL;
    ctx->type_cache_count    = 0;
    ctx->type_cache_capacity = 0;

    // initialize value tracking
    ctx->values         = NULL;
    ctx->value_keys     = NULL;
    ctx->value_count    = 0;
    ctx->value_capacity = 0;

    // initialize function context
    ctx->current_function      = NULL;
    ctx->current_function_type = NULL;
    ctx->exit_block            = NULL;
    ctx->return_value          = NULL;

    // initialize loop context
    ctx->loop_exit_blocks = NULL;
    ctx->loop_cont_blocks = NULL;
    ctx->loop_depth       = 0;
    ctx->loop_capacity    = 0;
}

void codegen_context_dnit(CodegenContext *ctx)
{
    free(ctx->cached_types);
    free(ctx->type_keys);
    free(ctx->values);
    free(ctx->value_keys);
    free(ctx->loop_exit_blocks);
    free(ctx->loop_cont_blocks);

    LLVMDisposeBuilder(ctx->llvm_builder);
    LLVMDisposeModule(ctx->llvm_module);
    LLVMContextDispose(ctx->llvm_context);
}

// type conversion helpers - add cache to prevent infinite recursion
static Type **converting_types    = NULL;
static int    converting_count    = 0;
static int    converting_capacity = 0;

static bool is_converting_type(Type *type)
{
    for (int i = 0; i < converting_count; i++)
    {
        if (converting_types[i] == type)
        {
            return true;
        }
    }
    return false;
}

static void mark_converting_type(Type *type)
{
    if (converting_count >= converting_capacity)
    {
        converting_capacity = converting_capacity ? converting_capacity * 2 : 8;
        converting_types    = realloc(converting_types, sizeof(Type *) * converting_capacity);
    }
    converting_types[converting_count++] = type;
}

static void unmark_converting_type(Type *type)
{
    for (int i = 0; i < converting_count; i++)
    {
        if (converting_types[i] == type)
        {
            // move last element to this position
            converting_types[i] = converting_types[--converting_count];
            break;
        }
    }
}

LLVMTypeRef codegen_type(CodegenContext *ctx, Type *type)
{
    if (!type)
    {
        return LLVMVoidTypeInContext(ctx->llvm_context);
    }

    // check cache first
    LLVMTypeRef cached = codegen_get_cached_type(ctx, type);
    if (cached)
    {
        return cached;
    }

    // check for circular reference
    if (is_converting_type(type))
    {
        // for circular function types, just return a pointer to opaque type for now
        if (type->kind == TYPE_FUNCTION)
        {
            // circular function type detected, use opaque pointer
            LLVMTypeRef opaque_ptr = LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_context), 0);
            codegen_cache_type(ctx, type, opaque_ptr);
            return opaque_ptr;
        }
        fprintf(stderr, "Error: Circular type reference detected\n");
        return NULL;
    }

    mark_converting_type(type);

    LLVMTypeRef llvm_type = NULL;

    switch (type->kind)
    {
    case TYPE_INT:
        if (type->size == 0)
        {
            fprintf(stderr, "Error: Integer type with zero size\n");
            return NULL;
        }
        llvm_type = LLVMIntTypeInContext(ctx->llvm_context, type->size * 8);
        break;

    case TYPE_FLOAT:
        if (type->size == 2)
        {
            llvm_type = LLVMHalfTypeInContext(ctx->llvm_context);
        }
        else if (type->size == 4)
        {
            llvm_type = LLVMFloatTypeInContext(ctx->llvm_context);
        }
        else if (type->size == 8)
        {
            llvm_type = LLVMDoubleTypeInContext(ctx->llvm_context);
        }
        break;

    case TYPE_PTR:
        if (type->ptr.base)
        {
            LLVMTypeRef base_type = codegen_type(ctx, type->ptr.base);
            llvm_type             = LLVMPointerType(base_type, 0);
        }
        else
        {
            // void pointer
            llvm_type = LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_context), 0);
        }
        break;

    case TYPE_ARRAY:
        if (type->array.size >= 0)
        {
            // bound array [N]T - store as LLVM array type
            LLVMTypeRef elem_type = codegen_type(ctx, type->array.elem_type);
            llvm_type             = LLVMArrayType(elem_type, type->array.size);
        }
        else
        {
            // unbound array []T - store as {ptr, len} struct
            LLVMTypeRef elem_type = codegen_type(ctx, type->array.elem_type);
            LLVMTypeRef ptr_type  = LLVMPointerType(elem_type, 0);
            LLVMTypeRef len_type  = LLVMInt64TypeInContext(ctx->llvm_context); // u64 length

            LLVMTypeRef field_types[2] = {ptr_type, len_type};
            llvm_type                  = LLVMStructTypeInContext(ctx->llvm_context, field_types, 2, false);
        }
        break;

    case TYPE_FUNCTION:
    {
        LLVMTypeRef return_type = LLVMVoidTypeInContext(ctx->llvm_context);
        if (type->function.return_type)
        {
            Type *ret_type = type->function.return_type;

            // follow aliases to get the actual type
            while (ret_type->kind == TYPE_ALIAS)
                ret_type = ret_type->alias.target;

            // if the return type is a function type, convert it to a function pointer
            if (ret_type->kind == TYPE_FUNCTION)
            {
                LLVMTypeRef func_type = codegen_type(ctx, ret_type);
                if (!func_type)
                {
                    unmark_converting_type(type);
                    return NULL;
                }
                return_type = LLVMPointerType(func_type, 0);
            }
            else
            {
                return_type = codegen_type(ctx, type->function.return_type);
                if (!return_type)
                {
                    unmark_converting_type(type);
                    return NULL;
                }
            }
        }

        LLVMTypeRef *param_types = NULL;

        if (type->function.param_count > 0)
        {
            param_types = malloc(sizeof(LLVMTypeRef) * type->function.param_count);
            for (int i = 0; i < type->function.param_count; i++)
            {
                Type *param_type = type->function.param_types[i];

                // follow aliases to get the actual type
                while (param_type->kind == TYPE_ALIAS)
                    param_type = param_type->alias.target;

                // for function parameters that are function types,
                // we need to convert them to function pointers in the LLVM signature
                if (param_type->kind == TYPE_FUNCTION)
                {
                    LLVMTypeRef func_type = codegen_type(ctx, param_type);
                    if (!func_type)
                    {
                        free(param_types);
                        unmark_converting_type(type);
                        return NULL;
                    }
                    param_types[i] = LLVMPointerType(func_type, 0);
                }
                else
                {
                    param_types[i] = codegen_type(ctx, param_type);
                    if (!param_types[i])
                    {
                        free(param_types);
                        unmark_converting_type(type);
                        return NULL;
                    }
                }
            }
        }

        llvm_type = LLVMFunctionType(return_type, param_types, type->function.param_count, false);
        free(param_types);
        break;
    }

    case TYPE_STRUCT:
    case TYPE_UNION:
    {
        // create named struct type
        const char *name = type->composite.name;
        if (name)
        {
            llvm_type = LLVMStructCreateNamed(ctx->llvm_context, name);
        }
        else
        {
            llvm_type = LLVMStructTypeInContext(ctx->llvm_context, NULL, 0, false);
        }

        // cache early to handle recursive types
        codegen_cache_type(ctx, type, llvm_type);

        // set body for struct
        if (type->composite.field_count > 0)
        {
            LLVMTypeRef *field_types = malloc(sizeof(LLVMTypeRef) * type->composite.field_count);
            for (int i = 0; i < type->composite.field_count; i++)
            {
                field_types[i] = codegen_type(ctx, type->composite.fields[i]->type);
            }

            if (name)
            {
                LLVMStructSetBody(llvm_type, field_types, type->composite.field_count, false);
            }
            else
            {
                llvm_type = LLVMStructTypeInContext(ctx->llvm_context, field_types, type->composite.field_count, false);
            }

            free(field_types);
        }

        return llvm_type; // already cached
    }

    case TYPE_ALIAS:
        llvm_type = codegen_type(ctx, type->alias.target);
        break;

    default:
        llvm_type = LLVMVoidTypeInContext(ctx->llvm_context);
        break;
    }

    if (llvm_type)
    {
        codegen_cache_type(ctx, type, llvm_type);
    }
    else
    {
        fprintf(stderr, "ERROR: Failed to create type kind %d\n", type->kind);
    }

    unmark_converting_type(type);

    return llvm_type;
}

// cache management
LLVMTypeRef codegen_get_cached_type(CodegenContext *ctx, Type *type)
{
    for (int i = 0; i < ctx->type_cache_count; i++)
    {
        if (ctx->type_keys[i] == type)
        {
            return ctx->cached_types[i];
        }
    }
    return NULL;
}

void codegen_cache_type(CodegenContext *ctx, Type *type, LLVMTypeRef llvm_type)
{
    if (ctx->type_cache_count >= ctx->type_cache_capacity)
    {
        ctx->type_cache_capacity = ctx->type_cache_capacity ? ctx->type_cache_capacity * 2 : 16;
        ctx->cached_types        = realloc(ctx->cached_types, sizeof(LLVMTypeRef) * ctx->type_cache_capacity);
        ctx->type_keys           = realloc(ctx->type_keys, sizeof(Type *) * ctx->type_cache_capacity);
    }

    ctx->type_keys[ctx->type_cache_count]    = type;
    ctx->cached_types[ctx->type_cache_count] = llvm_type;
    ctx->type_cache_count++;
}

// value tracking
LLVMValueRef codegen_get_value(CodegenContext *ctx, Symbol *symbol)
{
    for (int i = 0; i < ctx->value_count; i++)
    {
        if (ctx->value_keys[i] == symbol)
        {
            return ctx->values[i];
        }
    }
    return NULL;
}

void codegen_set_value(CodegenContext *ctx, Symbol *symbol, LLVMValueRef value)
{
    // check if already exists
    for (int i = 0; i < ctx->value_count; i++)
    {
        if (ctx->value_keys[i] == symbol)
        {
            ctx->values[i] = value;
            return;
        }
    }

    // add new entry
    if (ctx->value_count >= ctx->value_capacity)
    {
        ctx->value_capacity = ctx->value_capacity ? ctx->value_capacity * 2 : 16;
        ctx->values         = realloc(ctx->values, sizeof(LLVMValueRef) * ctx->value_capacity);
        ctx->value_keys     = realloc(ctx->value_keys, sizeof(Symbol *) * ctx->value_capacity);
    }

    ctx->value_keys[ctx->value_count] = symbol;
    ctx->values[ctx->value_count]     = value;
    ctx->value_count++;
}

// error handling
void codegen_error(CodegenContext *ctx, const char *format, ...)
{
    (void)ctx; // unused for now
    va_list args;
    va_start(args, format);
    fprintf(stderr, "codegen error: ");
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
}

// main codegen entry points
bool codegen_program(CodegenContext *ctx, AstNode *program)
{
    if (!program || program->kind != AST_PROGRAM)
    {
        codegen_error(ctx, "invalid program node");
        return false;
    }

    // generate all declarations
    for (int i = 0; i < program->program.decls->count; i++)
    {
        if (!codegen_declaration(ctx, program->program.decls->items[i]))
        {
            return false;
        }
    }

    return true;
}

bool codegen_module(CodegenContext *ctx, Module *module)
{
    if (!module || !module->ast)
    {
        codegen_error(ctx, "invalid module");
        return false;
    }

    return codegen_program(ctx, module->ast);
}

// declaration codegen
bool codegen_declaration(CodegenContext *ctx, AstNode *decl)
{
    if (!decl)
    {
        return false;
    }

    switch (decl->kind)
    {
    case AST_USE_DECL:
        return codegen_use_decl(ctx, decl);
    case AST_EXT_DECL:
        return codegen_ext_decl(ctx, decl);
    case AST_DEF_DECL:
        return codegen_def_decl(ctx, decl);
    case AST_VAL_DECL:
    case AST_VAR_DECL:
        return codegen_var_decl(ctx, decl);
    case AST_FUN_DECL:
        return codegen_fun_decl(ctx, decl);
    case AST_STR_DECL:
        return codegen_str_decl(ctx, decl);
    case AST_UNI_DECL:
        return codegen_uni_decl(ctx, decl);
    default:
        codegen_error(ctx, "unknown declaration kind");
        return false;
    }
}

bool codegen_use_decl(CodegenContext *ctx, AstNode *decl)
{
    (void)ctx;
    (void)decl; // unused
    // use declarations don't generate code directly
    return true;
}

bool codegen_ext_decl(CodegenContext *ctx, AstNode *decl)
{
    Symbol *symbol = decl->symbol;
    if (!symbol)
    {
        codegen_error(ctx, "external declaration missing symbol");
        return false;
    }

    LLVMTypeRef  llvm_type = codegen_type(ctx, symbol->type);
    LLVMValueRef value;

    if (symbol->type->kind == TYPE_FUNCTION)
    {
        // external function declaration
        value = LLVMAddFunction(ctx->llvm_module, decl->ext_decl.name, llvm_type);
    }
    else
    {
        // external variable declaration
        value = LLVMAddGlobal(ctx->llvm_module, llvm_type, decl->ext_decl.name);
        LLVMSetLinkage(value, LLVMExternalLinkage);
    }

    codegen_set_value(ctx, symbol, value);
    return true;
}

bool codegen_def_decl(CodegenContext *ctx, AstNode *decl)
{
    (void)ctx;
    (void)decl; // unused
    // type definitions don't generate code
    return true;
}

bool codegen_str_decl(CodegenContext *ctx, AstNode *decl)
{
    (void)ctx;
    (void)decl; // unused
    // struct declarations are handled during type conversion
    return true;
}

bool codegen_uni_decl(CodegenContext *ctx, AstNode *decl)
{
    (void)ctx;
    (void)decl; // unused
    // union declarations are handled during type conversion
    return true;
}

bool codegen_var_decl(CodegenContext *ctx, AstNode *decl)
{
    Symbol *symbol = decl->symbol;
    if (!symbol)
    {
        codegen_error(ctx, "variable declaration missing symbol");
        return false;
    }

    // for variables with function types, we need to use function pointers
    Type *var_type = symbol->type;
    while (var_type && var_type->kind == TYPE_ALIAS)
        var_type = var_type->alias.target;

    LLVMTypeRef llvm_type;
    if (var_type && var_type->kind == TYPE_FUNCTION)
    {
        // function variables should be stored as function pointers
        LLVMTypeRef func_type = codegen_type(ctx, var_type);
        if (!func_type)
        {
            return false;
        }
        llvm_type = LLVMPointerType(func_type, 0);
    }
    else
    {
        llvm_type = codegen_type(ctx, symbol->type);
        if (!llvm_type)
        {
            return false;
        }
    }

    LLVMValueRef value;

    if (ctx->current_function)
    {
        // local variable - create alloca
        LLVMBasicBlockRef current_block = LLVMGetInsertBlock(ctx->llvm_builder);
        LLVMBasicBlockRef entry_block   = LLVMGetEntryBasicBlock(ctx->current_function);

        // position at start of entry block for alloca
        LLVMValueRef first_instr = LLVMGetFirstInstruction(entry_block);
        if (first_instr)
        {
            LLVMPositionBuilderBefore(ctx->llvm_builder, first_instr);
        }
        else
        {
            LLVMPositionBuilderAtEnd(ctx->llvm_builder, entry_block);
        }

        value = LLVMBuildAlloca(ctx->llvm_builder, llvm_type, decl->var_decl.name);

        // restore builder position
        LLVMPositionBuilderAtEnd(ctx->llvm_builder, current_block); // handle initializer
        if (decl->var_decl.init)
        {
            LLVMValueRef init_value = codegen_expression(ctx, decl->var_decl.init);
            if (!init_value)
            {
                return false;
            }

            // for arrays, we need to copy the contents properly
            if (var_type && var_type->kind == TYPE_ARRAY)
            {
                if (var_type->array.size >= 0)
                {
                    // BOUND ARRAY [N]T - copy array data
                    LLVMValueRef array_data = LLVMBuildLoad2(ctx->llvm_builder, llvm_type, init_value, "");
                    LLVMBuildStore(ctx->llvm_builder, array_data, value);
                }
                else
                {
                    // UNBOUND ARRAY []T - copy {ptr, len} struct
                    LLVMValueRef slice_data = LLVMBuildLoad2(ctx->llvm_builder, llvm_type, init_value, "");
                    LLVMBuildStore(ctx->llvm_builder, slice_data, value);
                }
            }
            else
            {
                LLVMBuildStore(ctx->llvm_builder, init_value, value);
            }
        }
    }
    else
    {
        // global variable
        value = LLVMAddGlobal(ctx->llvm_module, llvm_type, decl->var_decl.name);

        if (decl->var_decl.init)
        {
            // handle global initializer
            LLVMValueRef init_value = codegen_expression(ctx, decl->var_decl.init);
            if (!init_value)
            {
                // for globals, we might need constant expressions
                codegen_error(ctx, "global variable initializer must be constant");
                return false;
            }
            LLVMSetInitializer(value, init_value);
        }
        else
        {
            // zero initialize
            LLVMSetInitializer(value, LLVMConstNull(llvm_type));
        }
    }

    codegen_set_value(ctx, symbol, value);
    return true;
}

bool codegen_fun_decl(CodegenContext *ctx, AstNode *decl)
{
    Symbol *symbol = decl->symbol;
    if (!symbol || symbol->type->kind != TYPE_FUNCTION)
    {
        codegen_error(ctx, "function declaration missing or invalid symbol");
        return false;
    }

    LLVMTypeRef  func_type = codegen_type(ctx, symbol->type);
    LLVMValueRef function  = LLVMAddFunction(ctx->llvm_module, decl->fun_decl.name, func_type);

    codegen_set_value(ctx, symbol, function);

    // if this is just a declaration (no body), we're done
    if (!decl->fun_decl.body)
    {
        return true;
    }

    // save current function context
    LLVMValueRef      prev_function      = ctx->current_function;
    Type             *prev_function_type = ctx->current_function_type;
    LLVMBasicBlockRef prev_exit_block    = ctx->exit_block;
    LLVMValueRef      prev_return_value  = ctx->return_value;

    // set up new function context
    ctx->current_function      = function;
    ctx->current_function_type = symbol->type;

    // create entry block
    LLVMBasicBlockRef entry_block = LLVMAppendBasicBlock(function, "entry");
    LLVMPositionBuilderAtEnd(ctx->llvm_builder, entry_block);

    // create exit block for returns
    ctx->exit_block = LLVMAppendBasicBlock(function, "exit");

    // create return value alloca if function returns something
    if (symbol->type->function.return_type)
    {
        Type *return_type_mach = symbol->type->function.return_type;

        // follow aliases
        while (return_type_mach->kind == TYPE_ALIAS)
            return_type_mach = return_type_mach->alias.target;

        LLVMTypeRef return_type_llvm;

        // if returning a function type, use function pointer for the alloca
        if (return_type_mach->kind == TYPE_FUNCTION)
        {
            LLVMTypeRef func_type = codegen_type(ctx, return_type_mach);
            if (!func_type)
            {
                // restore previous function context
                ctx->current_function      = prev_function;
                ctx->current_function_type = prev_function_type;
                ctx->exit_block            = prev_exit_block;
                ctx->return_value          = prev_return_value;
                return false;
            }
            return_type_llvm = LLVMPointerType(func_type, 0);
        }
        else
        {
            return_type_llvm = codegen_type(ctx, symbol->type->function.return_type);
            if (!return_type_llvm)
            {
                // restore previous function context
                ctx->current_function      = prev_function;
                ctx->current_function_type = prev_function_type;
                ctx->exit_block            = prev_exit_block;
                ctx->return_value          = prev_return_value;
                return false;
            }
        }

        if (!return_type_llvm)
        {
            fprintf(stderr, "ERROR: return_type_llvm is NULL!\n");
            ctx->current_function      = prev_function;
            ctx->current_function_type = prev_function_type;
            ctx->exit_block            = prev_exit_block;
            ctx->return_value          = prev_return_value;
            return false;
        }

        ctx->return_value = LLVMBuildAlloca(ctx->llvm_builder, return_type_llvm, "retval");
    }
    else
    {
        ctx->return_value = NULL;
    }

    // set up parameters
    int param_count = symbol->type->function.param_count;
    for (int i = 0; i < param_count; i++)
    {
        LLVMValueRef param = LLVMGetParam(function, i);

        if (decl->fun_decl.params && i < decl->fun_decl.params->count)
        {
            AstNode *param_decl   = decl->fun_decl.params->items[i];
            Symbol  *param_symbol = param_decl->symbol;

            if (param_symbol)
            {
                // get the parameter type from the symbol (which may be adjusted for function pointers)
                LLVMTypeRef param_type = codegen_type(ctx, param_symbol->type);
                if (!param_type)
                {
                    // restore previous function context
                    ctx->current_function      = prev_function;
                    ctx->current_function_type = prev_function_type;
                    ctx->exit_block            = prev_exit_block;
                    ctx->return_value          = prev_return_value;
                    return false;
                }

                LLVMValueRef param_alloca = LLVMBuildAlloca(ctx->llvm_builder, param_type, param_decl->param_decl.name);

                // store parameter value
                LLVMBuildStore(ctx->llvm_builder, param, param_alloca);

                // register parameter symbol
                codegen_set_value(ctx, param_symbol, param_alloca);
            }
        }
    }

    // generate function body
    bool success = codegen_statement(ctx, decl->fun_decl.body);

    // ensure we end with a branch to exit block
    LLVMBasicBlockRef current_block = LLVMGetInsertBlock(ctx->llvm_builder);
    if (current_block && !LLVMGetBasicBlockTerminator(current_block))
    {
        LLVMBuildBr(ctx->llvm_builder, ctx->exit_block);
    }

    // generate exit block
    LLVMPositionBuilderAtEnd(ctx->llvm_builder, ctx->exit_block);
    if (ctx->return_value)
    {
        Type *return_type_mach = symbol->type->function.return_type;

        // follow aliases
        while (return_type_mach->kind == TYPE_ALIAS)
            return_type_mach = return_type_mach->alias.target;

        LLVMTypeRef return_type_llvm;

        // if returning a function type, use function pointer for the load
        if (return_type_mach->kind == TYPE_FUNCTION)
        {
            LLVMTypeRef func_type = codegen_type(ctx, return_type_mach);
            if (!func_type)
            {
                // restore previous function context
                ctx->current_function      = prev_function;
                ctx->current_function_type = prev_function_type;
                ctx->exit_block            = prev_exit_block;
                ctx->return_value          = prev_return_value;
                return false;
            }
            return_type_llvm = LLVMPointerType(func_type, 0);
        }
        else
        {
            return_type_llvm = codegen_type(ctx, symbol->type->function.return_type);
            if (!return_type_llvm)
            {
                // restore previous function context
                ctx->current_function      = prev_function;
                ctx->current_function_type = prev_function_type;
                ctx->exit_block            = prev_exit_block;
                ctx->return_value          = prev_return_value;
                return false;
            }
        }

        LLVMValueRef ret_val = LLVMBuildLoad2(ctx->llvm_builder, return_type_llvm, ctx->return_value, "");
        LLVMBuildRet(ctx->llvm_builder, ret_val);
    }
    else
    {
        LLVMBuildRetVoid(ctx->llvm_builder);
    }

    // restore previous function context
    ctx->current_function      = prev_function;
    ctx->current_function_type = prev_function_type;
    ctx->exit_block            = prev_exit_block;
    ctx->return_value          = prev_return_value;

    return success;
}

// statement codegen
bool codegen_statement(CodegenContext *ctx, AstNode *stmt)
{
    if (!stmt)
    {
        return false;
    }

    switch (stmt->kind)
    {
    case AST_BLOCK_STMT:
        return codegen_block_stmt(ctx, stmt);
    case AST_EXPR_STMT:
        return codegen_expr_stmt(ctx, stmt);
    case AST_RET_STMT:
        return codegen_ret_stmt(ctx, stmt);
    case AST_IF_STMT:
        return codegen_if_stmt(ctx, stmt);
    case AST_FOR_STMT:
        return codegen_for_stmt(ctx, stmt);
    case AST_BRK_STMT:
        return codegen_brk_stmt(ctx, stmt);
    case AST_CNT_STMT:
        return codegen_cnt_stmt(ctx, stmt);
    default:
        codegen_error(ctx, "unknown statement kind");
        return false;
    }
}

bool codegen_block_stmt(CodegenContext *ctx, AstNode *stmt)
{
    // blocks can contain both declarations and statements
    for (int i = 0; i < stmt->block_stmt.stmts->count; i++)
    {
        AstNode *s = stmt->block_stmt.stmts->items[i];

        // handle declarations in block
        if (s->kind >= AST_USE_DECL && s->kind <= AST_PARAM_DECL)
        {
            if (!codegen_declaration(ctx, s))
            {
                return false;
            }
        }
        else
        {
            if (!codegen_statement(ctx, s))
            {
                return false;
            }
        }
    }

    return true;
}

bool codegen_expr_stmt(CodegenContext *ctx, AstNode *stmt)
{
    // evaluate expression but discard result
    LLVMValueRef result = codegen_expression(ctx, stmt->expr_stmt.expr);
    return result != NULL;
}

bool codegen_ret_stmt(CodegenContext *ctx, AstNode *stmt)
{
    if (stmt->ret_stmt.expr)
    {
        // return with value
        LLVMValueRef value = codegen_expression(ctx, stmt->ret_stmt.expr);
        if (!value)
        {
            fprintf(stderr, "ERROR: Return expression returned NULL value\n");
            return false;
        }

        if (ctx->return_value)
        {
            // store to return value alloca
            LLVMBuildStore(ctx->llvm_builder, value, ctx->return_value);
        }
    }

    // branch to exit block
    LLVMBuildBr(ctx->llvm_builder, ctx->exit_block);

    // create new unreachable block for any following code
    LLVMBasicBlockRef unreachable_block = LLVMAppendBasicBlock(ctx->current_function, "unreachable");
    LLVMPositionBuilderAtEnd(ctx->llvm_builder, unreachable_block);

    return true;
}

bool codegen_if_stmt(CodegenContext *ctx, AstNode *stmt)
{
    LLVMValueRef cond = codegen_expression(ctx, stmt->if_stmt.cond);
    if (!cond)
    {
        return false;
    }

    // convert condition to i1 (boolean)
    LLVMValueRef cond_bool = LLVMBuildICmp(ctx->llvm_builder, LLVMIntNE, cond, LLVMConstInt(LLVMTypeOf(cond), 0, false), "cond");

    LLVMBasicBlockRef then_block = LLVMAppendBasicBlock(ctx->current_function, "if.then");
    LLVMBasicBlockRef else_block = NULL;
    LLVMBasicBlockRef end_block  = LLVMAppendBasicBlock(ctx->current_function, "if.end");

    if (stmt->if_stmt.else_stmt)
    {
        else_block = LLVMAppendBasicBlock(ctx->current_function, "if.else");
        LLVMBuildCondBr(ctx->llvm_builder, cond_bool, then_block, else_block);
    }
    else
    {
        LLVMBuildCondBr(ctx->llvm_builder, cond_bool, then_block, end_block);
    }

    // generate then block
    LLVMPositionBuilderAtEnd(ctx->llvm_builder, then_block);
    if (!codegen_statement(ctx, stmt->if_stmt.then_stmt))
    {
        return false;
    }

    // branch to end if no terminator
    if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->llvm_builder)))
    {
        LLVMBuildBr(ctx->llvm_builder, end_block);
    }

    // generate else block if present
    if (else_block)
    {
        LLVMPositionBuilderAtEnd(ctx->llvm_builder, else_block);
        if (!codegen_statement(ctx, stmt->if_stmt.else_stmt))
        {
            return false;
        }

        // branch to end if no terminator
        if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->llvm_builder)))
        {
            LLVMBuildBr(ctx->llvm_builder, end_block);
        }
    }

    // continue with end block
    LLVMPositionBuilderAtEnd(ctx->llvm_builder, end_block);
    return true;
}

bool codegen_for_stmt(CodegenContext *ctx, AstNode *stmt)
{
    LLVMBasicBlockRef loop_header = LLVMAppendBasicBlock(ctx->current_function, "for.header");
    LLVMBasicBlockRef loop_body   = LLVMAppendBasicBlock(ctx->current_function, "for.body");
    LLVMBasicBlockRef loop_end    = LLVMAppendBasicBlock(ctx->current_function, "for.end");

    // expand loop context
    if (ctx->loop_depth >= ctx->loop_capacity)
    {
        ctx->loop_capacity    = ctx->loop_capacity ? ctx->loop_capacity * 2 : 4;
        ctx->loop_exit_blocks = realloc(ctx->loop_exit_blocks, sizeof(LLVMBasicBlockRef) * ctx->loop_capacity);
        ctx->loop_cont_blocks = realloc(ctx->loop_cont_blocks, sizeof(LLVMBasicBlockRef) * ctx->loop_capacity);
    }

    ctx->loop_exit_blocks[ctx->loop_depth] = loop_end;
    ctx->loop_cont_blocks[ctx->loop_depth] = loop_header;
    ctx->loop_depth++;

    // branch to header
    LLVMBuildBr(ctx->llvm_builder, loop_header);

    // generate header (condition check)
    LLVMPositionBuilderAtEnd(ctx->llvm_builder, loop_header);
    if (stmt->for_stmt.cond)
    {
        LLVMValueRef cond = codegen_expression(ctx, stmt->for_stmt.cond);
        if (!cond)
        {
            ctx->loop_depth--;
            return false;
        }

        LLVMValueRef cond_bool = LLVMBuildICmp(ctx->llvm_builder, LLVMIntNE, cond, LLVMConstInt(LLVMTypeOf(cond), 0, false), "loopcond");
        LLVMBuildCondBr(ctx->llvm_builder, cond_bool, loop_body, loop_end);
    }
    else
    {
        // infinite loop
        LLVMBuildBr(ctx->llvm_builder, loop_body);
    }

    // generate body
    LLVMPositionBuilderAtEnd(ctx->llvm_builder, loop_body);
    bool success = codegen_statement(ctx, stmt->for_stmt.body);

    // branch back to header if no terminator
    if (success && !LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->llvm_builder)))
    {
        LLVMBuildBr(ctx->llvm_builder, loop_header);
    }

    ctx->loop_depth--;

    // continue with end block
    LLVMPositionBuilderAtEnd(ctx->llvm_builder, loop_end);
    return success;
}

bool codegen_brk_stmt(CodegenContext *ctx, AstNode *stmt)
{
    (void)stmt; // unused
    if (ctx->loop_depth == 0)
    {
        codegen_error(ctx, "break statement outside loop");
        return false;
    }

    LLVMBasicBlockRef exit_block = ctx->loop_exit_blocks[ctx->loop_depth - 1];
    LLVMBuildBr(ctx->llvm_builder, exit_block);

    // create unreachable block
    LLVMBasicBlockRef unreachable_block = LLVMAppendBasicBlock(ctx->current_function, "unreachable");
    LLVMPositionBuilderAtEnd(ctx->llvm_builder, unreachable_block);

    return true;
}

bool codegen_cnt_stmt(CodegenContext *ctx, AstNode *stmt)
{
    (void)stmt; // unused
    if (ctx->loop_depth == 0)
    {
        codegen_error(ctx, "continue statement outside loop");
        return false;
    }

    LLVMBasicBlockRef cont_block = ctx->loop_cont_blocks[ctx->loop_depth - 1];
    LLVMBuildBr(ctx->llvm_builder, cont_block);

    // create unreachable block
    LLVMBasicBlockRef unreachable_block = LLVMAppendBasicBlock(ctx->current_function, "unreachable");
    LLVMPositionBuilderAtEnd(ctx->llvm_builder, unreachable_block);

    return true;
}

// expression codegen
LLVMValueRef codegen_expression(CodegenContext *ctx, AstNode *expr)
{
    if (!expr)
    {
        return NULL;
    }

    switch (expr->kind)
    {
    case AST_BINARY_EXPR:
        return codegen_binary_expr(ctx, expr);
    case AST_UNARY_EXPR:
        return codegen_unary_expr(ctx, expr);
    case AST_CALL_EXPR:
        return codegen_call_expr(ctx, expr);
    case AST_INDEX_EXPR:
        return codegen_index_expr(ctx, expr);
    case AST_FIELD_EXPR:
        return codegen_field_expr(ctx, expr);
    case AST_CAST_EXPR:
        return codegen_cast_expr(ctx, expr);
    case AST_IDENT_EXPR:
        return codegen_ident_expr(ctx, expr);
    case AST_LIT_EXPR:
        return codegen_lit_expr(ctx, expr);
    case AST_ARRAY_EXPR:
        return codegen_array_expr(ctx, expr);
    case AST_STRUCT_EXPR:
        return codegen_struct_expr(ctx, expr);
    case AST_TYPE_NAME:
        // type names that were resolved as variable references in semantic analysis
        if (expr->symbol && (expr->symbol->kind == SYMBOL_VAR || expr->symbol->kind == SYMBOL_PARAM))
        {
            return codegen_ident_expr(ctx, expr);
        }
        else
        {
            codegen_error(ctx, "type name used as expression");
            return NULL;
        }
    default:
        codegen_error(ctx, "unknown expression kind");
        return NULL;
    }
}

LLVMValueRef codegen_lit_expr(CodegenContext *ctx, AstNode *expr)
{
    switch (expr->lit_expr.kind)
    {
    case TOKEN_LIT_INT:
    {
        // determine the appropriate integer type based on context or default to i32
        Type *target_type = expr->type;
        if (!target_type)
        {
            codegen_error(ctx, "integer literal missing type information");
            return NULL;
        }

        LLVMTypeRef llvm_type = codegen_type(ctx, target_type);
        return LLVMConstInt(llvm_type, expr->lit_expr.int_val, target_type->is_signed);
    }

    case TOKEN_LIT_FLOAT:
    {
        Type *target_type = expr->type;
        if (!target_type)
        {
            codegen_error(ctx, "float literal missing type information");
            return NULL;
        }

        LLVMTypeRef llvm_type = codegen_type(ctx, target_type);
        return LLVMConstReal(llvm_type, expr->lit_expr.float_val);
    }

    case TOKEN_LIT_CHAR:
    {
        LLVMTypeRef char_type = LLVMInt8TypeInContext(ctx->llvm_context);
        return LLVMConstInt(char_type, expr->lit_expr.char_val, false);
    }

    case TOKEN_LIT_STRING:
    {
        // create global string constant
        LLVMValueRef str_val = LLVMBuildGlobalStringPtr(ctx->llvm_builder, expr->lit_expr.string_val, "str");
        return str_val;
    }

    default:
        codegen_error(ctx, "unknown literal kind");
        return NULL;
    }
}

LLVMValueRef codegen_ident_expr(CodegenContext *ctx, AstNode *expr)
{
    Symbol *symbol = expr->symbol;
    if (!symbol)
    {
        codegen_error(ctx, "identifier '%s' missing symbol", expr->ident_expr.name);
        return NULL;
    }

    LLVMValueRef value = codegen_get_value(ctx, symbol);
    if (!value)
    {
        codegen_error(ctx, "identifier '%s' not found in codegen context", expr->ident_expr.name);
        return NULL;
    }

    // for variables and parameters, we need to load the value
    if (symbol->kind == SYMBOL_VAR || symbol->kind == SYMBOL_PARAM)
    {
        Type *symbol_type = symbol->type;

        // follow type aliases
        while (symbol_type && symbol_type->kind == TYPE_ALIAS)
            symbol_type = symbol_type->alias.target;

        LLVMTypeRef value_type;
        if (symbol_type && symbol_type->kind == TYPE_FUNCTION)
        {
            // function variables are stored as function pointers
            LLVMTypeRef func_type = codegen_type(ctx, symbol_type);
            value_type            = LLVMPointerType(func_type, 0);
        }
        else
        {
            value_type = codegen_type(ctx, symbol->type);
        }

        return LLVMBuildLoad2(ctx->llvm_builder, value_type, value, "");
    }

    // for functions, return the function pointer directly
    if (symbol->kind == SYMBOL_FUNC)
    {
        return value;
    }

    return value;
}

LLVMValueRef codegen_call_expr(CodegenContext *ctx, AstNode *expr)
{
    // check for builtin functions first
    if (expr->call_expr.func->kind == AST_IDENT_EXPR)
    {
        const char *name = expr->call_expr.func->ident_expr.name;
        if (strcmp(name, "len") == 0 || strcmp(name, "size_of") == 0 || strcmp(name, "align_of") == 0 || strcmp(name, "offset_of") == 0)
        {
            return codegen_builtin_call(ctx, expr);
        }
    }

    // get the function to call
    LLVMValueRef func = codegen_expression(ctx, expr->call_expr.func);
    if (!func)
    {
        return NULL;
    }

    // determine function type
    Type *func_type = expr->call_expr.func->type;
    if (!func_type)
    {
        codegen_error(ctx, "function call missing type information");
        return NULL;
    }

    // handle function pointer calls vs direct function calls
    LLVMTypeRef  call_type = NULL;
    LLVMValueRef callable  = func;

    // follow type aliases to get the actual type
    while (func_type->kind == TYPE_ALIAS)
    {
        func_type = func_type->alias.target;
    }

    if (func_type->kind == TYPE_FUNCTION)
    {
        // direct function call
        call_type = codegen_type(ctx, func_type);
        callable  = func;
    }
    else if (func_type->kind == TYPE_PTR && func_type->ptr.base && func_type->ptr.base->kind == TYPE_FUNCTION)
    {
        // function pointer call
        Type *pointed_func_type = func_type->ptr.base;
        call_type               = codegen_type(ctx, pointed_func_type);
        callable                = func; // function pointer is already loaded
    }
    else
    {
        codegen_error(ctx, "attempting to call non-function type");
        return NULL;
    }

    // get the actual function type being called
    Type *actual_func_type = (func_type->kind == TYPE_FUNCTION) ? func_type : func_type->ptr.base;

    // prepare arguments
    int           arg_count = expr->call_expr.args ? expr->call_expr.args->count : 0;
    LLVMValueRef *args      = NULL;

    if (arg_count > 0)
    {
        args = malloc(sizeof(LLVMValueRef) * arg_count);

        for (int i = 0; i < arg_count; i++)
        {
            LLVMValueRef arg = codegen_expression(ctx, expr->call_expr.args->items[i]);
            if (!arg)
            {
                free(args);
                return NULL;
            }

            // handle function argument decay to pointer
            Type *param_type = actual_func_type->function.param_types[i];
            Type *arg_type   = expr->call_expr.args->items[i]->type;

            // follow aliases
            while (param_type->kind == TYPE_ALIAS)
            {
                param_type = param_type->alias.target;
            }
            while (arg_type->kind == TYPE_ALIAS)
            {
                arg_type = arg_type->alias.target;
            }

            // if parameter expects function pointer and we have a function, no conversion needed
            // LLVM handles function to function pointer decay automatically in calls
            if (param_type->kind == TYPE_PTR && param_type->ptr.base && param_type->ptr.base->kind == TYPE_FUNCTION && arg_type->kind == TYPE_FUNCTION)
            {
                // function argument decays to function pointer automatically
                args[i] = arg;
            }
            else
            {
                args[i] = arg;
            }
        }
    }

    // make the call
    LLVMValueRef result = LLVMBuildCall2(ctx->llvm_builder, call_type, callable, args, arg_count, "");

    free(args);
    return result;
}

LLVMValueRef codegen_builtin_call(CodegenContext *ctx, AstNode *expr)
{
    const char *name      = expr->call_expr.func->ident_expr.name;
    int         arg_count = expr->call_expr.args ? expr->call_expr.args->count : 0;

    if (strcmp(name, "len") == 0)
    {
        if (arg_count != 1)
        {
            codegen_error(ctx, "len() expects 1 argument");
            return NULL;
        }

        AstNode *arg      = expr->call_expr.args->items[0];
        Type    *arg_type = arg->type;

        if (arg_type->kind != TYPE_ARRAY)
        {
            codegen_error(ctx, "len() requires array type");
            return NULL;
        }

        LLVMTypeRef u64_type = LLVMInt64TypeInContext(ctx->llvm_context);

        if (arg_type->array.size >= 0)
        {
            // BOUND ARRAY [N]T - compile-time constant
            return LLVMConstInt(u64_type, arg_type->array.size, false);
        }
        else
        {
            // UNBOUND ARRAY []T - runtime length from {ptr, len} struct
            LLVMValueRef array_val = codegen_expression(ctx, arg);
            if (!array_val)
                return NULL;

            // For unbound arrays, arg should be an alloca of {ptr, len} struct
            // Extract the length field (index 1) which is u64
            LLVMTypeRef  slice_type = codegen_type(ctx, arg_type);
            LLVMValueRef len_ptr    = LLVMBuildStructGEP2(ctx->llvm_builder, slice_type, array_val, 1, "");
            return LLVMBuildLoad2(ctx->llvm_builder, u64_type, len_ptr, "");
        }
    }

    if (strcmp(name, "size_of") == 0)
    {
        if (arg_count != 1)
        {
            codegen_error(ctx, "size_of() expects 1 argument");
            return NULL;
        }

        Type    *target_type = NULL;
        AstNode *arg         = expr->call_expr.args->items[0];

        // argument can be type or expression
        if (arg->kind >= AST_TYPE_NAME && arg->kind <= AST_TYPE_UNI)
        {
            target_type = arg->type;
        }
        else
        {
            target_type = arg->type;
        }

        if (target_type)
        {
            LLVMTypeRef u32_type = LLVMInt32TypeInContext(ctx->llvm_context);
            return LLVMConstInt(u32_type, target_type->size, false);
        }
    }

    if (strcmp(name, "align_of") == 0)
    {
        if (arg_count != 1)
        {
            codegen_error(ctx, "align_of() expects 1 argument");
            return NULL;
        }

        Type    *target_type = NULL;
        AstNode *arg         = expr->call_expr.args->items[0];

        if (arg->kind >= AST_TYPE_NAME && arg->kind <= AST_TYPE_UNI)
        {
            target_type = arg->type;
        }
        else
        {
            target_type = arg->type;
        }

        if (target_type)
        {
            LLVMTypeRef u32_type = LLVMInt32TypeInContext(ctx->llvm_context);
            return LLVMConstInt(u32_type, target_type->alignment, false);
        }
    }

    codegen_error(ctx, "unknown builtin function: %s", name);
    return NULL;
}

// address computation for lvalues
LLVMValueRef codegen_lvalue_address(CodegenContext *ctx, AstNode *expr)
{
    switch (expr->kind)
    {
    case AST_IDENT_EXPR:
    {
        Symbol *symbol = expr->symbol;
        if (!symbol)
        {
            codegen_error(ctx, "identifier missing symbol");
            return NULL;
        }

        if (symbol->kind != SYMBOL_VAR && symbol->kind != SYMBOL_PARAM)
        {
            codegen_error(ctx, "identifier is not a variable");
            return NULL;
        }

        return codegen_get_value(ctx, symbol);
    }

    case AST_UNARY_EXPR:
        if (expr->unary_expr.op == TOKEN_AT)
        {
            // dereference - just evaluate the pointer expression
            return codegen_expression(ctx, expr->unary_expr.expr);
        }
        break;
    case AST_INDEX_EXPR:
    {
        LLVMValueRef index = codegen_expression(ctx, expr->index_expr.index);
        if (!index)
            return NULL;

        Type *array_type = expr->index_expr.array->type;

        if (array_type->array.size >= 0)
        {
            // BOUND ARRAY [N]T - direct indexing
            LLVMValueRef array_addr = (expr->index_expr.array->kind == AST_IDENT_EXPR) ? codegen_lvalue_address(ctx, expr->index_expr.array) : codegen_expression(ctx, expr->index_expr.array);

            if (!array_addr)
                return NULL;

            LLVMTypeRef  array_llvm_type = codegen_type(ctx, array_type);
            LLVMValueRef indices[]       = {LLVMConstInt(LLVMInt32TypeInContext(ctx->llvm_context), 0, 0), index};
            return LLVMBuildGEP2(ctx->llvm_builder, array_llvm_type, array_addr, indices, 2, "");
        }
        else
        {
            // UNBOUND ARRAY []T - access through {ptr, len} struct
            LLVMValueRef slice_addr = (expr->index_expr.array->kind == AST_IDENT_EXPR) ? codegen_lvalue_address(ctx, expr->index_expr.array) : codegen_expression(ctx, expr->index_expr.array);

            if (!slice_addr)
                return NULL;

            // Get ptr field from {ptr, len} struct
            LLVMTypeRef  slice_type = codegen_type(ctx, array_type);
            LLVMValueRef ptr_field  = LLVMBuildStructGEP2(ctx->llvm_builder, slice_type, slice_addr, 0, "");
            LLVMValueRef ptr        = LLVMBuildLoad2(ctx->llvm_builder, LLVMPointerType(codegen_type(ctx, array_type->array.elem_type), 0), ptr_field, "");

            // Index into the array
            LLVMTypeRef elem_type = codegen_type(ctx, array_type->array.elem_type);
            return LLVMBuildGEP2(ctx->llvm_builder, elem_type, ptr, &index, 1, "");
        }
    }

    case AST_FIELD_EXPR:
    {
        LLVMValueRef object_addr = codegen_lvalue_address(ctx, expr->field_expr.object);
        if (!object_addr)
        {
            return NULL;
        }

        // find field index
        Type *object_type = expr->field_expr.object->type;
        while (object_type->kind == TYPE_ALIAS)
        {
            object_type = object_type->alias.target;
        }

        if (object_type->kind != TYPE_STRUCT && object_type->kind != TYPE_UNION)
        {
            codegen_error(ctx, "field access on non-struct/union");
            return NULL;
        }

        int field_index = -1;
        for (int i = 0; i < object_type->composite.field_count; i++)
        {
            if (strcmp(object_type->composite.fields[i]->name, expr->field_expr.field) == 0)
            {
                field_index = i;
                break;
            }
        }

        if (field_index < 0)
        {
            codegen_error(ctx, "field not found");
            return NULL;
        }

        // get field pointer
        LLVMValueRef indices[] = {LLVMConstInt(LLVMInt32TypeInContext(ctx->llvm_context), 0, false), LLVMConstInt(LLVMInt32TypeInContext(ctx->llvm_context), field_index, false)};

        return LLVMBuildGEP2(ctx->llvm_builder, codegen_type(ctx, object_type), object_addr, indices, 2, "");
    }

    default:
        codegen_error(ctx, "expression is not an lvalue");
        return NULL;
    }

    return NULL;
}

LLVMValueRef codegen_binary_expr(CodegenContext *ctx, AstNode *expr)
{
    // handle assignment specially
    if (expr->binary_expr.op == TOKEN_EQUAL)
    {
        LLVMValueRef lvalue_addr = codegen_lvalue_address(ctx, expr->binary_expr.left);
        if (!lvalue_addr)
        {
            return NULL;
        }

        LLVMValueRef rvalue = codegen_expression(ctx, expr->binary_expr.right);
        if (!rvalue)
        {
            return NULL;
        }

        LLVMBuildStore(ctx->llvm_builder, rvalue, lvalue_addr);
        return rvalue;
    }

    // evaluate both operands
    LLVMValueRef left  = codegen_expression(ctx, expr->binary_expr.left);
    LLVMValueRef right = codegen_expression(ctx, expr->binary_expr.right);
    if (!left || !right)
    {
        return NULL;
    }

    Type *left_type = expr->binary_expr.left->type;

    switch (expr->binary_expr.op)
    {
    case TOKEN_PLUS:
        if (type_is_integer(left_type))
        {
            return LLVMBuildAdd(ctx->llvm_builder, left, right, "");
        }
        else
        {
            return LLVMBuildFAdd(ctx->llvm_builder, left, right, "");
        }

    case TOKEN_MINUS:
        if (type_is_integer(left_type))
        {
            return LLVMBuildSub(ctx->llvm_builder, left, right, "");
        }
        else
        {
            return LLVMBuildFSub(ctx->llvm_builder, left, right, "");
        }

    case TOKEN_STAR:
        if (type_is_integer(left_type))
        {
            return LLVMBuildMul(ctx->llvm_builder, left, right, "");
        }
        else
        {
            return LLVMBuildFMul(ctx->llvm_builder, left, right, "");
        }

    case TOKEN_SLASH:
        if (type_is_integer(left_type))
        {
            if (left_type->is_signed)
            {
                return LLVMBuildSDiv(ctx->llvm_builder, left, right, "");
            }
            else
            {
                return LLVMBuildUDiv(ctx->llvm_builder, left, right, "");
            }
        }
        else
        {
            return LLVMBuildFDiv(ctx->llvm_builder, left, right, "");
        }

    case TOKEN_PERCENT:
        if (left_type->is_signed)
        {
            return LLVMBuildSRem(ctx->llvm_builder, left, right, "");
        }
        else
        {
            return LLVMBuildURem(ctx->llvm_builder, left, right, "");
        }

    case TOKEN_AMPERSAND:
        return LLVMBuildAnd(ctx->llvm_builder, left, right, "");

    case TOKEN_PIPE:
        return LLVMBuildOr(ctx->llvm_builder, left, right, "");

    case TOKEN_CARET:
        return LLVMBuildXor(ctx->llvm_builder, left, right, "");

    case TOKEN_LESS_LESS:
        return LLVMBuildShl(ctx->llvm_builder, left, right, "");

    case TOKEN_GREATER_GREATER:
        if (left_type->is_signed)
        {
            return LLVMBuildAShr(ctx->llvm_builder, left, right, "");
        }
        else
        {
            return LLVMBuildLShr(ctx->llvm_builder, left, right, "");
        }

    case TOKEN_EQUAL_EQUAL:
        if (type_is_integer(left_type))
        {
            LLVMValueRef cmp = LLVMBuildICmp(ctx->llvm_builder, LLVMIntEQ, left, right, "");
            return LLVMBuildZExt(ctx->llvm_builder, cmp, LLVMInt8TypeInContext(ctx->llvm_context), "");
        }
        else
        {
            LLVMValueRef cmp = LLVMBuildFCmp(ctx->llvm_builder, LLVMRealOEQ, left, right, "");
            return LLVMBuildZExt(ctx->llvm_builder, cmp, LLVMInt8TypeInContext(ctx->llvm_context), "");
        }

    case TOKEN_BANG_EQUAL:
        if (type_is_integer(left_type))
        {
            LLVMValueRef cmp = LLVMBuildICmp(ctx->llvm_builder, LLVMIntNE, left, right, "");
            return LLVMBuildZExt(ctx->llvm_builder, cmp, LLVMInt8TypeInContext(ctx->llvm_context), "");
        }
        else
        {
            LLVMValueRef cmp = LLVMBuildFCmp(ctx->llvm_builder, LLVMRealONE, left, right, "");
            return LLVMBuildZExt(ctx->llvm_builder, cmp, LLVMInt8TypeInContext(ctx->llvm_context), "");
        }

    case TOKEN_LESS:
        if (type_is_integer(left_type))
        {
            LLVMIntPredicate pred = left_type->is_signed ? LLVMIntSLT : LLVMIntULT;
            LLVMValueRef     cmp  = LLVMBuildICmp(ctx->llvm_builder, pred, left, right, "");
            return LLVMBuildZExt(ctx->llvm_builder, cmp, LLVMInt8TypeInContext(ctx->llvm_context), "");
        }
        else
        {
            LLVMValueRef cmp = LLVMBuildFCmp(ctx->llvm_builder, LLVMRealOLT, left, right, "");
            return LLVMBuildZExt(ctx->llvm_builder, cmp, LLVMInt8TypeInContext(ctx->llvm_context), "");
        }

    case TOKEN_GREATER:
        if (type_is_integer(left_type))
        {
            LLVMIntPredicate pred = left_type->is_signed ? LLVMIntSGT : LLVMIntUGT;
            LLVMValueRef     cmp  = LLVMBuildICmp(ctx->llvm_builder, pred, left, right, "");
            return LLVMBuildZExt(ctx->llvm_builder, cmp, LLVMInt8TypeInContext(ctx->llvm_context), "");
        }
        else
        {
            LLVMValueRef cmp = LLVMBuildFCmp(ctx->llvm_builder, LLVMRealOGT, left, right, "");
            return LLVMBuildZExt(ctx->llvm_builder, cmp, LLVMInt8TypeInContext(ctx->llvm_context), "");
        }

    case TOKEN_LESS_EQUAL:
        if (type_is_integer(left_type))
        {
            LLVMIntPredicate pred = left_type->is_signed ? LLVMIntSLE : LLVMIntULE;
            LLVMValueRef     cmp  = LLVMBuildICmp(ctx->llvm_builder, pred, left, right, "");
            return LLVMBuildZExt(ctx->llvm_builder, cmp, LLVMInt8TypeInContext(ctx->llvm_context), "");
        }
        else
        {
            LLVMValueRef cmp = LLVMBuildFCmp(ctx->llvm_builder, LLVMRealOLE, left, right, "");
            return LLVMBuildZExt(ctx->llvm_builder, cmp, LLVMInt8TypeInContext(ctx->llvm_context), "");
        }

    case TOKEN_GREATER_EQUAL:
        if (type_is_integer(left_type))
        {
            LLVMIntPredicate pred = left_type->is_signed ? LLVMIntSGE : LLVMIntUGE;
            LLVMValueRef     cmp  = LLVMBuildICmp(ctx->llvm_builder, pred, left, right, "");
            return LLVMBuildZExt(ctx->llvm_builder, cmp, LLVMInt8TypeInContext(ctx->llvm_context), "");
        }
        else
        {
            LLVMValueRef cmp = LLVMBuildFCmp(ctx->llvm_builder, LLVMRealOGE, left, right, "");
            return LLVMBuildZExt(ctx->llvm_builder, cmp, LLVMInt8TypeInContext(ctx->llvm_context), "");
        }

    case TOKEN_AMPERSAND_AMPERSAND:
    {
        // short-circuit AND
        LLVMValueRef left_bool  = LLVMBuildICmp(ctx->llvm_builder, LLVMIntNE, left, LLVMConstInt(LLVMTypeOf(left), 0, false), "");
        LLVMValueRef right_bool = LLVMBuildICmp(ctx->llvm_builder, LLVMIntNE, right, LLVMConstInt(LLVMTypeOf(right), 0, false), "");
        LLVMValueRef result     = LLVMBuildAnd(ctx->llvm_builder, left_bool, right_bool, "");
        return LLVMBuildZExt(ctx->llvm_builder, result, LLVMInt8TypeInContext(ctx->llvm_context), "");
    }

    case TOKEN_PIPE_PIPE:
    {
        // short-circuit OR
        LLVMValueRef left_bool  = LLVMBuildICmp(ctx->llvm_builder, LLVMIntNE, left, LLVMConstInt(LLVMTypeOf(left), 0, false), "");
        LLVMValueRef right_bool = LLVMBuildICmp(ctx->llvm_builder, LLVMIntNE, right, LLVMConstInt(LLVMTypeOf(right), 0, false), "");
        LLVMValueRef result     = LLVMBuildOr(ctx->llvm_builder, left_bool, right_bool, "");
        return LLVMBuildZExt(ctx->llvm_builder, result, LLVMInt8TypeInContext(ctx->llvm_context), "");
    }

    default:
        codegen_error(ctx, "unknown binary operator");
        return NULL;
    }
}

LLVMValueRef codegen_unary_expr(CodegenContext *ctx, AstNode *expr)
{
    LLVMValueRef operand = codegen_expression(ctx, expr->unary_expr.expr);
    if (!operand)
    {
        return NULL;
    }

    Type *operand_type = expr->unary_expr.expr->type;

    switch (expr->unary_expr.op)
    {
    case TOKEN_MINUS:
        if (type_is_integer(operand_type))
        {
            return LLVMBuildNeg(ctx->llvm_builder, operand, "");
        }
        else
        {
            return LLVMBuildFNeg(ctx->llvm_builder, operand, "");
        }

    case TOKEN_PLUS:
        return operand; // unary plus is no-op

    case TOKEN_TILDE:
        return LLVMBuildNot(ctx->llvm_builder, operand, "");

    case TOKEN_BANG:
    {
        LLVMValueRef cmp = LLVMBuildICmp(ctx->llvm_builder, LLVMIntEQ, operand, LLVMConstInt(LLVMTypeOf(operand), 0, false), "");
        return LLVMBuildZExt(ctx->llvm_builder, cmp, LLVMInt8TypeInContext(ctx->llvm_context), "");
    }

    case TOKEN_QUESTION:
    {
        // address-of operator
        LLVMValueRef addr = codegen_lvalue_address(ctx, expr->unary_expr.expr);
        return addr;
    }

    case TOKEN_AT:
    {
        // dereference operator
        LLVMTypeRef target_type = codegen_type(ctx, expr->type);
        return LLVMBuildLoad2(ctx->llvm_builder, target_type, operand, "");
    }

    default:
        codegen_error(ctx, "unknown unary operator");
        return NULL;
    }
}

LLVMValueRef codegen_index_expr(CodegenContext *ctx, AstNode *expr)
{
    LLVMValueRef addr = codegen_lvalue_address(ctx, expr);
    if (!addr)
    {
        return NULL;
    }

    LLVMTypeRef elem_type = codegen_type(ctx, expr->type);
    return LLVMBuildLoad2(ctx->llvm_builder, elem_type, addr, "");
}

LLVMValueRef codegen_field_expr(CodegenContext *ctx, AstNode *expr)
{
    // check if this is a module field access
    if (expr->field_expr.object->kind == AST_IDENT_EXPR)
    {
        Symbol *obj_symbol = expr->field_expr.object->symbol;
        if (obj_symbol && obj_symbol->kind == SYMBOL_MODULE)
        {
            // module field access - return the symbol value directly
            Symbol *field_symbol = expr->symbol;
            if (!field_symbol)
            {
                codegen_error(ctx, "module field access missing symbol");
                return NULL;
            }

            LLVMValueRef value = codegen_get_value(ctx, field_symbol);
            if (!value)
            {
                codegen_error(ctx, "module field not found in codegen context");
                return NULL;
            }

            // for functions, return the function pointer directly
            if (field_symbol->kind == SYMBOL_FUNC)
            {
                return value;
            }

            // for variables, load the value
            if (field_symbol->kind == SYMBOL_VAR)
            {
                LLVMTypeRef value_type = codegen_type(ctx, field_symbol->type);
                return LLVMBuildLoad2(ctx->llvm_builder, value_type, value, "");
            }

            return value;
        }
    }

    // regular struct/union field access
    LLVMValueRef addr = codegen_lvalue_address(ctx, expr);
    if (!addr)
    {
        return NULL;
    }

    LLVMTypeRef field_type = codegen_type(ctx, expr->type);
    return LLVMBuildLoad2(ctx->llvm_builder, field_type, addr, "");
}

LLVMValueRef codegen_cast_expr(CodegenContext *ctx, AstNode *expr)
{
    LLVMValueRef value = codegen_expression(ctx, expr->cast_expr.expr);
    if (!value)
    {
        return NULL;
    }

    Type *from_type = expr->cast_expr.expr->type;
    Type *to_type   = expr->type;

    LLVMTypeRef from_llvm = codegen_type(ctx, from_type);
    LLVMTypeRef to_llvm   = codegen_type(ctx, to_type);

    // if types are the same, no cast needed
    if (from_llvm == to_llvm)
    {
        return value;
    }

    // integer to integer
    if (type_is_integer(from_type) && type_is_integer(to_type))
    {
        if (from_type->size < to_type->size)
        {
            // extending
            if (from_type->is_signed)
            {
                return LLVMBuildSExt(ctx->llvm_builder, value, to_llvm, "");
            }
            else
            {
                return LLVMBuildZExt(ctx->llvm_builder, value, to_llvm, "");
            }
        }
        else if (from_type->size > to_type->size)
        {
            // truncating
            return LLVMBuildTrunc(ctx->llvm_builder, value, to_llvm, "");
        }
        else
        {
            // same size, just bitcast
            return LLVMBuildBitCast(ctx->llvm_builder, value, to_llvm, "");
        }
    }

    // integer to float
    if (type_is_integer(from_type) && from_type->kind == TYPE_FLOAT)
    {
        if (from_type->is_signed)
        {
            return LLVMBuildSIToFP(ctx->llvm_builder, value, to_llvm, "");
        }
        else
        {
            return LLVMBuildUIToFP(ctx->llvm_builder, value, to_llvm, "");
        }
    }

    // float to integer
    if (from_type->kind == TYPE_FLOAT && type_is_integer(to_type))
    {
        if (to_type->is_signed)
        {
            return LLVMBuildFPToSI(ctx->llvm_builder, value, to_llvm, "");
        }
        else
        {
            return LLVMBuildFPToUI(ctx->llvm_builder, value, to_llvm, "");
        }
    }

    // float to float
    if (from_type->kind == TYPE_FLOAT && to_type->kind == TYPE_FLOAT)
    {
        if (from_type->size < to_type->size)
        {
            return LLVMBuildFPExt(ctx->llvm_builder, value, to_llvm, "");
        }
        else
        {
            return LLVMBuildFPTrunc(ctx->llvm_builder, value, to_llvm, "");
        }
    }

    // pointer casts
    if (from_type->kind == TYPE_PTR && to_type->kind == TYPE_PTR)
    {
        return LLVMBuildBitCast(ctx->llvm_builder, value, to_llvm, "");
    }

    // fallback to bitcast
    return LLVMBuildBitCast(ctx->llvm_builder, value, to_llvm, "");
}

LLVMValueRef codegen_array_expr(CodegenContext *ctx, AstNode *expr)
{
    Type *array_type = expr->type;
    if (!array_type || array_type->kind != TYPE_ARRAY)
    {
        codegen_error(ctx, "array expression missing array type");
        return NULL;
    }

    int         elem_count     = expr->array_expr.elems ? expr->array_expr.elems->count : 0;
    Type       *elem_type      = array_type->array.elem_type;
    LLVMTypeRef elem_llvm_type = codegen_type(ctx, elem_type);

    if (array_type->array.size >= 0)
    {
        // BOUND ARRAY [N]T - store as LLVM array
        LLVMTypeRef  array_llvm_type = LLVMArrayType(elem_llvm_type, array_type->array.size);
        LLVMValueRef array_alloca    = LLVMBuildAlloca(ctx->llvm_builder, array_llvm_type, "array_temp");

        // initialize to zero
        LLVMValueRef zero = LLVMConstNull(array_llvm_type);
        LLVMBuildStore(ctx->llvm_builder, zero, array_alloca);

        // set elements
        for (int i = 0; i < elem_count; i++)
        {
            LLVMValueRef elem_value = codegen_expression(ctx, expr->array_expr.elems->items[i]);
            if (!elem_value)
                return NULL;

            LLVMValueRef indices[2] = {LLVMConstInt(LLVMInt32TypeInContext(ctx->llvm_context), 0, 0), LLVMConstInt(LLVMInt32TypeInContext(ctx->llvm_context), i, 0)};
            LLVMValueRef elem_ptr   = LLVMBuildGEP2(ctx->llvm_builder, array_llvm_type, array_alloca, indices, 2, "");
            LLVMBuildStore(ctx->llvm_builder, elem_value, elem_ptr);
        }

        return array_alloca;
    }
    else
    {
        // UNBOUND ARRAY []T - store as {ptr, len} struct
        LLVMTypeRef  slice_type   = codegen_type(ctx, array_type); // {ptr, len}
        LLVMValueRef slice_alloca = LLVMBuildAlloca(ctx->llvm_builder, slice_type, "slice_temp");

        if (elem_count > 0)
        {
            // allocate memory for elements
            LLVMValueRef count      = LLVMConstInt(LLVMInt64TypeInContext(ctx->llvm_context), elem_count, 0);
            LLVMValueRef elem_size  = LLVMConstInt(LLVMInt64TypeInContext(ctx->llvm_context), elem_type->size, 0);
            LLVMValueRef total_size = LLVMBuildMul(ctx->llvm_builder, count, elem_size, "");

            // malloc call (for now, simple allocation)
            LLVMTypeRef  malloc_type = LLVMFunctionType(LLVMPointerType(LLVMInt8TypeInContext(ctx->llvm_context), 0), &(LLVMTypeRef){LLVMInt64TypeInContext(ctx->llvm_context)}, 1, false);
            LLVMValueRef malloc_func = LLVMAddFunction(ctx->llvm_module, "malloc", malloc_type);
            LLVMValueRef raw_ptr     = LLVMBuildCall2(ctx->llvm_builder, malloc_type, malloc_func, &total_size, 1, "");
            LLVMValueRef typed_ptr   = LLVMBuildBitCast(ctx->llvm_builder, raw_ptr, LLVMPointerType(elem_llvm_type, 0), "");

            // store ptr in struct
            LLVMValueRef ptr_field = LLVMBuildStructGEP2(ctx->llvm_builder, slice_type, slice_alloca, 0, "");
            LLVMBuildStore(ctx->llvm_builder, typed_ptr, ptr_field);

            // store length in struct
            LLVMValueRef len_field = LLVMBuildStructGEP2(ctx->llvm_builder, slice_type, slice_alloca, 1, "");
            LLVMBuildStore(ctx->llvm_builder, count, len_field);

            // initialize elements
            for (int i = 0; i < elem_count; i++)
            {
                LLVMValueRef elem_value = codegen_expression(ctx, expr->array_expr.elems->items[i]);
                if (!elem_value)
                    return NULL;

                LLVMValueRef index    = LLVMConstInt(LLVMInt32TypeInContext(ctx->llvm_context), i, 0);
                LLVMValueRef elem_ptr = LLVMBuildGEP2(ctx->llvm_builder, elem_llvm_type, typed_ptr, &index, 1, "");
                LLVMBuildStore(ctx->llvm_builder, elem_value, elem_ptr);
            }
        }
        else
        {
            // empty array
            LLVMValueRef null_ptr = LLVMConstNull(LLVMPointerType(elem_llvm_type, 0));
            LLVMValueRef zero_len = LLVMConstInt(LLVMInt64TypeInContext(ctx->llvm_context), 0, 0);

            LLVMValueRef ptr_field = LLVMBuildStructGEP2(ctx->llvm_builder, slice_type, slice_alloca, 0, "");
            LLVMBuildStore(ctx->llvm_builder, null_ptr, ptr_field);

            LLVMValueRef len_field = LLVMBuildStructGEP2(ctx->llvm_builder, slice_type, slice_alloca, 1, "");
            LLVMBuildStore(ctx->llvm_builder, zero_len, len_field);
        }

        return slice_alloca;
    }
}

LLVMValueRef codegen_struct_expr(CodegenContext *ctx, AstNode *expr)
{
    // get the struct/union type
    Type *composite_type = expr->type;
    if (!composite_type || (composite_type->kind != TYPE_STRUCT && composite_type->kind != TYPE_UNION))
    {
        codegen_error(ctx, "struct/union expression missing composite type");
        return NULL;
    }

    // get LLVM struct/union type
    LLVMTypeRef llvm_composite_type = codegen_type(ctx, composite_type);
    if (!llvm_composite_type)
    {
        return NULL;
    }

    // create temporary alloca for the struct/union
    LLVMValueRef composite_alloca = LLVMBuildAlloca(ctx->llvm_builder, llvm_composite_type, "composite_temp");

    // initialize all fields to zero first
    LLVMValueRef zero = LLVMConstNull(llvm_composite_type);
    LLVMBuildStore(ctx->llvm_builder, zero, composite_alloca);

    // set field values from the initializer list
    if (expr->struct_expr.fields)
    {
        for (int i = 0; i < expr->struct_expr.fields->count; i++)
        {
            AstNode *field_init = expr->struct_expr.fields->items[i];
            if (field_init->kind != AST_FIELD_EXPR)
            {
                codegen_error(ctx, "invalid field initializer in struct expression");
                return NULL;
            }

            const char *field_name       = field_init->field_expr.field;
            AstNode    *field_value_expr = field_init->field_expr.object;

            // find field index in struct/union
            int field_index = -1;
            for (int j = 0; j < composite_type->composite.field_count; j++)
            {
                if (strcmp(composite_type->composite.fields[j]->name, field_name) == 0)
                {
                    field_index = j;
                    break;
                }
            }

            if (field_index == -1)
            {
                codegen_error(ctx, "field '%s' not found in struct/union", field_name);
                return NULL;
            }

            // generate code for field value
            LLVMValueRef field_value = codegen_expression(ctx, field_value_expr);
            if (!field_value)
            {
                return NULL;
            }

            // get pointer to the field and store the value
            LLVMValueRef field_ptr = LLVMBuildStructGEP2(ctx->llvm_builder, llvm_composite_type, composite_alloca, field_index, "");
            LLVMBuildStore(ctx->llvm_builder, field_value, field_ptr);
        }
    }

    // load the complete struct/union value
    return LLVMBuildLoad2(ctx->llvm_builder, llvm_composite_type, composite_alloca, "");
}

// output generation
bool codegen_write_ir(CodegenContext *ctx, const char *filename)
{
    char *error = NULL;
    if (LLVMPrintModuleToFile(ctx->llvm_module, filename, &error))
    {
        fprintf(stderr, "Failed to write IR: %s\n", error);
        LLVMDisposeMessage(error);
        return false;
    }
    return true;
}

bool codegen_write_bitcode(CodegenContext *ctx, const char *filename)
{
    if (LLVMWriteBitcodeToFile(ctx->llvm_module, filename))
    {
        fprintf(stderr, "Failed to write bitcode\n");
        return false;
    }
    return true;
}

bool codegen_write_object(CodegenContext *ctx, const char *filename)
{
    // initialize targets
    LLVMInitializeAllTargetInfos();
    LLVMInitializeAllTargets();
    LLVMInitializeAllTargetMCs();
    LLVMInitializeAllAsmParsers();
    LLVMInitializeAllAsmPrinters();

    // get default target triple
    char *triple = LLVMGetDefaultTargetTriple();
    LLVMSetTarget(ctx->llvm_module, triple);

    char         *error = NULL;
    LLVMTargetRef target;
    if (LLVMGetTargetFromTriple(triple, &target, &error))
    {
        fprintf(stderr, "Failed to get target: %s\n", error);
        LLVMDisposeMessage(error);
        LLVMDisposeMessage(triple);
        return false;
    }

    // create target machine
    LLVMTargetMachineRef machine = LLVMCreateTargetMachine(target, triple, "", "", LLVMCodeGenLevelDefault, LLVMRelocDefault, LLVMCodeModelDefault);

    if (LLVMTargetMachineEmitToFile(machine, ctx->llvm_module, (char *)filename, LLVMObjectFile, &error))
    {
        fprintf(stderr, "Failed to emit object file: %s\n", error);
        LLVMDisposeMessage(error);
        LLVMDisposeTargetMachine(machine);
        LLVMDisposeMessage(triple);
        return false;
    }

    LLVMDisposeTargetMachine(machine);
    LLVMDisposeMessage(triple);
    return true;
}

bool codegen_link_executable(const char *object_file, const char *exe_file)
{
    // simple linking using system linker
    char command[1024];
    snprintf(command, sizeof(command), "clang %s -o %s", object_file, exe_file);
    return system(command) == 0;
}
