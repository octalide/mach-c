#include "semantic.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// forward declaration
static const char *type_to_string(Type *type);
static bool        is_lvalue_expression(AstNode *expr);

// helper structure for copying symbols during import
typedef struct ImportContext
{
    SemanticAnalyzer *analyzer;
    AstNode          *use_decl;
    bool              success;
} ImportContext;

// callback function to import symbols from a module
static void import_symbol_callback(Symbol *symbol, void *data);

// builtin type information
static struct
{
    const char *name;
    TypeKind    kind;
    unsigned    size;
    unsigned    align;
    bool        is_signed;
} builtin_types[] = {
    {"ptr", TYPE_PTR, 8, 8, false}, // untyped pointer
    {"u8", TYPE_INT, 1, 1, false},  {"u16", TYPE_INT, 2, 2, false}, {"u32", TYPE_INT, 4, 4, false},   {"u64", TYPE_INT, 8, 8, false},   {"i8", TYPE_INT, 1, 1, true},     {"i16", TYPE_INT, 2, 2, true},
    {"i32", TYPE_INT, 4, 4, true},  {"i64", TYPE_INT, 8, 8, true},  {"f16", TYPE_FLOAT, 2, 2, false}, {"f32", TYPE_FLOAT, 4, 4, false}, {"f64", TYPE_FLOAT, 8, 8, false},
};

#define BUILTIN_COUNT (sizeof(builtin_types) / sizeof(builtin_types[0]))

// hash function for symbol names
static unsigned int hash_symbol_name(const char *name, int capacity)
{
    unsigned int hash = 5381;
    for (int i = 0; name[i]; i++)
    {
        hash = ((hash << 5) + hash) + name[i];
    }
    return hash % capacity;
}

// callback function to import symbols from a module
static void import_symbol_callback(Symbol *symbol, void *data)
{
    ImportContext *ctx = (ImportContext *)data;

    // skip internal symbols (like builtin types that are already in global scope)
    if (symbol->kind == SYMBOL_TYPE)
    {
        // check if this is a builtin type by seeing if it's already in global scope
        Symbol *existing = symbol_table_lookup_current_scope(ctx->analyzer->global_scope, symbol->name);
        if (existing && existing->type == symbol->type)
            return; // skip builtin types
    }

    // import the symbol into current scope
    Symbol *imported = symbol_table_declare(ctx->analyzer->current_scope, symbol->name, symbol->kind, symbol->type, symbol->decl_node);
    if (!imported)
    {
        semantic_error(ctx->analyzer, ctx->use_decl, "symbol already declared: %s", symbol->name);
        ctx->success = false;
        return;
    }

    // copy additional fields
    imported->is_val = symbol->is_val;
    imported->module = symbol->module;
}

// check if an expression is an lvalue (can have its address taken)
static bool is_lvalue_expression(AstNode *expr)
{
    if (!expr)
        return false;

    switch (expr->kind)
    {
    case AST_IDENT_EXPR:
        // variables and parameters are lvalues
        return true;

    case AST_INDEX_EXPR:
        // array/pointer indexing results in an lvalue
        return true;

    case AST_FIELD_EXPR:
        // struct/union field access results in an lvalue
        return true;

    case AST_UNARY_EXPR:
        // dereferencing a pointer results in an lvalue
        return expr->unary_expr.op == TOKEN_AT;

    case AST_LIT_EXPR:
    case AST_CALL_EXPR:
    case AST_BINARY_EXPR:
    case AST_CAST_EXPR:
    default:
        // literals, function calls, binary expressions, casts are not lvalues
        return false;
    }
}

void semantic_analyzer_init(SemanticAnalyzer *analyzer, ModuleManager *manager)
{
    analyzer->module_manager               = manager;
    analyzer->had_error                    = false;
    analyzer->current_function_return_type = NULL;
    semantic_error_list_init(&analyzer->errors);

    // create global scope
    analyzer->global_scope = malloc(sizeof(SymbolTable));
    symbol_table_init(analyzer->global_scope, NULL);
    analyzer->current_scope = analyzer->global_scope;

    // initialize builtin types
    analyzer->builtin_count = (int)BUILTIN_COUNT;
    analyzer->builtin_types = malloc(BUILTIN_COUNT * sizeof(Type *));

    for (int i = 0; i < (int)BUILTIN_COUNT; i++)
    {
        analyzer->builtin_types[i] = type_create_builtin(builtin_types[i].kind, builtin_types[i].size, builtin_types[i].align, builtin_types[i].is_signed);

        // add to global symbol table
        symbol_table_declare(analyzer->global_scope, builtin_types[i].name, SYMBOL_TYPE, analyzer->builtin_types[i], NULL);
    }
}

void semantic_analyzer_dnit(SemanticAnalyzer *analyzer)
{
    // clean up builtin types
    for (int i = 0; i < analyzer->builtin_count; i++)
    {
        type_dnit(analyzer->builtin_types[i]);
        free(analyzer->builtin_types[i]);
    }
    free(analyzer->builtin_types);

    // clean up error list
    semantic_error_list_dnit(&analyzer->errors);

    // clean up symbol table
    symbol_table_dnit(analyzer->global_scope);
    free(analyzer->global_scope);
}

bool semantic_analyze_program(SemanticAnalyzer *analyzer, AstNode *program)
{
    if (program->kind != AST_PROGRAM)
    {
        semantic_error(analyzer, program, "expected program node");
        return false;
    }

    // first pass: collect all declarations
    for (int i = 0; i < program->program.decls->count; i++)
    {
        AstNode *decl = program->program.decls->items[i];
        semantic_analyze_declaration(analyzer, decl); // continue even on errors
    }

    return !analyzer->had_error;
}

bool semantic_analyze_declaration(SemanticAnalyzer *analyzer, AstNode *decl)
{
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
        semantic_error(analyzer, decl, "unknown declaration kind");
        return false;
    }
}

