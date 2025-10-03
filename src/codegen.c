#include "codegen.h"
#include "token.h"
#include "type.h"
#include "symbol.h"
#include <llvm-c/Analysis.h>
#include <llvm-c/Transforms/PassBuilder.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *sanitize_module_path(const char *path)
{
    if (!path)
        return NULL;
    size_t len = strlen(path);
    char  *buf = malloc(len + 1);
    if (!buf)
        return NULL;
    for (size_t i = 0; i < len; i++)
    {
        char c = path[i];
        buf[i] = (c == '.' || c == '/' || c == ':') ? '_' : c;
    }
    buf[len] = '\0';
    return buf;
}

// simple constant folding for integers/booleans used in global initializers
static bool codegen_eval_const_i64(CodegenContext *ctx, AstNode *expr, int64_t *out)
{
    (void)ctx;
    if (!expr || !out) return false;
    switch (expr->kind)
    {
    case AST_EXPR_LIT:
        if (expr->lit_expr.kind == TOKEN_LIT_INT) { *out = (int64_t)expr->lit_expr.int_val; return true; }
        if (expr->lit_expr.kind == TOKEN_LIT_CHAR) { *out = (int64_t)expr->lit_expr.char_val; return true; }
        return false;
    case AST_EXPR_IDENT:
        if (expr->symbol && (expr->symbol->kind == SYMBOL_VAL || expr->symbol->kind == SYMBOL_VAR) && expr->symbol->decl)
        {
            AstNode *decl = expr->symbol->decl;
            AstNode *init = NULL;
            if (decl->kind == AST_STMT_VAL || decl->kind == AST_STMT_VAR)
                init = decl->var_stmt.init;
            if (init)
                return codegen_eval_const_i64(ctx, init, out);
        }
        return false;
    case AST_EXPR_BINARY:
    {
        int64_t l = 0, r = 0;
        switch (expr->binary_expr.op)
        {
        case TOKEN_EQUAL_EQUAL:
            if (!codegen_eval_const_i64(ctx, expr->binary_expr.left, &l) || !codegen_eval_const_i64(ctx, expr->binary_expr.right, &r)) return false;
            *out = (l == r) ? 1 : 0; return true;
        case TOKEN_PIPE_PIPE:
            if (!codegen_eval_const_i64(ctx, expr->binary_expr.left, &l) || !codegen_eval_const_i64(ctx, expr->binary_expr.right, &r)) return false;
            *out = ((l != 0) || (r != 0)) ? 1 : 0; return true;
        case TOKEN_AMPERSAND_AMPERSAND:
            if (!codegen_eval_const_i64(ctx, expr->binary_expr.left, &l) || !codegen_eval_const_i64(ctx, expr->binary_expr.right, &r)) return false;
            *out = ((l != 0) && (r != 0)) ? 1 : 0; return true;
        default:
            return false;
        }
    }
    default:
        return false;
    }
}

void codegen_context_init(CodegenContext *ctx, const char *module_name, bool no_pie)
{
    ctx->context = LLVMContextCreate();
    ctx->module  = LLVMModuleCreateWithNameInContext(module_name, ctx->context);
    ctx->builder = LLVMCreateBuilderInContext(ctx->context);
    ctx->no_pie  = no_pie;

    // initialize target
    LLVMInitializeAllTargetInfos();
    LLVMInitializeAllTargets();
    LLVMInitializeAllTargetMCs();
    LLVMInitializeAllAsmParsers();
    LLVMInitializeAllAsmPrinters();

    char         *error = NULL;
    LLVMTargetRef target;
    if (LLVMGetTargetFromTriple(LLVMGetDefaultTargetTriple(), &target, &error) != 0)
    {
        fprintf(stderr, "error: failed to get target: %s\n", error);
        LLVMDisposeMessage(error);
        exit(EXIT_FAILURE);
    }

    ctx->target_machine = LLVMCreateTargetMachine(target, LLVMGetDefaultTargetTriple(), LLVMGetHostCPUName(), LLVMGetHostCPUFeatures(), LLVMCodeGenLevelDefault, ctx->no_pie ? LLVMRelocStatic : LLVMRelocPIC, LLVMCodeModelDefault);

    ctx->data_layout = LLVMCreateTargetDataLayout(ctx->target_machine);
    LLVMSetModuleDataLayout(ctx->module, ctx->data_layout);
    LLVMSetTarget(ctx->module, LLVMGetDefaultTargetTriple());

    // initialize maps
    ctx->symbol_map.symbols  = NULL;
    ctx->symbol_map.values   = NULL;
    ctx->symbol_map.count    = 0;
    ctx->symbol_map.capacity = 0;

    ctx->type_cache.types      = NULL;
    ctx->type_cache.llvm_types = NULL;
    ctx->type_cache.count      = 0;
    ctx->type_cache.capacity   = 0;

    ctx->current_function        = NULL;
    ctx->current_function_type   = NULL;
    ctx->break_block             = NULL;
    ctx->continue_block          = NULL;
    ctx->generating_mutable_init = false;

    ctx->errors     = NULL;
    ctx->has_errors = false;

    ctx->opt_level    = 2;
    ctx->debug_info   = false;
    ctx->is_runtime   = false;
    ctx->use_runtime  = false;
    ctx->package_name = NULL;

    ctx->current_varargs           = NULL;
    ctx->current_vararg_count      = 0;
    ctx->current_vararg_capacity   = 0;
    ctx->current_fixed_param_count = 0;
    ctx->source_file               = NULL;
    ctx->source_lexer              = NULL;
}

void codegen_context_dnit(CodegenContext *ctx)
{
    // clean up errors
    CodegenError *error = ctx->errors;
    while (error)
    {
        CodegenError *next = error->next;
        free(error->message);
        free(error);
        error = next;
    }

    // clean up maps
    free(ctx->symbol_map.symbols);
    free(ctx->symbol_map.values);
    free(ctx->type_cache.types);
    free(ctx->type_cache.llvm_types);

    // clean up llvm
    LLVMDisposeBuilder(ctx->builder);
    LLVMDisposeModule(ctx->module);
    LLVMDisposeTargetMachine(ctx->target_machine);
    LLVMContextDispose(ctx->context);
    free(ctx->package_name);

    free(ctx->current_varargs);
}

bool codegen_generate(CodegenContext *ctx, AstNode *root, SemanticAnalyzer *analyzer)
{
    if (root->kind != AST_PROGRAM)
    {
        codegen_error(ctx, root, "expected program node");
        return false;
    }

    // generate function declarations for all imported symbols
    if (analyzer && analyzer->symbol_table.global_scope)
    {
        Symbol *sym = analyzer->symbol_table.global_scope->symbols;
        while (sym)
        {
            if (sym->kind == SYMBOL_FUNC && !codegen_get_symbol_value(ctx, sym))
            {
                // create function declaration for imported function
                Type *func_type = sym->type;
                if (func_type && func_type->kind == TYPE_FUNCTION)
                {
                    LLVMTypeRef return_type = func_type->function.return_type ? codegen_get_llvm_type(ctx, func_type->function.return_type) : LLVMVoidTypeInContext(ctx->context);

                    LLVMTypeRef *param_types = NULL;
                    if (func_type->function.param_count > 0)
                    {
                        param_types = malloc(sizeof(LLVMTypeRef) * func_type->function.param_count);
                        for (size_t i = 0; i < func_type->function.param_count; i++)
                        {
                            param_types[i] = codegen_get_llvm_type(ctx, func_type->function.param_types[i]);
                        }
                    }

                    LLVMTypeRef llvm_func_type = LLVMFunctionType(return_type, param_types, func_type->function.param_count, func_type->function.is_variadic);

                    // determine function name for LLVM
                    const char *llvm_name = sym->name;
                    if (sym->func.is_external && sym->decl && sym->decl->kind == AST_STMT_EXT)
                    {
                        // use target symbol name for external functions
                        llvm_name = sym->decl->ext_stmt.symbol ? sym->decl->ext_stmt.symbol : sym->name;
                    }

                    LLVMValueRef func = LLVMAddFunction(ctx->module, llvm_name, llvm_func_type);
                    free(param_types);

                    // add to symbol map
                    codegen_set_symbol_value(ctx, sym, func);
                }
            }
            sym = sym->next;
        }
    }

    // generate top-level statements
    for (int i = 0; i < root->program.stmts->count; i++)
    {
        AstNode *stmt = root->program.stmts->items[i];
        codegen_stmt(ctx, stmt);
    }

    // verify module (do not abort process; print diagnostics)
    char *error = NULL;
    if (LLVMVerifyModule(ctx->module, LLVMPrintMessageAction, &error) != 0)
    {
        fprintf(stderr, "error: module verification failed: %s\n", error);
        LLVMDisposeMessage(error);
        ctx->has_errors = true;
    }

    // run optimization passes
    if (ctx->opt_level > 0 && !ctx->has_errors)
    {
        LLVMPassBuilderOptionsRef options = LLVMCreatePassBuilderOptions();

        char opt_str[16];
        snprintf(opt_str, sizeof(opt_str), "default<O%d>", ctx->opt_level);

        LLVMRunPasses(ctx->module, opt_str, ctx->target_machine, options);
        LLVMDisposePassBuilderOptions(options);
    }

    return !ctx->has_errors;
}

bool codegen_emit_object(CodegenContext *ctx, const char *filename)
{
    char *error = NULL;
    if (LLVMTargetMachineEmitToFile(ctx->target_machine, ctx->module, (char *)filename, LLVMObjectFile, &error) != 0)
    {
        fprintf(stderr, "error: failed to emit object file: %s\n", error);
        LLVMDisposeMessage(error);
        return false;
    }
    return true;
}

bool codegen_emit_llvm_ir(CodegenContext *ctx, const char *filename)
{
    char *error = NULL;
    if (LLVMPrintModuleToFile(ctx->module, filename, &error) != 0)
    {
        fprintf(stderr, "error: failed to emit llvm ir: %s\n", error);
        LLVMDisposeMessage(error);
        return false;
    }
    return true;
}

bool codegen_emit_assembly(CodegenContext *ctx, const char *filename)
{
    char *error = NULL;
    if (LLVMTargetMachineEmitToFile(ctx->target_machine, ctx->module, (char *)filename, LLVMAssemblyFile, &error) != 0)
    {
        fprintf(stderr, "error: failed to emit assembly: %s\n", error);
        LLVMDisposeMessage(error);
        return false;
    }
    return true;
}

void codegen_error(CodegenContext *ctx, AstNode *node, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    size_t len = vsnprintf(NULL, 0, fmt, args) + 1;
    va_end(args);

    char *message = malloc(len);
    va_start(args, fmt);
    vsnprintf(message, len, fmt, args);
    va_end(args);

    CodegenError *error = malloc(sizeof(CodegenError));
    error->message      = message;
    error->node         = node;
    error->next         = ctx->errors;
    ctx->errors         = error;
    ctx->has_errors     = true;
}

void codegen_print_errors(CodegenContext *ctx)
{
    for (CodegenError *error = ctx->errors; error; error = error->next)
    {
        if (ctx->source_lexer && error->node && error->node->token)
        {
            int   line = lexer_get_pos_line(ctx->source_lexer, error->node->token->pos);
            int   col  = lexer_get_pos_line_offset(ctx->source_lexer, error->node->token->pos) + 1;
            char *text = lexer_get_line_text(ctx->source_lexer, line);
            fprintf(stderr, "%s:%d:%d: codegen error: %s\n", ctx->source_file ? ctx->source_file : "<unknown>", line + 1, col, error->message);
            if (text)
            {
                fprintf(stderr, "%s\n", text);
                // simple caret underline
                for (int i = 0; i < col - 1; i++) fputc(' ', stderr);
                fputc('^', stderr); fputc('\n', stderr);
                free(text);
            }
        }
        else
        {
            fprintf(stderr, "codegen error: %s\n", error->message);
        }
    }
}

