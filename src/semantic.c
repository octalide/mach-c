#include "semantic.h"
#include "config.h"
#include "lexer.h"
#include "module.h"
#include "token.h"
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// forward declarations
static Type *semantic_analyze_array_as_struct(SemanticAnalyzer *analyzer, AstNode *expr, Type *specified_type, Type *resolved_type);
static Type *semantic_analyze_null_expr(SemanticAnalyzer *analyzer, AstNode *expr, Type *expected_type);

void semantic_analyzer_init(SemanticAnalyzer *analyzer)
{
    symbol_table_init(&analyzer->symbol_table);
    module_manager_init(&analyzer->module_manager);
    semantic_error_list_init(&analyzer->errors);
    analyzer->current_function = NULL;
    analyzer->loop_depth       = 0;
    analyzer->has_errors       = false;
    analyzer->current_module_name = NULL;
}

void semantic_analyzer_dnit(SemanticAnalyzer *analyzer)
{
    symbol_table_dnit(&analyzer->symbol_table);
    module_manager_dnit(&analyzer->module_manager);
    semantic_error_list_dnit(&analyzer->errors);
    analyzer->current_module_name = NULL;
}

void semantic_analyzer_set_module(SemanticAnalyzer *analyzer, const char *module_name)
{
    if (!analyzer)
        return;

    analyzer->current_module_name = module_name;

    if (!module_name)
    {
        analyzer->symbol_table.module_scope  = NULL;
        analyzer->symbol_table.current_scope = analyzer->symbol_table.global_scope;
        return;
    }

    Symbol *module_symbol = symbol_find_module(&analyzer->symbol_table, module_name);
    if (!module_symbol)
    {
        module_symbol = symbol_create_module(module_name, module_name);
        symbol_add_module(&analyzer->symbol_table, module_symbol);
    }

    if (module_symbol->module.scope)
    {
        module_symbol->module.scope->parent = analyzer->symbol_table.global_scope;
        analyzer->symbol_table.module_scope = module_symbol->module.scope;
        scope_enter(&analyzer->symbol_table, analyzer->symbol_table.module_scope);
    }
    else
    {
        analyzer->symbol_table.module_scope  = NULL;
        analyzer->symbol_table.current_scope = analyzer->symbol_table.global_scope;
    }
}

static bool semantic_in_top_level_scope(SemanticAnalyzer *analyzer)
{
    if (!analyzer)
        return false;

    if (analyzer->symbol_table.current_scope == analyzer->symbol_table.global_scope)
        return true;

    if (analyzer->symbol_table.module_scope && analyzer->symbol_table.current_scope == analyzer->symbol_table.module_scope)
        return true;

    return false;
}

static bool scope_has_module_import(Scope *scope, const char *module_name)
{
    if (!scope || !module_name)
        return false;

    for (Symbol *sym = scope->symbols; sym; sym = sym->next)
    {
        if (sym->is_imported && sym->import_module && strcmp(sym->import_module, module_name) == 0)
        {
            return true;
        }
    }

    return false;
}

bool semantic_analyze(SemanticAnalyzer *analyzer, AstNode *root)
{
    if (!analyzer || !root)
        return false;

    // initialize type system
    type_system_init();

    // NOTE: Built-in intrinsic functions like len, size_of, align_of are handled
    // directly in both semantic analysis and codegen as special cases

    // resolve module dependencies first
    if (root->kind == AST_PROGRAM)
    {
        if (!module_manager_resolve_dependencies(&analyzer->module_manager, root, "."))
        {
            analyzer->has_errors = true;
            return false;
        }

        // first pass: process use statements to create module symbols
        for (int i = 0; i < root->program.stmts->count; i++)
        {
            AstNode *stmt = root->program.stmts->items[i];
            if (stmt->kind == AST_STMT_USE)
            {
                if (!semantic_analyze_use_stmt(analyzer, stmt))
                {
                    analyzer->has_errors = true;
                }
            }
        }

        // second pass: analyze imported modules now that module symbols exist
        for (int i = 0; i < root->program.stmts->count; i++)
        {
            AstNode *stmt = root->program.stmts->items[i];
            if (stmt->kind == AST_STMT_USE)
            {
                if (!semantic_analyze_imported_module(analyzer, stmt))
                {
                    analyzer->has_errors = true;
                }
            }
        }

        // third pass: analyze the current module statements
        for (int i = 0; i < root->program.stmts->count; i++)
        {
            if (!semantic_analyze_stmt(analyzer, root->program.stmts->items[i]))
            {
                analyzer->has_errors = true;
            }
        }
    }

    return !analyzer->has_errors;
}

void semantic_print_errors(SemanticAnalyzer *analyzer, Lexer *lexer, const char *file_path)
{
    if (analyzer->errors.count > 0)
    {
        semantic_error_list_print(&analyzer->errors, lexer, file_path);
    }
}

// analyze expression with optional expected type for better inference
Type *semantic_analyze_expr_with_hint(SemanticAnalyzer *analyzer, AstNode *expr, Type *expected_type)
{
    if (!analyzer || !expr)
        return NULL;

    switch (expr->kind)
    {
    case AST_EXPR_LIT:
        return semantic_analyze_lit_expr_with_hint(analyzer, expr, expected_type);
    case AST_EXPR_NULL:
        return semantic_analyze_null_expr(analyzer, expr, expected_type);
    case AST_EXPR_BINARY:
        return semantic_analyze_binary_expr(analyzer, expr);
    case AST_EXPR_UNARY:
        return semantic_analyze_unary_expr(analyzer, expr);
    case AST_EXPR_CALL:
        return semantic_analyze_call_expr(analyzer, expr);
    case AST_EXPR_INDEX:
        return semantic_analyze_index_expr(analyzer, expr);
    case AST_EXPR_FIELD:
        return semantic_analyze_field_expr(analyzer, expr);
    case AST_EXPR_CAST:
        return semantic_analyze_cast_expr(analyzer, expr);
    case AST_EXPR_IDENT:
        return semantic_analyze_ident_expr(analyzer, expr);
    case AST_EXPR_ARRAY:
        return semantic_analyze_array_expr(analyzer, expr);
    case AST_EXPR_STRUCT:
        return semantic_analyze_struct_expr(analyzer, expr);
    case AST_EXPR_VARARGS:
        if (!analyzer->current_function || !analyzer->current_function->symbol)
        {
            semantic_error(analyzer, expr, "'...' used outside a variadic function");
            return NULL;
        }

        if (!analyzer->current_function->fun_stmt.is_variadic)
        {
            semantic_error(analyzer, expr, "'...' used outside a variadic function");
            return NULL;
        }

        Symbol *current_func_symbol = analyzer->current_function->symbol;
        if (!current_func_symbol || current_func_symbol->kind != SYMBOL_FUNC || !current_func_symbol->func.uses_mach_varargs)
        {
            semantic_error(analyzer, expr, "'...' forwarding requires a Mach variadic function");
            return NULL;
        }

        expr->type = type_ptr();
        return expr->type;
    default:
        semantic_error(analyzer, expr, "unknown expression kind");
        return NULL;
    }
}

// helper function to format type names for error messages
static void format_type_name(Type *type, char *buffer, size_t buffer_size)
{
    if (!type || !buffer || buffer_size == 0)
    {
        if (buffer && buffer_size > 0)
            buffer[0] = '\0';
        return;
    }

    FILE *tmp = fmemopen(buffer, buffer_size - 1, "w");
    if (tmp)
    {
        FILE *old_stdout = stdout;
        stdout           = tmp;
        type_print(type);
        stdout = old_stdout;
        fclose(tmp);
        buffer[buffer_size - 1] = '\0'; // ensure null termination
    }
    else
    {
        buffer[0] = '\0';
    }
}

// helper function to resolve types with current symbol table
static Type *resolve_type(SemanticAnalyzer *analyzer, AstNode *type_node)
{
    if (!type_node)
        return NULL;

    // cache resolved type in the AST node
    if (type_node->type)
        return type_node->type;

    Type *resolved = type_resolve(type_node, &analyzer->symbol_table);
    if (resolved)
    {
        type_node->type = resolved;
    }
    return resolved;
}

bool semantic_analyze_stmt(SemanticAnalyzer *analyzer, AstNode *stmt)
{
    if (!analyzer || !stmt)
        return false;

    switch (stmt->kind)
    {
    case AST_STMT_USE:
        // use statements are processed in an earlier pass, skip here
        return true;
    case AST_STMT_EXT:
        return semantic_analyze_ext_stmt(analyzer, stmt);
    case AST_STMT_DEF:
        return semantic_analyze_def_stmt(analyzer, stmt);
    case AST_STMT_VAL:
    case AST_STMT_VAR:
        return semantic_analyze_var_stmt(analyzer, stmt);
    case AST_STMT_FUN:
        return semantic_analyze_fun_stmt(analyzer, stmt);
    case AST_STMT_STR:
        return semantic_analyze_str_stmt(analyzer, stmt);
    case AST_STMT_UNI:
        return semantic_analyze_uni_stmt(analyzer, stmt);
    case AST_STMT_IF:
        return semantic_analyze_if_stmt(analyzer, stmt);
    case AST_STMT_OR:
        return semantic_analyze_or_stmt(analyzer, stmt);
    case AST_STMT_FOR:
        return semantic_analyze_for_stmt(analyzer, stmt);
    case AST_STMT_RET:
        return semantic_analyze_ret_stmt(analyzer, stmt);
    case AST_STMT_BLOCK:
        return semantic_analyze_block_stmt(analyzer, stmt);
    case AST_STMT_EXPR:
        return semantic_analyze_expr(analyzer, stmt->expr_stmt.expr) != NULL;
    case AST_STMT_BRK:
    case AST_STMT_CNT:
        if (analyzer->loop_depth == 0)
        {
            semantic_error(analyzer, stmt, "%s statement not within a loop", stmt->kind == AST_STMT_BRK ? "break" : "continue");
            return false;
        }
        return true;
    case AST_STMT_ASM:
        // inline assembly currently unchecked
        return true;
    default:
        semantic_error(analyzer, stmt, "unknown statement kind");
        return false;
    }
}