bool semantic_analyze_use_decl(SemanticAnalyzer *analyzer, AstNode *decl)
{
    // module should already be loaded by module manager
    Module *module = (Module *)decl->use_decl.module_sym;
    if (!module)
    {
        semantic_error(analyzer, decl, "module not loaded: %s", decl->use_decl.module_path);
        return false;
    }

    // analyze the imported module if not already done
    if (!module->is_analyzed)
    {
        if (!semantic_analyze_module(analyzer, module))
        {
            return false;
        }
        module->is_analyzed = true;
    }

    if (decl->use_decl.alias)
    {
        // aliased import: `use adder: add;`
        // add the module itself as a symbol so qualified access works
        Symbol *symbol = symbol_table_declare(analyzer->current_scope, decl->use_decl.alias, SYMBOL_MODULE, NULL, decl);
        if (!symbol)
        {
            semantic_error(analyzer, decl, "symbol already declared: %s", decl->use_decl.alias);
            return false;
        }
        symbol->module = module;
    }
    else
    {
        // direct import: `use add;`
        // import all symbols from the module directly into current scope
        if (!module->symbols)
        {
            semantic_error(analyzer, decl, "module has no symbols to import: %s", decl->use_decl.module_path);
            return false;
        }

        ImportContext ctx = {.analyzer = analyzer, .use_decl = decl, .success = true};

        symbol_table_iterate(module->symbols, import_symbol_callback, &ctx);

        if (!ctx.success)
        {
            return false;
        }
    }

    return true;
}