LLVMTypeRef codegen_get_llvm_type(CodegenContext *ctx, Type *type)
{
    if (!type)
        return NULL;

    // check cache
    for (int i = 0; i < ctx->type_cache.count; i++)
    {
        if (ctx->type_cache.types[i] == type)
        {
            return ctx->type_cache.llvm_types[i];
        }
    }

    LLVMTypeRef llvm_type = NULL;

    switch (type->kind)
    {
    case TYPE_U8:
        llvm_type = LLVMInt8TypeInContext(ctx->context);
        break;
    case TYPE_U16:
        llvm_type = LLVMInt16TypeInContext(ctx->context);
        break;
    case TYPE_U32:
        llvm_type = LLVMInt32TypeInContext(ctx->context);
        break;
    case TYPE_U64:
        llvm_type = LLVMInt64TypeInContext(ctx->context);
        break;
    case TYPE_I8:
        llvm_type = LLVMInt8TypeInContext(ctx->context);
        break;
    case TYPE_I16:
        llvm_type = LLVMInt16TypeInContext(ctx->context);
        break;
    case TYPE_I32:
        llvm_type = LLVMInt32TypeInContext(ctx->context);
        break;
    case TYPE_I64:
        llvm_type = LLVMInt64TypeInContext(ctx->context);
        break;
    case TYPE_F16:
        llvm_type = LLVMHalfTypeInContext(ctx->context);
        break;
    case TYPE_F32:
        llvm_type = LLVMFloatTypeInContext(ctx->context);
        break;
    case TYPE_F64:
        llvm_type = LLVMDoubleTypeInContext(ctx->context);
        break;
    case TYPE_PTR:
        llvm_type = LLVMPointerTypeInContext(ctx->context, 0);
        break;
    case TYPE_POINTER:
        llvm_type = LLVMPointerTypeInContext(ctx->context, 0);
        break;
    case TYPE_ARRAY:
    {
        // all arrays are fat pointers: {ptr data, u64 length}
        LLVMTypeRef fat_ptr_fields[2] = {
            LLVMPointerTypeInContext(ctx->context, 0), // data pointer
            LLVMInt64TypeInContext(ctx->context)       // length (u64)
        };
        llvm_type = LLVMStructTypeInContext(ctx->context, fat_ptr_fields, 2, false);
    }
    break;
    case TYPE_STRUCT:
    {
        // create opaque struct type first
        llvm_type = LLVMStructCreateNamed(ctx->context, type->name);

        // count fields
        int field_count = 0;
        for (Symbol *field = type->composite.fields; field; field = field->next)
        {
            field_count++;
        }

        // collect field types
        LLVMTypeRef *field_types = malloc(sizeof(LLVMTypeRef) * field_count);
        int          i           = 0;
        for (Symbol *field = type->composite.fields; field; field = field->next)
        {
            field_types[i++] = codegen_get_llvm_type(ctx, field->type);
        }

        LLVMStructSetBody(llvm_type, field_types, field_count, false);
        free(field_types);
    }
    break;
    case TYPE_UNION:
    {
        // unions are represented as a struct with a single field of the largest member type
        size_t      max_size     = 0;
        LLVMTypeRef largest_type = NULL;

        for (Symbol *field = type->composite.fields; field; field = field->next)
        {
            if (field->type->size > max_size)
            {
                max_size     = field->type->size;
                largest_type = codegen_get_llvm_type(ctx, field->type);
            }
        }

        if (largest_type)
        {
            llvm_type = LLVMStructTypeInContext(ctx->context, &largest_type, 1, false);
        }
        else
        {
            llvm_type = LLVMStructTypeInContext(ctx->context, NULL, 0, false);
        }
    }
    break;
    case TYPE_FUNCTION:
    {
        // functions are always represented as pointers in LLVM
        llvm_type = LLVMPointerTypeInContext(ctx->context, 0);
    }
    break;
    case TYPE_ALIAS:
        llvm_type = codegen_get_llvm_type(ctx, type->alias.target);
        break;
    }

    // cache the result
    if (llvm_type)
    {
        if (ctx->type_cache.count >= ctx->type_cache.capacity)
        {
            ctx->type_cache.capacity   = ctx->type_cache.capacity ? ctx->type_cache.capacity * 2 : 16;
            ctx->type_cache.types      = realloc(ctx->type_cache.types, sizeof(Type *) * ctx->type_cache.capacity);
            ctx->type_cache.llvm_types = realloc(ctx->type_cache.llvm_types, sizeof(LLVMTypeRef) * ctx->type_cache.capacity);
        }

        ctx->type_cache.types[ctx->type_cache.count]      = type;
        ctx->type_cache.llvm_types[ctx->type_cache.count] = llvm_type;
        ctx->type_cache.count++;
    }

    return llvm_type;
}

LLVMValueRef codegen_get_symbol_value(CodegenContext *ctx, Symbol *symbol)
{
    for (int i = 0; i < ctx->symbol_map.count; i++)
    {
        if (ctx->symbol_map.symbols[i] == symbol)
        {
            return ctx->symbol_map.values[i];
        }
    }
    return NULL;
}

void codegen_set_symbol_value(CodegenContext *ctx, Symbol *symbol, LLVMValueRef value)
{
    // check if already exists
    for (int i = 0; i < ctx->symbol_map.count; i++)
    {
        if (ctx->symbol_map.symbols[i] == symbol)
        {
            ctx->symbol_map.values[i] = value;
            return;
        }
    }

    // add new mapping
    if (ctx->symbol_map.count >= ctx->symbol_map.capacity)
    {
        ctx->symbol_map.capacity = ctx->symbol_map.capacity ? ctx->symbol_map.capacity * 2 : 16;
        ctx->symbol_map.symbols  = realloc(ctx->symbol_map.symbols, sizeof(Symbol *) * ctx->symbol_map.capacity);
        ctx->symbol_map.values   = realloc(ctx->symbol_map.values, sizeof(LLVMValueRef) * ctx->symbol_map.capacity);
    }

    ctx->symbol_map.symbols[ctx->symbol_map.count] = symbol;
    ctx->symbol_map.values[ctx->symbol_map.count]  = value;
    ctx->symbol_map.count++;
}

LLVMValueRef codegen_stmt(CodegenContext *ctx, AstNode *stmt)
{
    if (!stmt)
        return NULL;

    switch (stmt->kind)
    {
    case AST_STMT_VAR:
    case AST_STMT_VAL:
        return codegen_stmt_var(ctx, stmt);
    case AST_STMT_FUN:
        return codegen_stmt_fun(ctx, stmt);
    case AST_STMT_RET:
        return codegen_stmt_ret(ctx, stmt);
    case AST_STMT_IF:
        return codegen_stmt_if(ctx, stmt);
    case AST_STMT_OR:
        return codegen_stmt_if(ctx, stmt); // or statements have same structure as if
    case AST_STMT_FOR:
        return codegen_stmt_for(ctx, stmt);
    case AST_STMT_BLOCK:
        return codegen_stmt_block(ctx, stmt);
    case AST_STMT_EXPR:
        return codegen_stmt_expr(ctx, stmt);
    case AST_STMT_BRK:
        if (ctx->break_block)
        {
            LLVMBuildBr(ctx->builder, ctx->break_block);
        }
        else
        {
            codegen_error(ctx, stmt, "break outside loop");
        }
        return NULL;
    case AST_STMT_CNT:
        if (ctx->continue_block)
        {
            LLVMBuildBr(ctx->builder, ctx->continue_block);
        }
        else
        {
            codegen_error(ctx, stmt, "continue outside loop");
        }
        return NULL;
    case AST_STMT_EXT:
        return codegen_stmt_ext(ctx, stmt);
    case AST_STMT_DEF:
        // type definitions don't generate code
        return NULL;
    case AST_STMT_STR:
    case AST_STMT_UNI:
        // struct/union definitions don't generate code directly
        return NULL;
    case AST_STMT_USE:
        return codegen_stmt_use(ctx, stmt);
    case AST_STMT_ASM:
        if (stmt->asm_stmt.code)
        {
            // default clobbers to be safe for syscalls and general side-effect asm
            const char *default_clobbers = "~{memory},~{dirflag},~{fpsr},~{flags}";
            const char *constr           = stmt->asm_stmt.constraints && stmt->asm_stmt.constraints[0] ? stmt->asm_stmt.constraints : default_clobbers;

            if (ctx->current_function)
            {
                // inline asm in function: side-effect call with no operands
                LLVMTypeRef  void_ty    = LLVMVoidTypeInContext(ctx->context);
                LLVMTypeRef  asm_fn_ty  = LLVMFunctionType(void_ty, NULL, 0, false);
                LLVMValueRef inline_asm = LLVMGetInlineAsm(asm_fn_ty, stmt->asm_stmt.code, strlen(stmt->asm_stmt.code), constr, strlen(constr), true, false, LLVMInlineAsmDialectATT, false);
                LLVMBuildCall2(ctx->builder, asm_fn_ty, inline_asm, NULL, 0, "");
            }
            else
            {
                // module-level inline asm (e.g., directives)
                LLVMSetModuleInlineAsm2(ctx->module, stmt->asm_stmt.code, strlen(stmt->asm_stmt.code));
            }
        }
        return NULL;
    default:
        codegen_error(ctx, stmt, "unimplemented statement kind: %s", ast_node_kind_to_string(stmt->kind));
        return NULL;
    }
}

LLVMValueRef codegen_stmt_use(CodegenContext *ctx, AstNode *stmt)
{
    // for unaliased imports, symbols are already imported directly into current scope
    // during semantic analysis, so no codegen work needed
    Symbol *module_symbol = stmt->use_stmt.module_sym;
    if (!module_symbol)
    {
        // unaliased import - nothing to do in codegen
        return NULL;
    }

    // aliased import - process all functions from the imported module
    if (module_symbol->kind != SYMBOL_MODULE)
    {
        codegen_error(ctx, stmt, "invalid module symbol in use statement");
        return NULL;
    }

    // iterate through all symbols in the module scope
    if (!module_symbol->module.scope)
    {
        return NULL;
    }
    for (Symbol *symbol = module_symbol->module.scope->symbols; symbol; symbol = symbol->next)
    {
        if (symbol->kind == SYMBOL_FUNC)
        {
            // create function declaration for imported function
            Type       *func_type   = symbol->type;
            LLVMTypeRef return_type = func_type->function.return_type ? codegen_get_llvm_type(ctx, func_type->function.return_type) : LLVMVoidTypeInContext(ctx->context);

            LLVMTypeRef *param_types = NULL;
            if (func_type->function.param_count > 0)
            {
                param_types = malloc(sizeof(LLVMTypeRef) * func_type->function.param_count);
                for (size_t i = 0; i < func_type->function.param_count; i++)
                {
                    param_types[i] = codegen_get_llvm_type(ctx, func_type->function.param_types[i]);
                }
            }

            LLVMTypeRef llvm_func_type = LLVMFunctionType(return_type, param_types, func_type->function.param_count, func_type->function.is_variadic);

            // choose proper llvm symbol name
            char       *owned_name = NULL;
            const char *llvm_name  = NULL;
            if (symbol->decl && symbol->decl->kind == AST_STMT_EXT)
            {
                llvm_name = symbol->decl->ext_stmt.symbol ? symbol->decl->ext_stmt.symbol : symbol->name;
            }
            else if (!symbol->func.is_defined)
            {
                // treat as external-style import with plain name
                llvm_name = symbol->name;
            }
            else
            {
                // mangle with module path so it matches dependency object symbol
                const char *mod_path = module_symbol->module.path;
                char       *san      = sanitize_module_path(mod_path);
                if (san)
                {
                    size_t nlen = strlen(san) + 2 + strlen(symbol->name) + 1;
                    owned_name  = malloc(nlen);
                    if (owned_name)
                        snprintf(owned_name, nlen, "%s__%s", san, symbol->name);
                }
                free(san);
                llvm_name = owned_name ? owned_name : symbol->name;
            }

            LLVMValueRef func = LLVMGetNamedFunction(ctx->module, llvm_name);
            if (!func)
                func = LLVMAddFunction(ctx->module, llvm_name, llvm_func_type);
            free(param_types);
            // add to symbol map so it can be found later
            codegen_set_symbol_value(ctx, symbol, func);
            free(owned_name);
        }
        else if (symbol->kind == SYMBOL_VAL || symbol->kind == SYMBOL_VAR)
        {
            // declare global in current module for imported constant/variable
            LLVMTypeRef  gty  = codegen_get_llvm_type(ctx, symbol->type);
            LLVMValueRef gval = LLVMGetNamedGlobal(ctx->module, symbol->name);
            if (!gval)
            {
                gval = LLVMAddGlobal(ctx->module, gty, symbol->name);
            }
            if (symbol->kind == SYMBOL_VAL)
                LLVMSetGlobalConstant(gval, true);
            codegen_set_symbol_value(ctx, symbol, gval);
        }
    }

    return NULL;
}

LLVMValueRef codegen_stmt_ext(CodegenContext *ctx, AstNode *stmt)
{
    // generate LLVM function declaration for external function
    Type *func_type = stmt->symbol ? stmt->symbol->type : NULL;
    if (!func_type || func_type->kind != TYPE_FUNCTION)
    {
        codegen_error(ctx, stmt, "external declaration must have function type");
        return NULL;
    }

    // get the target symbol name (may be different from function name)
    const char *symbol_name = stmt->ext_stmt.symbol ? stmt->ext_stmt.symbol : stmt->ext_stmt.name;

    // check if function already exists
    LLVMValueRef func = LLVMGetNamedFunction(ctx->module, symbol_name);
    if (func)
    {
        // already declared, just update symbol mapping
        codegen_set_symbol_value(ctx, stmt->symbol, func);
        return func;
    }

    // create LLVM function type
    LLVMTypeRef return_type = func_type->function.return_type ? codegen_get_llvm_type(ctx, func_type->function.return_type) : LLVMVoidTypeInContext(ctx->context);

    LLVMTypeRef *param_types = NULL;
    if (func_type->function.param_count > 0)
    {
        param_types = malloc(sizeof(LLVMTypeRef) * func_type->function.param_count);
        for (size_t i = 0; i < func_type->function.param_count; i++)
        {
            param_types[i] = codegen_get_llvm_type(ctx, func_type->function.param_types[i]);
        }
    }

    LLVMTypeRef llvm_func_type = LLVMFunctionType(return_type, param_types, func_type->function.param_count, func_type->function.is_variadic);

    // create function declaration with target symbol name
    func = LLVMAddFunction(ctx->module, symbol_name, llvm_func_type);

    free(param_types);

    // add to symbol map
    codegen_set_symbol_value(ctx, stmt->symbol, func);

    return func;
}

