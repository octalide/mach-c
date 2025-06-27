#include "semantic.h"
#include "lexer.h"
#include "operator.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// builtin type sizes and alignments
static struct
{
    const char *name;
    TypeKind    kind;
    unsigned    size;
    unsigned    align;
    bool        is_signed;
} builtin_types[] = {{"void", TYPE_VOID, 0, 0, false}, {"any", TYPE_VOID, 0, 0, false}, // any is void in C
                     {"u8", TYPE_INT, 1, 1, false},    {"u16", TYPE_INT, 2, 2, false},  {"u32", TYPE_INT, 4, 4, false},  {"u64", TYPE_INT, 8, 8, false},  {"i8", TYPE_INT, 1, 1, true},   {"i16", TYPE_INT, 2, 2, true},
                     {"i32", TYPE_INT, 4, 4, true},    {"i64", TYPE_INT, 8, 8, true},   {"f32", TYPE_FLOAT, 4, 4, true}, {"f64", TYPE_FLOAT, 8, 8, true}, {NULL, TYPE_ERROR, 0, 0, false}};

// error management functions
void semantic_error_add(SemanticAnalyzer *analyzer, Node *node, const char *fmt, ...)
{
    if (!analyzer || !fmt)
        return;

    // initialize error list if needed
    if (!analyzer->errors.errors)
    {
        analyzer->errors.capacity = 10;
        analyzer->errors.errors   = calloc(analyzer->errors.capacity, sizeof(SemanticError *));
        analyzer->errors.count    = 0;
    }

    // expand error list if needed
    if (analyzer->errors.count >= analyzer->errors.capacity)
    {
        analyzer->errors.capacity *= 2;
        analyzer->errors.errors = realloc(analyzer->errors.errors, analyzer->errors.capacity * sizeof(SemanticError *));
    }

    // format the error message
    va_list args;
    va_start(args, fmt);

    char message[1024];
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    // create the error
    SemanticError *error = calloc(1, sizeof(SemanticError));
    error->message       = strdup(message);
    error->node          = node;

    // set location info from token and lexer
    if (node && node->token && analyzer->lexer)
    {
        error->filename  = strdup(analyzer->filename);
        error->line      = lexer_get_pos_line(analyzer->lexer, node->token->pos);
        error->column    = lexer_get_pos_line_offset(analyzer->lexer, node->token->pos);
        error->line_text = lexer_get_line_text(analyzer->lexer, error->line);
    }
    else
    {
        error->filename  = strdup(analyzer->filename ? analyzer->filename : "<unknown>");
        error->line      = 1;
        error->column    = 1;
        error->line_text = NULL;
    }

    analyzer->errors.errors[analyzer->errors.count] = error;
    analyzer->errors.count++;
    analyzer->has_errors = true;
}

void semantic_error_print_all(SemanticAnalyzer *analyzer)
{
    if (!analyzer || analyzer->errors.count == 0)
        return;

    for (size_t i = 0; i < analyzer->errors.count; i++)
    {
        SemanticError *error = analyzer->errors.errors[i];
        if (!error)
            continue;

        // print error with location
        if (error->line > 0)
        {
            fprintf(stderr, "%s:%d:%d: error: %s\n", error->filename, error->line, error->column, error->message);

            // print line context if available
            if (error->line_text)
            {
                fprintf(stderr, "%s\n", error->line_text);

                // print caret pointing to column
                for (int j = 1; j < error->column; j++)
                {
                    fprintf(stderr, " ");
                }
                fprintf(stderr, "^\n");
            }
        }
        else
        {
            fprintf(stderr, "error: %s\n", error->message);
        }
    }
}

void semantic_error_free_all(SemanticAnalyzer *analyzer)
{
    if (!analyzer || !analyzer->errors.errors)
        return;

    for (size_t i = 0; i < analyzer->errors.count; i++)
    {
        SemanticError *error = analyzer->errors.errors[i];
        if (error)
        {
            free(error->message);
            free(error->filename);
            free(error->line_text);
            free(error);
        }
    }

    free(analyzer->errors.errors);
    analyzer->errors.errors   = NULL;
    analyzer->errors.count    = 0;
    analyzer->errors.capacity = 0;
}

// type management
Type *type_new(TypeKind kind)
{
    Type *type = calloc(1, sizeof(Type));
    type->kind = kind;
    return type;
}