bool semantic_analyze_use_stmt(SemanticAnalyzer *analyzer, AstNode *stmt)
{
    char *module_path = stmt->use_stmt.module_path;
    char *raw_path    = NULL;

    if (module_path)
    {
        raw_path = strdup(module_path);
        ProjectConfig *pc        = (ProjectConfig *)analyzer->module_manager.config;
        char          *canonical = config_expand_module_path(pc, module_path);
        if (!canonical)
        {
            semantic_error(analyzer, stmt, "module path '%s' could not be resolved", raw_path ? raw_path : module_path);
            free(raw_path);
            return false;
        }

        free(stmt->use_stmt.module_path);
        stmt->use_stmt.module_path = canonical;
        module_path                = canonical;
    }

    Module *module = module_manager_find_module(&analyzer->module_manager, module_path);
    if (!module)
    {
        semantic_error(analyzer, stmt, "module '%s' not found", module_path);
        free(raw_path);
        return false;
    }

    free(raw_path);
    return true;
}

static bool semantic_import_public_symbols(SemanticAnalyzer *analyzer, Module *module, AstNode *use_stmt)
{
    if (!module->symbols || !module->symbols->global_scope)
    {
        semantic_error(analyzer, use_stmt, "module '%s' has no symbols to import", use_stmt->use_stmt.module_path);
        return false;
    }

    if (scope_has_module_import(analyzer->symbol_table.current_scope, module->name))
    {
        return true;
    }

    Scope *export_scope = module->symbols->module_scope ? module->symbols->module_scope : module->symbols->global_scope;
    if (!export_scope)
    {
        semantic_error(analyzer, use_stmt, "module '%s' has no export scope", use_stmt->use_stmt.module_path);
        return false;
    }

    for (Symbol *sym = export_scope->symbols; sym; sym = sym->next)
    {
        if (sym->kind == SYMBOL_MODULE || sym->is_imported || !sym->is_public)
            continue;

        Symbol *existing = symbol_lookup_scope(analyzer->symbol_table.current_scope, sym->name);
        if (existing)
        {
            if (existing->is_imported && existing->import_module && strcmp(existing->import_module, module->name) == 0)
                continue;
            semantic_error(analyzer, use_stmt, "symbol '%s' from module '%s' conflicts with existing symbol", sym->name, use_stmt->use_stmt.module_path);
            return false;
        }

        Symbol *imported_sym = symbol_create(sym->kind, sym->name, sym->type, sym->decl);
        imported_sym->is_imported   = true;
        imported_sym->is_public     = sym->is_public;
        imported_sym->has_const_i64 = sym->has_const_i64;
        imported_sym->const_i64     = sym->const_i64;
        imported_sym->import_module = module->name;

        switch (sym->kind)
        {
        case SYMBOL_VAR:
        case SYMBOL_VAL:
            imported_sym->var = sym->var;
            break;
        case SYMBOL_FUNC:
            imported_sym->func = sym->func;
            if (sym->func.extern_name)
                imported_sym->func.extern_name = strdup(sym->func.extern_name);
            if (sym->func.convention)
                imported_sym->func.convention = strdup(sym->func.convention);
            if (sym->func.mangled_name)
                imported_sym->func.mangled_name = strdup(sym->func.mangled_name);
            break;
        case SYMBOL_TYPE:
            imported_sym->type_def = sym->type_def;
            break;
        case SYMBOL_FIELD:
            imported_sym->field = sym->field;
            break;
        case SYMBOL_PARAM:
            imported_sym->param = sym->param;
            break;
        case SYMBOL_MODULE:
            // modules are skipped above
            break;
        }

        symbol_add(analyzer->symbol_table.current_scope, imported_sym);
    }

    return true;
}

bool semantic_analyze_imported_module(SemanticAnalyzer *analyzer, AstNode *use_stmt)
{
    const char *module_path = use_stmt->use_stmt.module_path;

    Module *module = module_manager_find_module(&analyzer->module_manager, module_path);
    if (!module)
    {
        semantic_error(analyzer, use_stmt, "module '%s' not found", module_path);
        return false;
    }

    if (module->is_analyzed)
    {
        return semantic_import_public_symbols(analyzer, module, use_stmt);
    }

    if (module->ast && module->ast->kind == AST_PROGRAM)
    {
        bool extern_only = true;
        for (int i = 0; i < module->ast->program.stmts->count; i++)
        {
            AstNode *s = module->ast->program.stmts->items[i];
            if (s->kind != AST_STMT_EXT)
            {
                extern_only = false;
                break;
            }
        }

        if (extern_only)
        {
            if (module->symbols)
            {
                symbol_table_dnit(module->symbols);
                free(module->symbols);
            }
            module->symbols = malloc(sizeof(SymbolTable));
            symbol_table_init(module->symbols);

            SemanticAnalyzer tmp;
            semantic_analyzer_init(&tmp);
            semantic_analyzer_set_module(&tmp, module->name);
            tmp.symbol_table = *module->symbols;

            bool ok = true;
            for (int i = 0; i < module->ast->program.stmts->count; i++)
            {
                if (!semantic_analyze_ext_stmt(&tmp, module->ast->program.stmts->items[i]))
                {
                    ok = false;
                    break;
                }
            }

            *module->symbols = tmp.symbol_table;
            tmp.symbol_table.global_scope  = NULL;
            tmp.symbol_table.current_scope = NULL;
            tmp.symbol_table.module_scope  = NULL;
            semantic_analyzer_dnit(&tmp);

            if (!ok)
            {
                semantic_error(analyzer, use_stmt, "failed to analyze externs in module '%s'", module_path);
                return false;
            }

            module->is_analyzed = true;
            return semantic_import_public_symbols(analyzer, module, use_stmt);
        }
    }

    if (!module->symbols)
    {
        module->symbols = malloc(sizeof(SymbolTable));
        symbol_table_init(module->symbols);
    }

    SemanticAnalyzer module_analyzer;
    semantic_analyzer_init(&module_analyzer);
    module_manager_set_config(&module_analyzer.module_manager, analyzer->module_manager.config, analyzer->module_manager.project_dir);
    semantic_analyzer_set_module(&module_analyzer, module->name);
    for (int i = 0; i < analyzer->module_manager.search_count; i++)
        module_manager_add_search_path(&module_analyzer.module_manager, analyzer->module_manager.search_paths[i]);
    for (int i = 0; i < analyzer->module_manager.alias_count; i++)
        module_manager_add_alias(&module_analyzer.module_manager, analyzer->module_manager.alias_names[i], analyzer->module_manager.alias_paths[i]);

    bool success = true;
    if (module->ast && module->ast->kind == AST_PROGRAM)
    {
        if (!module_manager_resolve_dependencies(&module_analyzer.module_manager, module->ast, "."))
        {
            success = false;
        }
        else
        {
            for (int i = 0; i < module->ast->program.stmts->count; i++)
            {
                AstNode *stmt = module->ast->program.stmts->items[i];
                if (stmt->kind == AST_STMT_USE)
                {
                    if (!semantic_analyze_use_stmt(&module_analyzer, stmt))
                    {
                        success = false;
                    }
                }
            }

            for (int i = 0; success && i < module->ast->program.stmts->count; i++)
            {
                AstNode *stmt = module->ast->program.stmts->items[i];
                if (stmt->kind == AST_STMT_USE)
                {
                    if (!semantic_analyze_imported_module(&module_analyzer, stmt))
                    {
                        success = false;
                    }
                }
            }

            for (int i = 0; success && i < module->ast->program.stmts->count; i++)
            {
                AstNode *stmt = module->ast->program.stmts->items[i];
                if (stmt->kind == AST_STMT_USE)
                    continue;
                if (!semantic_analyze_stmt(&module_analyzer, stmt))
                    success = false;
            }
        }
    }

    if (success)
    {
        if (module->symbols)
        {
            symbol_table_dnit(module->symbols);
            free(module->symbols);
        }
        module->symbols  = malloc(sizeof(SymbolTable));
        *module->symbols = module_analyzer.symbol_table;
        module_analyzer.symbol_table.global_scope  = NULL;
        module_analyzer.symbol_table.current_scope = NULL;
        module_analyzer.symbol_table.module_scope  = NULL;

        module->is_analyzed = true;
        success             = semantic_import_public_symbols(analyzer, module, use_stmt);
    }
    else
    {
        semantic_error(analyzer, use_stmt, "failed to analyze module '%s'", module_path);
    }

    for (int i = 0; i < module_analyzer.errors.count; i++)
    {
        semantic_error_list_add(&analyzer->errors, module_analyzer.errors.errors[i].token, module_analyzer.errors.errors[i].message, module->file_path);
    }

    for (int i = 0; i < module_analyzer.module_manager.capacity; i++)
    {
        Module *m = module_analyzer.module_manager.modules[i];
        while (m)
        {
            m->symbols = NULL;
            m->ast     = NULL;
            m          = m->next;
        }
    }

    semantic_error_list_dnit(&module_analyzer.errors);
    module_manager_dnit(&module_analyzer.module_manager);

    return success;
}