LLVMValueRef codegen_stmt_block(CodegenContext *ctx, AstNode *stmt)
{
    LLVMValueRef last = NULL;

    if (stmt->block_stmt.stmts)
    {
        for (int i = 0; i < stmt->block_stmt.stmts->count; i++)
        {
            last = codegen_stmt(ctx, stmt->block_stmt.stmts->items[i]);
        }
    }

    return last;
}

LLVMValueRef codegen_stmt_var(CodegenContext *ctx, AstNode *stmt)
{
    if (!stmt->symbol || !stmt->type)
    {
        codegen_error(ctx, stmt, "variable declaration missing symbol or type info");
        return NULL;
    }

    LLVMTypeRef llvm_type = codegen_get_llvm_type(ctx, stmt->type);
    if (!llvm_type)
    {
        codegen_error(ctx, stmt, "failed to get llvm type for variable");
        return NULL;
    }

    if (!ctx->current_function)
    {
        // top-level: create a true LLVM global
        const char *gname  = stmt->var_stmt.name;
        LLVMValueRef global = LLVMGetNamedGlobal(ctx->module, gname);
        if (!global)
        {
            global = LLVMAddGlobal(ctx->module, llvm_type, gname);
        }
        if (stmt->var_stmt.is_val)
        {
            LLVMSetGlobalConstant(global, true);
        }
        // initializer
        if (stmt->var_stmt.init)
        {
            // handle constants specially without emitting instructions
            AstNode *init = stmt->var_stmt.init;
            LLVMValueRef const_init = NULL;
            if (init->kind == AST_EXPR_LIT)
            {
                switch (init->lit_expr.kind)
                {
                case TOKEN_LIT_INT:
                {
                    const_init = LLVMConstInt(llvm_type, init->lit_expr.int_val, type_is_signed(stmt->type));
                    break;
                }
                case TOKEN_LIT_FLOAT:
                {
                    const_init = LLVMConstReal(llvm_type, init->lit_expr.float_val);
                    break;
                }
                case TOKEN_LIT_CHAR:
                {
                    const_init = LLVMConstInt(llvm_type, init->lit_expr.char_val, false);
                    break;
                }
                case TOKEN_LIT_STRING:
                {
                    // build fat pointer constant: { i8* data, i64 len }
                    if (type_resolve_alias(stmt->type)->kind == TYPE_ARRAY)
                    {
                        size_t len = strlen(init->lit_expr.string_val);
                        LLVMTypeRef arr_ty = LLVMArrayType(LLVMInt8TypeInContext(ctx->context), (unsigned)len);
                        char gbuf[128]; snprintf(gbuf, sizeof(gbuf), "%s.str", gname);
                        LLVMValueRef data_g = LLVMGetNamedGlobal(ctx->module, gbuf);
                        if (!data_g)
                        {
                            data_g = LLVMAddGlobal(ctx->module, arr_ty, gbuf);
                            LLVMSetGlobalConstant(data_g, true);
                            LLVMSetLinkage(data_g, LLVMPrivateLinkage);
                            LLVMValueRef str_c = LLVMConstStringInContext(ctx->context, init->lit_expr.string_val, (unsigned)len, false);
                            LLVMSetInitializer(data_g, str_c);
                        }
                        LLVMValueRef idxs[2] = {LLVMConstInt(LLVMInt32TypeInContext(ctx->context), 0, false), LLVMConstInt(LLVMInt32TypeInContext(ctx->context), 0, false)};
                        LLVMValueRef data_ptr = LLVMConstGEP2(arr_ty, data_g, idxs, 2);
                        data_ptr              = LLVMConstPointerCast(data_ptr, LLVMPointerTypeInContext(ctx->context, 0));
                        LLVMValueRef fields[2] = {data_ptr, LLVMConstInt(LLVMInt64TypeInContext(ctx->context), len, false)};
                        const_init              = LLVMConstStructInContext(ctx->context, fields, 2, false);
                    }
                    break;
                }
                default:
                    break;
                }
            }
            else
            {
                // try simple integer/boolean constant folding
                int64_t folded = 0;
                if (codegen_eval_const_i64(ctx, init, &folded))
                {
                    const_init = LLVMConstInt(llvm_type, (unsigned long long)folded, type_is_signed(stmt->type));
                }
            }
            if (!const_init)
            {
                codegen_error(ctx, stmt, "global variable initializer must be constant");
                return NULL;
            }
            LLVMSetInitializer(global, const_init);
        }
        codegen_set_symbol_value(ctx, stmt->symbol, global);
        return global;
    }
    else
    {
        // function-local: allocate and store
        LLVMValueRef alloca = codegen_create_alloca(ctx, llvm_type, stmt->var_stmt.name);
        codegen_set_symbol_value(ctx, stmt->symbol, alloca);
        if (stmt->var_stmt.init)
        {
            bool old_mutable_init        = ctx->generating_mutable_init;
            ctx->generating_mutable_init = !stmt->var_stmt.is_val;
            LLVMValueRef init_value      = codegen_expr(ctx, stmt->var_stmt.init);
            ctx->generating_mutable_init = old_mutable_init;
            if (!init_value)
            {
                codegen_error(ctx, stmt, "failed to generate initializer");
                return NULL;
            }
            if (stmt->var_stmt.init->kind == AST_EXPR_STRUCT)
            {
                LLVMValueRef loaded_struct = LLVMBuildLoad2(ctx->builder, llvm_type, init_value, "");
                LLVMBuildStore(ctx->builder, loaded_struct, alloca);
            }
            else
            {
                init_value = codegen_load_if_needed(ctx, init_value, stmt->var_stmt.init->type);
                LLVMBuildStore(ctx->builder, init_value, alloca);
            }
        }
        return alloca;
    }
}

LLVMValueRef codegen_stmt_fun(CodegenContext *ctx, AstNode *stmt)
{
    if (!stmt->symbol || !stmt->type)
    {
        codegen_error(ctx, stmt, "function declaration missing symbol or type info");
        return NULL;
    }

    // function name mangling: main handled specially; other functions get package__name prefix to avoid collisions
    const char *func_name    = stmt->fun_stmt.name;
    char       *mangled_name = NULL;

    // mangle rules for main:
    // - if compiling runtime module, its 'main' remains 'main'
    // - if building entry with runtime, user 'main' becomes '__mach_main'
    // - if building without runtime, user 'main' stays 'main'
    if (strcmp(func_name, "main") == 0)
    {
        if (ctx->is_runtime)
        {
            func_name = stmt->fun_stmt.name;
        }
        else if (ctx->use_runtime)
        {
            mangled_name = strdup("__mach_main");
            func_name    = mangled_name;
        }
    }
    // other functions: apply package prefix if available
    else if (stmt->fun_stmt.name && ctx->package_name)
    {
        size_t len   = strlen(ctx->package_name) + 2 + strlen(stmt->fun_stmt.name) + 1;
        mangled_name = malloc(len);
        snprintf(mangled_name, len, "%s__%s", ctx->package_name, stmt->fun_stmt.name);
        func_name = mangled_name;
    }

    // get or create function
    LLVMValueRef func = LLVMGetNamedFunction(ctx->module, func_name);
    if (!func)
    {
        // build proper function type
        Type       *func_type   = stmt->type;
        LLVMTypeRef return_type = func_type->function.return_type ? codegen_get_llvm_type(ctx, func_type->function.return_type) : LLVMVoidTypeInContext(ctx->context);

        LLVMTypeRef *param_types = NULL;
        if (func_type->function.param_count > 0)
        {
            param_types = malloc(sizeof(LLVMTypeRef) * func_type->function.param_count);
            for (size_t i = 0; i < func_type->function.param_count; i++)
            {
                param_types[i] = codegen_get_llvm_type(ctx, func_type->function.param_types[i]);
            }
        }

        LLVMTypeRef llvm_func_type = LLVMFunctionType(return_type, param_types, func_type->function.param_count, func_type->function.is_variadic);
        func                       = LLVMAddFunction(ctx->module, func_name, llvm_func_type);
        free(param_types);
    }

    // clean up mangled name
    if (mangled_name)
        free(mangled_name);

    codegen_set_symbol_value(ctx, stmt->symbol, func);

    // generate body if present
    if (stmt->fun_stmt.body)
    {
        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(ctx->context, func, "entry");
        LLVMPositionBuilderAtEnd(ctx->builder, entry);

        // save current function
    LLVMValueRef prev_function = ctx->current_function;
    Type        *prev_ftype    = ctx->current_function_type;
    ctx->current_function      = func;
    ctx->current_function_type = stmt->type;

        // set parameter names and create allocas for fixed params
        size_t fixed_params            = stmt->type->function.param_count;
        ctx->current_fixed_param_count = fixed_params;
        if (stmt->fun_stmt.params)
        {
            size_t fixed_index = 0;
            for (int i = 0; i < stmt->fun_stmt.params->count; i++)
            {
                AstNode *param = stmt->fun_stmt.params->items[i];
                if (param->param_stmt.is_variadic)
                    continue; // skip sentinel
                LLVMValueRef param_value = LLVMGetParam(func, (unsigned)fixed_index);
                LLVMSetValueName2(param_value, param->param_stmt.name, strlen(param->param_stmt.name));
                LLVMTypeRef  param_type   = codegen_get_llvm_type(ctx, param->type);
                LLVMValueRef param_alloca = codegen_create_alloca(ctx, param_type, param->param_stmt.name);
                LLVMBuildStore(ctx->builder, param_value, param_alloca);
                if (param->symbol)
                    codegen_set_symbol_value(ctx, param->symbol, param_alloca);
                fixed_index++;
            }
        }

        // collect variadic arguments (allocate and store each to allow address access)
        if (stmt->type->function.is_variadic)
        {
            unsigned total_params = LLVMCountParams(func);
            if (total_params > fixed_params)
            {
                size_t extra = total_params - fixed_params;
                if (ctx->current_vararg_capacity < (int)extra)
                {
                    ctx->current_varargs         = realloc(ctx->current_varargs, sizeof(LLVMValueRef) * extra);
                    ctx->current_vararg_capacity = (int)extra;
                }
                ctx->current_vararg_count = (int)extra;
                for (size_t vi = 0; vi < extra; vi++)
                {
                    LLVMValueRef param_value = LLVMGetParam(func, (unsigned)(fixed_params + vi));
                    LLVMTypeRef  pty         = LLVMTypeOf(param_value);
                    // apply C default argument promotions mirror for storage (floats -> double, small ints -> i32)
                    if (LLVMGetTypeKind(pty) == LLVMHalfTypeKind || LLVMGetTypeKind(pty) == LLVMFloatTypeKind)
                    {
                        param_value = LLVMBuildFPExt(ctx->builder, param_value, LLVMDoubleTypeInContext(ctx->context), "va_store_fpromote");
                        pty         = LLVMDoubleTypeInContext(ctx->context);
                    }
                    else if (LLVMGetTypeKind(pty) == LLVMIntegerTypeKind && LLVMGetIntTypeWidth(pty) < 32)
                    {
                        LLVMTypeRef i32ty = LLVMInt32TypeInContext(ctx->context);
                        param_value       = LLVMBuildZExt(ctx->builder, param_value, i32ty, "va_store_ipromote");
                        pty               = i32ty;
                    }
                    // create an alloca to take address; name as "va_arg" with index
                    char namebuf[32];
                    snprintf(namebuf, sizeof(namebuf), "va_arg_%zu", vi);
                    LLVMValueRef alloca = codegen_create_alloca(ctx, pty, namebuf);
                    LLVMBuildStore(ctx->builder, param_value, alloca);
                    ctx->current_varargs[vi] = alloca;
                }
            }
            else
            {
                ctx->current_vararg_count = 0;
            }
        }

        // generate body
        codegen_stmt(ctx, stmt->fun_stmt.body);

        // add implicit return if needed
        if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)))
        {
            if (stmt->type->function.return_type == NULL)
            {
                LLVMBuildRetVoid(ctx->builder);
            }
            else
            {
                // emit a default zero to keep IR valid; semantic analysis reports real missing returns
                LLVMTypeRef  ret_ty = codegen_get_llvm_type(ctx, stmt->type->function.return_type);
                LLVMValueRef zero   = LLVMConstNull(ret_ty);
                LLVMBuildRet(ctx->builder, zero);
            }
        }

        // restore previous function & clear variadic tracking
        ctx->current_vararg_count      = 0;
        ctx->current_fixed_param_count = 0;
    ctx->current_function          = prev_function;
    ctx->current_function_type     = prev_ftype;
    }

    return func;
}

