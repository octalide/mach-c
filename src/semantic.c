#include "semantic.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// builtin type registry
static struct
{
    const char *name;
    TypeKind    kind;
    unsigned    size;
    unsigned    alignment;
    bool        is_signed;
} builtin_types[] = {
    {"u8", TYPE_INT, 1, 1, false},
    {"u16", TYPE_INT, 2, 2, false},
    {"u32", TYPE_INT, 4, 4, false},
    {"u64", TYPE_INT, 8, 8, false},
    {"i8", TYPE_INT, 1, 1, true},
    {"i16", TYPE_INT, 2, 2, true},
    {"i32", TYPE_INT, 4, 4, true},
    {"i64", TYPE_INT, 8, 8, true},
    {"f16", TYPE_FLOAT, 2, 2, false},
    {"f32", TYPE_FLOAT, 4, 4, false},
    {"f64", TYPE_FLOAT, 8, 8, false},
    {"ptr", TYPE_PTR, 8, 8, false},
};

void semantic_analyzer_init(SemanticAnalyzer *analyzer, ModuleManager *manager)
{
    analyzer->global_scope = malloc(sizeof(SymbolTable));
    symbol_table_init(analyzer->global_scope, NULL);
    analyzer->current_scope                = analyzer->global_scope;
    analyzer->module_manager               = manager;
    analyzer->current_function_return_type = NULL;
    analyzer->had_error                    = false;
    analyzer->loop_depth                   = 0;
    semantic_error_list_init(&analyzer->errors);

    // register builtin types
    analyzer->builtin_count = sizeof(builtin_types) / sizeof(builtin_types[0]);
    analyzer->builtin_types = malloc(sizeof(Type *) * analyzer->builtin_count);

    for (int i = 0; i < analyzer->builtin_count; i++)
    {
        Type *type                 = type_create_builtin(builtin_types[i].kind, builtin_types[i].size, builtin_types[i].alignment, builtin_types[i].is_signed);
        analyzer->builtin_types[i] = type;
        symbol_table_declare(analyzer->global_scope, builtin_types[i].name, SYMBOL_TYPE, type, NULL);
    }
}

void semantic_analyzer_dnit(SemanticAnalyzer *analyzer)
{
    symbol_table_dnit(analyzer->global_scope);
    free(analyzer->global_scope);
    semantic_error_list_dnit(&analyzer->errors);

    for (int i = 0; i < analyzer->builtin_count; i++)
    {
        type_dnit(analyzer->builtin_types[i]);
    }
    free(analyzer->builtin_types);
}

void semantic_push_scope(SemanticAnalyzer *analyzer)
{
    SymbolTable *new_scope = malloc(sizeof(SymbolTable));
    symbol_table_init(new_scope, analyzer->current_scope);
    analyzer->current_scope = new_scope;
}

void semantic_pop_scope(SemanticAnalyzer *analyzer)
{
    if (analyzer->current_scope != analyzer->global_scope)
    {
        SymbolTable *old_scope  = analyzer->current_scope;
        analyzer->current_scope = old_scope->parent;
        symbol_table_dnit(old_scope);
        free(old_scope);
    }
}

void semantic_error(SemanticAnalyzer *analyzer, AstNode *node, const char *format, ...)
{
    va_list args;
    va_start(args, format);

    char buffer[512];
    vsnprintf(buffer, sizeof(buffer), format, args);

    semantic_error_list_add(&analyzer->errors, node, buffer);
    analyzer->had_error = true;

    va_end(args);
}

bool semantic_analyze_program(SemanticAnalyzer *analyzer, AstNode *program)
{
    if (!program || program->kind != AST_PROGRAM)
        return false;

    // analyze all declarations
    for (int i = 0; i < program->program.decls->count; i++)
    {
        if (!semantic_analyze_declaration(analyzer, program->program.decls->items[i]))
            return false;
    }

    return !analyzer->had_error;
}

bool semantic_analyze_module(SemanticAnalyzer *analyzer, Module *module)
{
    if (!module || !module->ast)
        return false;

    // create module's own symbol table if it doesn't have one
    if (!module->symbols)
    {
        module->symbols = malloc(sizeof(SymbolTable));
        symbol_table_init(module->symbols, analyzer->global_scope);
    }

    // temporarily switch to module's scope
    SymbolTable *prev_scope = analyzer->current_scope;
    analyzer->current_scope = module->symbols;

    bool result = semantic_analyze_program(analyzer, module->ast);

    // restore previous scope
    analyzer->current_scope = prev_scope;

    return result;
}

bool semantic_analyze_declaration(SemanticAnalyzer *analyzer, AstNode *decl)
{
    if (!decl)
        return false;

    switch (decl->kind)
    {
    case AST_USE_DECL:
        return semantic_analyze_use_decl(analyzer, decl);
    case AST_EXT_DECL:
        return semantic_analyze_ext_decl(analyzer, decl);
    case AST_DEF_DECL:
        return semantic_analyze_def_decl(analyzer, decl);
    case AST_VAL_DECL:
    case AST_VAR_DECL:
        return semantic_analyze_var_decl(analyzer, decl);
    case AST_FUN_DECL:
        return semantic_analyze_fun_decl(analyzer, decl);
    case AST_STR_DECL:
        return semantic_analyze_str_decl(analyzer, decl);
    case AST_UNI_DECL:
        return semantic_analyze_uni_decl(analyzer, decl);
    default:
        semantic_error(analyzer, decl, "Unknown declaration kind");
        return false;
    }
}

static void import_module_symbol(Symbol *symbol, void *data)
{
    SemanticAnalyzer *analyzer = (SemanticAnalyzer *)data;
    symbol_table_declare(analyzer->current_scope, symbol->name, symbol->kind, symbol->type, symbol->decl_node);
}