bool semantic_analyze_ext_stmt(SemanticAnalyzer *analyzer, AstNode *stmt)
{
    const char *name = stmt->ext_stmt.name;
    Type       *type = resolve_type(analyzer, stmt->ext_stmt.type);

    if (!type)
    {
        semantic_error(analyzer, stmt, "cannot resolve type for external function '%s'", name);
        return false;
    }

    if (type->kind != TYPE_FUNCTION)
    {
        semantic_error(analyzer, stmt, "external declaration '%s' must be a function type", name);
        return false;
    }

    // check for redefinition
    Symbol *existing = symbol_lookup_scope(analyzer->symbol_table.current_scope, name);
    if (existing)
    {
        semantic_error(analyzer, stmt, "redefinition of '%s'", name);
        return false;
    }

    Symbol *symbol           = symbol_create(SYMBOL_FUNC, name, type, stmt);
    symbol->func.is_external = true;
    if (stmt->ext_stmt.symbol)
        symbol->func.extern_name = strdup(stmt->ext_stmt.symbol);
    if (stmt->ext_stmt.convention)
        symbol->func.convention = strdup(stmt->ext_stmt.convention);
    if (semantic_in_top_level_scope(analyzer) && stmt->ext_stmt.is_public)
    {
        symbol->is_public = true;
    }
    symbol_add(analyzer->symbol_table.current_scope, symbol);
    stmt->symbol = symbol;

    return true;
}

bool semantic_analyze_def_stmt(SemanticAnalyzer *analyzer, AstNode *stmt)
{
    const char *name        = stmt->def_stmt.name;
    Type       *target_type = resolve_type(analyzer, stmt->def_stmt.type);

    if (!target_type)
    {
        semantic_error(analyzer, stmt, "cannot resolve target type for alias '%s'", name);
        return false;
    }

    // check for redefinition
    Symbol *existing = symbol_lookup_scope(analyzer->symbol_table.current_scope, name);
    if (existing)
    {
        semantic_error(analyzer, stmt, "redefinition of type '%s'", name);
        return false;
    }

    Type   *alias_type        = type_alias_create(name, target_type);
    Symbol *symbol            = symbol_create(SYMBOL_TYPE, name, alias_type, stmt);
    symbol->type_def.is_alias = true;
    if (semantic_in_top_level_scope(analyzer) && stmt->def_stmt.is_public)
    {
        symbol->is_public = true;
    }
    symbol_add(analyzer->symbol_table.current_scope, symbol);
    stmt->symbol = symbol;
    stmt->type   = alias_type;

    return true;
}

bool semantic_analyze_var_stmt(SemanticAnalyzer *analyzer, AstNode *stmt)
{
    const char *name   = stmt->var_stmt.name;
    bool        is_val = stmt->var_stmt.is_val;

    // resolve explicit type if given
    Type *explicit_type = NULL;
    if (stmt->var_stmt.type)
    {
        explicit_type = resolve_type(analyzer, stmt->var_stmt.type);
        if (!explicit_type)
        {
            semantic_error(analyzer, stmt, "cannot resolve type for variable '%s'", name);
            return false;
        }
    }

    // analyze initializer with type hint
    Type *init_type = NULL;
    if (stmt->var_stmt.init)
    {
        init_type = semantic_analyze_expr_with_hint(analyzer, stmt->var_stmt.init, explicit_type);
        if (!init_type)
        {
            semantic_error(analyzer, stmt, "invalid initializer for variable '%s'", name);
            return false;
        }
    }

    // determine final type
    Type *final_type = NULL;
    if (explicit_type && init_type)
    {
        if (!semantic_check_assignment(analyzer, explicit_type, init_type, stmt))
        {
            semantic_error(analyzer, stmt, "cannot assign %s to %s", type_to_string(init_type), type_to_string(explicit_type));
            return false;
        }
        final_type = explicit_type;
    }
    else if (explicit_type)
    {
        final_type = explicit_type;
    }
    else if (init_type)
    {
        final_type = init_type;
    }
    else
    {
        semantic_error(analyzer, stmt, "variable '%s' has no type or initializer", name);
        return false;
    }

    // check for redefinition
    Symbol *existing = symbol_lookup_scope(analyzer->symbol_table.current_scope, name);
    if (existing)
    {
        semantic_error(analyzer, stmt, "redefinition of '%s'", name);
        return false;
    }

    // create symbol
    SymbolKind kind       = is_val ? SYMBOL_VAL : SYMBOL_VAR;
    Symbol    *symbol     = symbol_create(kind, name, final_type, stmt);
    symbol->var.is_global = semantic_in_top_level_scope(analyzer);
    symbol->var.is_const  = is_val;
    if (symbol->var.is_global && stmt->var_stmt.is_public)
    {
        symbol->is_public = true;
    }

    symbol_add(analyzer->symbol_table.current_scope, symbol);
    stmt->symbol = symbol;
    stmt->type   = final_type;

    return true;
}

static bool stmt_has_return(AstNode *stmt)
{
    if (!stmt)
        return false;

    switch (stmt->kind)
    {
    case AST_STMT_RET:
        return true;

    case AST_STMT_BLOCK:
        if (stmt->block_stmt.stmts)
        {
            for (int i = 0; i < stmt->block_stmt.stmts->count; i++)
            {
                if (stmt_has_return(stmt->block_stmt.stmts->items[i]))
                    return true;
            }
        }
        return false;

    case AST_STMT_IF:
        if (!stmt_has_return(stmt->cond_stmt.body))
            return false;
        // walk chained or branches
        AstNode *or_node = stmt->cond_stmt.stmt_or;
        while (or_node)
        {
            if (!stmt_has_return(or_node->cond_stmt.body))
                return false;
            or_node = or_node->cond_stmt.stmt_or;
        }
        return true;

    case AST_STMT_OR:
        if (!stmt_has_return(stmt->cond_stmt.body))
            return false;
        if (stmt->cond_stmt.stmt_or)
            return stmt_has_return(stmt->cond_stmt.stmt_or);
        return true;

    default:
        return false;
    }
}

bool semantic_analyze_fun_stmt(SemanticAnalyzer *analyzer, AstNode *stmt)
{
    const char *name = stmt->fun_stmt.name;

    // resolve return type
    Type *return_type = NULL;
    if (stmt->fun_stmt.return_type)
    {
        return_type = resolve_type(analyzer, stmt->fun_stmt.return_type);
        if (!return_type)
        {
            semantic_error(analyzer, stmt, "cannot resolve return type for function '%s'", name);
            return false;
        }
    }

    // resolve parameter types
    size_t original_param_nodes = stmt->fun_stmt.params ? stmt->fun_stmt.params->count : 0;
    size_t fixed_param_count    = 0;
    // first pass: count fixed params (ignore variadic sentinel)
    for (size_t i = 0; i < original_param_nodes; i++)
    {
        AstNode *param = stmt->fun_stmt.params->items[i];
        if (!param->param_stmt.is_variadic)
            fixed_param_count++;
    }

    Type **param_types = NULL;
    if (fixed_param_count > 0)
        param_types = malloc(sizeof(Type *) * fixed_param_count);

    size_t idx = 0;
    for (size_t i = 0; i < original_param_nodes; i++)
    {
        AstNode *param = stmt->fun_stmt.params->items[i];
        if (param->param_stmt.is_variadic)
            continue;
        param_types[idx] = resolve_type(analyzer, param->param_stmt.type);
        if (!param_types[idx])
        {
            semantic_error(analyzer, param, "cannot resolve type for parameter '%s'", param->param_stmt.name);
            free(param_types);
            return false;
        }
        param->type = param_types[idx];
        idx++;
    }

    size_t param_count = fixed_param_count;

    bool  is_variadic = stmt->fun_stmt.is_variadic;
    Type *func_type   = type_function_create(return_type, param_types, param_count, is_variadic);
    free(param_types);

    // check for existing declaration
    Symbol *existing = symbol_lookup_scope(analyzer->symbol_table.current_scope, name);
    if (existing)
    {
        if (existing->kind != SYMBOL_FUNC)
        {
            semantic_error(analyzer, stmt, "redefinition of '%s' as different kind", name);
            return false;
        }
        if (!type_equals(existing->type, func_type))
        {
            semantic_error(analyzer, stmt, "conflicting types for function '%s'", name);
            return false;
        }
        if (stmt->fun_stmt.body && existing->func.is_defined)
        {
            semantic_error(analyzer, stmt, "redefinition of function '%s'", name);
            return false;
        }
        // update existing symbol
        if (stmt->fun_stmt.body)
        {
            existing->func.is_defined = true;
            existing->decl            = stmt;
        }
        existing->func.uses_mach_varargs = is_variadic;
        if (semantic_in_top_level_scope(analyzer) && stmt->fun_stmt.is_public)
        {
            existing->is_public = true;
        }
        stmt->symbol = existing;
        stmt->type   = func_type;
    }
    else
    {
        // create new symbol
        Symbol *symbol           = symbol_create(SYMBOL_FUNC, name, func_type, stmt);
        symbol->func.is_external         = false;
        symbol->func.is_defined          = (stmt->fun_stmt.body != NULL);
        symbol->func.uses_mach_varargs   = is_variadic;
        if (semantic_in_top_level_scope(analyzer) && stmt->fun_stmt.is_public)
        {
            symbol->is_public = true;
        }
        symbol_add(analyzer->symbol_table.current_scope, symbol);
        stmt->symbol = symbol;
        stmt->type   = func_type;
        existing     = symbol;
    }

    // analyze function body if present
    if (stmt->fun_stmt.body)
    {
        // create function scope
        Scope *func_scope = scope_push(&analyzer->symbol_table, name);

        // add parameters to function scope
        if (stmt->fun_stmt.params)
        {
            size_t param_index = 0;
            for (int i = 0; i < stmt->fun_stmt.params->count; i++)
            {
                AstNode *param = stmt->fun_stmt.params->items[i];
                if (param->param_stmt.is_variadic)
                    continue; // variadic sentinel has no symbol
                Symbol *param_symbol         = symbol_create(SYMBOL_PARAM, param->param_stmt.name, param->type, param);
                param_symbol->param.index    = param_index++;
                symbol_add(func_scope, param_symbol);
                param->symbol = param_symbol;
            }
        }

        // set current function for return type checking
        AstNode *prev_function     = analyzer->current_function;
        analyzer->current_function = stmt;

        // analyze body
        bool success = semantic_analyze_stmt(analyzer, stmt->fun_stmt.body);

        // check for missing return
        if (return_type && !stmt_has_return(stmt->fun_stmt.body))
        {
            semantic_error(analyzer, stmt, "function '%s' with return type must have a return statement", name);
            analyzer->has_errors = true;
            return false;
        }

        // restore previous function
        analyzer->current_function = prev_function;

        scope_pop(&analyzer->symbol_table);

        return success;
    }

    return true;
}