Type *semantic_analyze_type(SemanticAnalyzer *analyzer, AstNode *type_node)
{
    if (!type_node)
    {
        return NULL; // no return type
    }

    switch (type_node->kind)
    {
    case AST_TYPE_NAME:
    {
        const char *name    = type_node->type_name.name;
        Type       *builtin = semantic_get_builtin_type(analyzer, name);
        if (builtin)
        {
            return builtin;
        }

        // look up user-defined type
        Symbol *symbol = symbol_table_lookup(analyzer->current_scope, name);
        if (!symbol || symbol->kind != SYMBOL_TYPE)
        {
            semantic_error(analyzer, type_node, "unknown type: %s", name);
            return NULL;
        }

        return symbol->type;
    }

    case AST_TYPE_PTR:
    {
        Type *base = semantic_analyze_type(analyzer, type_node->type_ptr.base);
        return type_create_ptr(base);
    }

    case AST_TYPE_ARRAY:
    {
        Type *elem_type = semantic_analyze_type(analyzer, type_node->type_array.elem_type);
        if (!elem_type)
        {
            return NULL;
        }

        int size = -1; // unbound by default
        if (type_node->type_array.size)
        {
            // evaluate constant expression for array size
            if (type_node->type_array.size->kind == AST_LIT_EXPR && type_node->type_array.size->lit_expr.kind == TOKEN_LIT_INT)
            {
                size = (int)type_node->type_array.size->lit_expr.int_val;
            }
            else
            {
                semantic_error(analyzer, type_node, "array size must be a constant integer");
                return NULL;
            }
        }

        return type_create_array(elem_type, size);
    }

    case AST_TYPE_FUN:
    {
        // analyze parameter types
        Type **param_types = NULL;
        int    param_count = type_node->type_fun.params->count;

        if (param_count > 0)
        {
            param_types = malloc(param_count * sizeof(Type *));
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

        Type *return_type = semantic_analyze_type(analyzer, type_node->type_fun.return_type);
        return type_create_function(param_types, param_count, return_type);
    }

    default:
        semantic_error(analyzer, type_node, "unsupported type kind");
        return NULL;
    }
}

bool semantic_analyze_fun_decl(SemanticAnalyzer *analyzer, AstNode *decl)
{
    // analyze parameter types
    Type **param_types = NULL;
    int    param_count = decl->fun_decl.params->count;

    if (param_count > 0)
    {
        param_types = malloc(param_count * sizeof(Type *));
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

    // analyze return type (can be NULL for no return)
    Type *return_type = semantic_analyze_type(analyzer, decl->fun_decl.return_type);

    // create function type
    Type *func_type = type_create_function(param_types, param_count, return_type);

    // declare function in current scope
    Symbol *symbol = symbol_table_declare(analyzer->current_scope, decl->fun_decl.name, SYMBOL_FUNC, func_type, decl);
    if (!symbol)
    {
        semantic_error(analyzer, decl, "symbol already declared: %s", decl->fun_decl.name);
        type_dnit(func_type);
        free(func_type);
        return false;
    }

    // analyze function body if present
    if (decl->fun_decl.body)
    {
        symbol_table_push_scope(analyzer);

        // set current function return type for return statement checking
        Type *old_return_type                  = analyzer->current_function_return_type;
        analyzer->current_function_return_type = return_type;

        // add parameters to function scope
        for (int i = 0; i < param_count; i++)
        {
            AstNode *param = decl->fun_decl.params->items[i];
            symbol_table_declare(analyzer->current_scope, param->param_decl.name, SYMBOL_PARAM, param_types[i], param);
        }

        semantic_analyze_statement(analyzer, decl->fun_decl.body);

        // restore previous function return type
        analyzer->current_function_return_type = old_return_type;

        symbol_table_pop_scope(analyzer);
    }

    decl->type   = func_type;
    decl->symbol = symbol;

    return true;
}

Type *semantic_get_builtin_type(SemanticAnalyzer *analyzer, const char *name)
{
    for (int i = 0; i < analyzer->builtin_count; i++)
    {
        if (strcmp(builtin_types[i].name, name) == 0)
        {
            return analyzer->builtin_types[i];
        }
    }
    return NULL;
}

// type creation functions
Type *type_create_builtin(TypeKind kind, unsigned size, unsigned alignment, bool is_signed)
{
    Type *type      = malloc(sizeof(Type));
    type->kind      = kind;
    type->size      = size;
    type->alignment = alignment;
    type->is_signed = is_signed;
    return type;
}

Type *type_create_ptr(Type *base)
{
    Type *type      = malloc(sizeof(Type));
    type->kind      = TYPE_PTR;
    type->size      = 8; // 64-bit pointers
    type->alignment = 8;
    type->is_signed = false;
    type->ptr.base  = base;
    return type;
}

Type *type_create_array(Type *elem_type, int size)
{
    Type *type            = malloc(sizeof(Type));
    type->kind            = TYPE_ARRAY;
    type->size            = (size > 0) ? size * elem_type->size : 0;
    type->alignment       = elem_type->alignment;
    type->is_signed       = false;
    type->array.elem_type = elem_type;
    type->array.size      = size;
    return type;
}

Type *type_create_function(Type **param_types, int param_count, Type *return_type)
{
    Type *type                 = malloc(sizeof(Type));
    type->kind                 = TYPE_FUNCTION;
    type->size                 = 8; // function pointer size
    type->alignment            = 8;
    type->is_signed            = false;
    type->function.param_types = param_types;
    type->function.param_count = param_count;
    type->function.return_type = return_type;
    return type;
}

// symbol table operations
void symbol_table_init(SymbolTable *table, SymbolTable *parent)
{
    table->capacity = 16;
    table->count    = 0;
    table->symbols  = calloc(table->capacity, sizeof(Symbol *));
    table->parent   = parent;
}

void symbol_table_dnit(SymbolTable *table)
{
    for (int i = 0; i < table->capacity; i++)
    {
        Symbol *symbol = table->symbols[i];
        while (symbol)
        {
            Symbol *next = symbol->next;
            free(symbol->name);
            free(symbol);
            symbol = next;
        }
    }
    free(table->symbols);
}

Symbol *symbol_table_declare(SymbolTable *table, const char *name, SymbolKind kind, Type *type, AstNode *decl)
{
    // check if already declared in current scope
    if (symbol_table_lookup_current_scope(table, name))
    {
        return NULL; // already declared
    }

    unsigned int index = hash_symbol_name(name, table->capacity);

    Symbol *symbol    = malloc(sizeof(Symbol));
    symbol->kind      = kind;
    symbol->name      = strdup(name);
    symbol->type      = type;
    symbol->decl_node = decl;
    symbol->is_val    = false;
    symbol->module    = NULL;
    symbol->next      = table->symbols[index];

    table->symbols[index] = symbol;
    table->count++;

    return symbol;
}

Symbol *symbol_table_lookup(SymbolTable *table, const char *name)
{
    SymbolTable *current = table;
    while (current)
    {
        Symbol *symbol = symbol_table_lookup_current_scope(current, name);
        if (symbol)
        {
            return symbol;
        }
        current = current->parent;
    }
    return NULL;
}

Symbol *symbol_table_lookup_current_scope(SymbolTable *table, const char *name)
{
    unsigned int index  = hash_symbol_name(name, table->capacity);
    Symbol      *symbol = table->symbols[index];

    while (symbol)
    {
        if (strcmp(symbol->name, name) == 0)
        {
            return symbol;
        }
        symbol = symbol->next;
    }

    return NULL;
}

void symbol_table_push_scope(SemanticAnalyzer *analyzer)
{
    SymbolTable *new_scope = malloc(sizeof(SymbolTable));
    symbol_table_init(new_scope, analyzer->current_scope);
    analyzer->current_scope = new_scope;
}

void symbol_table_pop_scope(SemanticAnalyzer *analyzer)
{
    SymbolTable *old_scope  = analyzer->current_scope;
    analyzer->current_scope = old_scope->parent;
    symbol_table_dnit(old_scope);
    free(old_scope);
}

void symbol_table_iterate(SymbolTable *table, void (*callback)(Symbol *symbol, void *data), void *data)
{
    if (!table || !callback)
        return;

    for (int i = 0; i < table->capacity; i++)
    {
        Symbol *symbol = table->symbols[i];
        while (symbol)
        {
            callback(symbol, data);
            symbol = symbol->next;
        }
    }
}

void type_dnit(Type *type)
{
    if (!type)
        return;

    switch (type->kind)
    {
    case TYPE_ARRAY:
        // elem_type is owned by someone else, don't free
        break;
    case TYPE_FUNCTION:
        free(type->function.param_types);
        break;
    case TYPE_STRUCT:
    case TYPE_UNION:
        free(type->composite.fields);
        free(type->composite.name);
        break;
    case TYPE_ALIAS:
        free(type->alias.name);
        break;
    default:
        break;
    }
}

void semantic_error(SemanticAnalyzer *analyzer, AstNode *node, const char *format, ...)
{
    analyzer->had_error = true;

    va_list args;
    va_start(args, format);

    // format the error message
    char *message = malloc(1024); // reasonable buffer size
    vsnprintf(message, 1024, format, args);

    // add to error list
    semantic_error_list_add(&analyzer->errors, node, message);

    free(message);
    va_end(args);
}

// error list operations
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
        list->capacity = (list->capacity == 0) ? 4 : list->capacity * 2;
        list->errors   = realloc(list->errors, list->capacity * sizeof(SemanticError));
    }

    list->errors[list->count].node    = node;
    list->errors[list->count].message = strdup(message);
    list->count++;
}

void semantic_error_list_print(SemanticErrorList *list, Lexer *lexer, const char *file_path)
{
    for (int i = 0; i < list->count; i++)
    {
        SemanticError *error = &list->errors[i];

        if (error->node && error->node->token && lexer)
        {
            Token *token     = error->node->token;
            int    line      = lexer_get_pos_line(lexer, token->pos);
            int    col       = lexer_get_pos_line_offset(lexer, token->pos);
            char  *line_text = lexer_get_line_text(lexer, line);
            if (line_text == NULL)
            {
                line_text = strdup("unable to retrieve line text");
            }

            printf("error: %s\n", error->message);
            printf("%s:%d:%d\n", file_path ? file_path : "<unknown>", line + 1, col);
            printf("%5d | %-*s\n", line + 1, col > 1 ? col - 1 : 0, line_text);
            printf("      | %*s^\n", col - 1, "");

            free(line_text);
        }
        else
        {
            // fallback for errors without location info
            printf("error: %s\n", error->message);
            if (file_path)
                printf("%s: <location unknown>\n", file_path);
        }
    }
}