bool semantic_analyze_use_decl(SemanticAnalyzer *analyzer, AstNode *decl)
{
    Module *module = module_manager_find_module(analyzer->module_manager, decl->use_decl.module_path);
    if (!module)
    {
        semantic_error(analyzer, decl, "Module '%s' not found", decl->use_decl.module_path);
        return false;
    }

    // ensure module is analyzed
    if (!module->is_analyzed)
    {
        SemanticAnalyzer module_analyzer;
        semantic_analyzer_init(&module_analyzer, analyzer->module_manager);
        if (!semantic_analyze_module(&module_analyzer, module))
        {
            semantic_analyzer_dnit(&module_analyzer);
            return false;
        }
        module->is_analyzed = true;
        semantic_analyzer_dnit(&module_analyzer);
    }

    Symbol *sym = symbol_table_declare(analyzer->current_scope, decl->use_decl.alias ? decl->use_decl.alias : decl->use_decl.module_path, SYMBOL_MODULE, NULL, decl);

    if (!sym)
    {
        semantic_error(analyzer, decl, "Module name '%s' already declared", decl->use_decl.alias ? decl->use_decl.alias : decl->use_decl.module_path);
        return false;
    }

    sym->module               = module;
    decl->use_decl.module_sym = sym;

    // import symbols if unaliased
    if (!decl->use_decl.alias)
    {
        symbol_table_iterate(module->symbols, import_module_symbol, analyzer);
    }

    return true;
}

bool semantic_analyze_ext_decl(SemanticAnalyzer *analyzer, AstNode *decl)
{
    Type *type = semantic_analyze_type(analyzer, decl->ext_decl.type);
    if (!type)
        return false;

    Symbol *sym = symbol_table_declare(analyzer->current_scope, decl->ext_decl.name, type->kind == TYPE_FUNCTION ? SYMBOL_FUNC : SYMBOL_VAR, type, decl);

    if (!sym)
    {
        semantic_error(analyzer, decl, "External symbol '%s' already declared", decl->ext_decl.name);
        return false;
    }

    decl->symbol = sym;
    return true;
}

bool semantic_analyze_def_decl(SemanticAnalyzer *analyzer, AstNode *decl)
{
    Type *type = semantic_analyze_type(analyzer, decl->def_decl.type);
    if (!type)
        return false;

    Type *alias = type_create_alias(decl->def_decl.name, type);

    Symbol *sym = symbol_table_declare(analyzer->current_scope, decl->def_decl.name, SYMBOL_TYPE, alias, decl);

    if (!sym)
    {
        semantic_error(analyzer, decl, "Type '%s' already declared", decl->def_decl.name);
        type_dnit(alias);
        return false;
    }

    decl->symbol = sym;
    return true;
}

bool semantic_analyze_var_decl(SemanticAnalyzer *analyzer, AstNode *decl)
{
    Type *type       = NULL;
    bool  init_valid = true;

    // analyze initializer first to infer type if needed
    if (decl->var_decl.init)
    {
        Type *init_type = semantic_analyze_expression(analyzer, decl->var_decl.init);
        if (!init_type)
        {
            init_valid = false;
            // if we have an explicit type, continue with that
            if (!decl->var_decl.type)
            {
                semantic_error(analyzer, decl, "Variable '%s' requires type or initializer", decl->var_decl.name);
                return false;
            }
        }

        if (decl->var_decl.type)
        {
            type = semantic_analyze_type(analyzer, decl->var_decl.type);
            if (!type)
                return false;

            // only check compatibility if initializer is valid
            if (init_valid)
            {
                // first check basic type compatibility
                if (!type_is_assignable(type, init_type))
                {
                    semantic_error(analyzer, decl, "Cannot assign %s to %s", type_to_string(init_type), type_to_string(type));
                    init_valid = false;
                }
                // then check literal bounds if it's a literal and types are compatible
                else if (decl->var_decl.init->kind == AST_LIT_EXPR)
                {
                    if (!type_can_accept_literal(type, decl->var_decl.init))
                    {
                        semantic_error(analyzer, decl, "Literal value out of range for type %s", type_to_string(type));
                        init_valid = false;
                    }
                }
            }
        }
        else if (init_valid)
        {
            type = init_type;
        }
    }
    else if (decl->var_decl.type)
    {
        type = semantic_analyze_type(analyzer, decl->var_decl.type);
        if (!type)
            return false;
    }
    else
    {
        semantic_error(analyzer, decl, "Variable '%s' requires type or initializer", decl->var_decl.name);
        return false;
    }

    // add variable to symbol table even if initializer failed, as long as we have a valid type
    Symbol *sym = symbol_table_declare(analyzer->current_scope, decl->var_decl.name, SYMBOL_VAR, type, decl);

    if (!sym)
    {
        semantic_error(analyzer, decl, "Variable '%s' already declared", decl->var_decl.name);
        return false;
    }

    sym->is_val  = decl->var_decl.is_val;
    decl->symbol = sym;
    decl->type   = type;

    return init_valid;
}

bool semantic_analyze_fun_decl(SemanticAnalyzer *analyzer, AstNode *decl)
{
    // build function type
    Type **param_types = NULL;
    int    param_count = 0;

    if (decl->fun_decl.params)
    {
        param_count = decl->fun_decl.params->count;
        param_types = malloc(sizeof(Type *) * param_count);

        for (int i = 0; i < param_count; i++)
        {
            AstNode *param      = decl->fun_decl.params->items[i];
            Type    *param_type = semantic_analyze_type(analyzer, param->param_decl.type);
            if (!param_type)
            {
                free(param_types);
                return false;
            }
            param_types[i] = param_type;
        }
    }

    Type *return_type = NULL;
    if (decl->fun_decl.return_type)
    {
        return_type = semantic_analyze_type(analyzer, decl->fun_decl.return_type);
        if (!return_type)
        {
            free(param_types);
            return false;
        }
    }

    Type *fun_type = type_create_function(param_types, param_count, return_type);

    Symbol *sym = symbol_table_declare(analyzer->current_scope, decl->fun_decl.name, SYMBOL_FUNC, fun_type, decl);

    if (!sym)
    {
        semantic_error(analyzer, decl, "Function '%s' already declared", decl->fun_decl.name);
        type_dnit(fun_type);
        return false;
    }

    decl->symbol = sym;
    decl->type   = fun_type;

    // analyze function body
    if (decl->fun_decl.body)
    {
        semantic_push_scope(analyzer);

        // add parameters to scope
        for (int i = 0; i < param_count; i++)
        {
            AstNode *param      = decl->fun_decl.params->items[i];
            Type    *param_type = param_types[i];

            // follow type aliases to see if this is a function type
            Type *resolved_type = param_type;
            while (resolved_type->kind == TYPE_ALIAS)
                resolved_type = resolved_type->alias.target;

            // function parameters are stored as function pointers
            if (resolved_type->kind == TYPE_FUNCTION)
            {
                param_type = type_create_ptr(resolved_type);
            }

            Symbol *param_sym = symbol_table_declare(analyzer->current_scope, param->param_decl.name, SYMBOL_PARAM, param_type, param);
            param->symbol     = param_sym;
        }

        // set current function return type
        Type *prev_return_type                 = analyzer->current_function_return_type;
        analyzer->current_function_return_type = return_type;

        bool success = semantic_analyze_statement(analyzer, decl->fun_decl.body);

        analyzer->current_function_return_type = prev_return_type;
        semantic_pop_scope(analyzer);

        if (!success)
            return false;
    }

    return true;
}

