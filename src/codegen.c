#include "codegen.h"
#include <llvm-c/TargetMachine.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static LLVMTypeRef  get_llvm_type(CodeGenerator *gen, Type *type);
static CodegenValue codegen_expression(CodeGenerator *gen, Node *node);
static bool         codegen_statement(CodeGenerator *gen, Node *node);
static bool         codegen_node(CodeGenerator *gen, Node *node);

void codegen_init(CodeGenerator *gen, SemanticAnalyzer *analyzer, const char *module_name)
{
    gen->context = LLVMContextCreate();
    gen->module  = LLVMModuleCreateWithNameInContext(module_name, gen->context);
    gen->builder = LLVMCreateBuilderInContext(gen->context);

    gen->current_function = NULL;
    gen->loop_continue    = NULL;
    gen->loop_break       = NULL;
    gen->current_scope    = NULL;

    gen->symbols    = calloc(1, sizeof(Symbol *));
    gen->values     = calloc(1, sizeof(LLVMValueRef));
    gen->types      = calloc(1, sizeof(Type *));
    gen->llvm_types = calloc(1, sizeof(LLVMTypeRef));

    gen->analyzer = analyzer;

    LLVMInitializeAllTargetInfos();
    LLVMInitializeAllTargets();
    LLVMInitializeAllTargetMCs();
    LLVMInitializeAllAsmParsers();
    LLVMInitializeAllAsmPrinters();

    // set target triple and data layout
    char *triple = LLVMGetDefaultTargetTriple();
    LLVMSetTarget(gen->module, triple);

    LLVMTargetRef target;
    char         *error = NULL;
    if (LLVMGetTargetFromTriple(triple, &target, &error) == 0)
    {
        LLVMTargetMachineRef machine = LLVMCreateTargetMachine(target, triple, "generic", "", LLVMCodeGenLevelDefault, LLVMRelocDefault, LLVMCodeModelDefault);

        LLVMTargetDataRef data_layout     = LLVMCreateTargetDataLayout(machine);
        char             *data_layout_str = LLVMCopyStringRepOfTargetData(data_layout);
        LLVMSetDataLayout(gen->module, data_layout_str);

        LLVMDisposeMessage(data_layout_str);
        LLVMDisposeTargetData(data_layout);
        LLVMDisposeTargetMachine(machine);
    }
    else
    {
        fprintf(stderr, "Warning: Could not set target: %s\n", error);
        LLVMDisposeMessage(error);
    }

    LLVMDisposeMessage(triple);
}

void codegen_dnit(CodeGenerator *gen)
{
    if (!gen)
        return;

    free(gen->symbols);
    free(gen->values);
    free(gen->types);
    free(gen->llvm_types);

    LLVMDisposeBuilder(gen->builder);
    LLVMDisposeModule(gen->module);
    LLVMContextDispose(gen->context);
}

static void register_value(CodeGenerator *gen, Symbol *symbol, LLVMValueRef value)
{
    size_t count = 0;
    while (gen->symbols[count])
        count++;

    gen->symbols = realloc(gen->symbols, (count + 2) * sizeof(Symbol *));
    gen->values  = realloc(gen->values, (count + 2) * sizeof(LLVMValueRef));

    gen->symbols[count]     = symbol;
    gen->values[count]      = value;
    gen->symbols[count + 1] = NULL;
}

static LLVMValueRef lookup_value(CodeGenerator *gen, Symbol *symbol)
{
    for (size_t i = 0; gen->symbols[i]; i++)
    {
        if (gen->symbols[i] == symbol)
        {
            return gen->values[i];
        }
    }
    return NULL;
}

static void cache_llvm_type(CodeGenerator *gen, Type *type, LLVMTypeRef llvm_type)
{
    size_t count = 0;
    while (gen->types[count])
        count++;

    gen->types      = realloc(gen->types, (count + 2) * sizeof(Type *));
    gen->llvm_types = realloc(gen->llvm_types, (count + 2) * sizeof(LLVMTypeRef));

    gen->types[count]      = type;
    gen->llvm_types[count] = llvm_type;
    gen->types[count + 1]  = NULL;
}