// stub implementations for remaining functions
bool semantic_analyze_module(SemanticAnalyzer *analyzer, Module *module)
{
    // create a separate scope for the module to avoid symbol conflicts
    SymbolTable *old_scope = analyzer->current_scope;

    // create module scope with global scope as parent (for builtin types)
    SymbolTable *module_scope = malloc(sizeof(SymbolTable));
    symbol_table_init(module_scope, analyzer->global_scope);
    analyzer->current_scope = module_scope;

    bool result = semantic_analyze_program(analyzer, module->ast);

    // restore original scope
    analyzer->current_scope = old_scope;

    if (result)
    {
        // preserve the module's symbol table for use in imports
        module->symbols = module_scope;
    }
    else
    {
        // clean up on failure
        symbol_table_dnit(module_scope);
        free(module_scope);
    }

    return result;
}

bool semantic_analyze_ext_decl(SemanticAnalyzer *analyzer, AstNode *decl)
{
    Type *type = semantic_analyze_type(analyzer, decl->ext_decl.type);
    if (!type)
        return false;

    Symbol *symbol = symbol_table_declare(analyzer->current_scope, decl->ext_decl.name, SYMBOL_VAR, type, decl);
    if (!symbol)
    {
        semantic_error(analyzer, decl, "symbol already declared: %s", decl->ext_decl.name);
        return false;
    }

    decl->type   = type;
    decl->symbol = symbol;
    return true;
}

bool semantic_analyze_def_decl(SemanticAnalyzer *analyzer, AstNode *decl)
{
    Type *target_type = semantic_analyze_type(analyzer, decl->def_decl.type);
    if (!target_type)
        return false;

    Type *alias_type = type_create_alias(decl->def_decl.name, target_type);

    Symbol *symbol = symbol_table_declare(analyzer->current_scope, decl->def_decl.name, SYMBOL_TYPE, alias_type, decl);
    if (!symbol)
    {
        semantic_error(analyzer, decl, "symbol already declared: %s", decl->def_decl.name);
        type_dnit(alias_type);
        free(alias_type);
        return false;
    }

    decl->type   = alias_type;
    decl->symbol = symbol;
    return true;
}

bool semantic_analyze_var_decl(SemanticAnalyzer *analyzer, AstNode *decl)
{
    Type *var_type = semantic_analyze_type(analyzer, decl->var_decl.type);
    if (!var_type)
        return false;

    Symbol *symbol = symbol_table_declare(analyzer->current_scope, decl->var_decl.name, SYMBOL_VAR, var_type, decl);
    if (!symbol)
    {
        semantic_error(analyzer, decl, "symbol already declared: %s", decl->var_decl.name);
        return false;
    }

    symbol->is_val = decl->var_decl.is_val;

    // analyze initializer if present
    if (decl->var_decl.init)
    {
        Type *init_type = semantic_analyze_expression(analyzer, decl->var_decl.init);
        if (!init_type)
            return false;

        if (!type_is_assignable(var_type, init_type))
        {
            semantic_error(analyzer, decl, "cannot assign value of type '%s' to variable '%s' of type '%s'", type_to_string(init_type), decl->var_decl.name, type_to_string(var_type));
            return false;
        }
    }

    decl->type   = var_type;
    decl->symbol = symbol;
    return true;
}

bool semantic_analyze_str_decl(SemanticAnalyzer *analyzer, AstNode *decl)
{
    Type *struct_type = type_create_struct(decl->str_decl.name);

    // declare the struct type before analyzing fields (for recursive types)
    Symbol *symbol = symbol_table_declare(analyzer->current_scope, decl->str_decl.name, SYMBOL_TYPE, struct_type, decl);
    if (!symbol)
    {
        semantic_error(analyzer, decl, "symbol already declared: %s", decl->str_decl.name);
        type_dnit(struct_type);
        free(struct_type);
        return false;
    }

    // analyze fields
    int field_count                    = decl->str_decl.fields->count;
    struct_type->composite.field_count = field_count;

    if (field_count > 0)
    {
        struct_type->composite.fields = malloc(field_count * sizeof(Symbol *));

        unsigned offset    = 0;
        unsigned max_align = 1;

        for (int i = 0; i < field_count; i++)
        {
            AstNode *field      = decl->str_decl.fields->items[i];
            Type    *field_type = semantic_analyze_type(analyzer, field->field_decl.type);
            if (!field_type)
                return false;

            // align field
            unsigned align = field_type->alignment;
            if (align > max_align)
                max_align = align;
            offset = (offset + align - 1) & ~(align - 1);

            Symbol *field_symbol    = malloc(sizeof(Symbol));
            field_symbol->kind      = SYMBOL_FIELD;
            field_symbol->name      = strdup(field->field_decl.name);
            field_symbol->type      = field_type;
            field_symbol->decl_node = field;
            field_symbol->is_val    = false;
            field_symbol->module    = NULL;
            field_symbol->next      = NULL;

            struct_type->composite.fields[i] = field_symbol;

            offset += field_type->size;
        }

        // align struct size
        offset                 = (offset + max_align - 1) & ~(max_align - 1);
        struct_type->size      = offset;
        struct_type->alignment = max_align;
    }

    decl->type   = struct_type;
    decl->symbol = symbol;
    return true;
}