void type_free(Type *type)
{
    if (!type)
        return;

    switch (type->kind)
    {
    case TYPE_ARRAY:
    case TYPE_POINTER:
        // base types are shared, don't free
        break;
    case TYPE_FUNCTION:
        free(type->function.params);
        break;
    case TYPE_STRUCT:
    case TYPE_UNION:
        free(type->composite.name);
        free(type->composite.fields);
        break;
    case TYPE_ALIAS:
        free(type->alias.name);
        break;
    default:
        break;
    }
    free(type);
}

Type *type_builtin(const char *name)
{
    for (int i = 0; builtin_types[i].name != NULL; i++)
    {
        if (strcmp(builtin_types[i].name, name) == 0)
        {
            Type *type      = type_new(builtin_types[i].kind);
            type->size      = builtin_types[i].size;
            type->align     = builtin_types[i].align;
            type->is_signed = builtin_types[i].is_signed;
            return type;
        }
    }
    return NULL;
}

Type *type_pointer(Type *base)
{
    Type *type         = type_new(TYPE_POINTER);
    type->pointer.base = base;
    type->size         = 8; // 64-bit pointers
    type->align        = 8;
    return type;
}

Type *type_array(Type *element, size_t size)
{
    Type *type          = type_new(TYPE_ARRAY);
    type->array.element = element;
    type->array.size    = size;
    type->size          = size ? size * element->size : 0;
    type->align         = element->align;
    return type;
}

Type *type_function(Type *return_type, Type **params, bool is_variadic)
{
    Type *type                 = type_new(TYPE_FUNCTION);
    type->function.return_type = return_type;
    type->function.params      = params;
    type->function.is_variadic = is_variadic;
    type->size                 = 8; // function pointer size
    type->align                = 8;
    return type;
}

Type *type_struct(const char *name)
{
    Type *type             = type_new(TYPE_STRUCT);
    type->composite.name   = name ? strdup(name) : NULL;
    type->composite.fields = NULL;
    return type;
}

Type *type_union(const char *name)
{
    Type *type             = type_new(TYPE_UNION);
    type->composite.name   = name ? strdup(name) : NULL;
    type->composite.fields = NULL;
    return type;
}

Type *type_alias(const char *name, Type *base)
{
    Type *type       = type_new(TYPE_ALIAS);
    type->alias.name = strdup(name);
    type->alias.base = base;
    type->size       = base->size;
    type->align      = base->align;
    type->is_signed  = base->is_signed;
    return type;
}

bool type_equal(Type *a, Type *b)
{
    if (!a || !b)
        return false;
    if (a == b)
        return true;

    // resolve aliases
    while (a->kind == TYPE_ALIAS)
        a = a->alias.base;
    while (b->kind == TYPE_ALIAS)
        b = b->alias.base;

    if (a->kind != b->kind)
        return false;

    switch (a->kind)
    {
    case TYPE_VOID:
    case TYPE_BOOL:
    case TYPE_INT:
    case TYPE_FLOAT:
        return a->size == b->size && a->is_signed == b->is_signed;
    case TYPE_POINTER:
        return type_equal(a->pointer.base, b->pointer.base);
    case TYPE_ARRAY:
        return a->array.size == b->array.size && type_equal(a->array.element, b->array.element);
    case TYPE_FUNCTION:
        if (!type_equal(a->function.return_type, b->function.return_type))
            return false;
        if (a->function.is_variadic != b->function.is_variadic)
            return false;
        // compare params
        size_t count_a = 0, count_b = 0;
        if (a->function.params)
        {
            while (a->function.params[count_a])
                count_a++;
        }
        if (b->function.params)
        {
            while (b->function.params[count_b])
                count_b++;
        }
        if (count_a != count_b)
            return false;
        for (size_t i = 0; i < count_a; i++)
        {
            if (!type_equal(a->function.params[i], b->function.params[i]))
                return false;
        }
        return true;
    case TYPE_STRUCT:
    case TYPE_UNION:
        return a == b; // structural types by reference
    default:
        return false;
    }
}

bool type_compatible(Type *a, Type *b)
{
    if (type_equal(a, b))
        return true;

    // resolve aliases
    while (a->kind == TYPE_ALIAS)
        a = a->alias.base;
    while (b->kind == TYPE_ALIAS)
        b = b->alias.base;

    // check for :: cast compatibility (same size)
    if (a->size == b->size && a->size > 0)
    {
        // allow casting between same-size types
        return true;
    }

    // pointer to void* compatibility
    if (a->kind == TYPE_POINTER && b->kind == TYPE_POINTER)
    {
        if (a->pointer.base->kind == TYPE_VOID || b->pointer.base->kind == TYPE_VOID)
            return true;
    }

    // allow literal integers to be assigned to any integer type
    // this handles cases like: var x: i32 = 42;
    // where 42 is analyzed as u8 but needs to be assigned to i32
    if (a->kind == TYPE_INT && b->kind == TYPE_INT)
    {
        return true;
    }

    return false;
}