LLVMValueRef codegen_stmt_ret(CodegenContext *ctx, AstNode *stmt)
{
    if (stmt->ret_stmt.expr)
    {
        LLVMValueRef value = codegen_expr(ctx, stmt->ret_stmt.expr);
        if (!value)
        {
            codegen_error(ctx, stmt, "failed to generate return value");
            return NULL;
        }
        value = codegen_load_if_needed(ctx, value, stmt->ret_stmt.expr->type);
        // cast to declared return type if available (prefer Mach function type)
        LLVMTypeRef rty = NULL;
        if (ctx->current_function_type && ctx->current_function_type->kind == TYPE_FUNCTION && ctx->current_function_type->function.return_type)
        {
            rty = codegen_get_llvm_type(ctx, ctx->current_function_type->function.return_type);
        }
        // safe fallback: derive from current llvm function
        if (!rty && ctx->current_function)
        {
            LLVMTypeRef fty = LLVMTypeOf(ctx->current_function);
            if (fty && LLVMGetTypeKind(fty) == LLVMFunctionTypeKind)
                rty = LLVMGetReturnType(fty);
            else if (fty && LLVMGetTypeKind(fty) == LLVMPointerTypeKind)
            {
                LLVMTypeRef el = LLVMGetElementType(fty);
                if (el && LLVMGetTypeKind(el) == LLVMFunctionTypeKind)
                    rty = LLVMGetReturnType(el);
            }
        }
        LLVMTypeRef vty = LLVMTypeOf(value);
        if (rty && vty)
        {
            LLVMTypeKind rk = LLVMGetTypeKind(rty);
            LLVMTypeKind vk = LLVMGetTypeKind(vty);
            if (vk == LLVMIntegerTypeKind && rk == LLVMIntegerTypeKind)
            {
                unsigned vw = LLVMGetIntTypeWidth(vty);
                unsigned rw = LLVMGetIntTypeWidth(rty);
                if (vw < rw)
                    value = LLVMBuildZExt(ctx->builder, value, rty, "zext_ret");
                else if (vw > rw)
                    value = LLVMBuildTrunc(ctx->builder, value, rty, "trunc_ret");
            }
            else if (vk == LLVMPointerTypeKind && rk == LLVMIntegerTypeKind)
            {
                value = LLVMBuildPtrToInt(ctx->builder, value, rty, "ptrtoint_ret");
            }
            else if (vk == LLVMIntegerTypeKind && rk == LLVMPointerTypeKind)
            {
                value = LLVMBuildIntToPtr(ctx->builder, value, rty, "inttoptr_ret");
            }
        }
        return LLVMBuildRet(ctx->builder, value);
    }
    else
    {
        return LLVMBuildRetVoid(ctx->builder);
    }
}

LLVMValueRef codegen_stmt_if(CodegenContext *ctx, AstNode *stmt)
{
    LLVMValueRef cond_value = NULL;
    if (stmt->cond_stmt.cond)
    {
        cond_value = codegen_expr(ctx, stmt->cond_stmt.cond);
        if (!cond_value)
        {
            codegen_error(ctx, stmt, "failed to generate if condition");
            return NULL;
        }
        Type *ct = type_resolve_alias(stmt->cond_stmt.cond->type);
        if (type_is_integer(ct) || type_is_pointer_like(ct))
        {
            LLVMValueRef zero = LLVMConstNull(LLVMTypeOf(cond_value));
            cond_value        = LLVMBuildICmp(ctx->builder, LLVMIntNE, cond_value, zero, "ifcond");
        }
        else if (type_is_float(ct))
        {
            LLVMValueRef zero = LLVMConstReal(LLVMTypeOf(cond_value), 0.0);
            cond_value        = LLVMBuildFCmp(ctx->builder, LLVMRealONE, cond_value, zero, "ifcond");
        }
        else
        {
            codegen_error(ctx, stmt, "invalid truthiness for condition (struct/union not allowed)");
            return NULL;
        }
    }
    else
    {
        // else-only branch should execute unconditionally when reached
        // create a constant true condition
        cond_value = LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, false);
    }

    LLVMBasicBlockRef then_block  = LLVMAppendBasicBlockInContext(ctx->context, ctx->current_function, "then");
    LLVMBasicBlockRef else_block  = LLVMAppendBasicBlockInContext(ctx->context, ctx->current_function, "else");
    LLVMBasicBlockRef merge_block = LLVMAppendBasicBlockInContext(ctx->context, ctx->current_function, "ifcont");

    LLVMBuildCondBr(ctx->builder, cond_value, then_block, else_block);

    // generate then block
    LLVMPositionBuilderAtEnd(ctx->builder, then_block);
    codegen_stmt(ctx, stmt->cond_stmt.body);
    bool then_falls_through = false;
    if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)))
    {
        LLVMBuildBr(ctx->builder, merge_block);
        then_falls_through = true;
    }

    // generate else block (or chain)
    LLVMPositionBuilderAtEnd(ctx->builder, else_block);
    if (stmt->cond_stmt.stmt_or)
    {
        codegen_stmt(ctx, stmt->cond_stmt.stmt_or);
    }
    bool else_falls_through = false;
    if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)))
    {
        LLVMBuildBr(ctx->builder, merge_block);
        else_falls_through = true;
    }

    // continue at merge block
    LLVMPositionBuilderAtEnd(ctx->builder, merge_block);

    // if both branches terminated (no fallthrough), mark merge as unreachable to avoid false missing-return
    if (!then_falls_through && !else_falls_through)
    {
        LLVMBuildUnreachable(ctx->builder);
    }

    return NULL;
}

LLVMValueRef codegen_stmt_for(CodegenContext *ctx, AstNode *stmt)
{
    LLVMBasicBlockRef loop_block = LLVMAppendBasicBlockInContext(ctx->context, ctx->current_function, "loop");
    LLVMBasicBlockRef body_block = LLVMAppendBasicBlockInContext(ctx->context, ctx->current_function, "loop.body");
    LLVMBasicBlockRef exit_block = LLVMAppendBasicBlockInContext(ctx->context, ctx->current_function, "loop.exit");

    // save loop context
    LLVMBasicBlockRef prev_break    = ctx->break_block;
    LLVMBasicBlockRef prev_continue = ctx->continue_block;
    ctx->break_block                = exit_block;
    ctx->continue_block             = loop_block;

    // jump to loop header
    LLVMBuildBr(ctx->builder, loop_block);

    // generate loop header
    LLVMPositionBuilderAtEnd(ctx->builder, loop_block);

    if (stmt->for_stmt.cond)
    {
        LLVMValueRef cond_value = codegen_expr(ctx, stmt->for_stmt.cond);
        if (!cond_value)
        {
            codegen_error(ctx, stmt, "failed to generate loop condition");
            return NULL;
        }
        Type *ct = type_resolve_alias(stmt->for_stmt.cond->type);
        if (type_is_integer(ct) || type_is_pointer_like(ct))
        {
            cond_value = LLVMBuildICmp(ctx->builder, LLVMIntNE, cond_value, LLVMConstNull(LLVMTypeOf(cond_value)), "loopcond");
        }
        else if (type_is_float(ct))
        {
            cond_value = LLVMBuildFCmp(ctx->builder, LLVMRealONE, cond_value, LLVMConstReal(LLVMTypeOf(cond_value), 0.0), "loopcond");
        }
        else
        {
            codegen_error(ctx, stmt, "invalid truthiness for loop condition (struct/union not allowed)");
            return NULL;
        }

        LLVMBuildCondBr(ctx->builder, cond_value, body_block, exit_block);
    }
    else
    {
        // infinite loop
        LLVMBuildBr(ctx->builder, body_block);
    }

    // generate body
    LLVMPositionBuilderAtEnd(ctx->builder, body_block);
    codegen_stmt(ctx, stmt->for_stmt.body);
    if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)))
    {
        LLVMBuildBr(ctx->builder, loop_block);
    }

    // restore loop context
    ctx->break_block    = prev_break;
    ctx->continue_block = prev_continue;

    // continue at exit block
    LLVMPositionBuilderAtEnd(ctx->builder, exit_block);

    return NULL;
}

LLVMValueRef codegen_stmt_expr(CodegenContext *ctx, AstNode *stmt)
{
    // evaluate the expression but discard the result in statement context
    codegen_expr(ctx, stmt->expr_stmt.expr);
    return NULL;
}

LLVMValueRef codegen_expr(CodegenContext *ctx, AstNode *expr)
{
    if (!expr)
        return NULL;

    switch (expr->kind)
    {
    case AST_EXPR_LIT:
        return codegen_expr_lit(ctx, expr);
    case AST_EXPR_IDENT:
        return codegen_expr_ident(ctx, expr);
    case AST_EXPR_BINARY:
        return codegen_expr_binary(ctx, expr);
    case AST_EXPR_UNARY:
        return codegen_expr_unary(ctx, expr);
    case AST_EXPR_CALL:
        return codegen_expr_call(ctx, expr);
    case AST_EXPR_CAST:
        return codegen_expr_cast(ctx, expr);
    case AST_EXPR_FIELD:
        return codegen_expr_field(ctx, expr);
    case AST_EXPR_INDEX:
        return codegen_expr_index(ctx, expr);
    case AST_EXPR_ARRAY:
        return codegen_expr_array(ctx, expr);
    case AST_EXPR_STRUCT:
        return codegen_expr_struct(ctx, expr);
    default:
        codegen_error(ctx, expr, "unimplemented expression kind");
        return NULL;
    }
}

LLVMValueRef codegen_expr_lit(CodegenContext *ctx, AstNode *expr)
{
    switch (expr->lit_expr.kind)
    {
    case TOKEN_LIT_INT:
    {
        Type *t = type_resolve_alias(expr->type);
        // pointer-context: handle null and intptr -> ptr
        if (t && (t->kind == TYPE_PTR || t->kind == TYPE_POINTER))
        {
            if (expr->lit_expr.int_val == 0)
            {
                LLVMTypeRef pty = codegen_get_llvm_type(ctx, expr->type);
                if (!pty)
                    pty = LLVMPointerTypeInContext(ctx->context, 0);
                return LLVMConstNull(pty);
            }
            LLVMValueRef as_int = LLVMConstInt(LLVMInt64TypeInContext(ctx->context), expr->lit_expr.int_val, false);
            LLVMTypeRef  pty    = codegen_get_llvm_type(ctx, expr->type);
            if (!pty)
                pty = LLVMPointerTypeInContext(ctx->context, 0);
            return LLVMConstIntToPtr(as_int, pty);
        }
        // integer literal: use the semantic integer type if available, else i64 fallback
        LLVMTypeRef ity = codegen_get_llvm_type(ctx, expr->type);
        if (!ity || LLVMGetTypeKind(ity) != LLVMIntegerTypeKind)
            ity = LLVMInt64TypeInContext(ctx->context);
        bool is_signed = type_is_signed(expr->type);
        return LLVMConstInt(ity, expr->lit_expr.int_val, is_signed);
    }
    case TOKEN_LIT_FLOAT:
    {
        LLVMTypeRef type = codegen_get_llvm_type(ctx, expr->type);
        if (!type)
            type = LLVMDoubleTypeInContext(ctx->context);
        else
        {
            LLVMTypeKind k = LLVMGetTypeKind(type);
            if (!(k == LLVMHalfTypeKind || k == LLVMFloatTypeKind || k == LLVMDoubleTypeKind))
                type = LLVMDoubleTypeInContext(ctx->context);
        }
        return LLVMConstReal(type, expr->lit_expr.float_val);
    }
    case TOKEN_LIT_CHAR:
    {
        LLVMTypeRef type = codegen_get_llvm_type(ctx, expr->type);
        if (!type || LLVMGetTypeKind(type) != LLVMIntegerTypeKind)
            type = LLVMInt8TypeInContext(ctx->context);
        return LLVMConstInt(type, expr->lit_expr.char_val, false);
    }
    case TOKEN_LIT_STRING:
    {
        // create global string constant
        LLVMValueRef str_ptr = LLVMBuildGlobalStringPtr(ctx->builder, expr->lit_expr.string_val, "str");

        // if the target type is an array (fat pointer), create the fat pointer structure
        Type *resolved_type = type_resolve_alias(expr->type);
        if (resolved_type && resolved_type->kind == TYPE_ARRAY)
        {
            // create fat pointer: { ptr data, u64 length }
            LLVMTypeRef  fat_ptr_type = codegen_get_llvm_type(ctx, expr->type);
            LLVMValueRef fat_ptr      = LLVMGetUndef(fat_ptr_type);

            // set data pointer
            fat_ptr = LLVMBuildInsertValue(ctx->builder, fat_ptr, str_ptr, 0, "fat_ptr_data");

            // set length (strlen of the literal)
            size_t       str_len = strlen(expr->lit_expr.string_val);
            LLVMValueRef len_val = LLVMConstInt(LLVMInt64TypeInContext(ctx->context), str_len, false);
            fat_ptr              = LLVMBuildInsertValue(ctx->builder, fat_ptr, len_val, 1, "fat_ptr_len");

            return fat_ptr;
        }
        else
        {
            // for non-array types, return simple pointer
            return str_ptr;
        }
    }
    default:
        codegen_error(ctx, expr, "unknown literal kind");
        return NULL;
    }
}