bool semantic_analyze_uni_decl(SemanticAnalyzer *analyzer, AstNode *decl)
{
    Type *union_type = type_create_union(decl->uni_decl.name);

    // declare the union type before analyzing fields (for recursive types)
    Symbol *symbol = symbol_table_declare(analyzer->current_scope, decl->uni_decl.name, SYMBOL_TYPE, union_type, decl);
    if (!symbol)
    {
        semantic_error(analyzer, decl, "symbol already declared: %s", decl->uni_decl.name);
        type_dnit(union_type);
        free(union_type);
        return false;
    }

    // analyze fields
    int field_count                   = decl->uni_decl.fields->count;
    union_type->composite.field_count = field_count;

    if (field_count > 0)
    {
        union_type->composite.fields = malloc(field_count * sizeof(Symbol *));

        unsigned max_size  = 0;
        unsigned max_align = 1;

        for (int i = 0; i < field_count; i++)
        {
            AstNode *field      = decl->uni_decl.fields->items[i];
            Type    *field_type = semantic_analyze_type(analyzer, field->field_decl.type);
            if (!field_type)
                return false;

            if (field_type->size > max_size)
                max_size = field_type->size;
            if (field_type->alignment > max_align)
                max_align = field_type->alignment;

            Symbol *field_symbol    = malloc(sizeof(Symbol));
            field_symbol->kind      = SYMBOL_FIELD;
            field_symbol->name      = strdup(field->field_decl.name);
            field_symbol->type      = field_type;
            field_symbol->decl_node = field;
            field_symbol->is_val    = false;
            field_symbol->module    = NULL;
            field_symbol->next      = NULL;

            union_type->composite.fields[i] = field_symbol;
        }

        // align union size
        max_size              = (max_size + max_align - 1) & ~(max_align - 1);
        union_type->size      = max_size;
        union_type->alignment = max_align;
    }

    decl->type   = union_type;
    decl->symbol = symbol;
    return true;
}

bool semantic_analyze_statement(SemanticAnalyzer *analyzer, AstNode *stmt)
{
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
        // analyze condition
        if (stmt->for_stmt.cond)
        {
            Type *cond_type = semantic_analyze_expression(analyzer, stmt->for_stmt.cond);
            if (!cond_type)
                return false;
        }
        // analyze body
        return semantic_analyze_statement(analyzer, stmt->for_stmt.body);
    case AST_BRK_STMT:
    case AST_CNT_STMT:
        return true; // no semantic analysis needed
    default:
        semantic_error(analyzer, stmt, "unknown statement kind");
        return false;
    }
}

bool semantic_analyze_block_stmt(SemanticAnalyzer *analyzer, AstNode *stmt)
{
    symbol_table_push_scope(analyzer);

    bool success = true;
    for (int i = 0; i < stmt->block_stmt.stmts->count; i++)
    {
        AstNode *child = stmt->block_stmt.stmts->items[i];

        // handle local declarations
        if (child->kind == AST_VAL_DECL || child->kind == AST_VAR_DECL)
        {
            if (!semantic_analyze_var_decl(analyzer, child))
                success = false;
        }
        else
        {
            if (!semantic_analyze_statement(analyzer, child))
                success = false;
        }
    }

    symbol_table_pop_scope(analyzer);
    return success;
}

bool semantic_analyze_ret_stmt(SemanticAnalyzer *analyzer, AstNode *stmt)
{
    // check if we're inside a function
    if (!analyzer->current_function_return_type)
    {
        semantic_error(analyzer, stmt, "return statement outside function");
        return false;
    }

    if (stmt->ret_stmt.expr)
    {
        // function has return value
        Type *return_type = semantic_analyze_expression(analyzer, stmt->ret_stmt.expr);
        if (!return_type)
            return false;

        // check if function expects a return value
        if (!analyzer->current_function_return_type)
        {
            semantic_error(analyzer, stmt, "unexpected return value in function with no return type");
            return false;
        }

        // check return type compatibility
        if (!type_is_assignable(analyzer->current_function_return_type, return_type))
        {
            semantic_error(analyzer, stmt, "cannot return value of type '%s' from function returning '%s'", type_to_string(return_type), type_to_string(analyzer->current_function_return_type));
            return false;
        }

        stmt->ret_stmt.expr->type = return_type;
    }
    else
    {
        // function returns nothing
        if (analyzer->current_function_return_type)
        {
            semantic_error(analyzer, stmt, "missing return value in function that returns type '%s'", type_to_string(analyzer->current_function_return_type));
            return false;
        }
    }

    return true;
}