size_t type_sizeof(Type *type)
{
    while (type && type->kind == TYPE_ALIAS)
    {
        type = type->alias.base;
    }
    return type ? type->size : 0;
}

size_t type_alignof(Type *type)
{
    while (type && type->kind == TYPE_ALIAS)
    {
        type = type->alias.base;
    }
    return type ? type->align : 0;
}

// scope management
Scope *scope_new(Scope *parent)
{
    Scope *scope   = calloc(1, sizeof(Scope));
    scope->parent  = parent;
    scope->symbols = calloc(1, sizeof(Symbol *));
    scope->level   = parent ? parent->level + 1 : 0;
    return scope;
}

void scope_free(Scope *scope)
{
    if (!scope)
        return;

    if (scope->symbols)
    {
        for (int i = 0; scope->symbols[i]; i++)
        {
            symbol_free(scope->symbols[i]);
        }
        free(scope->symbols);
    }
    free(scope);
}

Symbol *scope_lookup(Scope *scope, const char *name)
{
    while (scope)
    {
        if (scope->symbols)
        {
            for (int i = 0; scope->symbols[i]; i++)
            {
                if (strcmp(scope->symbols[i]->name, name) == 0)
                {
                    return scope->symbols[i];
                }
            }
        }
        scope = scope->parent;
    }
    return NULL;
}

Symbol *scope_define(Scope *scope, const char *name, SymbolKind kind, Type *type)
{
    // check for redefinition in current scope
    if (scope->symbols)
    {
        for (int i = 0; scope->symbols[i]; i++)
        {
            if (strcmp(scope->symbols[i]->name, name) == 0)
            {
                return NULL; // already defined
            }
        }
    }

    Symbol *symbol = symbol_new(name, kind, type);
    symbol->scope  = scope;

    // add to scope
    size_t count = 0;
    if (scope->symbols)
    {
        while (scope->symbols[count])
            count++;
    }

    scope->symbols            = realloc(scope->symbols, (count + 2) * sizeof(Symbol *));
    scope->symbols[count]     = symbol;
    scope->symbols[count + 1] = NULL;

    return symbol;
}

// symbol management
Symbol *symbol_new(const char *name, SymbolKind kind, Type *type)
{
    Symbol *symbol = calloc(1, sizeof(Symbol));
    symbol->name   = strdup(name);
    symbol->kind   = kind;
    symbol->type   = type;
    return symbol;
}

void symbol_free(Symbol *symbol)
{
    if (!symbol)
        return;
    free(symbol->name);
    free(symbol);
}

void semantic_init(SemanticAnalyzer *analyzer, Node *ast, Lexer *lexer, const char *filename)
{
    analyzer->ast        = ast;
    analyzer->global     = scope_new(NULL);
    analyzer->current    = analyzer->global;
    analyzer->has_errors = false;
    analyzer->lexer      = lexer;
    analyzer->filename   = filename ? strdup(filename) : strdup("<unknown>");

    // initialize error list
    analyzer->errors.errors   = NULL;
    analyzer->errors.count    = 0;
    analyzer->errors.capacity = 0;

    // register builtin types
    for (int i = 0; builtin_types[i].name != NULL; i++)
    {
        Type *type      = type_new(builtin_types[i].kind);
        type->size      = builtin_types[i].size;
        type->align     = builtin_types[i].align;
        type->is_signed = builtin_types[i].is_signed;
        scope_define(analyzer->global, builtin_types[i].name, SYMBOL_TYPE, type);
    }
}

void semantic_dnit(SemanticAnalyzer *analyzer)
{
    if (!analyzer)
        return;

    scope_free(analyzer->global);
    semantic_error_free_all(analyzer);
    free(analyzer->filename);

    analyzer->ast      = NULL;
    analyzer->global   = NULL;
    analyzer->current  = NULL;
    analyzer->types    = NULL;
    analyzer->lexer    = NULL;
    analyzer->filename = NULL;
}

// forward declarations for analysis functions
static Type *analyze_type(SemanticAnalyzer *analyzer, Node *node);
static Type *analyze_expression(SemanticAnalyzer *analyzer, Node *node);
static bool  analyze_statement(SemanticAnalyzer *analyzer, Node *node);
static bool  analyze_node(SemanticAnalyzer *analyzer, Node *node);