bool semantic_analyze_str_stmt(SemanticAnalyzer *analyzer, AstNode *stmt)
{
    const char *name = stmt->str_stmt.name;

    // check for redefinition
    Symbol *existing = symbol_lookup_scope(analyzer->symbol_table.current_scope, name);
    if (existing)
    {
        semantic_error(analyzer, stmt, "redefinition of struct '%s'", name);
        return false;
    }

    Type   *struct_type   = type_struct_create(name);
    Symbol *struct_symbol = symbol_create(SYMBOL_TYPE, name, struct_type, stmt);
    if (semantic_in_top_level_scope(analyzer) && stmt->str_stmt.is_public)
    {
        struct_symbol->is_public = true;
    }
    symbol_add(analyzer->symbol_table.current_scope, struct_symbol);

    // analyze fields
    if (stmt->str_stmt.fields)
    {
        for (int i = 0; i < stmt->str_stmt.fields->count; i++)
        {
            AstNode *field      = stmt->str_stmt.fields->items[i];
            Type    *field_type = resolve_type(analyzer, field->field_stmt.type);

            if (!field_type)
            {
                semantic_error(analyzer, field, "cannot resolve type for field '%s'", field->field_stmt.name);
                continue;
            }

            symbol_add_field(struct_symbol, field->field_stmt.name, field_type, field);
        }
    }

    // calculate layout
    symbol_calculate_struct_layout(struct_symbol);

    stmt->symbol = struct_symbol;
    stmt->type   = struct_type;

    return true;
}

bool semantic_analyze_uni_stmt(SemanticAnalyzer *analyzer, AstNode *stmt)
{
    const char *name = stmt->uni_stmt.name;

    // check for redefinition
    Symbol *existing = symbol_lookup_scope(analyzer->symbol_table.current_scope, name);
    if (existing)
    {
        semantic_error(analyzer, stmt, "redefinition of union '%s'", name);
        return false;
    }

    Type   *union_type   = type_union_create(name);
    Symbol *union_symbol = symbol_create(SYMBOL_TYPE, name, union_type, stmt);
    if (semantic_in_top_level_scope(analyzer) && stmt->uni_stmt.is_public)
    {
        union_symbol->is_public = true;
    }
    symbol_add(analyzer->symbol_table.current_scope, union_symbol);

    // analyze fields
    if (stmt->uni_stmt.fields)
    {
        for (int i = 0; i < stmt->uni_stmt.fields->count; i++)
        {
            AstNode *field      = stmt->uni_stmt.fields->items[i];
            Type    *field_type = resolve_type(analyzer, field->field_stmt.type);

            if (!field_type)
            {
                semantic_error(analyzer, field, "cannot resolve type for field '%s'", field->field_stmt.name);
                continue;
            }

            symbol_add_field(union_symbol, field->field_stmt.name, field_type, field);
        }
    }

    // calculate layout
    symbol_calculate_union_layout(union_symbol);

    stmt->symbol = union_symbol;
    stmt->type   = union_type;

    return true;
}

bool semantic_analyze_if_stmt(SemanticAnalyzer *analyzer, AstNode *stmt)
{
    // analyze condition
    Type *cond_type = semantic_analyze_expr(analyzer, stmt->cond_stmt.cond);
    if (!cond_type)
    {
        semantic_error(analyzer, stmt, "invalid condition in if statement");
        return false;
    }

    // condition should be truthy (numeric or pointer-like)
    if (!type_is_truthy(cond_type))
    {
        semantic_error(analyzer, stmt, "condition must be numeric or pointer-like");
        return false;
    }

    // analyze body
    bool success = semantic_analyze_stmt(analyzer, stmt->cond_stmt.body);

    // analyze or clause if present
    if (stmt->cond_stmt.stmt_or)
    {
        success &= semantic_analyze_stmt(analyzer, stmt->cond_stmt.stmt_or);
    }

    return success;
}

bool semantic_analyze_or_stmt(SemanticAnalyzer *analyzer, AstNode *stmt)
{
    bool success = true;

    // else-branch: 'or { ... }' has no condition
    if (stmt->cond_stmt.cond)
    {
        Type *cond_type = semantic_analyze_expr(analyzer, stmt->cond_stmt.cond);
        if (!cond_type)
        {
            semantic_error(analyzer, stmt, "invalid condition in or statement");
            success = false;
        }
        else if (!type_is_truthy(cond_type))
        {
            semantic_error(analyzer, stmt, "condition must be numeric or pointer-like");
            success = false;
        }
    }

    // analyze body
    if (success)
    {
        success &= semantic_analyze_stmt(analyzer, stmt->cond_stmt.body);
    }
    else
    {
        // even if condition failed, attempt to analyze body to surface nested errors
        semantic_analyze_stmt(analyzer, stmt->cond_stmt.body);
    }

    if (stmt->cond_stmt.stmt_or)
    {
        if (!semantic_analyze_stmt(analyzer, stmt->cond_stmt.stmt_or))
        {
            success = false;
        }
    }

    return success;
}

bool semantic_analyze_for_stmt(SemanticAnalyzer *analyzer, AstNode *stmt)
{
    bool success = true;

    // analyze condition if present
    if (stmt->for_stmt.cond)
    {
        Type *cond_type = semantic_analyze_expr(analyzer, stmt->for_stmt.cond);
        if (!cond_type)
        {
            semantic_error(analyzer, stmt, "invalid condition in for loop");
            success = false;
        }
        else if (!type_is_truthy(cond_type))
        {
            semantic_error(analyzer, stmt, "loop condition must be numeric or pointer-like");
            success = false;
        }
    }

    // enter loop context
    analyzer->loop_depth++;

    // analyze body
    success &= semantic_analyze_stmt(analyzer, stmt->for_stmt.body);

    // exit loop context
    analyzer->loop_depth--;

    return success;
}

bool semantic_analyze_ret_stmt(SemanticAnalyzer *analyzer, AstNode *stmt)
{
    if (!analyzer->current_function)
    {
        semantic_error(analyzer, stmt, "return statement outside function");
        return false;
    }

    Type *expected_return = analyzer->current_function->type->function.return_type;

    if (stmt->ret_stmt.expr)
    {
        // use expected return type as hint for better literal inference
        if (expected_return == NULL)
        {
            semantic_error(analyzer, stmt, "cannot return a value from a void function");
            return false;
        }
        Type *actual_return = semantic_analyze_expr_with_hint(analyzer, stmt->ret_stmt.expr, expected_return);
        if (!actual_return)
        {
            semantic_error(analyzer, stmt, "invalid return expression");
            return false;
        }

        if (!semantic_check_assignment(analyzer, expected_return, actual_return, stmt))
        {
            return false;
        }
    }
    else
    {
        // no return expression
        if (expected_return != NULL)
        {
            semantic_error(analyzer, stmt, "function must return a value");
            return false;
        }
    }

    return true;
}

bool semantic_analyze_block_stmt(SemanticAnalyzer *analyzer, AstNode *stmt)
{
    // create new scope for block
    scope_push(&analyzer->symbol_table, "block");

    bool success = true;
    if (stmt->block_stmt.stmts)
    {
        for (int i = 0; i < stmt->block_stmt.stmts->count; i++)
        {
            if (!semantic_analyze_stmt(analyzer, stmt->block_stmt.stmts->items[i]))
            {
                success = false;
            }
        }
    }

    scope_pop(&analyzer->symbol_table);

    return success;
}

// continue with expression analysis in next part due to length...

Type *semantic_analyze_expr(SemanticAnalyzer *analyzer, AstNode *expr)
{
    return semantic_analyze_expr_with_hint(analyzer, expr, NULL);
}

static Type *type_promote_binary(Type *left, Type *right)
{
    // if either is float, promote to float
    if (type_is_float(left) || type_is_float(right))
    {
        if (left->kind == TYPE_F64 || right->kind == TYPE_F64)
            return type_f64();
        if (left->kind == TYPE_F32 || right->kind == TYPE_F32)
            return type_f32();
        return type_f16();
    }

    // both are integers - promote to larger/signed
    if (type_is_signed(left) || type_is_signed(right))
    {
        // at least one is signed
        if (left->size >= right->size)
            return type_is_signed(left) ? left : right;
        else
            return type_is_signed(right) ? right : left;
    }

    // both unsigned - use larger
    return left->size >= right->size ? left : right;
}