LLVMValueRef codegen_expr_ident(CodegenContext *ctx, AstNode *expr)
{
    if (!expr->symbol)
    {
        codegen_error(ctx, expr, "identifier has no symbol");
        return NULL;
    }

    LLVMValueRef value = codegen_get_symbol_value(ctx, expr->symbol);
    if (!value)
    {
        // declare external global for module-level val/var
        if (expr->symbol->kind == SYMBOL_VAR || expr->symbol->kind == SYMBOL_VAL)
        {
            LLVMTypeRef ty = codegen_get_llvm_type(ctx, expr->symbol->type);
            value          = LLVMGetNamedGlobal(ctx->module, expr->ident_expr.name);
            if (!value)
            {
                value = LLVMAddGlobal(ctx->module, ty, expr->ident_expr.name);
            }
            codegen_set_symbol_value(ctx, expr->symbol, value);
        }
        else
        {
            codegen_error(ctx, expr, "undefined symbol '%s'", expr->ident_expr.name);
            return NULL;
        }
    }

    // load value if it's a variable
    if (expr->symbol->kind == SYMBOL_VAR || expr->symbol->kind == SYMBOL_VAL || expr->symbol->kind == SYMBOL_PARAM)
    {
        if (codegen_is_lvalue(expr))
        {
            return value; // return address for lvalues
        }
        else
        {
            return LLVMBuildLoad2(ctx->builder, codegen_get_llvm_type(ctx, expr->type), value, "");
        }
    }

    return value;
}

LLVMValueRef codegen_expr_binary(CodegenContext *ctx, AstNode *expr)
{
    // handle assignment specially
    if (expr->binary_expr.op == TOKEN_EQUAL)
    {
        // special-case deref on LHS: @ptr = value
        if (expr->binary_expr.left->kind == AST_EXPR_UNARY && expr->binary_expr.left->unary_expr.op == TOKEN_AT)
        {
            AstNode    *ptr_node = expr->binary_expr.left->unary_expr.expr;
            LLVMValueRef dest    = codegen_expr(ctx, ptr_node);
            if (!dest)
            {
                codegen_error(ctx, expr, "failed to generate destination pointer for store");
                return NULL;
            }
            // ensure dest is the pointer value (load from alloca/global/gep if needed)
            if (LLVMIsAAllocaInst(dest) || LLVMIsAGlobalVariable(dest) || LLVMIsAGetElementPtrInst(dest))
            {
                LLVMTypeRef ptr_ty = codegen_get_llvm_type(ctx, ptr_node->type);
                dest               = LLVMBuildLoad2(ctx->builder, ptr_ty, dest, "");
            }
            LLVMValueRef rhs = codegen_expr(ctx, expr->binary_expr.right);
            if (!rhs)
            {
                codegen_error(ctx, expr, "failed to generate rhs for store");
                return NULL;
            }
            rhs = codegen_load_if_needed(ctx, rhs, expr->binary_expr.right->type);
            LLVMBuildStore(ctx->builder, rhs, dest);
            return rhs;
        }

        LLVMValueRef lhs = codegen_expr(ctx, expr->binary_expr.left);
        LLVMValueRef rhs = codegen_expr(ctx, expr->binary_expr.right);

        if (!lhs || !rhs)
        {
            codegen_error(ctx, expr, "failed to generate assignment operands");
            return NULL;
        }

        // load rhs value if needed
        rhs = codegen_load_if_needed(ctx, rhs, expr->binary_expr.right->type);

        LLVMBuildStore(ctx->builder, rhs, lhs);
        return rhs;
    }

    // generate operands
    LLVMValueRef lhs = codegen_expr(ctx, expr->binary_expr.left);
    LLVMValueRef rhs = codegen_expr(ctx, expr->binary_expr.right);

    if (!lhs || !rhs)
    {
        codegen_error(ctx, expr, "failed to generate binary operands");
        return NULL;
    }

    // load values if needed
    lhs = codegen_load_if_needed(ctx, lhs, expr->binary_expr.left->type);
    rhs = codegen_load_if_needed(ctx, rhs, expr->binary_expr.right->type);

    // handle type conversions for comparison operations
    Type *lhs_type = type_resolve_alias(expr->binary_expr.left->type);
    Type *rhs_type = type_resolve_alias(expr->binary_expr.right->type);

    // handle integer type mismatches by extending smaller type
    if (type_is_integer(lhs_type) && type_is_integer(rhs_type) && lhs_type->kind != rhs_type->kind)
    {
        size_t lhs_size = type_sizeof(lhs_type);
        size_t rhs_size = type_sizeof(rhs_type);

        if (lhs_size < rhs_size)
        {
            bool        is_signed   = type_is_signed(lhs_type);
            LLVMTypeRef target_type = codegen_get_llvm_type(ctx, rhs_type);
            lhs                     = is_signed ? LLVMBuildSExt(ctx->builder, lhs, target_type, "sext") : LLVMBuildZExt(ctx->builder, lhs, target_type, "zext");
        }
        else if (rhs_size < lhs_size)
        {
            bool        is_signed   = type_is_signed(rhs_type);
            LLVMTypeRef target_type = codegen_get_llvm_type(ctx, lhs_type);
            rhs                     = is_signed ? LLVMBuildSExt(ctx->builder, rhs, target_type, "sext") : LLVMBuildZExt(ctx->builder, rhs, target_type, "zext");
        }
    }

    // generate operation based on operator and types
    bool is_float  = type_is_float(expr->type);
    bool is_signed = type_is_signed(expr->type);

    // untyped pointer comparisons (ptr == int): cast integer to pointer type
    if ((expr->binary_expr.op == TOKEN_EQUAL_EQUAL || expr->binary_expr.op == TOKEN_BANG_EQUAL))
    {
        if (type_is_pointer_like(lhs_type) && type_is_integer(rhs_type))
        {
            LLVMTypeRef lhs_ty = LLVMTypeOf(lhs);
            rhs                 = LLVMBuildIntToPtr(ctx->builder, rhs, lhs_ty, "int2ptr");
        }
        else if (type_is_integer(lhs_type) && type_is_pointer_like(rhs_type))
        {
            LLVMTypeRef rhs_ty = LLVMTypeOf(rhs);
            lhs                 = LLVMBuildIntToPtr(ctx->builder, lhs, rhs_ty, "int2ptr");
        }
        else if (type_is_integer(lhs_type) && type_is_integer(rhs_type))
        {
            // align integer widths for icmp
            LLVMTypeRef lty = LLVMTypeOf(lhs);
            LLVMTypeRef rty = LLVMTypeOf(rhs);
            if (LLVMGetTypeKind(lty) == LLVMIntegerTypeKind && LLVMGetTypeKind(rty) == LLVMIntegerTypeKind && lty != rty)
            {
                unsigned lw = LLVMGetIntTypeWidth(lty);
                unsigned rw = LLVMGetIntTypeWidth(rty);
                if (lw != rw)
                {
                    if (rw < lw)
                        rhs = LLVMBuildZExt(ctx->builder, rhs, lty, "zext_icmp");
                    else
                        rhs = LLVMBuildTrunc(ctx->builder, rhs, lty, "trunc_icmp");
                }
            }
        }
    }

    // pointer arithmetic: ptr + int or ptr - int
    if ((expr->binary_expr.op == TOKEN_PLUS || expr->binary_expr.op == TOKEN_MINUS) && type_is_pointer_like(lhs_type) && type_is_integer(rhs_type))
    {
        LLVMValueRef index = rhs;
        if (expr->binary_expr.op == TOKEN_MINUS)
        {
            index = LLVMBuildNeg(ctx->builder, rhs, "negindex");
        }
    // byte addressing with i8 element type (opaque ptr compatible)
    LLVMTypeRef i8ty = LLVMInt8TypeInContext(ctx->context);
    LLVMValueRef gep  = LLVMBuildGEP2(ctx->builder, i8ty, lhs, &index, 1, "ptradd");
        return gep;
    }

    switch (expr->binary_expr.op)
    {
    case TOKEN_PLUS:
        return is_float ? LLVMBuildFAdd(ctx->builder, lhs, rhs, "add") : LLVMBuildAdd(ctx->builder, lhs, rhs, "add");
    case TOKEN_MINUS:
        return is_float ? LLVMBuildFSub(ctx->builder, lhs, rhs, "sub") : LLVMBuildSub(ctx->builder, lhs, rhs, "sub");
    case TOKEN_STAR:
        return is_float ? LLVMBuildFMul(ctx->builder, lhs, rhs, "mul") : LLVMBuildMul(ctx->builder, lhs, rhs, "mul");
    case TOKEN_SLASH:
        return is_float ? LLVMBuildFDiv(ctx->builder, lhs, rhs, "div") : (is_signed ? LLVMBuildSDiv(ctx->builder, lhs, rhs, "div") : LLVMBuildUDiv(ctx->builder, lhs, rhs, "div"));
    case TOKEN_PERCENT:
        return is_signed ? LLVMBuildSRem(ctx->builder, lhs, rhs, "rem") : LLVMBuildURem(ctx->builder, lhs, rhs, "rem");
    case TOKEN_AMPERSAND:
        // bitwise and (integers)
        return LLVMBuildAnd(ctx->builder, lhs, rhs, "band");
    case TOKEN_PIPE:
        // bitwise or
        return LLVMBuildOr(ctx->builder, lhs, rhs, "bor");
    case TOKEN_EQUAL_EQUAL:
        // allow pointer equality
        return LLVMBuildICmp(ctx->builder, LLVMIntEQ, lhs, rhs, "eq");
    case TOKEN_BANG_EQUAL:
        return LLVMBuildICmp(ctx->builder, LLVMIntNE, lhs, rhs, "ne");
    case TOKEN_LESS:
        return is_float ? LLVMBuildFCmp(ctx->builder, LLVMRealOLT, lhs, rhs, "lt") : LLVMBuildICmp(ctx->builder, LLVMIntSLT, lhs, rhs, "lt");
    case TOKEN_LESS_EQUAL:
        return is_float ? LLVMBuildFCmp(ctx->builder, LLVMRealOLE, lhs, rhs, "le") : LLVMBuildICmp(ctx->builder, LLVMIntSLE, lhs, rhs, "le");
    case TOKEN_GREATER:
        return is_float ? LLVMBuildFCmp(ctx->builder, LLVMRealOGT, lhs, rhs, "gt") : LLVMBuildICmp(ctx->builder, LLVMIntSGT, lhs, rhs, "gt");
    case TOKEN_GREATER_EQUAL:
        return is_float ? LLVMBuildFCmp(ctx->builder, LLVMRealOGE, lhs, rhs, "ge") : LLVMBuildICmp(ctx->builder, LLVMIntSGE, lhs, rhs, "ge");
    case TOKEN_AMPERSAND_AMPERSAND:
    {
        LLVMValueRef l1 = LLVMBuildICmp(ctx->builder, LLVMIntNE, lhs, LLVMConstNull(LLVMTypeOf(lhs)), "l1");
        LLVMValueRef r1 = LLVMBuildICmp(ctx->builder, LLVMIntNE, rhs, LLVMConstNull(LLVMTypeOf(rhs)), "r1");
        return LLVMBuildAnd(ctx->builder, l1, r1, "and");
    }
    case TOKEN_PIPE_PIPE:
    case TOKEN_KW_OR:
    {
        LLVMValueRef l1 = LLVMBuildICmp(ctx->builder, LLVMIntNE, lhs, LLVMConstNull(LLVMTypeOf(lhs)), "l1");
        LLVMValueRef r1 = LLVMBuildICmp(ctx->builder, LLVMIntNE, rhs, LLVMConstNull(LLVMTypeOf(rhs)), "r1");
        return LLVMBuildOr(ctx->builder, l1, r1, "or");
    }
    case TOKEN_CARET:
        return LLVMBuildXor(ctx->builder, lhs, rhs, "xor");
    case TOKEN_LESS_LESS:
        return LLVMBuildShl(ctx->builder, lhs, rhs, "shl");
    case TOKEN_GREATER_GREATER:
        return is_signed ? LLVMBuildAShr(ctx->builder, lhs, rhs, "shr") : LLVMBuildLShr(ctx->builder, lhs, rhs, "shr");
        {
        default:
            codegen_error(ctx, expr, "unimplemented binary operator");
            return NULL;
        }
    }
}

