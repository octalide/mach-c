#include "semantic.h"
#include "operator.h"
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

void semantic_init(SemanticAnalyzer *analyzer, Node *ast)
{
    analyzer->ast        = ast;
    analyzer->global     = scope_new(NULL);
    analyzer->current    = analyzer->global;
    analyzer->has_errors = false;

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

    analyzer->ast     = NULL;
    analyzer->global  = NULL;
    analyzer->current = NULL;
    analyzer->types   = NULL;
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
            fprintf(stderr, "error: identifier node missing string value\n");
            analyzer->has_errors = true;
            return NULL;
        }

        Symbol *sym = scope_lookup(analyzer->current, node->str_value);
        if (!sym || sym->kind != SYMBOL_TYPE)
        {
            fprintf(stderr, "error: unknown type '%s'\n", node->str_value);
            analyzer->has_errors = true;
            return NULL;
        }
        return sym->type;
    }

    case NODE_TYPE_POINTER:
    {
        if (!node->single)
        {
            fprintf(stderr, "error: pointer type missing base type\n");
            analyzer->has_errors = true;
            return NULL;
        }
        Type *base = analyze_type(analyzer, node->single);
        return base ? type_pointer(base) : NULL;
    }

    case NODE_TYPE_ARRAY:
    {
        if (!node->binary.right)
        {
            fprintf(stderr, "error: array type missing element type\n");
            analyzer->has_errors = true;
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
        fprintf(stderr, "error: unhandled type node kind %d\n", node->kind);
        analyzer->has_errors = true;
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

    case NODE_IDENTIFIER:
    {
        if (!node->str_value)
        {
            fprintf(stderr, "error: identifier node missing string value\n");
            analyzer->has_errors = true;
            return NULL;
        }

        Symbol *sym = scope_lookup(analyzer->current, node->str_value);
        if (!sym)
        {
            fprintf(stderr, "error: undefined identifier '%s'\n", node->str_value);
            analyzer->has_errors = true;
            return NULL;
        }
        type = sym->type;
        break;
    }

    case NODE_EXPR_BINARY:
    {
        if (!node->binary.left || !node->binary.right)
        {
            fprintf(stderr, "error: binary expression missing operands\n");
            analyzer->has_errors = true;
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
                fprintf(stderr, "error: incompatible types in assignment\n");
                analyzer->has_errors = true;
                return NULL;
            }
            type = left_type;
            break;

        case OP_ADD:
        case OP_SUB:
            if (!type_compatible(left_type, right_type))
            {
                fprintf(stderr, "error: incompatible types in arithmetic\n");
                analyzer->has_errors = true;
                return NULL;
            }
            type = left_type;
            break;

        case OP_GREATER:
            type = type_builtin("u8");
            break;

        default:
            fprintf(stderr, "error: unhandled binary operator %d\n", node->binary.op);
            analyzer->has_errors = true;
            return NULL;
        }
        break;
    }

    default:
        fprintf(stderr, "error: unhandled expression node kind %d\n", node->kind);
        analyzer->has_errors = true;
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
            fprintf(stderr, "error: declaration missing name\n");
            analyzer->has_errors = true;
            return false;
        }

        char *name = node->decl.name->str_value;
        Type *type = NULL;

        if (node->decl.type)
        {
            type = analyze_type(analyzer, node->decl.type);
        }
        else if (node->decl.init)
        {
            type = analyze_expression(analyzer, node->decl.init);
        }

        if (!type)
        {
            fprintf(stderr, "error: cannot determine type for '%s'\n", name);
            analyzer->has_errors = true;
            return false;
        }

        if (node->decl.init)
        {
            Type *init_type = analyze_expression(analyzer, node->decl.init);
            if (!init_type || !type_compatible(type, init_type))
            {
                fprintf(stderr, "error: incompatible types in initialization\n");
                analyzer->has_errors = true;
                return false;
            }
        }

        SymbolKind kind = node->kind == NODE_STMT_VAL ? SYMBOL_CONSTANT : SYMBOL_VARIABLE;
        if (!scope_define(analyzer->current, name, kind, type))
        {
            fprintf(stderr, "error: redefinition of '%s'\n", name);
            analyzer->has_errors = true;
            return false;
        }
        return true;
    }

    case NODE_STMT_FUNCTION:
    {
        if (!node->function.name || !node->function.name->str_value)
        {
            fprintf(stderr, "error: function missing name\n");
            analyzer->has_errors = true;
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
            fprintf(stderr, "error: redefinition of function '%s'\n", name);
            analyzer->has_errors = true;
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
        if (node->conditional.condition)
        {
            Type *cond_type = analyze_expression(analyzer, node->conditional.condition);
            if (!cond_type)
                return false;
        }

        bool result = analyze_node(analyzer, node->conditional.body);

        if (node->conditional.else_body)
        {
            result = result && analyze_node(analyzer, node->conditional.else_body);
        }

        return result;
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
        fprintf(stderr, "error: unhandled statement node kind %d (%s)\n", node->kind, node_kind_name(node->kind));
        analyzer->has_errors = true;
        return false;
    }
}

static bool analyze_node(SemanticAnalyzer *analyzer, Node *node)
{
    if (!node || !analyzer)
        return false;

    switch (node->kind)
    {
    case NODE_PROGRAM:
    case NODE_BLOCK:
        if (node->children)
        {
            for (int i = 0; node->children[i]; i++)
            {
                if (!analyze_node(analyzer, node->children[i]))
                    return false;
            }
        }
        return true;

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