static bool is_lvalue(AstNode *expr)
{
    if (!expr)
        return false;

    switch (expr->kind)
    {
    case AST_EXPR_IDENT:
        // check if it's a variable (not a function or type)
        if (expr->symbol)
        {
            return expr->symbol->kind == SYMBOL_VAR || expr->symbol->kind == SYMBOL_VAL || expr->symbol->kind == SYMBOL_PARAM;
        }
        return true;

    case AST_EXPR_INDEX:
        return true; // array elements are lvalues

    case AST_EXPR_FIELD:
        return is_lvalue(expr->field_expr.object);

    case AST_EXPR_UNARY:
        if (expr->unary_expr.op == TOKEN_AT)
        {
            return true; // *ptr is an lvalue
        }
        return false;

    default:
        return false;
    }
}

Type *semantic_analyze_binary_expr(SemanticAnalyzer *analyzer, AstNode *expr)
{
    // for assignment, analyze left first to get the target type
    if (expr->binary_expr.op == TOKEN_EQUAL)
    {
        Type *left_type = semantic_analyze_expr(analyzer, expr->binary_expr.left);
        if (!left_type)
            return NULL;

        // check if left side is an lvalue
        if (!is_lvalue(expr->binary_expr.left))
        {
            semantic_error(analyzer, expr, "cannot assign to non-lvalue");
            return NULL;
        }

        // use left type as hint for right side
        Type *right_type = semantic_analyze_expr_with_hint(analyzer, expr->binary_expr.right, left_type);
        if (!right_type)
            return NULL;

        if (!semantic_check_assignment(analyzer, left_type, right_type, expr))
        {
            return NULL;
        }

        expr->type = left_type;
        return left_type;
    }

    // for other operations, analyze both sides with context-aware literal promotion
    Type *left_type  = semantic_analyze_expr(analyzer, expr->binary_expr.left);
    Type *right_type = semantic_analyze_expr(analyzer, expr->binary_expr.right);

    if (!left_type || !right_type)
        return NULL;

    // if one operand is a literal and the other is a concrete type, promote the literal
    bool left_is_literal  = expr->binary_expr.left->kind == AST_EXPR_LIT;
    bool right_is_literal = expr->binary_expr.right->kind == AST_EXPR_LIT;

    if (left_is_literal && !right_is_literal && type_is_integer(left_type) && type_is_integer(right_type))
    {
        // re-analyze left literal with right type as hint
        Type *promoted_left = semantic_analyze_expr_with_hint(analyzer, expr->binary_expr.left, right_type);
        if (promoted_left)
            left_type = promoted_left;
    }
    else if (right_is_literal && !left_is_literal && type_is_integer(right_type) && type_is_integer(left_type))
    {
        // re-analyze right literal with left type as hint
        Type *promoted_right = semantic_analyze_expr_with_hint(analyzer, expr->binary_expr.right, left_type);
        if (promoted_right)
            right_type = promoted_right;
    }

    // determine result type based on operation
    Type *result_type = NULL;
    switch (expr->binary_expr.op)
    {
    case TOKEN_EQUAL_EQUAL:
    case TOKEN_BANG_EQUAL:
    case TOKEN_LESS:
    case TOKEN_LESS_EQUAL:
    case TOKEN_GREATER:
    case TOKEN_GREATER_EQUAL:
    case TOKEN_AMPERSAND_AMPERSAND:
    case TOKEN_PIPE_PIPE:
        // comparison/logical operators return u8 (bool)
        result_type = type_u8();
        break;

    case TOKEN_PLUS:
    case TOKEN_MINUS:
        // pointer arithmetic
        if (type_is_pointer_like(left_type) && type_is_integer(right_type))
        {
            result_type = left_type;
        }
        else if (type_is_numeric(left_type) && type_is_numeric(right_type))
        {
            result_type = type_promote_binary(left_type, right_type);
        }
        else
        {
            semantic_error(analyzer, expr, "invalid operands for %s", expr->binary_expr.op == TOKEN_PLUS ? "addition" : "subtraction");
            return NULL;
        }
        break;

    case TOKEN_STAR:
    case TOKEN_SLASH:
    case TOKEN_PERCENT:
        if (!type_is_numeric(left_type) || !type_is_numeric(right_type))
        {
            semantic_error(analyzer, expr, "arithmetic requires numeric operands");
            return NULL;
        }
        result_type = type_promote_binary(left_type, right_type);
        break;

    case TOKEN_AMPERSAND:
    case TOKEN_PIPE:
    case TOKEN_CARET:
    case TOKEN_LESS_LESS:
    case TOKEN_GREATER_GREATER:
        if (!type_is_integer(left_type) || !type_is_integer(right_type))
        {
            semantic_error(analyzer, expr, "bitwise operations require integer operands");
            return NULL;
        }
        result_type = type_promote_binary(left_type, right_type);
        break;

    default:
        semantic_error(analyzer, expr, "unknown binary operator");
        return NULL;
    }

    expr->type = result_type;
    return result_type;
}

Type *semantic_analyze_unary_expr(SemanticAnalyzer *analyzer, AstNode *expr)
{
    Type *operand_type = semantic_analyze_expr(analyzer, expr->unary_expr.expr);
    if (!operand_type)
        return NULL;

    if (!semantic_check_unary_op(analyzer, expr->unary_expr.op, operand_type, expr))
    {
        return NULL;
    }

    switch (expr->unary_expr.op)
    {
    case TOKEN_QUESTION:
        if (!is_lvalue(expr->unary_expr.expr))
        {
            semantic_error(analyzer, expr, "address-of requires lvalue");
            return NULL;
        }
        expr->type = type_pointer_create(operand_type);
        return expr->type;

    case TOKEN_AT: // dereference
        if (operand_type->kind == TYPE_POINTER)
        {
            expr->type = operand_type->pointer.base;
            return expr->type;
        }
        semantic_error(analyzer, expr, "cannot dereference non-pointer type");
        return NULL;
    case TOKEN_BANG:            // logical not
        expr->type = type_u8(); // logical not returns u8 (0 or 1)
        return type_u8();

    case TOKEN_TILDE: // bitwise not
        if (!type_is_integer(operand_type))
        {
            semantic_error(analyzer, expr, "bitwise not requires integer operand");
            return NULL;
        }
        expr->type = operand_type;
        return operand_type;

    default: // arithmetic unary
        expr->type = operand_type;
        return operand_type;
    }
}

Type *semantic_analyze_call_expr(SemanticAnalyzer *analyzer, AstNode *expr)
{
    // check for builtin/intrinsic functions first
    if (expr->call_expr.func->kind == AST_EXPR_IDENT)
    {
        const char *func_name = expr->call_expr.func->ident_expr.name;

        // handle size_of() intrinsic
        if (strcmp(func_name, "size_of") == 0)
        {
            if (!expr->call_expr.args || expr->call_expr.args->count != 1)
            {
                semantic_error(analyzer, expr, "size_of() expects exactly one argument");
                return NULL;
            }

            AstNode *arg      = expr->call_expr.args->items[0];
            Type    *arg_type = semantic_analyze_expr(analyzer, arg);
            if (!arg_type)
                return NULL;

            expr->type = type_u64(); // size_of() returns u64
            return expr->type;
        }

        // handle align_of() intrinsic
        if (strcmp(func_name, "align_of") == 0)
        {
            if (!expr->call_expr.args || expr->call_expr.args->count != 1)
            {
                semantic_error(analyzer, expr, "align_of() expects exactly one argument");
                return NULL;
            }

            AstNode *arg      = expr->call_expr.args->items[0];
            Type    *arg_type = semantic_analyze_expr(analyzer, arg);
            if (!arg_type)
                return NULL;

            expr->type = type_u64(); // align_of() returns u64
            return expr->type;
        }

        // handle offset_of() intrinsic - get offset of struct field
        if (strcmp(func_name, "offset_of") == 0)
        {
            if (!expr->call_expr.args || expr->call_expr.args->count != 2)
            {
                semantic_error(analyzer, expr, "offset_of() expects exactly two arguments (type, field)");
                return NULL;
            }

            // first argument should be a type (handled as expression for now)
            AstNode *type_arg  = expr->call_expr.args->items[0];
            AstNode *field_arg = expr->call_expr.args->items[1];

            // field argument should be an identifier
            if (field_arg->kind != AST_EXPR_IDENT)
            {
                semantic_error(analyzer, expr, "offset_of() second argument must be a field name");
                return NULL;
            }

            Type *struct_type = semantic_analyze_expr(analyzer, type_arg);
            if (!struct_type)
                return NULL;

            struct_type = type_resolve_alias(struct_type);
            if (struct_type->kind != TYPE_STRUCT)
            {
                semantic_error(analyzer, expr, "offset_of() first argument must be a struct type");
                return NULL;
            }

            expr->type = type_u64(); // offset_of() returns u64
            return expr->type;
        }

        // handle type_of() intrinsic - get type info as runtime value
        if (strcmp(func_name, "type_of") == 0)
        {
            if (!expr->call_expr.args || expr->call_expr.args->count != 1)
            {
                semantic_error(analyzer, expr, "type_of() expects exactly one argument");
                return NULL;
            }

            AstNode *arg      = expr->call_expr.args->items[0];
            Type    *arg_type = semantic_analyze_expr(analyzer, arg);
            if (!arg_type)
                return NULL;

            // type_of() returns a compile-time constant representing the type
            // for now, we'll represent this as a u64 hash/id
            expr->type = type_u64(); // type_of() returns u64
            return expr->type;
        }

        // handle va_count() intrinsic - number of variadic args inside current variadic function
        if (strcmp(func_name, "va_count") == 0)
        {
            if (!analyzer->current_function || !analyzer->current_function->fun_stmt.is_variadic)
            {
                semantic_error(analyzer, expr, "va_count() used outside a variadic function");
                return NULL;
            }
            if (expr->call_expr.args && expr->call_expr.args->count != 0)
            {
                semantic_error(analyzer, expr, "va_count() expects no arguments");
                return NULL;
            }
            expr->type = type_u64();
            return expr->type;
        }

        // handle va_arg(i) intrinsic - pointer to i-th variadic argument (as untyped ptr)
        if (strcmp(func_name, "va_arg") == 0)
        {
            if (!analyzer->current_function || !analyzer->current_function->fun_stmt.is_variadic)
            {
                semantic_error(analyzer, expr, "va_arg() used outside a variadic function");
                return NULL;
            }
            if (!expr->call_expr.args || expr->call_expr.args->count != 1)
            {
                semantic_error(analyzer, expr, "va_arg() expects exactly one argument (index)");
                return NULL;
            }
            AstNode *idx = expr->call_expr.args->items[0];
            Type    *it  = semantic_analyze_expr(analyzer, idx);
            if (!it || !type_is_integer(it))
            {
                semantic_error(analyzer, expr, "va_arg() index must be integer");
                return NULL;
            }
            expr->type = type_ptr();
            return expr->type;
        }
    }

    Type *func_type = semantic_analyze_expr(analyzer, expr->call_expr.func);
    if (!func_type)
        return NULL;

    // resolve aliases to get the underlying type
    Type *resolved_type = type_resolve_alias(func_type);
    if (!resolved_type)
    {
        semantic_error(analyzer, expr, "expression is not callable");
        return NULL;
    }

    if (resolved_type->kind != TYPE_FUNCTION)
    {
        semantic_error(analyzer, expr, "expression is not callable");
        return NULL;
    }

    if (!semantic_check_function_call(analyzer, resolved_type, expr->call_expr.args, expr))
    {
        return NULL;
    }

    // functions with no return type are valid but have no expression type
    expr->type = resolved_type->function.return_type;
    return resolved_type->function.return_type ? resolved_type->function.return_type : type_ptr(); // return dummy type for void functions
}