static LLVMTypeRef lookup_llvm_type(CodeGenerator *gen, Type *type)
{
    for (size_t i = 0; gen->types[i]; i++)
    {
        if (gen->types[i] == type)
        {
            return gen->llvm_types[i];
        }
    }
    return NULL;
}

static LLVMTypeRef get_llvm_type(CodeGenerator *gen, Type *type)
{
    if (!type)
        return LLVMVoidTypeInContext(gen->context);

    LLVMTypeRef cached = lookup_llvm_type(gen, type);
    if (cached)
        return cached;

    while (type->kind == TYPE_ALIAS)
    {
        type = type->alias.base;
    }

    LLVMTypeRef llvm_type = NULL;

    switch (type->kind)
    {
    case TYPE_VOID:
        llvm_type = LLVMVoidTypeInContext(gen->context);
        break;

    case TYPE_INT:
        if (type->size == 1)
            llvm_type = LLVMInt8TypeInContext(gen->context);
        else if (type->size == 2)
            llvm_type = LLVMInt16TypeInContext(gen->context);
        else if (type->size == 4)
            llvm_type = LLVMInt32TypeInContext(gen->context);
        else if (type->size == 8)
            llvm_type = LLVMInt64TypeInContext(gen->context);
        break;

    case TYPE_FLOAT:
        if (type->size == 4)
            llvm_type = LLVMFloatTypeInContext(gen->context);
        else if (type->size == 8)
            llvm_type = LLVMDoubleTypeInContext(gen->context);
        break;

    case TYPE_POINTER:
        llvm_type = LLVMPointerType(get_llvm_type(gen, type->pointer.base), 0);
        break;

    case TYPE_ARRAY:
        if (type->array.size > 0)
        {
            llvm_type = LLVMArrayType(get_llvm_type(gen, type->array.element), type->array.size);
        }
        else
        {
            llvm_type = LLVMPointerType(get_llvm_type(gen, type->array.element), 0);
        }
        break;

    case TYPE_FUNCTION:
    {
        LLVMTypeRef return_type = get_llvm_type(gen, type->function.return_type);

        size_t param_count = 0;
        if (type->function.params)
        {
            while (type->function.params[param_count])
                param_count++;
        }

        LLVMTypeRef *param_types = calloc(param_count, sizeof(LLVMTypeRef));
        for (size_t i = 0; i < param_count; i++)
        {
            param_types[i] = get_llvm_type(gen, type->function.params[i]);
        }

        llvm_type = LLVMFunctionType(return_type, param_types, param_count, type->function.is_variadic);
        free(param_types);
        break;
    }

    case TYPE_STRUCT:
    {
        llvm_type = LLVMStructCreateNamed(gen->context, type->composite.name);
        cache_llvm_type(gen, type, llvm_type);

        if (type->composite.fields)
        {
            size_t field_count = 0;
            while (type->composite.fields[field_count])
                field_count++;

            LLVMTypeRef *field_types = calloc(field_count, sizeof(LLVMTypeRef));
            for (size_t i = 0; i < field_count; i++)
            {
                field_types[i] = get_llvm_type(gen, type->composite.fields[i]->type);
            }

            LLVMStructSetBody(llvm_type, field_types, field_count, 0);
            free(field_types);
        }
        return llvm_type;
    }

    case TYPE_UNION:
    {
        llvm_type = LLVMStructCreateNamed(gen->context, type->composite.name);
        cache_llvm_type(gen, type, llvm_type);

        if (type->size > 0)
        {
            LLVMTypeRef array_type = LLVMArrayType(LLVMInt8TypeInContext(gen->context), type->size);
            LLVMStructSetBody(llvm_type, &array_type, 1, 0);
        }
        return llvm_type;
    }

    default:
        return LLVMVoidTypeInContext(gen->context);
    }

    if (llvm_type)
    {
        cache_llvm_type(gen, type, llvm_type);
    }

    return llvm_type;
}