bool semantic_analyze_str_decl(SemanticAnalyzer *analyzer, AstNode *decl)
{
    Type *str_type = type_create_struct(decl->str_decl.name);

    if (decl->str_decl.name)
    {
        Symbol *sym = symbol_table_declare(analyzer->current_scope, decl->str_decl.name, SYMBOL_TYPE, str_type, decl);

        if (!sym)
        {
            semantic_error(analyzer, decl, "Struct '%s' already declared", decl->str_decl.name);
            type_dnit(str_type);
            return false;
        }

        decl->symbol = sym;
    }

    // analyze fields
    if (decl->str_decl.fields)
    {
        int field_count                 = decl->str_decl.fields->count;
        str_type->composite.fields      = malloc(sizeof(Symbol *) * field_count);
        str_type->composite.field_count = field_count;

        unsigned offset    = 0;
        unsigned max_align = 1;

        for (int i = 0; i < field_count; i++)
        {
            AstNode *field      = decl->str_decl.fields->items[i];
            Type    *field_type = semantic_analyze_type(analyzer, field->field_decl.type);
            if (!field_type)
            {
                return false;
            }

            // create field symbol directly (not through symbol table)
            Symbol *field_sym    = malloc(sizeof(Symbol));
            field_sym->name      = strdup(field->field_decl.name);
            field_sym->kind      = SYMBOL_FIELD;
            field_sym->type      = field_type;
            field_sym->decl_node = field;
            field_sym->is_val    = false;
            field_sym->module    = NULL;

            // check for duplicate field names
            for (int j = 0; j < i; j++)
            {
                if (strcmp(str_type->composite.fields[j]->name, field_sym->name) == 0)
                {
                    semantic_error(analyzer, field, "Field '%s' already declared", field->field_decl.name);
                    free(field_sym->name);
                    free(field_sym);
                    return false;
                }
            }

            // align field
            unsigned align = field_type->alignment;
            offset         = (offset + align - 1) & ~(align - 1);

            field->symbol                 = field_sym;
            str_type->composite.fields[i] = field_sym;

            offset += field_type->size;
            if (align > max_align)
                max_align = align;
        }

        str_type->size      = (offset + max_align - 1) & ~(max_align - 1);
        str_type->alignment = max_align;
    }

    decl->type = str_type;
    return true;
}

bool semantic_analyze_uni_decl(SemanticAnalyzer *analyzer, AstNode *decl)
{
    Type *uni_type = type_create_union(decl->uni_decl.name);

    if (decl->uni_decl.name)
    {
        Symbol *sym = symbol_table_declare(analyzer->current_scope, decl->uni_decl.name, SYMBOL_TYPE, uni_type, decl);

        if (!sym)
        {
            semantic_error(analyzer, decl, "Union '%s' already declared", decl->uni_decl.name);
            type_dnit(uni_type);
            return false;
        }

        decl->symbol = sym;
    }

    // analyze fields
    if (decl->uni_decl.fields)
    {
        int field_count                 = decl->uni_decl.fields->count;
        uni_type->composite.fields      = malloc(sizeof(Symbol *) * field_count);
        uni_type->composite.field_count = field_count;

        unsigned max_size  = 0;
        unsigned max_align = 1;

        for (int i = 0; i < field_count; i++)
        {
            AstNode *field      = decl->uni_decl.fields->items[i];
            Type    *field_type = semantic_analyze_type(analyzer, field->field_decl.type);
            if (!field_type)
            {
                return false;
            }

            // create field symbol directly (not through symbol table)
            Symbol *field_sym    = malloc(sizeof(Symbol));
            field_sym->name      = strdup(field->field_decl.name);
            field_sym->kind      = SYMBOL_FIELD;
            field_sym->type      = field_type;
            field_sym->decl_node = field;
            field_sym->is_val    = false;
            field_sym->module    = NULL;

            // check for duplicate field names
            for (int j = 0; j < i; j++)
            {
                if (strcmp(uni_type->composite.fields[j]->name, field_sym->name) == 0)
                {
                    semantic_error(analyzer, field, "Field '%s' already declared", field->field_decl.name);
                    free(field_sym->name);
                    free(field_sym);
                    return false;
                }
            }

            field->symbol                 = field_sym;
            uni_type->composite.fields[i] = field_sym;

            if (field_type->size > max_size)
                max_size = field_type->size;
            if (field_type->alignment > max_align)
                max_align = field_type->alignment;
        }

        uni_type->size      = max_size;
        uni_type->alignment = max_align;
    }

    decl->type = uni_type;
    return true;
}