static Type *analyze_type(SemanticAnalyzer *analyzer, Node *node)
{
    if (!node || !analyzer)
        return NULL;

    switch (node->kind)
    {
    case NODE_IDENTIFIER:
    {
        if (!node->str_value)
        {
            semantic_error_add(analyzer, node, "identifier node missing string value");
            return NULL;
        }

        Symbol *sym = scope_lookup(analyzer->current, node->str_value);
        if (!sym || sym->kind != SYMBOL_TYPE)
        {
            semantic_error_add(analyzer, node, "unknown type '%s'", node->str_value);
            return NULL;
        }
        return sym->type;
    }

    case NODE_TYPE_POINTER:
    {
        if (!node->single)
        {
            semantic_error_add(analyzer, node, "pointer type missing base type");
            return NULL;
        }
        Type *base = analyze_type(analyzer, node->single);
        return base ? type_pointer(base) : NULL;
    }

    case NODE_TYPE_ARRAY:
    {
        if (!node->binary.right)
        {
            semantic_error_add(analyzer, node, "array type missing element type");
            return NULL;
        }

        Type *element = analyze_type(analyzer, node->binary.right);
        if (!element)
            return NULL;

        size_t size = 0;
        if (node->binary.left && node->binary.left->kind == NODE_LIT_INT)
        {
            size = node->binary.left->int_value;
        }
        return type_array(element, size);
    }

    default:
        semantic_error_add(analyzer, node, "unhandled type node kind %d", node->kind);
        return NULL;
    }
}

static Type *analyze_expression(SemanticAnalyzer *analyzer, Node *node)
{
    if (!node || !analyzer)
        return NULL;

    Type *type = NULL;

    switch (node->kind)
    {
    case NODE_LIT_INT:
        type = type_builtin("i32");
        break;

    case NODE_LIT_CHAR:
        // characters are u8
        type = type_builtin("u8");
        break;

    case NODE_LIT_STRING:
        // strings are dynamic arrays of u8: [_]u8
        type = type_array(type_builtin("u8"), 0); // 0 size indicates dynamic array
        break;

    case NODE_IDENTIFIER:
    {
        if (!node->str_value)
        {
            semantic_error_add(analyzer, node, "identifier node missing string value");
            return NULL;
        }

        Symbol *sym = scope_lookup(analyzer->current, node->str_value);
        if (!sym)
        {
            semantic_error_add(analyzer, node, "undefined identifier '%s'", node->str_value);
            return NULL;
        }
        type = sym->type;
        break;
    }

    case NODE_EXPR_BINARY:
    {
        if (!node->binary.left || !node->binary.right)
        {
            semantic_error_add(analyzer, node, "binary expression missing operands");
            return NULL;
        }

        Type *left_type  = analyze_expression(analyzer, node->binary.left);
        Type *right_type = analyze_expression(analyzer, node->binary.right);
        if (!left_type || !right_type)
            return NULL;

        switch (node->binary.op)
        {
        case OP_ASSIGN:
            if (!type_compatible(left_type, right_type))
            {
                semantic_error_add(analyzer, node, "incompatible types in assignment");
                return NULL;
            }
            type = left_type;
            break;

        case OP_ADD:
        case OP_SUB:
            if (!type_compatible(left_type, right_type))
            {
                semantic_error_add(analyzer, node, "incompatible types in arithmetic");
                return NULL;
            }
            type = left_type;
            break;

        case OP_GREATER:
            type = type_builtin("u8");
            break;

        default:
            semantic_error_add(analyzer, node, "unhandled binary operator %d", node->binary.op);
            return NULL;
        }
        break;
    }

    default:
        semantic_error_add(analyzer, node, "unhandled expression node kind %d", node->kind);
        return NULL;
    }

    if (type)
        node->type = type;
    return type;
}