static CodegenValue codegen_literal(CodeGenerator *gen, Node *node)
{
    CodegenValue result = {0};

    switch (node->kind)
    {
    case NODE_LIT_INT:
    {
        Type *type   = type_builtin("i32");
        result.type  = type;
        result.value = LLVMConstInt(get_llvm_type(gen, type), node->int_value, type->is_signed);
        break;
    }

    case NODE_LIT_FLOAT:
    {
        Type *type   = type_builtin("f64");
        result.type  = type;
        result.value = LLVMConstReal(get_llvm_type(gen, type), node->float_value);
        break;
    }

    case NODE_LIT_CHAR:
    {
        Type *type   = type_builtin("u8");
        result.type  = type;
        result.value = LLVMConstInt(get_llvm_type(gen, type), node->char_value, 0);
        break;
    }

    case NODE_LIT_STRING:
    {
        result.value = LLVMBuildGlobalStringPtr(gen->builder, node->str_value, ".str");
        result.type  = type_pointer(type_builtin("u8"));
        break;
    }

    default:
        fprintf(stderr, "error: unsupported literal node kind\n");
        break;
    }

    return result;
}

static CodegenValue codegen_binary_op(CodeGenerator *gen, Node *node)
{
    CodegenValue left   = codegen_expression(gen, node->binary.left);
    CodegenValue right  = codegen_expression(gen, node->binary.right);
    CodegenValue result = {0};

    // handle assignment specially - left side stays as lvalue
    if (node->binary.op == OP_ASSIGN)
    {
        if (!left.is_lvalue)
        {
            fprintf(stderr, "error: assignment requires lvalue\n");
            return result;
        }
        if (right.is_lvalue)
        {
            right.value = LLVMBuildLoad2(gen->builder, get_llvm_type(gen, right.type), right.value, "");
        }
        LLVMBuildStore(gen->builder, right.value, left.value);
        result = right;
        return result;
    }

    // for other operations, load both operands if they reference memory
    if (left.value && LLVMGetTypeKind(LLVMTypeOf(left.value)) == LLVMPointerTypeKind)
    {
        left.value = LLVMBuildLoad2(gen->builder, get_llvm_type(gen, left.type), left.value, "");
    }
    if (right.value && LLVMGetTypeKind(LLVMTypeOf(right.value)) == LLVMPointerTypeKind)
    {
        right.value = LLVMBuildLoad2(gen->builder, get_llvm_type(gen, right.type), right.value, "");
    }

    bool is_float  = left.type->kind == TYPE_FLOAT;
    bool is_signed = left.type->is_signed;

    switch (node->binary.op)
    {
    case OP_ADD:
        if (left.type->kind == TYPE_POINTER && right.type->kind == TYPE_INT)
        {
            result.value = LLVMBuildGEP2(gen->builder, get_llvm_type(gen, left.type->pointer.base), left.value, &right.value, 1, "");
            result.type  = left.type;
        }
        else if (is_float)
        {
            result.value = LLVMBuildFAdd(gen->builder, left.value, right.value, "");
            result.type  = left.type;
        }
        else
        {
            result.value = LLVMBuildAdd(gen->builder, left.value, right.value, "");
            result.type  = left.type;
        }
        break;

    case OP_SUB:
        if (is_float)
        {
            result.value = LLVMBuildFSub(gen->builder, left.value, right.value, "");
        }
        else
        {
            result.value = LLVMBuildSub(gen->builder, left.value, right.value, "");
        }
        result.type = left.type;
        break;

    case OP_MUL:
        if (is_float)
        {
            result.value = LLVMBuildFMul(gen->builder, left.value, right.value, "");
        }
        else
        {
            result.value = LLVMBuildMul(gen->builder, left.value, right.value, "");
        }
        result.type = left.type;
        break;

    case OP_DIV:
        if (is_float)
        {
            result.value = LLVMBuildFDiv(gen->builder, left.value, right.value, "");
        }
        else if (is_signed)
        {
            result.value = LLVMBuildSDiv(gen->builder, left.value, right.value, "");
        }
        else
        {
            result.value = LLVMBuildUDiv(gen->builder, left.value, right.value, "");
        }
        result.type = left.type;
        break;

    case OP_GREATER:
        if (is_float)
        {
            result.value = LLVMBuildFCmp(gen->builder, LLVMRealOGT, left.value, right.value, "");
        }
        else if (is_signed)
        {
            result.value = LLVMBuildICmp(gen->builder, LLVMIntSGT, left.value, right.value, "");
        }
        else
        {
            result.value = LLVMBuildICmp(gen->builder, LLVMIntUGT, left.value, right.value, "");
        }
        result.type = type_builtin("u8");
        break;

    case OP_BITWISE_AND:
        result.value = LLVMBuildAnd(gen->builder, left.value, right.value, "");
        result.type  = left.type;
        break;

    case OP_BITWISE_OR:
        result.value = LLVMBuildOr(gen->builder, left.value, right.value, "");
        result.type  = left.type;
        break;

    default:
        fprintf(stderr, "error: unsupported binary operator\n");
        break;
    }

    return result;
}