bool semantic_analyze_if_stmt(SemanticAnalyzer *analyzer, AstNode *stmt)
{
    // analyze condition
    Type *cond_type = semantic_analyze_expression(analyzer, stmt->if_stmt.cond);
    if (!cond_type)
        return false;

    // analyze then statement
    if (!semantic_analyze_statement(analyzer, stmt->if_stmt.then_stmt))
        return false;

    // analyze else statement if present
    if (stmt->if_stmt.else_stmt)
    {
        if (!semantic_analyze_statement(analyzer, stmt->if_stmt.else_stmt))
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
    case AST_IDENT_EXPR:
        return semantic_analyze_ident_expr(analyzer, expr);
    case AST_LIT_EXPR:
    {
        Type *type = NULL;
        switch (expr->lit_expr.kind)
        {
        case TOKEN_LIT_INT:
            type = semantic_get_builtin_type(analyzer, "i32"); // default integer type
            break;
        case TOKEN_LIT_FLOAT:
            type = semantic_get_builtin_type(analyzer, "f64"); // default float type
            break;
        case TOKEN_LIT_CHAR:
            type = semantic_get_builtin_type(analyzer, "u8");
            break;
        case TOKEN_LIT_STRING:
            type = type_create_ptr(semantic_get_builtin_type(analyzer, "u8")); // *u8
            break;
        default:
            semantic_error(analyzer, expr, "unknown literal type");
            return NULL;
        }
        expr->type = type;
        return type;
    }
    case AST_INDEX_EXPR:
    {
        Type *array_type = semantic_analyze_expression(analyzer, expr->index_expr.array);
        Type *index_type = semantic_analyze_expression(analyzer, expr->index_expr.index);

        if (!array_type || !index_type)
            return NULL;

        if (array_type->kind != TYPE_ARRAY && array_type->kind != TYPE_PTR)
        {
            semantic_error(analyzer, expr, "cannot index expression of type '%s' (not an array or pointer)", type_to_string(array_type));
            return NULL;
        }

        // check index type is integer
        if (index_type->kind != TYPE_INT)
        {
            semantic_error(analyzer, expr, "array index must be an integer, not '%s'", type_to_string(index_type));
            return NULL;
        }

        Type *elem_type = (array_type->kind == TYPE_ARRAY) ? array_type->array.elem_type : array_type->ptr.base;
        expr->type      = elem_type;
        return elem_type;
    }
    case AST_FIELD_EXPR:
    {
        // check if this is module qualified access (e.g., adder.function_name) BEFORE analyzing the object
        if (expr->field_expr.object->kind == AST_IDENT_EXPR)
        {
            Symbol *object_symbol = symbol_table_lookup(analyzer->current_scope, expr->field_expr.object->ident_expr.name);
            if (object_symbol && object_symbol->kind == SYMBOL_MODULE)
            {
                // this is qualified access to a module
                Module *module = object_symbol->module;
                if (!module || !module->symbols)
                {
                    semantic_error(analyzer, expr, "module has no symbols: %s", expr->field_expr.object->ident_expr.name);
                    return NULL;
                }

                // look up the symbol in the module's symbol table
                Symbol *symbol = symbol_table_lookup_current_scope(module->symbols, expr->field_expr.field);
                if (!symbol)
                {
                    semantic_error(analyzer, expr, "module '%s' has no symbol named '%s'", expr->field_expr.object->ident_expr.name, expr->field_expr.field);
                    return NULL;
                }

                if (symbol->kind != SYMBOL_VAR && symbol->kind != SYMBOL_FUNC && symbol->kind != SYMBOL_PARAM)
                {
                    semantic_error(analyzer, expr, "'%s' is not a value", expr->field_expr.field);
                    return NULL;
                }

                expr->type   = symbol->type;
                expr->symbol = symbol;

                // set the object's type and symbol for completeness
                expr->field_expr.object->type   = NULL; // modules don't have a type
                expr->field_expr.object->symbol = object_symbol;

                return symbol->type;
            }
        }

        // not a module access, analyze as regular struct/union field access
        Type *object_type = semantic_analyze_expression(analyzer, expr->field_expr.object);
        if (!object_type)
            return NULL;

        if (object_type->kind != TYPE_STRUCT && object_type->kind != TYPE_UNION)
        {
            semantic_error(analyzer, expr, "cannot access field '%s' on expression of type '%s' (not a struct or union)", expr->field_expr.field, type_to_string(object_type));
            return NULL;
        }

        // look up field in struct/union
        const char *field_name = expr->field_expr.field;
        for (int i = 0; i < object_type->composite.field_count; i++)
        {
            Symbol *field = object_type->composite.fields[i];
            if (strcmp(field->name, field_name) == 0)
            {
                expr->type   = field->type;
                expr->symbol = field;
                return field->type;
            }
        }

        semantic_error(analyzer, expr, "no field named '%s' in %s", field_name, type_to_string(object_type));
        return NULL;
    }
    case AST_CAST_EXPR:
    {
        Type *target_type = semantic_analyze_type(analyzer, expr->cast_expr.type);
        Type *source_type = semantic_analyze_expression(analyzer, expr->cast_expr.expr);

        if (!target_type || !source_type)
            return NULL;

        // validate cast is legal - for now allow most casts between compatible sizes
        if ((target_type->kind == TYPE_INT || target_type->kind == TYPE_FLOAT || target_type->kind == TYPE_PTR) && (source_type->kind == TYPE_INT || source_type->kind == TYPE_FLOAT || source_type->kind == TYPE_PTR))
        {
            expr->type = target_type;
            return target_type;
        }

        semantic_error(analyzer, expr, "invalid cast from '%s' to '%s'", type_to_string(source_type), type_to_string(target_type));
        return NULL;
    }
    case AST_STRUCT_EXPR:
    {
        Type *target_type = semantic_analyze_type(analyzer, expr->struct_expr.type);
        if (!target_type)
            return NULL;

        if (target_type->kind != TYPE_STRUCT && target_type->kind != TYPE_UNION)
        {
            semantic_error(analyzer, expr, "cannot initialize type '%s' with composite literal (must be struct or union)", type_to_string(target_type));
            return NULL;
        }

        // for unions, only one field can be initialized
        if (target_type->kind == TYPE_UNION && expr->struct_expr.fields->count > 1)
        {
            semantic_error(analyzer, expr, "union can only initialize one field, got %d", expr->struct_expr.fields->count);
            return NULL;
        }

        // analyze field initializers and check they match the struct/union definition
        for (int i = 0; i < expr->struct_expr.fields->count; i++)
        {
            AstNode *field_init = expr->struct_expr.fields->items[i];

            // field initializers are stored as field expressions where:
            // field_expr.field = field name, field_expr.object = value expression
            if (field_init->kind != AST_FIELD_EXPR)
            {
                semantic_error(analyzer, field_init, "invalid field initializer");
                return NULL;
            }

            const char *field_name = field_init->field_expr.field;

            // find this field in the struct/union definition
            Symbol *field_symbol = NULL;
            for (int j = 0; j < target_type->composite.field_count; j++)
            {
                Symbol *field = target_type->composite.fields[j];
                if (strcmp(field->name, field_name) == 0)
                {
                    field_symbol = field;
                    break;
                }
            }

            if (!field_symbol)
            {
                semantic_error(analyzer, field_init, "%s '%s' has no field named '%s'", target_type->kind == TYPE_STRUCT ? "struct" : "union", type_to_string(target_type), field_name);
                return NULL;
            }

            // analyze the value expression
            Type *value_type = semantic_analyze_expression(analyzer, field_init->field_expr.object);
            if (!value_type)
                return NULL;

            // check type compatibility
            if (!type_is_assignable(field_symbol->type, value_type))
            {
                semantic_error(analyzer, field_init, "cannot assign value of type '%s' to field '%s' of type '%s'", type_to_string(value_type), field_name, type_to_string(field_symbol->type));
                return NULL;
            }

            // set the field reference for the initializer
            field_init->symbol = field_symbol;
        }

        expr->type = target_type;
        return target_type;
    }
    default:
        semantic_error(analyzer, expr, "unknown expression kind");
        return NULL;
    }
}