LLVMValueRef codegen_expr_unary(CodegenContext *ctx, AstNode *expr)
{
    // special-case '?0' early to avoid evaluating the 0 literal with no type hint
    if (expr->unary_expr.op == TOKEN_QUESTION &&
        expr->unary_expr.expr &&
        expr->unary_expr.expr->kind == AST_EXPR_LIT &&
        expr->unary_expr.expr->lit_expr.kind == TOKEN_LIT_INT &&
        expr->unary_expr.expr->lit_expr.int_val == 0)
    {
        LLVMTypeRef ptr_ty = codegen_get_llvm_type(ctx, expr->type);
        if (!ptr_ty)
            ptr_ty = LLVMPointerTypeInContext(ctx->context, 0);
        return LLVMConstNull(ptr_ty);
    }

    LLVMValueRef operand = codegen_expr(ctx, expr->unary_expr.expr);
    if (!operand)
    {
        codegen_error(ctx, expr, "failed to generate unary operand");
        return NULL;
    }

    switch (expr->unary_expr.op)
    {
    case TOKEN_MINUS:
        operand = codegen_load_if_needed(ctx, operand, expr->unary_expr.expr->type);
        if (type_is_float(expr->type))
        {
            return LLVMBuildFNeg(ctx->builder, operand, "neg");
        }
        else
        {
            return LLVMBuildNeg(ctx->builder, operand, "neg");
        }
    case TOKEN_QUESTION: // address-of or null literal '?0'
        // special-case '?0' -> null pointer of the unary expression's type
        if (expr->unary_expr.expr->kind == AST_EXPR_LIT && expr->unary_expr.expr->lit_expr.kind == TOKEN_LIT_INT && expr->unary_expr.expr->lit_expr.int_val == 0)
        {
            LLVMTypeRef ptr_ty = codegen_get_llvm_type(ctx, expr->type);
            return LLVMConstNull(ptr_ty);
        }
        // for general address-of, return the address (ident/index/field generate addresses already)
        return operand;
    case TOKEN_AT: // dereference
        // for dereference, we always need to load the pointer value first if it's in an alloca/global
        if (LLVMIsAAllocaInst(operand) || LLVMIsAGlobalVariable(operand) || LLVMIsAGetElementPtrInst(operand))
        {
            LLVMTypeRef ptr_type = codegen_get_llvm_type(ctx, expr->unary_expr.expr->type);
            operand              = LLVMBuildLoad2(ctx->builder, ptr_type, operand, "");
        }
        // then dereference the pointer
        return LLVMBuildLoad2(ctx->builder, codegen_get_llvm_type(ctx, expr->type), operand, "deref");
    case TOKEN_TILDE: // bitwise not
        operand = codegen_load_if_needed(ctx, operand, expr->unary_expr.expr->type);
        return LLVMBuildNot(ctx->builder, operand, "bnot");
    default:
        codegen_error(ctx, expr, "unimplemented unary operator");
        return NULL;
    }
}

LLVMValueRef codegen_expr_call(CodegenContext *ctx, AstNode *expr)
{
    // check for builtin/intrinsic functions first
    if (expr->call_expr.func->kind == AST_EXPR_IDENT)
    {
        const char *func_name = expr->call_expr.func->ident_expr.name;

        // early handling for variadic intrinsics (no symbol required)
        if (strcmp(func_name, "va_count") == 0)
        {
            if (expr->call_expr.args && expr->call_expr.args->count != 0)
            {
                codegen_error(ctx, expr, "va_count() expects no arguments");
                return NULL;
            }
            return LLVMConstInt(LLVMInt64TypeInContext(ctx->context), (unsigned long long)ctx->current_vararg_count, false);
        }
        if (strcmp(func_name, "va_arg") == 0)
        {
            if (!expr->call_expr.args || expr->call_expr.args->count != 1)
            {
                codegen_error(ctx, expr, "va_arg() expects exactly one argument");
                return NULL;
            }
            AstNode     *idx_node = expr->call_expr.args->items[0];
            LLVMValueRef idx_val  = codegen_expr(ctx, idx_node);
            if (!idx_val)
            {
                codegen_error(ctx, expr, "failed to generate va_arg index");
                return NULL;
            }
            idx_val = codegen_load_if_needed(ctx, idx_val, idx_node->type);
            if (ctx->current_vararg_count == 0)
            {
                codegen_error(ctx, expr, "no variadic arguments available");
                return NULL;
            }
            if (LLVMIsAConstantInt(idx_val))
            {
                unsigned long long c = LLVMConstIntGetZExtValue(idx_val);
                if (c >= (unsigned long long)ctx->current_vararg_count)
                {
                    codegen_error(ctx, expr, "va_arg index out of range");
                    return NULL;
                }
                LLVMTypeRef ptr_ty = LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0);
                return LLVMBuildBitCast(ctx->builder, ctx->current_varargs[c], ptr_ty, "va_arg_ptr");
            }
            // dynamic: create switch
            LLVMTypeRef  ptr_ty      = LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0);
            LLVMValueRef result_slot = codegen_create_alloca(ctx, ptr_ty, "va_arg_dyn");
            LLVMBuildStore(ctx->builder, LLVMConstNull(ptr_ty), result_slot);
            LLVMBasicBlockRef end_block = LLVMAppendBasicBlockInContext(ctx->context, ctx->current_function, "va_arg_dyn_end");
            LLVMValueRef      sw        = LLVMBuildSwitch(ctx->builder, idx_val, end_block, ctx->current_vararg_count);
            for (int i = 0; i < ctx->current_vararg_count; i++)
            {
                LLVMBasicBlockRef case_block = LLVMAppendBasicBlockInContext(ctx->context, ctx->current_function, "va_arg_case");
                LLVMAddCase(sw, LLVMConstInt(LLVMTypeOf(idx_val), (unsigned long long)i, false), case_block);
                LLVMPositionBuilderAtEnd(ctx->builder, case_block);
                LLVMValueRef casted = LLVMBuildBitCast(ctx->builder, ctx->current_varargs[i], ptr_ty, "va_arg_ptr_case");
                LLVMBuildStore(ctx->builder, casted, result_slot);
                LLVMBuildBr(ctx->builder, end_block);
            }
            LLVMPositionBuilderAtEnd(ctx->builder, end_block);
            return LLVMBuildLoad2(ctx->builder, ptr_ty, result_slot, "");
        }

        // handle len() intrinsic
        if (strcmp(func_name, "len") == 0)
        {
            if (!expr->call_expr.args || expr->call_expr.args->count != 1)
            {
                codegen_error(ctx, expr, "len() expects exactly one argument");
                return NULL;
            }

            AstNode *arg      = expr->call_expr.args->items[0];
            Type    *arg_type = type_resolve_alias(arg->type);

            if (arg_type->kind == TYPE_ARRAY)
            {
                // set the expression type for the len() call
                expr->type             = type_u64();
                LLVMValueRef array_val = codegen_expr(ctx, arg);
                if (!array_val)
                {
                    codegen_error(ctx, expr, "failed to generate array argument for len()");
                    return NULL;
                }
                array_val = codegen_load_if_needed(ctx, array_val, arg_type);
                return LLVMBuildExtractValue(ctx->builder, array_val, 1, "array_length");
            }
            else
            {
                codegen_error(ctx, expr, "len() can only be applied to arrays");
                return NULL;
            }
        }

        // handle size_of() intrinsic
        if (strcmp(func_name, "size_of") == 0)
        {
            if (!expr->call_expr.args || expr->call_expr.args->count != 1)
            {
                codegen_error(ctx, expr, "size_of() expects exactly one argument");
                return NULL;
            }

            AstNode *arg      = expr->call_expr.args->items[0];
            Type    *arg_type = type_resolve_alias(arg->type);
            size_t   size     = type_sizeof(arg_type);

            // set the expression type for the size_of() call
            expr->type = type_u64();

            return LLVMConstInt(LLVMInt64TypeInContext(ctx->context), size, false);
        }

        // handle align_of() intrinsic
        if (strcmp(func_name, "align_of") == 0)
        {
            if (!expr->call_expr.args || expr->call_expr.args->count != 1)
            {
                codegen_error(ctx, expr, "align_of() expects exactly one argument");
                return NULL;
            }

            AstNode *arg       = expr->call_expr.args->items[0];
            Type    *arg_type  = type_resolve_alias(arg->type);
            size_t   alignment = type_alignof(arg_type);

            // set the expression type for the align_of() call
            expr->type = type_u64();

            return LLVMConstInt(LLVMInt64TypeInContext(ctx->context), alignment, false);
        }

        // handle offset_of() intrinsic
        if (strcmp(func_name, "offset_of") == 0)
        {
            if (!expr->call_expr.args || expr->call_expr.args->count != 2)
            {
                codegen_error(ctx, expr, "offset_of() expects exactly two arguments");
                return NULL;
            }

            AstNode *type_arg  = expr->call_expr.args->items[0];
            AstNode *field_arg = expr->call_expr.args->items[1];

            Type       *struct_type = type_resolve_alias(type_arg->type);
            const char *field_name  = field_arg->ident_expr.name;

            // calculate field offset
            size_t offset = 0;
            bool   found  = false;

            if (struct_type->kind == TYPE_STRUCT && struct_type->composite.fields)
            {
                for (size_t i = 0; i < struct_type->composite.field_count; i++)
                {
                    Symbol *field = &struct_type->composite.fields[i];
                    if (strcmp(field->name, field_name) == 0)
                    {
                        offset = field->field.offset;
                        found  = true;
                        break;
                    }
                }
            }

            if (!found)
            {
                codegen_error(ctx, expr, "field '%s' not found in struct", field_name);
                return NULL;
            }

            expr->type = type_u64();
            return LLVMConstInt(LLVMInt64TypeInContext(ctx->context), offset, false);
        }

        // handle type_of() intrinsic
        if (strcmp(func_name, "type_of") == 0)
        {
            if (!expr->call_expr.args || expr->call_expr.args->count != 1)
            {
                codegen_error(ctx, expr, "type_of() expects exactly one argument");
                return NULL;
            }

            AstNode *arg      = expr->call_expr.args->items[0];
            Type    *arg_type = type_resolve_alias(arg->type);

            // generate a simple hash of the type for runtime identification
            // this is a basic implementation - you could make it more sophisticated
            uint64_t type_hash = (uint64_t)arg_type->kind;
            type_hash          = type_hash * 31 + arg_type->size;
            type_hash          = type_hash * 31 + arg_type->alignment;

            expr->type = type_u64();
            return LLVMConstInt(LLVMInt64TypeInContext(ctx->context), type_hash, false);
        }
    }

    LLVMValueRef func = codegen_expr(ctx, expr->call_expr.func);
    if (!func)
    {
        codegen_error(ctx, expr, "failed to generate function for call");
        return NULL;
    }

    // load function pointer if needed (for function parameters)
    func = codegen_load_if_needed(ctx, func, expr->call_expr.func->type);

    // get function type early for parameter type checking
    Type *func_type = type_resolve_alias(expr->call_expr.func->type);
    if (!func_type || func_type->kind != TYPE_FUNCTION)
    {
        codegen_error(ctx, expr, "call expression does not have function type");
        return NULL;
    }

    // handle internal variadic intrinsics after resolving function type
    if (expr->call_expr.func->kind == AST_EXPR_IDENT)
    {
        const char *fname = expr->call_expr.func->ident_expr.name;
        if (strcmp(fname, "va_count") == 0)
        {
            // returns u64 constant of active variadic count
            if (ctx->current_vararg_count < 0)
                return LLVMConstInt(LLVMInt64TypeInContext(ctx->context), 0, false);
            return LLVMConstInt(LLVMInt64TypeInContext(ctx->context), (unsigned long long)ctx->current_vararg_count, false);
        }
        else if (strcmp(fname, "va_arg") == 0)
        {
            if (!expr->call_expr.args || expr->call_expr.args->count != 1)
            {
                codegen_error(ctx, expr, "va_arg expects exactly one index argument");
                return NULL;
            }
            AstNode     *idx_node = expr->call_expr.args->items[0];
            LLVMValueRef idx_val  = codegen_expr(ctx, idx_node);
            if (!idx_val)
            {
                codegen_error(ctx, expr, "failed to generate index for va_arg");
                return NULL;
            }
            idx_val = codegen_load_if_needed(ctx, idx_val, idx_node->type);
            // bounds not enforced at runtime yet
            // select pointer from array via switch / loop (simple linear since small)
            // build array of constants
            if (ctx->current_vararg_count == 0)
            {
                codegen_error(ctx, expr, "no variadic arguments available");
                return NULL;
            }
            // we'll lower to: allocate array of i8* constants referencing each alloca casted to i8*
            LLVMTypeRef   ptr_ty    = LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0);
            LLVMValueRef *case_vals = malloc(sizeof(LLVMValueRef) * ctx->current_vararg_count);
            for (int i = 0; i < ctx->current_vararg_count; i++)
            {
                case_vals[i] = LLVMBuildBitCast(ctx->builder, ctx->current_varargs[i], ptr_ty, "va_cast");
            }
            // simple approach: if index is constant and in range return directly
            if (LLVMIsAConstantInt(idx_val))
            {
                unsigned long long c = LLVMConstIntGetZExtValue(idx_val);
                if (c < (unsigned long long)ctx->current_vararg_count)
                {
                    LLVMValueRef result = case_vals[c];
                    free(case_vals);
                    return result;
                }
                free(case_vals);
                codegen_error(ctx, expr, "va_arg index out of range");
                return NULL;
            }
            // dynamic index: build switch
            LLVMValueRef result_alloca = codegen_create_alloca(ctx, ptr_ty, "va_arg_result");
            LLVMValueRef zero          = LLVMConstNull(ptr_ty);
            LLVMBuildStore(ctx->builder, zero, result_alloca);
            LLVMBasicBlockRef end_block   = LLVMAppendBasicBlockInContext(ctx->context, ctx->current_function, "va_arg_end");
            LLVMValueRef      switch_inst = LLVMBuildSwitch(ctx->builder, idx_val, end_block, ctx->current_vararg_count);
            for (int i = 0; i < ctx->current_vararg_count; i++)
            {
                LLVMBasicBlockRef case_block = LLVMAppendBasicBlockInContext(ctx->context, ctx->current_function, "va_case");
                LLVMAddCase(switch_inst, LLVMConstInt(LLVMTypeOf(idx_val), (unsigned long long)i, false), case_block);
                LLVMPositionBuilderAtEnd(ctx->builder, case_block);
                LLVMBuildStore(ctx->builder, case_vals[i], result_alloca);
                LLVMBuildBr(ctx->builder, end_block);
            }
            LLVMPositionBuilderAtEnd(ctx->builder, end_block);
            free(case_vals);
            return LLVMBuildLoad2(ctx->builder, ptr_ty, result_alloca, "");
        }
    }

    // generate arguments
    int           arg_count = expr->call_expr.args ? expr->call_expr.args->count : 0;
    LLVMValueRef *args      = NULL;

    if (arg_count > 0)
    {
        args = malloc(sizeof(LLVMValueRef) * arg_count);
        for (int i = 0; i < arg_count; i++)
        {
            args[i] = codegen_expr(ctx, expr->call_expr.args->items[i]);
            if (!args[i])
            {
                free(args);
                codegen_error(ctx, expr, "failed to generate call argument %d", i);
                return NULL;
            }

            Type *arg_type = expr->call_expr.args->items[i]->type;
            args[i]        = codegen_load_if_needed(ctx, args[i], arg_type);

            bool is_fixed = i < (int)func_type->function.param_count;
            if (!is_fixed)
            {
                // variadic argument: perform minimal array decay
                Type *at = type_resolve_alias(arg_type);
                if (at && at->kind == TYPE_ARRAY)
                {
                    args[i] = LLVMBuildExtractValue(ctx->builder, args[i], 0, "vararg_array_data");
                }
                else if (at && type_is_float(at) && at->size < 8)
                {
                    // float / f16 -> f64
                    args[i] = LLVMBuildFPExt(ctx->builder, args[i], LLVMDoubleTypeInContext(ctx->context), "vararg_fpromote");
                }
                else if (at && type_is_integer(at) && at->size < 4)
                {
                    // integer types smaller than 32 bits promote to 32-bit
                    LLVMTypeRef i32ty = LLVMInt32TypeInContext(ctx->context);
                    if (type_is_signed(at))
                        args[i] = LLVMBuildSExt(ctx->builder, args[i], i32ty, "vararg_ipromote");
                    else
                        args[i] = LLVMBuildZExt(ctx->builder, args[i], i32ty, "vararg_ipromote");
                }
                continue;
            }

            Type *param_type = func_type->function.param_types[i];
            Type *pt         = type_resolve_alias(param_type);
            Type *at         = type_resolve_alias(arg_type);

            if (at && at->kind == TYPE_ARRAY && pt && (pt->kind == TYPE_POINTER || pt->kind == TYPE_PTR))
            {
                args[i] = LLVMBuildExtractValue(ctx->builder, args[i], 0, "array_data");
            }

            if (pt && at && type_is_numeric(pt) && type_is_numeric(at))
            {
                LLVMTypeRef expected_llvm = codegen_get_llvm_type(ctx, param_type);
                if (type_is_float(at) && type_is_float(pt))
                {
                    if (at->size < pt->size)
                        args[i] = LLVMBuildFPExt(ctx->builder, args[i], expected_llvm, "fpext");
                    else if (at->size > pt->size)
                        args[i] = LLVMBuildFPTrunc(ctx->builder, args[i], expected_llvm, "fptrunc");
                }
                else if (type_is_integer(at) && type_is_integer(pt))
                {
                    if (at->size < pt->size)
                    {
                        if (type_is_signed(at))
                            args[i] = LLVMBuildSExt(ctx->builder, args[i], expected_llvm, "sext");
                        else
                            args[i] = LLVMBuildZExt(ctx->builder, args[i], expected_llvm, "zext");
                    }
                    else if (at->size > pt->size)
                    {
                        args[i] = LLVMBuildTrunc(ctx->builder, args[i], expected_llvm, "trunc");
                    }
                }
                else if (type_is_float(at) && type_is_integer(pt))
                {
                    if (type_is_signed(pt))
                        args[i] = LLVMBuildFPToSI(ctx->builder, args[i], expected_llvm, "fptosi");
                    else
                        args[i] = LLVMBuildFPToUI(ctx->builder, args[i], expected_llvm, "fptoui");
                }
                else if (type_is_integer(at) && type_is_float(pt))
                {
                    if (type_is_signed(at))
                        args[i] = LLVMBuildSIToFP(ctx->builder, args[i], expected_llvm, "sitofp");
                    else
                        args[i] = LLVMBuildUIToFP(ctx->builder, args[i], expected_llvm, "uitofp");
                }
            }
            else if (pt && at && (type_is_pointer_like(pt) && type_is_pointer_like(at)))
            {
                // allow pointer-to-pointer casts at call sites via bitcast
                LLVMTypeRef expected_llvm = codegen_get_llvm_type(ctx, param_type);
                if (expected_llvm && LLVMTypeOf(args[i]) != expected_llvm)
                {
                    args[i] = LLVMBuildBitCast(ctx->builder, args[i], expected_llvm, "ptrcast");
                }
            }
            else if (pt && at && type_is_pointer_like(pt) && type_is_integer(at))
            {
                LLVMTypeRef expected_llvm = codegen_get_llvm_type(ctx, param_type);
                if (expected_llvm)
                    args[i] = LLVMBuildIntToPtr(ctx->builder, args[i], expected_llvm, "inttoptr_arg");
            }
            else if (pt && at && type_is_integer(pt) && type_is_pointer_like(at))
            {
                LLVMTypeRef expected_llvm = codegen_get_llvm_type(ctx, param_type);
                if (expected_llvm)
                    args[i] = LLVMBuildPtrToInt(ctx->builder, args[i], expected_llvm, "ptrtoint_arg");
            }
        }
    }

    // build the actual function type for the call
    LLVMTypeRef return_type = func_type->function.return_type ? codegen_get_llvm_type(ctx, func_type->function.return_type) : LLVMVoidTypeInContext(ctx->context);

    LLVMTypeRef *param_types = NULL;
    if (func_type->function.param_count > 0)
    {
        param_types = malloc(sizeof(LLVMTypeRef) * func_type->function.param_count);
        for (size_t i = 0; i < func_type->function.param_count; i++)
        {
            param_types[i] = codegen_get_llvm_type(ctx, func_type->function.param_types[i]);
        }
    }

    LLVMTypeRef  llvm_func_type = LLVMFunctionType(return_type, param_types, func_type->function.param_count, func_type->function.is_variadic);
    LLVMValueRef result         = LLVMBuildCall2(ctx->builder, llvm_func_type, func, args, arg_count, "");

    free(param_types);
    free(args);
    return result;
}