static CodegenValue codegen_expression(CodeGenerator *gen, Node *node)
{
    CodegenValue result = {0};

    switch (node->kind)
    {
    case NODE_LIT_INT:
    case NODE_LIT_FLOAT:
    case NODE_LIT_CHAR:
    case NODE_LIT_STRING:
        return codegen_literal(gen, node);

    case NODE_IDENTIFIER:
    {
        Symbol *sym = NULL;

        // first check current function scope
        if (gen->current_scope)
        {
            sym = scope_lookup((Scope *)gen->current_scope, node->str_value);
        }

        // fallback to global scope
        if (!sym)
        {
            sym = scope_lookup(gen->analyzer->global, node->str_value);
        }

        if (!sym)
        {
            fprintf(stderr, "error: undefined identifier '%s'\n", node->str_value);
            return result;
        }

        LLVMValueRef value = lookup_value(gen, sym);
        if (!value)
        {
            fprintf(stderr, "error: no value for symbol '%s'\n", node->str_value);
            return result;
        }

        result.value     = value;
        result.type      = sym->type;
        result.is_lvalue = (sym->kind == SYMBOL_VARIABLE);
        break;
    }

    case NODE_EXPR_BINARY:
        return codegen_binary_op(gen, node);

    case NODE_EXPR_INDEX:
    {
        CodegenValue array = codegen_expression(gen, node->binary.left);
        CodegenValue index = codegen_expression(gen, node->binary.right);

        if (index.is_lvalue)
        {
            index.value = LLVMBuildLoad2(gen->builder, get_llvm_type(gen, index.type), index.value, "");
        }

        if (array.type->kind == TYPE_ARRAY)
        {
            LLVMValueRef indices[] = {LLVMConstInt(LLVMInt64TypeInContext(gen->context), 0, 0), index.value};
            result.value           = LLVMBuildGEP2(gen->builder, get_llvm_type(gen, array.type), array.value, indices, 2, "");
            result.type            = array.type->array.element;
        }
        else if (array.type->kind == TYPE_POINTER)
        {
            if (array.is_lvalue)
            {
                array.value = LLVMBuildLoad2(gen->builder, get_llvm_type(gen, array.type), array.value, "");
            }
            result.value = LLVMBuildGEP2(gen->builder, get_llvm_type(gen, array.type->pointer.base), array.value, &index.value, 1, "");
            result.type  = array.type->pointer.base;
        }
        result.is_lvalue = true;
        break;
    }

    case NODE_EXPR_MEMBER:
    {
        CodegenValue struct_val  = codegen_expression(gen, node->binary.left);
        char        *member_name = node->binary.right->str_value;

        size_t  field_index = 0;
        Symbol *field       = NULL;
        for (size_t i = 0; struct_val.type->composite.fields[i]; i++)
        {
            if (strcmp(struct_val.type->composite.fields[i]->name, member_name) == 0)
            {
                field       = struct_val.type->composite.fields[i];
                field_index = i;
                break;
            }
        }

        if (!field)
        {
            fprintf(stderr, "error: no member '%s'\n", member_name);
            return result;
        }

        if (struct_val.type->kind == TYPE_STRUCT)
        {
            result.value = LLVMBuildStructGEP2(gen->builder, get_llvm_type(gen, struct_val.type), struct_val.value, field_index, "");
        }
        else if (struct_val.type->kind == TYPE_UNION)
        {
            LLVMTypeRef field_ptr_type = LLVMPointerType(get_llvm_type(gen, field->type), 0);
            result.value               = LLVMBuildBitCast(gen->builder, struct_val.value, field_ptr_type, "");
        }

        result.type      = field->type;
        result.is_lvalue = true;
        break;
    }

    case NODE_EXPR_CAST:
    {
        CodegenValue expr        = codegen_expression(gen, node->binary.left);
        Type        *target_type = node->binary.right->type;

        if (!target_type)
        {
            fprintf(stderr, "error: cast node missing type information\n");
            return result;
        }

        if (expr.is_lvalue)
        {
            expr.value = LLVMBuildLoad2(gen->builder, get_llvm_type(gen, expr.type), expr.value, "");
        }

        result.value = LLVMBuildBitCast(gen->builder, expr.value, get_llvm_type(gen, target_type), "");
        result.type  = target_type;
        break;
    }

    default:
        fprintf(stderr, "error: unsupported expression node kind: %d\n", node->kind);
        break;
    }

    return result;
}