Type *semantic_analyze_binary_expr(SemanticAnalyzer *analyzer, AstNode *expr)
{
    Type *left_type  = semantic_analyze_expression(analyzer, expr->binary_expr.left);
    Type *right_type = semantic_analyze_expression(analyzer, expr->binary_expr.right);

    if (!left_type || !right_type)
        return NULL;

    // get common type for operands
    Type *common_type = type_get_common_type(left_type, right_type);
    if (!common_type)
    {
        semantic_error(analyzer, expr, "incompatible types '%s' and '%s' in binary expression", type_to_string(left_type), type_to_string(right_type));
        return NULL;
    }

    Type *result_type = common_type;

    // for comparison operators, result is always a boolean (using u8)
    switch (expr->binary_expr.op)
    {
    case TOKEN_EQUAL_EQUAL:
    case TOKEN_BANG_EQUAL:
    case TOKEN_LESS:
    case TOKEN_GREATER:
    case TOKEN_LESS_EQUAL:
    case TOKEN_GREATER_EQUAL:
        result_type = semantic_get_builtin_type(analyzer, "u8"); // boolean result
        break;
    default:
        break;
    }

    expr->type = result_type;
    return result_type;
}

Type *semantic_analyze_unary_expr(SemanticAnalyzer *analyzer, AstNode *expr)
{
    Type *operand_type = semantic_analyze_expression(analyzer, expr->unary_expr.expr);
    if (!operand_type)
        return NULL;

    Type *result_type = operand_type;

    switch (expr->unary_expr.op)
    {
    case TOKEN_MINUS:
    case TOKEN_PLUS:
        if (operand_type->kind != TYPE_INT && operand_type->kind != TYPE_FLOAT)
        {
            semantic_error(analyzer, expr, "unary '%c' cannot be applied to operand of type '%s'", (expr->unary_expr.op == TOKEN_MINUS) ? '-' : '+', type_to_string(operand_type));
            return NULL;
        }
        break;
    case TOKEN_BANG:
        result_type = semantic_get_builtin_type(analyzer, "u8"); // boolean result
        break;
    case TOKEN_TILDE:
        if (operand_type->kind != TYPE_INT)
        {
            semantic_error(analyzer, expr, "bitwise not '~' cannot be applied to operand of type '%s' (requires integer type)", type_to_string(operand_type));
            return NULL;
        }
        break;
    case TOKEN_AT: // dereference
        if (operand_type->kind != TYPE_PTR)
        {
            semantic_error(analyzer, expr, "cannot dereference expression of type '%s' (not a pointer)", type_to_string(operand_type));
            return NULL;
        }
        result_type = operand_type->ptr.base;
        if (!result_type)
        {
            semantic_error(analyzer, expr, "cannot dereference untyped pointer 'ptr'");
            return NULL;
        }
        break;
    case TOKEN_QUESTION: // address-of
        // check if the operand is an lvalue (addressable)
        if (!is_lvalue_expression(expr->unary_expr.expr))
        {
            semantic_error(analyzer, expr, "cannot take address of non-lvalue expression");
            return NULL;
        }
        result_type = type_create_ptr(operand_type);
        break;
    default:
        semantic_error(analyzer, expr, "unknown unary operator");
        return NULL;
    }

    expr->type = result_type;
    return result_type;
}

Type *semantic_analyze_call_expr(SemanticAnalyzer *analyzer, AstNode *expr)
{
    Type *func_type = semantic_analyze_expression(analyzer, expr->call_expr.func);
    if (!func_type)
        return NULL;

    if (func_type->kind != TYPE_FUNCTION)
    {
        semantic_error(analyzer, expr, "cannot call expression of type '%s' (not a function)", type_to_string(func_type));
        return NULL;
    }

    // check argument count
    int expected_args = func_type->function.param_count;
    int actual_args   = expr->call_expr.args->count;

    if (expected_args != actual_args)
    {
        semantic_error(analyzer, expr, "function expects %d arguments, got %d", expected_args, actual_args);
        return NULL;
    }

    // check argument types
    for (int i = 0; i < actual_args; i++)
    {
        Type *arg_type = semantic_analyze_expression(analyzer, expr->call_expr.args->items[i]);
        if (!arg_type)
            return NULL;

        Type *param_type = func_type->function.param_types[i];
        if (!type_is_assignable(param_type, arg_type))
        {
            semantic_error(analyzer, expr, "argument %d: cannot pass '%s' to parameter of type '%s'", i + 1, type_to_string(arg_type), type_to_string(param_type));
            return NULL;
        }
    }

    Type *return_type = func_type->function.return_type;
    expr->type        = return_type;
    return return_type; // can be NULL for functions with no return
}

Type *semantic_analyze_ident_expr(SemanticAnalyzer *analyzer, AstNode *expr)
{
    Symbol *symbol = symbol_table_lookup(analyzer->current_scope, expr->ident_expr.name);
    if (!symbol)
    {
        semantic_error(analyzer, expr, "undefined identifier: %s", expr->ident_expr.name);
        return NULL;
    }

    if (symbol->kind == SYMBOL_MODULE)
    {
        semantic_error(analyzer, expr, "cannot use module '%s' as a value (use qualified access like '%s.symbol')", expr->ident_expr.name, expr->ident_expr.name);
        return NULL;
    }

    if (symbol->kind != SYMBOL_VAR && symbol->kind != SYMBOL_FUNC && symbol->kind != SYMBOL_PARAM)
    {
        semantic_error(analyzer, expr, "identifier is not a value: %s", expr->ident_expr.name);
        return NULL;
    }

    expr->type   = symbol->type;
    expr->symbol = symbol;
    return symbol->type;
}