LLVMValueRef codegen_expr_cast(CodegenContext *ctx, AstNode *expr)
{
    LLVMValueRef value = codegen_expr(ctx, expr->cast_expr.expr);
    if (!value)
    {
        codegen_error(ctx, expr, "failed to generate cast operand");
        return NULL;
    }

    // operate on the loaded value unless we're decaying arrays
    // loading before type resolution ensures correct scalar casts
    // underlying array-to-pointer cases are handled later

    Type *from_type = expr->cast_expr.expr->type;
    Type *to_type   = expr->type;

    // resolve type aliases for proper type checking
    Type *resolved_from_type = type_resolve_alias(from_type);
    Type *resolved_to_type   = type_resolve_alias(to_type);
    if (!resolved_from_type)
        resolved_from_type = from_type;
    if (!resolved_to_type)
        resolved_to_type = to_type;

    // for array to pointer decay, don't load the array value; otherwise load
    bool is_array_decay = (resolved_from_type->kind == TYPE_ARRAY && (resolved_to_type->kind == TYPE_POINTER || resolved_to_type->kind == TYPE_PTR));
    if (!is_array_decay)
        value = codegen_load_if_needed(ctx, value, from_type);

    LLVMTypeRef to_llvm_type = codegen_get_llvm_type(ctx, to_type);

    // numeric casts
    if (type_is_numeric(resolved_from_type) && type_is_numeric(resolved_to_type))
    {
        if (type_is_float(resolved_from_type) && type_is_float(resolved_to_type))
        {
            // float to float
            if (resolved_from_type->size < resolved_to_type->size)
            {
                return LLVMBuildFPExt(ctx->builder, value, to_llvm_type, "fpext");
            }
            else if (resolved_from_type->size > resolved_to_type->size)
            {
                return LLVMBuildFPTrunc(ctx->builder, value, to_llvm_type, "fptrunc");
            }
            return value;
        }
        else if (type_is_float(resolved_from_type) && type_is_integer(resolved_to_type))
        {
            // float to int
            if (type_is_signed(resolved_to_type))
            {
                return LLVMBuildFPToSI(ctx->builder, value, to_llvm_type, "fptosi");
            }
            else
            {
                return LLVMBuildFPToUI(ctx->builder, value, to_llvm_type, "fptoui");
            }
        }
        else if (type_is_integer(resolved_from_type) && type_is_float(resolved_to_type))
        {
            // int to float
            if (type_is_signed(resolved_from_type))
            {
                return LLVMBuildSIToFP(ctx->builder, value, to_llvm_type, "sitofp");
            }
            else
            {
                return LLVMBuildUIToFP(ctx->builder, value, to_llvm_type, "uitofp");
            }
        }
        else
        {
            // int to int
            if (resolved_from_type->size < resolved_to_type->size)
            {
                if (type_is_signed(resolved_from_type))
                {
                    return LLVMBuildSExt(ctx->builder, value, to_llvm_type, "sext");
                }
                else
                {
                    return LLVMBuildZExt(ctx->builder, value, to_llvm_type, "zext");
                }
            }
            else if (resolved_from_type->size > resolved_to_type->size)
            {
                return LLVMBuildTrunc(ctx->builder, value, to_llvm_type, "trunc");
            }
            return value;
        }
    }

    // integer to pointer cast
    if (type_is_numeric(resolved_from_type) && type_is_pointer_like(resolved_to_type))
    {
        return LLVMBuildIntToPtr(ctx->builder, value, to_llvm_type, "inttoptr");
    }

    // pointer casts
    if (type_is_pointer_like(resolved_from_type) && type_is_pointer_like(resolved_to_type))
    {
        return value; // all pointers are opaque in LLVM
    }

    codegen_error(ctx, expr, "unimplemented cast");
    return NULL;
}

LLVMValueRef codegen_create_alloca(CodegenContext *ctx, LLVMTypeRef type, const char *name)
{
    // if we're not in a function, this is a global variable - handle differently
    if (!ctx->current_function)
    {
        // create a global variable instead of an alloca
        LLVMValueRef global = LLVMAddGlobal(ctx->module, type, name);
        LLVMSetInitializer(global, LLVMConstNull(type));
        LLVMSetLinkage(global, LLVMInternalLinkage);
        return global;
    }

    // create allocas in entry block for better optimization
    LLVMBasicBlockRef entry       = LLVMGetEntryBasicBlock(ctx->current_function);
    LLVMBuilderRef    tmp_builder = LLVMCreateBuilderInContext(ctx->context);

    // position at start of entry block
    LLVMValueRef first_inst = LLVMGetFirstInstruction(entry);
    if (first_inst)
    {
        LLVMPositionBuilderBefore(tmp_builder, first_inst);
    }
    else
    {
        LLVMPositionBuilderAtEnd(tmp_builder, entry);
    }

    LLVMValueRef alloca = LLVMBuildAlloca(tmp_builder, type, name);
    LLVMDisposeBuilder(tmp_builder);

    return alloca;
}

LLVMValueRef codegen_load_if_needed(CodegenContext *ctx, LLVMValueRef value, Type *type)
{
    // if no expected type, assume value is already in the right form
    if (!type)
    {
        return value;
    }

    // resolve any type aliases first
    type = type_resolve_alias(type);

    // if the expected type is a pointer (excluding functions), don't load
    if (type->kind == TYPE_PTR || type->kind == TYPE_POINTER)
    {
        return value;
    }

    // check if this is already a loaded value or a function/constant
    if (!LLVMIsAAllocaInst(value) && !LLVMIsAGlobalVariable(value) && !LLVMIsAGetElementPtrInst(value))
    {
        return value;
    }

    // load the value
    LLVMTypeRef llvm_type = codegen_get_llvm_type(ctx, type);
    return LLVMBuildLoad2(ctx->builder, llvm_type, value, "");
}