static bool codegen_var_decl(CodeGenerator *gen, Node *node)
{
    char *name = node->decl.name->str_value;

    Symbol *sym = NULL;

    // first check current function scope
    if (gen->current_scope)
    {
        sym = scope_lookup((Scope *)gen->current_scope, name);
    }

    // fallback to global scope
    if (!sym)
    {
        sym = scope_lookup(gen->analyzer->global, name);
    }

    if (!sym)
        return false;

    Type *type = sym->type;

    LLVMValueRef alloca = LLVMBuildAlloca(gen->builder, get_llvm_type(gen, type), name);
    register_value(gen, sym, alloca);

    if (node->decl.init)
    {
        CodegenValue init = codegen_expression(gen, node->decl.init);
        if (init.is_lvalue)
        {
            init.value = LLVMBuildLoad2(gen->builder, get_llvm_type(gen, init.type), init.value, "");
        }
        LLVMBuildStore(gen->builder, init.value, alloca);
    }

    return true;
}

static bool codegen_function(CodeGenerator *gen, Node *node)
{
    char   *name = node->function.name->str_value;
    Symbol *sym  = scope_lookup(gen->analyzer->global, name);
    if (!sym)
        return false;

    Type       *func_type      = sym->type;
    LLVMTypeRef llvm_func_type = get_llvm_type(gen, func_type);

    LLVMValueRef func = LLVMGetNamedFunction(gen->module, name);
    if (!func)
    {
        func = LLVMAddFunction(gen->module, name, llvm_func_type);
    }

    register_value(gen, sym, func);

    if (sym->is_external || !node->function.body)
    {
        return true;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlock(func, "entry");
    LLVMPositionBuilderAtEnd(gen->builder, entry);

    LLVMValueRef old_func  = gen->current_function;
    void        *old_scope = gen->current_scope;
    gen->current_function  = func;
    gen->current_scope     = node->function.scope;

    if (node->function.params && node->function.scope)
    {
        Scope *func_scope = (Scope *)node->function.scope;
        for (size_t i = 0; node->function.params[i]; i++)
        {
            Node   *param      = node->function.params[i];
            char   *param_name = param->decl.name->str_value;
            Symbol *param_sym  = scope_lookup(func_scope, param_name);

            if (param_sym)
            {
                LLVMValueRef param_value  = LLVMGetParam(func, i);
                LLVMValueRef param_alloca = LLVMBuildAlloca(gen->builder, get_llvm_type(gen, param_sym->type), param_name);
                LLVMBuildStore(gen->builder, param_value, param_alloca);
                register_value(gen, param_sym, param_alloca);
            }
        }
    }

    codegen_node(gen, node->function.body);

    if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(gen->builder)))
    {
        if (func_type->function.return_type->kind == TYPE_VOID)
        {
            LLVMBuildRetVoid(gen->builder);
        }
        else
        {
            LLVMBuildRet(gen->builder, LLVMConstNull(get_llvm_type(gen, func_type->function.return_type)));
        }
    }

    gen->current_function = old_func;
    gen->current_scope    = old_scope;
    return true;
}