Type *semantic_analyze_index_expr(SemanticAnalyzer *analyzer, AstNode *expr)
{
    Type *array_type = semantic_analyze_expr(analyzer, expr->index_expr.array);
    Type *index_type = semantic_analyze_expr(analyzer, expr->index_expr.index);

    if (!array_type || !index_type)
        return NULL;

    // check index type
    if (!type_is_integer(index_type))
    {
        semantic_error(analyzer, expr, "array index must be integer");
        return NULL;
    }

    // resolve type aliases before checking array type
    Type *resolved_type = type_resolve_alias(array_type);
    if (!resolved_type)
        resolved_type = array_type;

    // check array type
    if (resolved_type->kind == TYPE_ARRAY)
    {
        // all arrays are fat pointers, so no compile-time bounds check
        expr->type = resolved_type->array.elem_type;
        return expr->type;
    }
    else if (resolved_type->kind == TYPE_POINTER)
    {
        expr->type = resolved_type->pointer.base;
        return expr->type;
    }
    else
    {
        semantic_error(analyzer, expr, "subscripted value is not an array or pointer");
        return NULL;
    }
}

Type *semantic_analyze_field_expr(SemanticAnalyzer *analyzer, AstNode *expr)
{
    Type *object_type = semantic_analyze_expr(analyzer, expr->field_expr.object);

    // check if this is module member access
    if (expr->field_expr.object->kind == AST_EXPR_IDENT)
    {
        Symbol *object_symbol = expr->field_expr.object->symbol;
        if (object_symbol && object_symbol->kind == SYMBOL_MODULE)
        {
            // this is module.member access
            Symbol *member = symbol_lookup_scope(object_symbol->module.scope, expr->field_expr.field);
            if (!member)
            {
                semantic_error(analyzer, expr, "no symbol named '%s' in module '%s'", expr->field_expr.field, object_symbol->name);
                return NULL;
            }

            expr->type   = member->type;
            expr->symbol = member;
            return member->type;
        }
    }

    if (!object_type)
        return NULL;

    object_type = type_resolve_alias(object_type);

    if (object_type && object_type->kind == TYPE_POINTER)
    {
        object_type = type_resolve_alias(object_type->pointer.base);
    }

    if (!object_type)
        return NULL;

    if (object_type->kind == TYPE_ARRAY)
    {
        if (strcmp(expr->field_expr.field, "length") == 0)
        {
            expr->type = type_u64();
            return expr->type;
        }
        if (strcmp(expr->field_expr.field, "data") == 0)
        {
            expr->type = type_pointer_create(object_type->array.elem_type);
            return expr->type;
        }

        semantic_error(analyzer, expr, "no member named '%s' on array", expr->field_expr.field);
        return NULL;
    }

    // handle pointer to struct dereference
    if (object_type->kind != TYPE_STRUCT && object_type->kind != TYPE_UNION)
    {
        semantic_error(analyzer, expr, "member access on non-struct/union type");
        return NULL;
    }

    // find field in type
    for (Symbol *field = object_type->composite.fields; field; field = field->next)
    {
        if (field->name && strcmp(field->name, expr->field_expr.field) == 0)
        {
            expr->type   = field->type;
            expr->symbol = field;
            return field->type;
        }
    }

    semantic_error(analyzer, expr, "no member named '%s'", expr->field_expr.field);
    return NULL;
}

Type *semantic_analyze_cast_expr(SemanticAnalyzer *analyzer, AstNode *expr)
{
    Type *source_type = semantic_analyze_expr(analyzer, expr->cast_expr.expr);
    Type *target_type = resolve_type(analyzer, expr->cast_expr.type);

    if (!source_type || !target_type)
        return NULL;

    // store resolved target type in the cast type node
    if (expr->cast_expr.type)
    {
        expr->cast_expr.type->type = target_type;
    }

    if (!type_can_cast_to(source_type, target_type))
    {
        char source_str[256] = {0};
        char target_str[256] = {0};
        format_type_name(source_type, source_str, sizeof(source_str));
        format_type_name(target_type, target_str, sizeof(target_str));

        semantic_error(analyzer, expr, "invalid cast from `%s` to `%s`", source_str, target_str);
        return NULL;
    }

    expr->type = target_type;
    return target_type;
}

Type *semantic_analyze_ident_expr(SemanticAnalyzer *analyzer, AstNode *expr)
{
    const char *name   = expr->ident_expr.name;
    Symbol     *symbol = symbol_lookup(&analyzer->symbol_table, name);

    if (!symbol)
    {
        semantic_error(analyzer, expr, "undefined identifier '%s'", name);
        return NULL;
    }

    expr->symbol = symbol;

    // module symbols don't have a regular type, but they're valid for field access
    if (symbol->kind == SYMBOL_MODULE)
    {
        expr->type = NULL; // modules don't have a type per se
        return type_ptr(); // return a dummy type to indicate success
    }

    expr->type = symbol->type;
    return symbol->type;
}

Type *semantic_analyze_lit_expr(SemanticAnalyzer *analyzer, AstNode *expr)
{
    return semantic_analyze_lit_expr_with_hint(analyzer, expr, NULL);
}

Type *semantic_analyze_lit_expr_with_hint(SemanticAnalyzer *analyzer, AstNode *expr, Type *expected_type)
{
    (void)analyzer; // unused

    switch (expr->lit_expr.kind)
    {
    case TOKEN_LIT_INT:
        // determine integer type based on value and context
        {
            unsigned long long value = expr->lit_expr.int_val;
            Type              *int_type;

            // allow 0 (including 'nil') to adopt expected pointer type in context
            if (expected_type && type_is_pointer_like(expected_type) && value == 0)
            {
                expr->type = expected_type;
                return expected_type;
            }

            // if we have an expected integer type and value fits, use it
            if (expected_type && type_is_integer(expected_type))
            {
                bool fits = false;
                switch (expected_type->kind)
                {
                case TYPE_U8:
                    fits = (value <= UINT8_MAX);
                    break;
                case TYPE_U16:
                    fits = (value <= UINT16_MAX);
                    break;
                case TYPE_U32:
                    fits = (value <= UINT32_MAX);
                    break;
                case TYPE_U64:
                    fits = true;
                    break;
                case TYPE_I8:
                    fits = (value <= INT8_MAX);
                    break;
                case TYPE_I16:
                    fits = (value <= INT16_MAX);
                    break;
                case TYPE_I32:
                    fits = (value <= INT32_MAX);
                    break;
                case TYPE_I64:
                    fits = (value <= INT64_MAX);
                    break;
                default:
                    fits = false;
                    break;
                }

                if (fits)
                {
                    expr->type = expected_type;
                    return expected_type;
                }
            }

            // fall back to smallest type that can hold the value
            if (value <= UINT8_MAX)
            {
                int_type = type_u8();
            }
            else if (value <= UINT16_MAX)
            {
                int_type = type_u16();
            }
            else if (value <= UINT32_MAX)
            {
                int_type = type_u32();
            }
            else
            {
                int_type = type_u64();
            }

            expr->type = int_type;
            return int_type;
        }

    case TOKEN_LIT_FLOAT:
        // if we have an expected float type, use it
        if (expected_type && type_is_float(expected_type))
        {
            expr->type = expected_type;
            return expected_type;
        }
        // default to f64
        expr->type = type_f64();
        return type_f64();

    case TOKEN_LIT_CHAR:
        // if we have an expected integer type that can hold a char, use it
        if (expected_type && type_is_integer(expected_type) && expected_type->size >= 1)
        {
            expr->type = expected_type;
            return expected_type;
        }
        // default to u8
        expr->type = type_u8();
        return type_u8();

    case TOKEN_LIT_STRING:
        expr->type = type_array_create(type_u8());
        return expr->type;

    default:
        return NULL;
    }
}