bool semantic_analyze_statement(SemanticAnalyzer *analyzer, AstNode *stmt)
{
    if (!stmt)
        return false;

    switch (stmt->kind)
    {
    case AST_BLOCK_STMT:
        return semantic_analyze_block_stmt(analyzer, stmt);
    case AST_EXPR_STMT:
        return semantic_analyze_expression(analyzer, stmt->expr_stmt.expr) != NULL;
    case AST_RET_STMT:
        return semantic_analyze_ret_stmt(analyzer, stmt);
    case AST_IF_STMT:
        return semantic_analyze_if_stmt(analyzer, stmt);
    case AST_FOR_STMT:
        return semantic_analyze_for_stmt(analyzer, stmt);
    case AST_BRK_STMT:
        return semantic_analyze_brk_stmt(analyzer, stmt);
    case AST_CNT_STMT:
        return semantic_analyze_cnt_stmt(analyzer, stmt);
    default:
        semantic_error(analyzer, stmt, "Unknown statement kind");
        return false;
    }
}

bool semantic_analyze_block_stmt(SemanticAnalyzer *analyzer, AstNode *stmt)
{
    semantic_push_scope(analyzer);

    bool success = true;
    for (int i = 0; i < stmt->block_stmt.stmts->count; i++)
    {
        AstNode *s = stmt->block_stmt.stmts->items[i];

        // handle declarations in block
        if (s->kind >= AST_USE_DECL && s->kind <= AST_PARAM_DECL)
        {
            if (!semantic_analyze_declaration(analyzer, s))
                success = false;
        }
        else
        {
            if (!semantic_analyze_statement(analyzer, s))
                success = false;
        }
    }

    semantic_pop_scope(analyzer);
    return success;
}

bool semantic_analyze_ret_stmt(SemanticAnalyzer *analyzer, AstNode *stmt)
{
    Type *ret_type = NULL;

    if (stmt->ret_stmt.expr)
    {
        ret_type = semantic_analyze_expression(analyzer, stmt->ret_stmt.expr);
        if (!ret_type)
            return false;
    }

    if (analyzer->current_function_return_type)
    {
        if (!ret_type)
        {
            semantic_error(analyzer, stmt, "Function expects return value of type %s", type_to_string(analyzer->current_function_return_type));
            return false;
        }

        if (!type_is_assignable(analyzer->current_function_return_type, ret_type))
        {
            semantic_error(analyzer, stmt, "Cannot return %s from function expecting %s", type_to_string(ret_type), type_to_string(analyzer->current_function_return_type));
            return false;
        }
    }
    else if (ret_type)
    {
        semantic_error(analyzer, stmt, "Function does not return a value");
        return false;
    }

    return true;
}

bool semantic_analyze_if_stmt(SemanticAnalyzer *analyzer, AstNode *stmt)
{
    Type *cond_type = semantic_analyze_expression(analyzer, stmt->if_stmt.cond);
    if (!cond_type)
        return false;

    if (!type_is_integer(cond_type))
    {
        semantic_error(analyzer, stmt->if_stmt.cond, "Condition must be integer type, got %s", type_to_string(cond_type));
        return false;
    }

    if (!semantic_analyze_statement(analyzer, stmt->if_stmt.then_stmt))
        return false;

    if (stmt->if_stmt.else_stmt)
    {
        if (!semantic_analyze_statement(analyzer, stmt->if_stmt.else_stmt))
            return false;
    }

    return true;
}

bool semantic_analyze_for_stmt(SemanticAnalyzer *analyzer, AstNode *stmt)
{
    if (stmt->for_stmt.cond)
    {
        Type *cond_type = semantic_analyze_expression(analyzer, stmt->for_stmt.cond);
        if (!cond_type)
            return false;

        if (!type_is_integer(cond_type))
        {
            semantic_error(analyzer, stmt->for_stmt.cond, "Loop condition must be integer type, got %s", type_to_string(cond_type));
            return false;
        }
    }

    analyzer->loop_depth++;
    bool success = semantic_analyze_statement(analyzer, stmt->for_stmt.body);
    analyzer->loop_depth--;

    return success;
}

bool semantic_analyze_brk_stmt(SemanticAnalyzer *analyzer, AstNode *stmt)
{
    if (analyzer->loop_depth == 0)
    {
        semantic_error(analyzer, stmt, "brk statement outside of loop");
        return false;
    }
    return true;
}

bool semantic_analyze_cnt_stmt(SemanticAnalyzer *analyzer, AstNode *stmt)
{
    if (analyzer->loop_depth == 0)
    {
        semantic_error(analyzer, stmt, "cnt statement outside of loop");
        return false;
    }
    return true;
}

Type *semantic_analyze_expression(SemanticAnalyzer *analyzer, AstNode *expr)
{
    if (!expr)
        return NULL;

    switch (expr->kind)
    {
    case AST_BINARY_EXPR:
        return semantic_analyze_binary_expr(analyzer, expr);
    case AST_UNARY_EXPR:
        return semantic_analyze_unary_expr(analyzer, expr);
    case AST_CALL_EXPR:
        return semantic_analyze_call_expr(analyzer, expr);
    case AST_INDEX_EXPR:
        return semantic_analyze_index_expr(analyzer, expr);
    case AST_FIELD_EXPR:
        return semantic_analyze_field_expr(analyzer, expr);
    case AST_CAST_EXPR:
        return semantic_analyze_cast_expr(analyzer, expr);
    case AST_IDENT_EXPR:
        return semantic_analyze_ident_expr(analyzer, expr);
    case AST_LIT_EXPR:
        return semantic_analyze_lit_expr(analyzer, expr);
    case AST_ARRAY_EXPR:
        return semantic_analyze_array_expr(analyzer, expr);
    case AST_STRUCT_EXPR:
        return semantic_analyze_struct_expr(analyzer, expr);
    default:
        semantic_error(analyzer, expr, "Unknown expression kind");
        return NULL;
    }
}