static bool codegen_if_or(CodeGenerator *gen, Node *node)
{
    LLVMBasicBlockRef then_block = LLVMAppendBasicBlock(gen->current_function, "then");
    LLVMBasicBlockRef else_block = LLVMAppendBasicBlock(gen->current_function, "else");
    LLVMBasicBlockRef end_block  = LLVMAppendBasicBlock(gen->current_function, "end");

    if (node->conditional.condition)
    {
        CodegenValue cond = codegen_expression(gen, node->conditional.condition);
        if (cond.is_lvalue)
        {
            cond.value = LLVMBuildLoad2(gen->builder, get_llvm_type(gen, cond.type), cond.value, "");
        }

        if (cond.type->kind != TYPE_INT || cond.type->size != 1)
        {
            cond.value = LLVMBuildICmp(gen->builder, LLVMIntNE, cond.value, LLVMConstNull(get_llvm_type(gen, cond.type)), "");
        }

        LLVMBuildCondBr(gen->builder, cond.value, then_block, else_block);
    }
    else
    {
        LLVMBuildBr(gen->builder, then_block);
    }

    LLVMPositionBuilderAtEnd(gen->builder, then_block);
    codegen_node(gen, node->conditional.body);
    if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(gen->builder)))
    {
        LLVMBuildBr(gen->builder, end_block);
    }

    LLVMPositionBuilderAtEnd(gen->builder, else_block);
    if (node->conditional.else_body)
    {
        codegen_node(gen, node->conditional.else_body);
    }
    if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(gen->builder)))
    {
        LLVMBuildBr(gen->builder, end_block);
    }

    LLVMPositionBuilderAtEnd(gen->builder, end_block);
    return true;
}

static bool codegen_statement(CodeGenerator *gen, Node *node)
{
    switch (node->kind)
    {
    case NODE_STMT_VAL:
    case NODE_STMT_VAR:
        return codegen_var_decl(gen, node);

    case NODE_STMT_FUNCTION:
        return codegen_function(gen, node);

    case NODE_STMT_IF:
        return codegen_if_or(gen, node);

    case NODE_STMT_RETURN:
        if (node->single)
        {
            CodegenValue ret_val = codegen_expression(gen, node->single);
            if (ret_val.is_lvalue)
            {
                ret_val.value = LLVMBuildLoad2(gen->builder, get_llvm_type(gen, ret_val.type), ret_val.value, "");
            }
            LLVMBuildRet(gen->builder, ret_val.value);
        }
        else
        {
            LLVMBuildRetVoid(gen->builder);
        }
        return true;

    case NODE_STMT_EXPRESSION:
        codegen_expression(gen, node->single);
        return true;

    default:
        return true;
    }
}

static bool codegen_node(CodeGenerator *gen, Node *node)
{
    if (!node)
        return true;

    switch (node->kind)
    {
    case NODE_PROGRAM:
    case NODE_BLOCK:
        if (node->children)
        {
            for (size_t i = 0; node->children[i]; i++)
            {
                if (!codegen_node(gen, node->children[i]))
                {
                    return false;
                }
            }
        }
        return true;

    default:
        return codegen_statement(gen, node);
    }
}

bool codegen_generate(CodeGenerator *gen)
{
    if (!gen || !gen->analyzer || !gen->analyzer->ast)
        return false;

    return codegen_node(gen, gen->analyzer->ast);
}

bool codegen_emit_ir(CodeGenerator *gen, const char *filename)
{
    char *error = NULL;
    if (LLVMPrintModuleToFile(gen->module, filename, &error))
    {
        fprintf(stderr, "error: failed to write IR: %s\n", error);
        LLVMDisposeMessage(error);
        return false;
    }
    return true;
}

bool codegen_emit_object(CodeGenerator *gen, const char *filename)
{
    char *error  = NULL;
    char *triple = LLVMGetDefaultTargetTriple();

    LLVMTargetRef target;
    if (LLVMGetTargetFromTriple(triple, &target, &error))
    {
        fprintf(stderr, "error: failed to get target: %s\n", error);
        LLVMDisposeMessage(error);
        return false;
    }

    LLVMTargetMachineRef machine = LLVMCreateTargetMachine(target, triple, "generic", "", LLVMCodeGenLevelDefault, LLVMRelocDefault, LLVMCodeModelDefault);

    if (LLVMTargetMachineEmitToFile(machine, gen->module, (char *)filename, LLVMObjectFile, &error))
    {
        fprintf(stderr, "error: failed to emit object: %s\n", error);
        LLVMDisposeMessage(error);
        return false;
    }

    LLVMDisposeTargetMachine(machine);
    return true;
}