static Type *semantic_analyze_null_expr(SemanticAnalyzer *analyzer, AstNode *expr, Type *expected_type)
{
    Type *resolved = NULL;

    if (expected_type)
    {
        if (!type_is_pointer_like(expected_type))
        {
            semantic_error(analyzer, expr, "'nil' expects a pointer-like context");
            return NULL;
        }
        resolved = expected_type;
    }
    else
    {
        resolved = type_ptr();
    }

    expr->type = resolved;
    return resolved;
}

Type *semantic_analyze_array_expr(SemanticAnalyzer *analyzer, AstNode *expr)
{
    Type *specified_type = resolve_type(analyzer, expr->array_expr.type);
    if (!specified_type)
    {
        semantic_error(analyzer, expr, "cannot resolve array type");
        return NULL;
    }

    // resolve type aliases before checking the type
    Type *resolved_type = type_resolve_alias(specified_type);
    if (!resolved_type)
        resolved_type = specified_type;

    // check if the resolved type is already an array type
    if (resolved_type->kind == TYPE_ARRAY)
    {
        // for []T{...} or alias{...} syntax where alias resolves to array, use it directly
        // but validate that all elements match the element type
        Type *elem_type  = resolved_type->array.elem_type;
        int   elem_count = expr->array_expr.elems ? expr->array_expr.elems->count : 0;

        if (elem_count == 2)
        {
            AstNode *data_node = expr->array_expr.elems->items[0];
            AstNode *len_node  = expr->array_expr.elems->items[1];

            Type *data_type = semantic_analyze_expr(analyzer, data_node);
            if (!data_type)
                return NULL;

            Type *len_type = semantic_analyze_expr(analyzer, len_node);
            if (!len_type)
                return NULL;

            if (type_is_pointer_like(data_type) && type_is_integer(len_type))
            {
                Type *expected_ptr = type_pointer_create(elem_type);
                if (!semantic_check_assignment(analyzer, expected_ptr, data_type, data_node))
                {
                    return NULL;
                }

                Type *expected_len = type_u64();
                if (!semantic_check_assignment(analyzer, expected_len, len_type, len_node))
                {
                    return NULL;
                }

                expr->array_expr.is_slice_literal = true;
                expr->type                        = specified_type;
                return expr->type;
            }
        }

        if (expr->array_expr.elems)
        {
            for (int i = 0; i < expr->array_expr.elems->count; i++)
            {
                Type *elem_result = semantic_analyze_expr_with_hint(analyzer, expr->array_expr.elems->items[i], elem_type);
                if (!elem_result)
                    return NULL;

                if (!semantic_check_assignment(analyzer, elem_type, elem_result, expr->array_expr.elems->items[i]))
                {
                    return NULL;
                }
            }
        }

        // return the original specified type (preserving alias)
        expr->type = specified_type;
        return expr->type;
    }
    else if (resolved_type->kind == TYPE_STRUCT || resolved_type->kind == TYPE_UNION)
    {
        // this is actually a struct/union literal using type{...} syntax
        return semantic_analyze_array_as_struct(analyzer, expr, specified_type, resolved_type);
    }
    else
    {
        // for T{...} syntax, create a new array of type T
        Type *elem_type = resolved_type;

        if (expr->array_expr.elems)
        {
            for (int i = 0; i < expr->array_expr.elems->count; i++)
            {
                Type *elem_result = semantic_analyze_expr_with_hint(analyzer, expr->array_expr.elems->items[i], elem_type);
                if (!elem_result)
                    return NULL;

                if (!semantic_check_assignment(analyzer, elem_type, elem_result, expr->array_expr.elems->items[i]))
                {
                    return NULL;
                }
            }
        }

        expr->type = type_array_create(elem_type);
        return expr->type;
    }
}

// helper function to convert array expression to struct initialization
static Type *semantic_analyze_array_as_struct(SemanticAnalyzer *analyzer, AstNode *expr, Type *specified_type, Type *resolved_type)
{
    // for struct{value1, value2, ...} syntax, we need to match values to fields in order
    if (!expr->array_expr.elems)
    {
        // empty initializer is valid
        expr->type = specified_type;
        return specified_type;
    }

    // collect struct fields in order
    Symbol *field       = resolved_type->composite.fields;
    int     field_count = 0;
    while (field)
    {
        field_count++;
        field = field->next;
    }

    // check that we don't have more initializers than fields
    if (expr->array_expr.elems->count > field_count)
    {
        semantic_error(analyzer, expr, "too many initializers for struct (expected %d, got %d)", field_count, expr->array_expr.elems->count);
        return NULL;
    }

    // validate each element against corresponding field type
    field = resolved_type->composite.fields;
    for (int i = 0; i < expr->array_expr.elems->count && field; i++, field = field->next)
    {
        Type *elem_result = semantic_analyze_expr_with_hint(analyzer, expr->array_expr.elems->items[i], field->type);
        if (!elem_result)
            return NULL;

        if (!semantic_check_assignment(analyzer, field->type, elem_result, expr->array_expr.elems->items[i]))
        {
            return NULL;
        }
    }

    // return the original specified type (preserving alias)
    expr->type = specified_type;
    return specified_type;
}

Type *semantic_analyze_struct_expr(SemanticAnalyzer *analyzer, AstNode *expr)
{
    Type *struct_type = resolve_type(analyzer, expr->struct_expr.type);
    if (!struct_type)
    {
        semantic_error(analyzer, expr, "cannot resolve struct type");
        return NULL;
    }

    // resolve type aliases before checking the type
    Type *resolved_type = type_resolve_alias(struct_type);
    if (!resolved_type)
        resolved_type = struct_type;

    if (resolved_type->kind != TYPE_STRUCT && resolved_type->kind != TYPE_UNION)
    {
        semantic_error(analyzer, expr, "not a struct or union type");
        return NULL;
    }

    // analyze field initializers
    if (expr->struct_expr.fields)
    {
        for (int i = 0; i < expr->struct_expr.fields->count; i++)
        {
            AstNode *field_init = expr->struct_expr.fields->items[i];

            // field initializers should be field expressions or assignments
            if (field_init->kind == AST_EXPR_FIELD)
            {
                // find the field in the struct type
                Symbol *field_sym = NULL;
                for (Symbol *field = resolved_type->composite.fields; field; field = field->next)
                {
                    if (field->name && strcmp(field->name, field_init->field_expr.field) == 0)
                    {
                        field_sym = field;
                        break;
                    }
                }

                if (!field_sym)
                {
                    semantic_error(analyzer, field_init, "no field named '%s' in struct", field_init->field_expr.field);
                    return NULL;
                }

                // analyze the initializer value with field type as hint
                Type *init_type = semantic_analyze_expr_with_hint(analyzer, field_init->field_expr.object, field_sym->type);
                if (!init_type)
                {
                    return NULL;
                }

                // check type compatibility
                if (!semantic_check_assignment(analyzer, field_sym->type, init_type, field_init))
                {
                    return NULL;
                }
            }
            else
            {
                // for other expressions, just analyze them
                if (!semantic_analyze_expr(analyzer, field_init))
                {
                    return NULL;
                }
            }
        }
    }

    // return the original type (preserving alias)
    expr->type = struct_type;
    return struct_type;
}

bool semantic_check_assignment(SemanticAnalyzer *analyzer, Type *target, Type *source, AstNode *node)
{
    if (type_equals(target, source))
        return true;

    // for assignments, be more restrictive than general casting
    if (!type_can_assign_to(source, target))
    {
        char target_str[256] = {0};
        char source_str[256] = {0};
        format_type_name(target, target_str, sizeof(target_str));
        format_type_name(source, source_str, sizeof(source_str));

        semantic_error(analyzer, node, "incompatible types in assignment: expected `%s`, got `%s`", target_str, source_str);
        return false;
    }

    return true;
}