Type *semantic_analyze_binary_expr(SemanticAnalyzer *analyzer, AstNode *expr)
{
    Type *left_type = semantic_analyze_expression(analyzer, expr->binary_expr.left);
    if (!left_type)
        return NULL;

    Type *right_type = semantic_analyze_expression(analyzer, expr->binary_expr.right);
    if (!right_type)
        return NULL;

    switch (expr->binary_expr.op)
    {
    case TOKEN_EQUAL: // assignment
    {
        // check lvalue
        if (expr->binary_expr.left->kind != AST_IDENT_EXPR && expr->binary_expr.left->kind != AST_INDEX_EXPR && expr->binary_expr.left->kind != AST_FIELD_EXPR &&
            (expr->binary_expr.left->kind != AST_UNARY_EXPR || expr->binary_expr.left->unary_expr.op != TOKEN_AT))
        {
            semantic_error(analyzer, expr->binary_expr.left, "Cannot assign to non-lvalue");
            return NULL;
        }

        // check if assigning to val
        if (expr->binary_expr.left->kind == AST_IDENT_EXPR)
        {
            Symbol *sym = expr->binary_expr.left->symbol;
            if (sym && sym->is_val)
            {
                semantic_error(analyzer, expr, "Cannot assign to val '%s'", sym->name);
                return NULL;
            }
        }

        // first check basic type compatibility
        if (!type_is_assignable(left_type, right_type))
        {
            semantic_error(analyzer, expr, "Cannot assign %s to %s", type_to_string(right_type), type_to_string(left_type));
            return NULL;
        }

        // then check literal bounds if it's a literal and types are compatible
        if (expr->binary_expr.right->kind == AST_LIT_EXPR)
        {
            if (!type_can_accept_literal(left_type, expr->binary_expr.right))
            {
                semantic_error(analyzer, expr, "Literal value out of range for type %s", type_to_string(left_type));
                return NULL;
            }
        }

        expr->type = left_type;
        return left_type;
    }

    case TOKEN_PLUS:
    case TOKEN_MINUS:
    case TOKEN_STAR:
    case TOKEN_SLASH:
    case TOKEN_PERCENT:
    {
        if (!type_is_numeric(left_type) || !type_is_numeric(right_type))
        {
            semantic_error(analyzer, expr, "Arithmetic operations require numeric types, got %s and %s", type_to_string(left_type), type_to_string(right_type));
            return NULL;
        }

        Type *result_type = type_get_common_type(left_type, right_type);
        expr->type        = result_type;
        return result_type;
    }

    case TOKEN_AMPERSAND:
    case TOKEN_PIPE:
    case TOKEN_CARET:
    case TOKEN_LESS_LESS:
    case TOKEN_GREATER_GREATER:
    {
        if (!type_is_integer(left_type) || !type_is_integer(right_type))
        {
            semantic_error(analyzer, expr, "Bitwise operations require integer types, got %s and %s", type_to_string(left_type), type_to_string(right_type));
            return NULL;
        }

        Type *result_type = type_get_common_type(left_type, right_type);
        expr->type        = result_type;
        return result_type;
    }

    case TOKEN_AMPERSAND_AMPERSAND:
    case TOKEN_PIPE_PIPE:
    {
        if (!type_is_integer(left_type) || !type_is_integer(right_type))
        {
            semantic_error(analyzer, expr, "Logical operations require integer types, got %s and %s", type_to_string(left_type), type_to_string(right_type));
            return NULL;
        }

        // logical operations return u8
        expr->type = semantic_get_builtin_type(analyzer, "u8");
        return expr->type;
    }

    case TOKEN_EQUAL_EQUAL:
    case TOKEN_BANG_EQUAL:
    case TOKEN_LESS:
    case TOKEN_GREATER:
    case TOKEN_LESS_EQUAL:
    case TOKEN_GREATER_EQUAL:
    {
        if (!type_equals(left_type, right_type) && !(type_is_numeric(left_type) && type_is_numeric(right_type)))
        {
            semantic_error(analyzer, expr, "Cannot compare %s and %s", type_to_string(left_type), type_to_string(right_type));
            return NULL;
        }

        // comparison operations return u8
        expr->type = semantic_get_builtin_type(analyzer, "u8");
        return expr->type;
    }

    default:
        semantic_error(analyzer, expr, "Unknown binary operator");
        return NULL;
    }
}

Type *semantic_analyze_unary_expr(SemanticAnalyzer *analyzer, AstNode *expr)
{
    Type *operand_type = semantic_analyze_expression(analyzer, expr->unary_expr.expr);
    if (!operand_type)
        return NULL;

    switch (expr->unary_expr.op)
    {
    case TOKEN_MINUS:
    case TOKEN_PLUS:
    {
        if (!type_is_numeric(operand_type))
        {
            semantic_error(analyzer, expr, "Unary +/- requires numeric type, got %s", type_to_string(operand_type));
            return NULL;
        }
        expr->type = operand_type;
        return operand_type;
    }

    case TOKEN_TILDE:
    {
        if (!type_is_integer(operand_type))
        {
            semantic_error(analyzer, expr, "Bitwise NOT requires integer type, got %s", type_to_string(operand_type));
            return NULL;
        }
        expr->type = operand_type;
        return operand_type;
    }

    case TOKEN_BANG:
    {
        if (!type_is_integer(operand_type))
        {
            semantic_error(analyzer, expr, "Logical NOT requires integer type, got %s", type_to_string(operand_type));
            return NULL;
        }
        // logical not returns u8
        expr->type = semantic_get_builtin_type(analyzer, "u8");
        return expr->type;
    }

    case TOKEN_QUESTION: // address-of
    {
        Type *ptr_type = type_create_ptr(operand_type);
        expr->type     = ptr_type;
        return ptr_type;
    }

    case TOKEN_AT: // dereference
    {
        if (operand_type->kind != TYPE_PTR)
        {
            semantic_error(analyzer, expr, "Cannot dereference non-pointer type %s", type_to_string(operand_type));
            return NULL;
        }
        expr->type = operand_type->ptr.base;
        return operand_type->ptr.base;
    }

    default:
        semantic_error(analyzer, expr, "Unknown unary operator");
        return NULL;
    }
}