static bool analyze_statement(SemanticAnalyzer *analyzer, Node *node)
{
    if (!node || !analyzer)
        return false;

    switch (node->kind)
    {
    case NODE_STMT_VAL:
    case NODE_STMT_VAR:
    {
        if (!node->decl.name || !node->decl.name->str_value)
        {
            semantic_error_add(analyzer, node, "declaration missing name");
            return false;
        }

        char *name    = node->decl.name->str_value;
        Type *type    = NULL;
        bool  success = true;

        if (node->decl.type)
        {
            type = analyze_type(analyzer, node->decl.type);
            if (!type)
                success = false;
        }
        else if (node->decl.init)
        {
            type = analyze_expression(analyzer, node->decl.init);
            if (!type)
                success = false;
        }

        if (!type)
        {
            semantic_error_add(analyzer, node, "cannot determine type for '%s'", name);
            success = false;
        }

        if (node->decl.init && type)
        {
            Type *init_type = analyze_expression(analyzer, node->decl.init);
            if (!init_type || !type_compatible(type, init_type))
            {
                semantic_error_add(analyzer, node, "incompatible types in initialization");
                success = false;
            }
        }

        if (type)
        {
            SymbolKind kind = node->kind == NODE_STMT_VAL ? SYMBOL_CONSTANT : SYMBOL_VARIABLE;
            if (!scope_define(analyzer->current, name, kind, type))
            {
                semantic_error_add(analyzer, node, "redefinition of '%s'", name);
                success = false;
            }
        }

        return success;
    }

    case NODE_STMT_FUNCTION:
    {
        if (!node->function.name || !node->function.name->str_value)
        {
            semantic_error_add(analyzer, node, "function missing name");
            return false;
        }

        char *name        = node->function.name->str_value;
        Type *return_type = analyze_type(analyzer, node->function.return_type);
        if (!return_type)
            return false;

        // analyze parameters to build function type
        Type **param_types = NULL;
        if (node->function.params)
        {
            size_t count = 0;
            while (node->function.params[count])
                count++;

            param_types = calloc(count + 1, sizeof(Type *));
            for (size_t i = 0; i < count; i++)
            {
                param_types[i] = analyze_type(analyzer, node->function.params[i]->decl.type);
                if (!param_types[i])
                {
                    free(param_types);
                    return false;
                }
            }
        }

        Type *func_type = type_function(return_type, param_types, node->function.is_variadic);
        if (!scope_define(analyzer->current, name, SYMBOL_FUNCTION, func_type))
        {
            semantic_error_add(analyzer, node, "redefinition of function '%s'", name);
            return false;
        }

        if (node->function.body)
        {
            Scope *func_scope = scope_new(analyzer->current);
            Scope *old_scope  = analyzer->current;
            analyzer->current = func_scope;

            // define parameters in function scope
            if (node->function.params)
            {
                for (int i = 0; node->function.params[i]; i++)
                {
                    Node *param      = node->function.params[i];
                    char *param_name = param->decl.name->str_value;
                    Type *param_type = analyze_type(analyzer, param->decl.type);
                    if (!param_type || !scope_define(func_scope, param_name, SYMBOL_VARIABLE, param_type))
                    {
                        analyzer->has_errors = true;
                    }
                }
            }

            bool result = analyze_node(analyzer, node->function.body);

            analyzer->current = old_scope;
            // store function scope for codegen to use
            node->function.scope = func_scope;

            if (!result)
                return false;
        }
        return true;
    }

    case NODE_STMT_IF:
    {
        bool success = true;

        if (node->conditional.condition)
        {
            Type *cond_type = analyze_expression(analyzer, node->conditional.condition);
            if (!cond_type)
                success = false;
        }

        if (!analyze_node(analyzer, node->conditional.body))
            success = false;

        if (node->conditional.else_body)
        {
            if (!analyze_node(analyzer, node->conditional.else_body))
                success = false;
        }

        return success;
    }

    case NODE_STMT_EXPRESSION:
    {
        if (node->single)
        {
            analyze_expression(analyzer, node->single);
        }
        return true;
    }

    case NODE_STMT_RETURN:
    {
        if (node->single)
        {
            analyze_expression(analyzer, node->single);
        }
        return true;
    }

    case NODE_BLOCK:
    {
        if (node->children)
        {
            for (int i = 0; node->children[i]; i++)
            {
                if (!analyze_node(analyzer, node->children[i]))
                    return false;
            }
        }
        return true;
    }

    case NODE_STMT_BREAK:
    case NODE_STMT_CONTINUE:
        return true;

    default:
        semantic_error_add(analyzer, node, "unhandled statement node kind %d (%s)", node->kind, node_kind_name(node->kind));
        return false;
    }
}

static bool analyze_node(SemanticAnalyzer *analyzer, Node *node)
{
    if (!node || !analyzer)
        return false;

    bool success = true;

    switch (node->kind)
    {
    case NODE_PROGRAM:
    case NODE_BLOCK:
        if (node->children)
        {
            for (int i = 0; node->children[i]; i++)
            {
                // continue analyzing even if individual nodes fail
                if (!analyze_node(analyzer, node->children[i]))
                    success = false;
            }
        }
        return success;

    default:
        return analyze_statement(analyzer, node);
    }
}

// main analysis function
bool semantic_analyze(SemanticAnalyzer *analyzer)
{
    if (!analyzer || !analyzer->ast)
        return false;

    analyzer->has_errors = false;
    analyze_node(analyzer, analyzer->ast);

    return !analyzer->has_errors;
}