Type *type_create_struct(const char *name)
{
    Type *type                  = malloc(sizeof(Type));
    type->kind                  = TYPE_STRUCT;
    type->size                  = 0;
    type->alignment             = 1;
    type->is_signed             = false;
    type->composite.fields      = NULL;
    type->composite.field_count = 0;
    type->composite.name        = name ? strdup(name) : NULL;
    return type;
}

Type *type_create_union(const char *name)
{
    Type *type                  = malloc(sizeof(Type));
    type->kind                  = TYPE_UNION;
    type->size                  = 0;
    type->alignment             = 1;
    type->is_signed             = false;
    type->composite.fields      = NULL;
    type->composite.field_count = 0;
    type->composite.name        = name ? strdup(name) : NULL;
    return type;
}

Type *type_create_alias(const char *name, Type *target)
{
    Type *type         = malloc(sizeof(Type));
    type->kind         = TYPE_ALIAS;
    type->size         = target->size;
    type->alignment    = target->alignment;
    type->is_signed    = target->is_signed;
    type->alias.name   = strdup(name);
    type->alias.target = target;
    return type;
}

bool type_equals(Type *a, Type *b)
{
    if (!a || !b)
        return a == b;

    if (a->kind != b->kind)
        return false;

    switch (a->kind)
    {
    case TYPE_PTR:
        return type_equals(a->ptr.base, b->ptr.base);
    case TYPE_ARRAY:
        return a->array.size == b->array.size && type_equals(a->array.elem_type, b->array.elem_type);
    case TYPE_FUNCTION:
        if (a->function.param_count != b->function.param_count)
            return false;
        if (!type_equals(a->function.return_type, b->function.return_type))
            return false;
        for (int i = 0; i < a->function.param_count; i++)
        {
            if (!type_equals(a->function.param_types[i], b->function.param_types[i]))
                return false;
        }
        return true;
    case TYPE_ALIAS:
        return type_equals(a->alias.target, b->alias.target);
    default:
        return a == b; // builtin types are compared by identity
    }
}

bool type_is_assignable(Type *target, Type *source)
{
    if (type_equals(target, source))
        return true;

    // allow assignment between compatible numeric types
    if (target->kind == TYPE_INT && source->kind == TYPE_INT)
    {
        // implement proper numeric promotion rules
        // allow assignment if target can hold source value
        return target->size >= source->size || (target->is_signed == source->is_signed);
    }

    if (target->kind == TYPE_FLOAT && source->kind == TYPE_FLOAT)
    {
        return true;
    }

    // allow ptr assignment if target is untyped ptr
    if (target->kind == TYPE_PTR && source->kind == TYPE_PTR)
    {
        if (!target->ptr.base) // untyped ptr can accept any pointer
            return true;
        return type_equals(target->ptr.base, source->ptr.base);
    }

    // resolve aliases
    if (target->kind == TYPE_ALIAS)
        return type_is_assignable(target->alias.target, source);
    if (source->kind == TYPE_ALIAS)
        return type_is_assignable(target, source->alias.target);

    return false;
}

Type *type_get_common_type(Type *a, Type *b)
{
    if (type_equals(a, b))
        return a;

    // numeric promotion rules
    if (a->kind == TYPE_INT && b->kind == TYPE_INT)
    {
        // promote to larger type
        if (a->size >= b->size)
            return a;
        else
            return b;
    }

    if (a->kind == TYPE_FLOAT && b->kind == TYPE_FLOAT)
    {
        // promote to larger type
        if (a->size >= b->size)
            return a;
        else
            return b;
    }

    // int to float promotion
    if (a->kind == TYPE_INT && b->kind == TYPE_FLOAT)
        return b;
    if (a->kind == TYPE_FLOAT && b->kind == TYPE_INT)
        return a;

    // pointer compatibility
    if (a->kind == TYPE_PTR && b->kind == TYPE_PTR)
    {
        if (!a->ptr.base)
            return a; // untyped ptr
        if (!b->ptr.base)
            return b; // untyped ptr
        if (type_equals(a->ptr.base, b->ptr.base))
            return a;
    }

    return NULL; // no common type
}

// helper function to get a human-readable type name for error messages
static const char *type_to_string(Type *type)
{
    if (!type)
        return "no type";

    switch (type->kind)
    {
    case TYPE_PTR:
        if (!type->ptr.base)
            return "ptr";
        else
        {
            static char ptr_str[256];
            snprintf(ptr_str, sizeof(ptr_str), "*%s", type_to_string(type->ptr.base));
            return ptr_str;
        }
    case TYPE_INT:
    case TYPE_FLOAT:
        // find builtin type name
        for (size_t i = 0; i < BUILTIN_COUNT; i++)
        {
            if (builtin_types[i].kind == type->kind && builtin_types[i].size == type->size && builtin_types[i].is_signed == type->is_signed)
            {
                return builtin_types[i].name;
            }
        }
        return type->kind == TYPE_INT ? "integer" : "float";
    case TYPE_ARRAY:
    {
        static char array_str[256];
        if (type->array.size == -1)
            snprintf(array_str, sizeof(array_str), "[_]%s", type_to_string(type->array.elem_type));
        else
            snprintf(array_str, sizeof(array_str), "[%d]%s", type->array.size, type_to_string(type->array.elem_type));
        return array_str;
    }
    case TYPE_FUNCTION:
        return "function";
    case TYPE_STRUCT:
        if (type->composite.name)
        {
            static char struct_str[256];
            snprintf(struct_str, sizeof(struct_str), "struct %s", type->composite.name);
            return struct_str;
        }
        return "anonymous struct";
    case TYPE_UNION:
        if (type->composite.name)
        {
            static char union_str[256];
            snprintf(union_str, sizeof(union_str), "union %s", type->composite.name);
            return union_str;
        }
        return "anonymous union";
    case TYPE_ALIAS:
        return type->alias.name;
    default:
        return "unknown type";
    }
}