Type *semantic_analyze_call_expr(SemanticAnalyzer *analyzer, AstNode *expr)
{
    Type *func_type = semantic_analyze_expression(analyzer, expr->call_expr.func);
    if (!func_type)
        return NULL;

    // follow type aliases
    while (func_type->kind == TYPE_ALIAS)
        func_type = func_type->alias.target;

    // handle function pointers
    if (func_type->kind == TYPE_PTR && func_type->ptr.base && func_type->ptr.base->kind == TYPE_FUNCTION)
    {
        func_type = func_type->ptr.base;
    }
    else if (func_type->kind != TYPE_FUNCTION)
    {
        semantic_error(analyzer, expr, "Cannot call non-function type %s", type_to_string(func_type));
        return NULL;
    }

    // check builtin functions
    if (expr->call_expr.func->kind == AST_IDENT_EXPR)
    {
        const char *name = expr->call_expr.func->ident_expr.name;
        if (strcmp(name, "len") == 0 || strcmp(name, "size_of") == 0 || strcmp(name, "align_of") == 0 || strcmp(name, "offset_of") == 0)
        {
            return semantic_analyze_builtin_expr(analyzer, expr);
        }
    }

    // check argument count
    int arg_count = expr->call_expr.args ? expr->call_expr.args->count : 0;
    if (arg_count != func_type->function.param_count)
    {
        semantic_error(analyzer, expr, "Function expects %d arguments, got %d", func_type->function.param_count, arg_count);
        return NULL;
    }

    // check argument types
    for (int i = 0; i < arg_count; i++)
    {
        Type *arg_type = semantic_analyze_expression(analyzer, expr->call_expr.args->items[i]);
        if (!arg_type)
            return NULL;

        Type *param_type = func_type->function.param_types[i];

        // handle function argument decay to pointer
        if (param_type->kind == TYPE_PTR && param_type->ptr.base && param_type->ptr.base->kind == TYPE_FUNCTION)
        {
            // parameter expects function pointer, allow both function and function pointer
            if (arg_type->kind == TYPE_FUNCTION)
            {
                // function decays to pointer for parameter passing
                continue;
            }
            else if (arg_type->kind == TYPE_PTR && arg_type->ptr.base && arg_type->ptr.base->kind == TYPE_FUNCTION)
            {
                // check function pointer compatibility
                if (!type_equals(param_type->ptr.base, arg_type->ptr.base))
                {
                    semantic_error(analyzer, expr->call_expr.args->items[i], "Argument %d: incompatible function pointer types", i + 1);
                    return NULL;
                }
                continue;
            }
        }

        if (!type_is_assignable(param_type, arg_type))
        {
            semantic_error(analyzer, expr->call_expr.args->items[i], "Argument %d: cannot pass %s to parameter of type %s", i + 1, type_to_string(arg_type), type_to_string(param_type));
            return NULL;
        }
    }

    expr->type = func_type->function.return_type;
    return func_type->function.return_type;
}

Type *semantic_analyze_builtin_expr(SemanticAnalyzer *analyzer, AstNode *expr)
{
    const char *name      = expr->call_expr.func->ident_expr.name;
    int         arg_count = expr->call_expr.args ? expr->call_expr.args->count : 0;

    if (strcmp(name, "len") == 0)
    {
        if (arg_count != 1)
        {
            semantic_error(analyzer, expr, "len() expects 1 argument, got %d", arg_count);
            return NULL;
        }

        Type *arg_type = semantic_analyze_expression(analyzer, expr->call_expr.args->items[0]);
        if (!arg_type)
            return NULL;

        if (arg_type->kind != TYPE_ARRAY)
        {
            semantic_error(analyzer, expr, "len() requires array type, got %s", type_to_string(arg_type));
            return NULL;
        }

        expr->type = semantic_get_builtin_type(analyzer, "u32");
        return expr->type;
    }
    else if (strcmp(name, "size_of") == 0 || strcmp(name, "align_of") == 0)
    {
        if (arg_count != 1)
        {
            semantic_error(analyzer, expr, "%s() expects 1 argument, got %d", name, arg_count);
            return NULL;
        }

        // argument can be type or expression
        AstNode *arg      = expr->call_expr.args->items[0];
        Type    *arg_type = NULL;

        if (arg->kind >= AST_TYPE_NAME && arg->kind <= AST_TYPE_UNI)
        {
            arg_type = semantic_analyze_type(analyzer, arg);
        }
        else
        {
            arg_type = semantic_analyze_expression(analyzer, arg);
        }

        if (!arg_type)
            return NULL;

        expr->type = semantic_get_builtin_type(analyzer, "u32");
        return expr->type;
    }
    else if (strcmp(name, "offset_of") == 0)
    {
        if (arg_count != 1)
        {
            semantic_error(analyzer, expr, "offset_of() expects 1 argument, got %d", arg_count);
            return NULL;
        }

        // argument must be field access
        AstNode *arg = expr->call_expr.args->items[0];
        if (arg->kind != AST_FIELD_EXPR)
        {
            semantic_error(analyzer, expr, "offset_of() requires field access expression");
            return NULL;
        }

        // just need to validate the field access
        Type *field_type = semantic_analyze_field_expr(analyzer, arg);
        if (!field_type)
            return NULL;

        expr->type = semantic_get_builtin_type(analyzer, "u32");
        return expr->type;
    }

    return NULL;
}

Type *semantic_analyze_index_expr(SemanticAnalyzer *analyzer, AstNode *expr)
{
    Type *array_type = semantic_analyze_expression(analyzer, expr->index_expr.array);
    if (!array_type)
        return NULL;

    Type *index_type = semantic_analyze_expression(analyzer, expr->index_expr.index);
    if (!index_type)
        return NULL;

    if (!type_is_integer(index_type))
    {
        semantic_error(analyzer, expr->index_expr.index, "Array index must be integer type, got %s", type_to_string(index_type));
        return NULL;
    }

    if (array_type->kind == TYPE_ARRAY)
    {
        expr->type = array_type->array.elem_type;
        return array_type->array.elem_type;
    }
    else if (array_type->kind == TYPE_PTR)
    {
        expr->type = array_type->ptr.base;
        return array_type->ptr.base;
    }
    else
    {
        semantic_error(analyzer, expr, "Cannot index non-array/pointer type %s", type_to_string(array_type));
        return NULL;
    }
}