bool codegen_is_lvalue(AstNode *expr)
{
    switch (expr->kind)
    {
    case AST_EXPR_IDENT:
        return true;
    case AST_EXPR_INDEX:
        return true;
    case AST_EXPR_FIELD:
        return codegen_is_lvalue(expr->field_expr.object);
    case AST_EXPR_UNARY:
        return expr->unary_expr.op == TOKEN_AT;
    default:
        return false;
    }
}

LLVMValueRef codegen_expr_field(CodegenContext *ctx, AstNode *expr)
{
    // module member access: object is an identifier bound to a module symbol
    if (expr->field_expr.object && expr->field_expr.object->kind == AST_EXPR_IDENT)
    {
        Symbol *obj_sym = expr->field_expr.object->symbol;
        if (obj_sym && obj_sym->kind == SYMBOL_MODULE)
        {
            // semantic analysis stored the resolved member symbol in expr->symbol
            if (expr->symbol)
            {
                LLVMValueRef value = codegen_get_symbol_value(ctx, expr->symbol);
                if (!value)
                {
                    codegen_error(ctx, expr, "undefined symbol '%s'", expr->field_expr.field);
                    return NULL;
                }
                return value;
            }
        }
    }

    LLVMValueRef object = codegen_expr(ctx, expr->field_expr.object);
    if (!object)
    {
        codegen_error(ctx, expr, "failed to generate object for field access");
        return NULL;
    }

    // get the object type
    Type *object_type = expr->field_expr.object->type;
    object_type       = type_resolve_alias(object_type);

    if (object_type->kind != TYPE_STRUCT)
    {
        codegen_error(ctx, expr, "field access on non-struct type");
        return NULL;
    }

    // ensure we have an address to GEP from; structs may come back as values
    if (!LLVMIsAAllocaInst(object) && !LLVMIsAGlobalVariable(object) && !LLVMIsAGetElementPtrInst(object))
    {
        LLVMTypeRef  llvm_struct_type = codegen_get_llvm_type(ctx, object_type);
        LLVMValueRef tmp              = codegen_create_alloca(ctx, llvm_struct_type, "tmp_struct");
        LLVMBuildStore(ctx->builder, object, tmp);
        object = tmp;
    }

    // find the field
    Symbol *field_symbol = NULL;
    int     field_index  = 0;
    for (Symbol *field = object_type->composite.fields; field; field = field->next)
    {
        if (field->name && strcmp(field->name, expr->field_expr.field) == 0)
        {
            field_symbol = field;
            break;
        }
        field_index++;
    }

    if (!field_symbol)
    {
        codegen_error(ctx, expr, "no field named '%s'", expr->field_expr.field);
        return NULL;
    }

    // generate GEP for field access
    LLVMValueRef indices[] = {LLVMConstInt(LLVMInt32TypeInContext(ctx->context), 0, false), LLVMConstInt(LLVMInt32TypeInContext(ctx->context), field_index, false)};

    LLVMTypeRef struct_type = codegen_get_llvm_type(ctx, object_type);
    return LLVMBuildGEP2(ctx->builder, struct_type, object, indices, 2, "field");
}

LLVMValueRef codegen_expr_index(CodegenContext *ctx, AstNode *expr)
{
    LLVMValueRef array = codegen_expr(ctx, expr->index_expr.array);
    LLVMValueRef index = codegen_expr(ctx, expr->index_expr.index);

    if (!array || !index)
    {
        codegen_error(ctx, expr, "failed to generate array indexing operands");
        return NULL;
    }

    // load index if needed
    index = codegen_load_if_needed(ctx, index, expr->index_expr.index->type);

    // get array type
    Type *array_type = expr->index_expr.array->type;
    array_type       = type_resolve_alias(array_type);

    if (array_type->kind == TYPE_ARRAY)
    {
        // runtime bounds checking for all arrays (fat pointer)
        LLVMBasicBlockRef bounds_fail_block = LLVMAppendBasicBlockInContext(ctx->context, ctx->current_function, "bounds_fail");
        LLVMBasicBlockRef bounds_ok_block   = LLVMAppendBasicBlockInContext(ctx->context, ctx->current_function, "bounds_ok");

        // extract fat pointer fields
        LLVMValueRef fat_ptr  = codegen_load_if_needed(ctx, array, array_type);
        LLVMValueRef length   = LLVMBuildExtractValue(ctx->builder, fat_ptr, 1, "array_length");
        LLVMValueRef data_ptr = LLVMBuildExtractValue(ctx->builder, fat_ptr, 0, "array_data");

        LLVMValueRef bounds_violated;
        if (type_is_signed(expr->index_expr.index->type))
        {
            LLVMValueRef zero         = LLVMConstInt(LLVMTypeOf(index), 0, false);
            LLVMValueRef is_negative  = LLVMBuildICmp(ctx->builder, LLVMIntSLT, index, zero, "is_negative");
            LLVMValueRef index_ext    = LLVMBuildSExt(ctx->builder, index, LLVMTypeOf(length), "index_ext");
            LLVMValueRef is_too_large = LLVMBuildICmp(ctx->builder, LLVMIntSGE, index_ext, length, "is_too_large");
            bounds_violated           = LLVMBuildOr(ctx->builder, is_negative, is_too_large, "bounds_violated");
        }
        else
        {
            LLVMValueRef index_ext = LLVMBuildZExt(ctx->builder, index, LLVMTypeOf(length), "index_ext");
            bounds_violated        = LLVMBuildICmp(ctx->builder, LLVMIntUGE, index_ext, length, "bounds_violated");
        }

        LLVMBuildCondBr(ctx->builder, bounds_violated, bounds_fail_block, bounds_ok_block);

        // bounds fail: call abort
        LLVMPositionBuilderAtEnd(ctx->builder, bounds_fail_block);
        LLVMTypeRef  abort_type = LLVMFunctionType(LLVMVoidTypeInContext(ctx->context), NULL, 0, false);
        LLVMValueRef abort_func = LLVMGetNamedFunction(ctx->module, "abort");
        if (!abort_func)
        {
            abort_func = LLVMAddFunction(ctx->module, "abort", abort_type);
        }
        LLVMBuildCall2(ctx->builder, abort_type, abort_func, NULL, 0, "");
        LLVMBuildUnreachable(ctx->builder);

        // continue with bounds ok - do the actual indexing
        LLVMPositionBuilderAtEnd(ctx->builder, bounds_ok_block);
        LLVMTypeRef elem_type = codegen_get_llvm_type(ctx, array_type->array.elem_type);
        return LLVMBuildGEP2(ctx->builder, elem_type, data_ptr, &index, 1, "index");
    }
    else if (array_type->kind == TYPE_POINTER || array_type->kind == TYPE_PTR)
    {
        // pointer indexing - need to load the actual pointer value from the variable
        if (LLVMIsAAllocaInst(array) || LLVMIsAGlobalVariable(array))
        {
            LLVMTypeRef llvm_type = codegen_get_llvm_type(ctx, array_type);
            array                 = LLVMBuildLoad2(ctx->builder, llvm_type, array, "ptr_val");
        }
        LLVMTypeRef elem_type = codegen_get_llvm_type(ctx, array_type->pointer.base);
        return LLVMBuildGEP2(ctx->builder, elem_type, array, &index, 1, "index");
    }
    else
    {
        codegen_error(ctx, expr, "indexing non-array type");
        return NULL;
    }
}

LLVMValueRef codegen_expr_array(CodegenContext *ctx, AstNode *expr)
{
    Type *array_type = expr->type;

    // resolve type aliases to get the underlying array type
    Type *resolved_type = type_resolve_alias(array_type);
    if (!resolved_type)
        resolved_type = array_type;

    if (!resolved_type || resolved_type->kind != TYPE_ARRAY)
    {
        codegen_error(ctx, expr, "array literal has invalid type");
        return NULL;
    }

    Type       *elem_type      = resolved_type->array.elem_type;
    LLVMTypeRef llvm_elem_type = codegen_get_llvm_type(ctx, elem_type);
    if (!llvm_elem_type)
    {
        return NULL;
    }

    size_t      elem_count      = expr->array_expr.elems ? expr->array_expr.elems->count : 0;
    LLVMTypeRef llvm_array_type = LLVMArrayType(llvm_elem_type, elem_count);

    // generate element values
    LLVMValueRef *elem_values = malloc(sizeof(LLVMValueRef) * elem_count);
    for (size_t i = 0; i < elem_count; i++)
    {
        elem_values[i] = codegen_expr(ctx, expr->array_expr.elems->items[i]);
        if (!elem_values[i])
        {
            free(elem_values);
            return NULL;
        }
        elem_values[i] = codegen_load_if_needed(ctx, elem_values[i], expr->array_expr.elems->items[i]->type);
    }

    // create array constant
    LLVMValueRef array_const = LLVMConstArray(llvm_elem_type, elem_values, elem_count);

    // always create a global constant and return a fat pointer
    static int global_counter = 0;
    char       global_name[64];
    snprintf(global_name, sizeof(global_name), "array_literal_%d", global_counter++);

    LLVMValueRef global = LLVMAddGlobal(ctx->module, llvm_array_type, global_name);
    LLVMSetInitializer(global, array_const);
    LLVMSetGlobalConstant(global, !ctx->generating_mutable_init); // mutable if generating mutable init
    LLVMSetLinkage(global, LLVMPrivateLinkage);

    // create fat pointer struct {data, length}
    LLVMTypeRef  fat_ptr_type = codegen_get_llvm_type(ctx, array_type);
    LLVMValueRef fat_ptr      = LLVMGetUndef(fat_ptr_type);

    // set data pointer (cast global array to element pointer)
    LLVMValueRef indices[2] = {LLVMConstInt(LLVMInt32TypeInContext(ctx->context), 0, 0), LLVMConstInt(LLVMInt32TypeInContext(ctx->context), 0, 0)};
    LLVMValueRef data_ptr   = LLVMBuildGEP2(ctx->builder, llvm_array_type, global, indices, 2, "array_data");
    fat_ptr                 = LLVMBuildInsertValue(ctx->builder, fat_ptr, data_ptr, 0, "fat_ptr_data");

    // set length
    LLVMValueRef length = LLVMConstInt(LLVMInt64TypeInContext(ctx->context), elem_count, false);
    fat_ptr             = LLVMBuildInsertValue(ctx->builder, fat_ptr, length, 1, "fat_ptr_len");

    free(elem_values);
    return fat_ptr;
}

LLVMValueRef codegen_expr_struct(CodegenContext *ctx, AstNode *expr)
{
    Type *struct_type = expr->type;
    struct_type       = type_resolve_alias(struct_type);

    if (struct_type->kind != TYPE_STRUCT)
    {
        codegen_error(ctx, expr, "struct literal has non-struct type");
        return NULL;
    }

    LLVMTypeRef llvm_struct_type = codegen_get_llvm_type(ctx, struct_type);

    // create a temporary alloca for the struct
    LLVMValueRef struct_alloca = codegen_create_alloca(ctx, llvm_struct_type, "struct_lit");

    // initialize to zero
    LLVMBuildStore(ctx->builder, LLVMConstNull(llvm_struct_type), struct_alloca);

    // process field initializers if any
    if (expr->struct_expr.fields && expr->struct_expr.fields->count > 0)
    {
        for (int i = 0; i < expr->struct_expr.fields->count; i++)
        {
            AstNode *field_init = expr->struct_expr.fields->items[i];

            // field initializers are stored as field expressions
            if (field_init->kind == AST_EXPR_FIELD)
            {
                const char *field_name = field_init->field_expr.field;
                AstNode    *init_value = field_init->field_expr.object;

                // find the field
                Symbol *field_symbol = NULL;
                int     field_index  = 0;
                for (Symbol *field = struct_type->composite.fields; field; field = field->next)
                {
                    if (field->name && strcmp(field->name, field_name) == 0)
                    {
                        field_symbol = field;
                        break;
                    }
                    field_index++;
                }

                if (!field_symbol)
                {
                    codegen_error(ctx, field_init, "no field named '%s'", field_name);
                    return NULL;
                }

                // generate the initializer value
                LLVMValueRef value = codegen_expr(ctx, init_value);
                if (!value)
                {
                    codegen_error(ctx, field_init, "failed to generate field initializer");
                    return NULL;
                }

                value = codegen_load_if_needed(ctx, value, init_value->type);

                // generate GEP for field and store
                LLVMValueRef indices[] = {LLVMConstInt(LLVMInt32TypeInContext(ctx->context), 0, false), LLVMConstInt(LLVMInt32TypeInContext(ctx->context), field_index, false)};

                LLVMValueRef field_ptr = LLVMBuildGEP2(ctx->builder, llvm_struct_type, struct_alloca, indices, 2, "field_ptr");
                LLVMBuildStore(ctx->builder, value, field_ptr);
            }
        }
    }

    return struct_alloca;
}