bool semantic_check_binary_op(SemanticAnalyzer *analyzer, TokenKind op, Type *left, Type *right, AstNode *node)
{
    switch (op)
    {
    case TOKEN_PLUS:
    case TOKEN_MINUS:
    case TOKEN_STAR:
    case TOKEN_SLASH:
    case TOKEN_PERCENT:
        if (!type_is_numeric(left) || !type_is_numeric(right))
        {
            semantic_error(analyzer, node, "arithmetic requires numeric operands");
            return false;
        }
        return true;

    case TOKEN_EQUAL_EQUAL:
    case TOKEN_BANG_EQUAL:
        if (!type_equals(left, right) && !type_can_cast_to(left, right) && !type_can_cast_to(right, left))
        {
            char left_str[256]  = {0};
            char right_str[256] = {0};
            format_type_name(left, left_str, sizeof(left_str));
            format_type_name(right, right_str, sizeof(right_str));

            semantic_error(analyzer, node, "incompatible types for comparison: `%s` and `%s`", left_str, right_str);
            return false;
        }
        return true;

    case TOKEN_LESS:
    case TOKEN_LESS_EQUAL:
    case TOKEN_GREATER:
    case TOKEN_GREATER_EQUAL:
        if (!type_is_numeric(left) || !type_is_numeric(right))
        {
            semantic_error(analyzer, node, "ordering comparison requires numeric operands");
            return false;
        }
        return true;

    case TOKEN_AMPERSAND_AMPERSAND:
    case TOKEN_PIPE_PIPE:
    case TOKEN_KW_OR:
        // in Mach, logical operators work on any type (truthiness)
        return true;

    case TOKEN_AMPERSAND:
    case TOKEN_PIPE:
    case TOKEN_CARET:
    case TOKEN_LESS_LESS:
    case TOKEN_GREATER_GREATER:
        if (!type_is_integer(left) || !type_is_integer(right))
        {
            semantic_error(analyzer, node, "bitwise operations require integer operands");
            return false;
        }
        return true;

    case TOKEN_EQUAL:
        // assignment: check if right can be assigned to left
        if (!semantic_check_assignment(analyzer, left, right, node))
        {
            return false;
        }
        return true;

    default:
        semantic_error(analyzer, node, "unknown binary operator");
        return false;
    }
}

bool semantic_check_unary_op(SemanticAnalyzer *analyzer, TokenKind op, Type *operand, AstNode *node)
{
    switch (op)
    {
    case TOKEN_PLUS:
    case TOKEN_MINUS:
        if (!type_is_numeric(operand))
        {
            semantic_error(analyzer, node, "arithmetic unary requires numeric operand");
            return false;
        }
        return true;

    case TOKEN_BANG:
        // logical not works on any type
        return true;

    case TOKEN_TILDE:
        if (!type_is_integer(operand))
        {
            semantic_error(analyzer, node, "bitwise not requires integer operand");
            return false;
        }
        return true;

    case TOKEN_QUESTION:
        if (!is_lvalue(node->unary_expr.expr))
        {
            semantic_error(analyzer, node, "address-of operator requires an lvalue");
            return false;
        }
        return true;

    case TOKEN_AT:
        if (!type_is_pointer_like(operand))
        {
            semantic_error(analyzer, node, "dereference requires pointer-like operand");
            return false;
        }
        return true;

    default:
        semantic_error(analyzer, node, "unknown unary operator");
        return false;
    }
}

bool semantic_check_function_call(SemanticAnalyzer *analyzer, Type *func_type, AstList *args, AstNode *node)
{
    size_t fixed_count  = func_type->function.param_count;
    size_t actual_count = args ? args->count : 0;
    bool   is_variadic  = func_type->function.is_variadic;

    bool   forwards_varargs = false;
    size_t forward_index     = SIZE_MAX;

    if (args)
    {
        for (size_t i = 0; i < actual_count; i++)
        {
            AstNode *arg = args->items[i];
            if (arg && arg->kind == AST_EXPR_VARARGS)
            {
                if (forwards_varargs)
                {
                    semantic_error(analyzer, node, "multiple '...' arguments are not allowed");
                    return false;
                }
                forwards_varargs = true;
                forward_index    = i;
            }
        }
    }

    if (!is_variadic)
    {
        if (forwards_varargs)
        {
            semantic_error(analyzer, node, "'...' cannot be used with non-variadic functions");
            return false;
        }
        if (fixed_count != actual_count)
        {
            semantic_error(analyzer, node, "function expects %zu arguments, got %zu", fixed_count, actual_count);
            return false;
        }
    }
    else
    {
        if (actual_count < fixed_count)
        {
            semantic_error(analyzer, node, "variadic function expects at least %zu arguments, got %zu", fixed_count, actual_count);
            return false;
        }
    }

    if (forwards_varargs)
    {
        if (!analyzer->current_function || !analyzer->current_function->symbol || !analyzer->current_function->fun_stmt.is_variadic)
        {
            semantic_error(analyzer, node, "'...' used outside a variadic function");
            return false;
        }

        if (forward_index < fixed_count)
        {
            semantic_error(analyzer, node, "'...' cannot satisfy required parameters");
            return false;
        }

        if (forward_index != actual_count - 1)
        {
            semantic_error(analyzer, node, "'...' must be the final argument");
            return false;
        }

        if (actual_count > fixed_count + 1)
        {
            semantic_error(analyzer, node, "additional arguments are not allowed when forwarding '...'");
            return false;
        }

        Symbol *callee_symbol = (node && node->call_expr.func) ? node->call_expr.func->symbol : NULL;
        bool    callee_uses_mach_varargs = is_variadic;
        if (callee_symbol && callee_symbol->kind == SYMBOL_FUNC)
        {
            callee_uses_mach_varargs = callee_symbol->func.uses_mach_varargs;
        }

        if (!callee_uses_mach_varargs)
        {
            semantic_error(analyzer, node, "'...' forwarding requires a Mach variadic function");
            return false;
        }
    }

    // check fixed arguments
    for (size_t i = 0; i < fixed_count; i++)
    {
        AstNode *arg_node = args ? args->items[i] : NULL;
        if (!arg_node)
        {
            semantic_error(analyzer, node, "missing argument for parameter %zu", i);
            return false;
        }
        if (arg_node->kind == AST_EXPR_VARARGS)
        {
            semantic_error(analyzer, arg_node, "'...' cannot bind to a fixed parameter");
            return false;
        }
        Type *param_type = func_type->function.param_types[i];
        Type *arg_type   = semantic_analyze_expr_with_hint(analyzer, arg_node, param_type);
        if (!arg_type)
            return false;
        if (!semantic_check_assignment(analyzer, param_type, arg_type, arg_node))
            return false;
    }

    // analyze variadic arguments for side effects and basic validity
    if (is_variadic)
    {
        for (size_t i = fixed_count; i < actual_count; i++)
        {
            AstNode *arg_node = args->items[i];
            if (arg_node->kind == AST_EXPR_VARARGS)
            {
                continue;
            }
            if (forwards_varargs)
            {
                semantic_error(analyzer, arg_node, "additional arguments are not allowed when forwarding '...'");
                return false;
            }
            // no type checking beyond being a valid expression
            if (!semantic_analyze_expr(analyzer, arg_node))
                return false;
        }
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
        if (list->errors[i].token)
        {
            token_dnit(list->errors[i].token);
            free(list->errors[i].token);
        }
        free(list->errors[i].message);
        free(list->errors[i].file_path);
    }
    free(list->errors);
}

void semantic_error_list_add(SemanticErrorList *list, Token *token, const char *message, const char *file_path)
{
    if (list->count >= list->capacity)
    {
        list->capacity = list->capacity ? list->capacity * 2 : 8;
        list->errors   = realloc(list->errors, sizeof(SemanticError) * list->capacity);
    }

    Token *error_token = NULL;
    if (token)
    {
        error_token = malloc(sizeof(Token));
        if (error_token)
        {
            token_copy(token, error_token);
        }
    }

    list->errors[list->count].token     = error_token;
    list->errors[list->count].message   = strdup(message);
    list->errors[list->count].file_path = file_path ? strdup(file_path) : NULL;
    list->count++;
}

void semantic_error_list_print(SemanticErrorList *list, Lexer *lexer, const char *file_path)
{
    for (int i = 0; i < list->count; i++)
    {
        Token      *token           = list->errors[i].token;
        const char *error_file_path = list->errors[i].file_path ? list->errors[i].file_path : file_path;

        printf("error: %s\n", list->errors[i].message);

        if (token && error_file_path)
        {
            // if the error is from the same file as the main lexer, use it for detailed info
            if (lexer && file_path && error_file_path && strcmp(error_file_path, file_path) == 0)
            {
                int   line      = lexer_get_pos_line(lexer, token->pos);
                int   col       = lexer_get_pos_line_offset(lexer, token->pos);
                char *line_text = lexer_get_line_text(lexer, line);

                if (!line_text)
                {
                    line_text = strdup("unable to retrieve line text");
                }

                printf("%s:%d:%d\n", error_file_path, line + 1, col);
                printf("%5d | %-*s\n", line + 1, col > 1 ? col - 1 : 0, line_text);
                printf("      | %*s^\n", col - 1, "");

                free(line_text);
            }
            else
            {
                // for errors from other files, just show basic location info
                printf("%s:pos:%d\n", error_file_path, token->pos);
                printf("      | (from imported module)\n");
            }
        }
        else if (error_file_path)
        {
            printf("%s\n", error_file_path);
            printf("      | (location unavailable)\n");
        }
    }
}

void semantic_error(SemanticAnalyzer *analyzer, AstNode *node, const char *fmt, ...)
{
    analyzer->has_errors = true;

    // format the message
    va_list args;
    va_start(args, fmt);

    size_t len = vsnprintf(NULL, 0, fmt, args) + 1;
    va_end(args);

    char *message = malloc(len);
    va_start(args, fmt);
    vsnprintf(message, len, fmt, args);
    va_end(args);

    // add to error list
    Token *token = (node && node->token) ? node->token : NULL;
    semantic_error_list_add(&analyzer->errors, token, message, NULL);

    free(message);
}

void semantic_warning(SemanticAnalyzer *analyzer, AstNode *node, const char *fmt, ...)
{
    (void)analyzer; // unused for now
    (void)node;     // unused for now

    printf("semantic warning: ");

    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    printf("\n");
}