Type *semantic_analyze_field_expr(SemanticAnalyzer *analyzer, AstNode *expr)
{
    Type *object_type = semantic_analyze_expression(analyzer, expr->field_expr.object);
    if (!object_type)
        return NULL;

    // follow aliases
    while (object_type->kind == TYPE_ALIAS)
        object_type = object_type->alias.target;

    if (object_type->kind != TYPE_STRUCT && object_type->kind != TYPE_UNION)
    {
        semantic_error(analyzer, expr, "Cannot access field of non-struct/union type %s", type_to_string(object_type));
        return NULL;
    }

    // find field
    for (int i = 0; i < object_type->composite.field_count; i++)
    {
        Symbol *field = object_type->composite.fields[i];
        if (strcmp(field->name, expr->field_expr.field) == 0)
        {
            expr->type   = field->type;
            expr->symbol = field;
            return field->type;
        }
    }

    semantic_error(analyzer, expr, "No field '%s' in type %s", expr->field_expr.field, type_to_string(object_type));
    return NULL;
}

Type *semantic_analyze_cast_expr(SemanticAnalyzer *analyzer, AstNode *expr)
{
    Type *from_type = semantic_analyze_expression(analyzer, expr->cast_expr.expr);
    if (!from_type)
        return NULL;

    Type *to_type = semantic_analyze_type(analyzer, expr->cast_expr.type);
    if (!to_type)
        return NULL;

    if (!semantic_check_cast_validity(analyzer, from_type, to_type, expr))
        return NULL;

    expr->type = to_type;
    return to_type;
}

Type *semantic_analyze_ident_expr(SemanticAnalyzer *analyzer, AstNode *expr)
{
    Symbol *sym = symbol_table_lookup(analyzer->current_scope, expr->ident_expr.name);
    if (!sym)
    {
        semantic_error(analyzer, expr, "Undefined identifier '%s'", expr->ident_expr.name);
        return NULL;
    }

    expr->symbol = sym;

    if (sym->kind == SYMBOL_TYPE)
    {
        semantic_error(analyzer, expr, "Type '%s' used as value", expr->ident_expr.name);
        return NULL;
    }

    // function names decay to function pointers when used as values
    if (sym->kind == SYMBOL_FUNC)
    {
        Type *func_ptr_type = type_create_ptr(sym->type);
        expr->type          = func_ptr_type;
        return func_ptr_type;
    }

    expr->type = sym->type;
    return sym->type;
}

Type *semantic_analyze_lit_expr(SemanticAnalyzer *analyzer, AstNode *expr)
{
    switch (expr->lit_expr.kind)
    {
    case TOKEN_LIT_INT:
    {
        // default to i32 for integer literals
        expr->type = semantic_get_builtin_type(analyzer, "i32");
        return expr->type;
    }

    case TOKEN_LIT_FLOAT:
    {
        // default to f64 for float literals
        expr->type = semantic_get_builtin_type(analyzer, "f64");
        return expr->type;
    }

    case TOKEN_LIT_CHAR:
    {
        expr->type = semantic_get_builtin_type(analyzer, "u8");
        return expr->type;
    }

    case TOKEN_LIT_STRING:
    {
        // strings are []u8
        Type *char_type   = semantic_get_builtin_type(analyzer, "u8");
        Type *string_type = type_create_array(char_type, -1);
        expr->type        = string_type;
        return string_type;
    }

    default:
        semantic_error(analyzer, expr, "Unknown literal type");
        return NULL;
    }
}

Type *semantic_analyze_array_expr(SemanticAnalyzer *analyzer, AstNode *expr)
{
    Type *array_type = semantic_analyze_type(analyzer, expr->array_expr.type);
    if (!array_type)
        return NULL;

    // extract element type from the array type
    Type *elem_type = NULL;
    if (array_type->kind == TYPE_ARRAY)
    {
        elem_type = array_type->array.elem_type;
    }
    else
    {
        semantic_error(analyzer, expr, "Array expression must have array type, got %s", type_to_string(array_type));
        return NULL;
    }

    int elem_count = expr->array_expr.elems ? expr->array_expr.elems->count : 0;

    // check all elements match expected type
    for (int i = 0; i < elem_count; i++)
    {
        Type *elem_expr_type = semantic_analyze_expression(analyzer, expr->array_expr.elems->items[i]);
        if (!elem_expr_type)
            return NULL;

        AstNode *elem_node = expr->array_expr.elems->items[i];

        // special handling for literal values with bounds checking
        if (elem_node->kind == AST_LIT_EXPR)
        {
            if (!type_can_accept_literal(elem_type, elem_node))
            {
                semantic_error(analyzer, elem_node, "Array element %d: literal value out of range for type %s", i, type_to_string(elem_type));
                return NULL;
            }
        }
        else if (!type_is_assignable(elem_type, elem_expr_type))
        {
            semantic_error(analyzer, elem_node, "Array element %d: cannot assign %s to array of %s", i, type_to_string(elem_expr_type), type_to_string(elem_type));
            return NULL;
        }
    }

    // create array type with actual element count for unbound arrays
    Type *result_type = type_create_array(elem_type, elem_count);
    expr->type        = result_type;
    return result_type;
}

Type *semantic_analyze_struct_expr(SemanticAnalyzer *analyzer, AstNode *expr)
{
    Type *struct_type = semantic_analyze_type(analyzer, expr->struct_expr.type);
    if (!struct_type)
        return NULL;

    // follow aliases
    while (struct_type->kind == TYPE_ALIAS)
        struct_type = struct_type->alias.target;

    if (struct_type->kind != TYPE_STRUCT && struct_type->kind != TYPE_UNION)
    {
        semantic_error(analyzer, expr, "Cannot initialize non-struct/union type %s", type_to_string(struct_type));
        return NULL;
    }

    // TODO: validate field initializers

    expr->type = struct_type;
    return struct_type;
}

Type *semantic_analyze_type(SemanticAnalyzer *analyzer, AstNode *type_node)
{
    if (!type_node)
        return NULL;

    switch (type_node->kind)
    {
    case AST_TYPE_NAME:
    {
        Symbol *sym = symbol_table_lookup(analyzer->current_scope, type_node->type_name.name);
        if (!sym)
        {
            semantic_error(analyzer, type_node, "Unknown type '%s'", type_node->type_name.name);
            return NULL;
        }

        if (sym->kind != SYMBOL_TYPE)
        {
            semantic_error(analyzer, type_node, "'%s' is not a type", type_node->type_name.name);
            return NULL;
        }

        type_node->type = sym->type;
        return sym->type;
    }

    case AST_TYPE_PTR:
    {
        Type *base = NULL;
        if (type_node->type_ptr.base)
        {
            base = semantic_analyze_type(analyzer, type_node->type_ptr.base);
            if (!base)
                return NULL;
        }

        Type *ptr_type  = type_create_ptr(base);
        type_node->type = ptr_type;
        return ptr_type;
    }

    case AST_TYPE_ARRAY:
    {
        Type *elem_type = semantic_analyze_type(analyzer, type_node->type_array.elem_type);
        if (!elem_type)
            return NULL;

        int size = -1;
        if (type_node->type_array.size)
        {
            Type *size_type = semantic_analyze_expression(analyzer, type_node->type_array.size);
            if (!size_type)
                return NULL;

            if (!type_is_integer(size_type))
            {
                semantic_error(analyzer, type_node->type_array.size, "Array size must be integer");
                return NULL;
            }

            // TODO: evaluate constant expression
            if (type_node->type_array.size->kind == AST_LIT_EXPR)
            {
                size = (int)type_node->type_array.size->lit_expr.int_val;
            }
        }

        Type *array_type = type_create_array(elem_type, size);
        type_node->type  = array_type;
        return array_type;
    }

    case AST_TYPE_FUN:
    {
        Type **param_types = NULL;
        int    param_count = 0;

        if (type_node->type_fun.params)
        {
            param_count = type_node->type_fun.params->count;
            param_types = malloc(sizeof(Type *) * param_count);

            for (int i = 0; i < param_count; i++)
            {
                param_types[i] = semantic_analyze_type(analyzer, type_node->type_fun.params->items[i]);
                if (!param_types[i])
                {
                    free(param_types);
                    return NULL;
                }
            }
        }

        Type *return_type = NULL;
        if (type_node->type_fun.return_type)
        {
            return_type = semantic_analyze_type(analyzer, type_node->type_fun.return_type);
            if (!return_type)
            {
                free(param_types);
                return NULL;
            }
        }

        Type *fun_type  = type_create_function(param_types, param_count, return_type);
        type_node->type = fun_type;
        return fun_type;
    }

    case AST_TYPE_STR:
    {
        // handle inline struct type
        AstNode str_decl         = {0};
        str_decl.kind            = AST_STR_DECL;
        str_decl.str_decl.name   = type_node->type_str.name;
        str_decl.str_decl.fields = type_node->type_str.fields;

        if (!semantic_analyze_str_decl(analyzer, &str_decl))
            return NULL;

        type_node->type = str_decl.type;
        return str_decl.type;
    }

    case AST_TYPE_UNI:
    {
        // handle inline union type
        AstNode uni_decl         = {0};
        uni_decl.kind            = AST_UNI_DECL;
        uni_decl.uni_decl.name   = type_node->type_uni.name;
        uni_decl.uni_decl.fields = type_node->type_uni.fields;

        if (!semantic_analyze_uni_decl(analyzer, &uni_decl))
            return NULL;

        type_node->type = uni_decl.type;
        return uni_decl.type;
    }

    default:
        semantic_error(analyzer, type_node, "Unknown type kind");
        return NULL;
    }
}

Type *semantic_get_builtin_type(SemanticAnalyzer *analyzer, const char *name)
{
    Symbol *sym = symbol_table_lookup(analyzer->global_scope, name);
    if (sym && sym->kind == SYMBOL_TYPE)
        return sym->type;
    return NULL;
}

bool semantic_check_cast_validity(SemanticAnalyzer *analyzer, Type *from, Type *to, AstNode *node)
{
    if (!type_can_cast(from, to))
    {
        semantic_error(analyzer, node, "Cannot cast %s to %s", type_to_string(from), type_to_string(to));
        return false;
    }
    return true;
}

void semantic_error_list_init(SemanticErrorList *list)
{
    list->errors   = NULL;
    list->count    = 0;
    list->capacity = 0;
}

void semantic_error_list_dnit(SemanticErrorList *list)
{
    for (int i = 0; i < list->count; i++)
    {
        free(list->errors[i].message);
    }
    free(list->errors);
}

void semantic_error_list_add(SemanticErrorList *list, AstNode *node, const char *message)
{
    if (list->count >= list->capacity)
    {
        list->capacity = list->capacity ? list->capacity * 2 : 8;
        list->errors   = realloc(list->errors, sizeof(SemanticError) * list->capacity);
    }

    list->errors[list->count].node    = node;
    list->errors[list->count].message = strdup(message);
    list->count++;
}

void semantic_error_list_print(SemanticErrorList *list, Lexer *lexer, const char *file_path)
{
    for (int i = 0; i < list->count; i++)
    {
        SemanticError *err = &list->errors[i];
        if (err->node && err->node->token)
        {
            Token *token     = err->node->token;
            int    line      = lexer_get_pos_line(lexer, token->pos);
            int    col       = lexer_get_pos_line_offset(lexer, token->pos);
            char  *line_text = lexer_get_line_text(lexer, line);
            if (line_text == NULL)
            {
                line_text = strdup("unable to retrieve line text");
            }

            printf("error: %s\n", err->message);
            printf("%s:%d:%d\n", file_path, line + 1, col);
            printf("%5d | %-*s\n", line + 1, col > 1 ? col - 1 : 0, line_text);
            printf("      | %*s^\n", col - 1, "");

            free(line_text);
        }
        else
        {
            printf("error: %s\n", err->message);
            printf("%s:0:0\n", file_path);
        }
    }
}
