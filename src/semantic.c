#include "semantic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// forward declarations
static Type *resolve_type_node(SemanticAnalyzer *analyzer, Node *type_node);
static bool  check_function(SemanticAnalyzer *analyzer, Node *func_node);
static Type *check_binary_expression(SemanticAnalyzer *analyzer, Node *expr);
static Type *check_unary_expression(SemanticAnalyzer *analyzer, Node *expr);
static Type *check_call_expression(SemanticAnalyzer *analyzer, Node *expr);

bool semantic_init(SemanticAnalyzer *analyzer)
{
    memset(analyzer, 0, sizeof(SemanticAnalyzer));

    analyzer->symbol_capacity = 64;
    analyzer->symbols         = malloc(sizeof(SemanticSymbol) * analyzer->symbol_capacity);
    if (!analyzer->symbols)
    {
        return false;
    }

    analyzer->current_scope_level = 0;
    return true;
}

void semantic_dnit(SemanticAnalyzer *analyzer)
{
    if (!analyzer)
        return;

    // free all symbols
    for (size_t i = 0; i < analyzer->symbol_count; i++)
    {
        free(analyzer->symbols[i].name);
        type_destroy(analyzer->symbols[i].type);
    }
    free(analyzer->symbols);

    memset(analyzer, 0, sizeof(SemanticAnalyzer));
}

bool semantic_analyze(SemanticAnalyzer *analyzer, Node *program)
{
    if (!analyzer || !program)
    {
        semantic_error(analyzer, "Invalid parameters");
        return false;
    }

    if (program->kind != NODE_PROGRAM)
    {
        semantic_error(analyzer, "Expected program node");
        return false;
    }

    // analyze all top-level declarations
    if (program->children)
    {
        for (size_t i = 0; i < node_list_count(program->children); i++)
        {
            Node *stmt = program->children[i];

            switch (stmt->kind)
            {
            case NODE_STMT_FUNCTION:
                check_function(analyzer, stmt);
                break;

            case NODE_STMT_VAL:
            case NODE_STMT_VAR:
                check_statement(analyzer, stmt);
                break;

            default:
                semantic_error_node(analyzer, stmt, "Unexpected top-level statement");
                break;
            }
        }
    }

    return !analyzer->has_error;
}

// type system implementation
Type *type_create_void(void)
{
    Type *type = calloc(1, sizeof(Type));
    type->kind = TYPE_VOID;
    return type;
}

Type *type_create_int(size_t bit_width, bool is_unsigned)
{
    Type *type        = calloc(1, sizeof(Type));
    type->kind        = TYPE_INT;
    type->bit_width   = bit_width;
    type->is_unsigned = is_unsigned;
    return type;
}

Type *type_create_float(size_t bit_width)
{
    Type *type      = calloc(1, sizeof(Type));
    type->kind      = TYPE_FLOAT;
    type->bit_width = bit_width;
    return type;
}

Type *type_create_pointer(Type *base_type)
{
    Type *type      = calloc(1, sizeof(Type));
    type->kind      = TYPE_POINTER;
    type->base_type = base_type;
    return type;
}

Type *type_create_array(Type *element_type, size_t size)
{
    Type *type       = calloc(1, sizeof(Type));
    type->kind       = TYPE_ARRAY;
    type->base_type  = element_type;
    type->array_size = size;
    return type;
}

Type *type_create_function(Type *return_type, Type **param_types, size_t param_count, bool is_variadic)
{
    Type *type        = calloc(1, sizeof(Type));
    type->kind        = TYPE_FUNCTION;
    type->return_type = return_type;
    type->param_count = param_count;
    type->is_variadic = is_variadic;

    if (param_count > 0 && param_types)
    {
        type->param_types = malloc(sizeof(Type *) * param_count);
        for (size_t i = 0; i < param_count; i++)
        {
            type->param_types[i] = param_types[i];
        }
    }

    return type;
}

void type_destroy(Type *type)
{
    if (!type)
        return;

    if (type->base_type)
        type_destroy(type->base_type);
    if (type->return_type)
        type_destroy(type->return_type);

    if (type->param_types)
    {
        for (size_t i = 0; i < type->param_count; i++)
        {
            type_destroy(type->param_types[i]);
        }
        free(type->param_types);
    }

    free(type);
}

Type *type_clone(Type *type)
{
    if (!type)
        return NULL;

    switch (type->kind)
    {
    case TYPE_VOID:
        return type_create_void();
    case TYPE_INT:
        return type_create_int(type->bit_width, type->is_unsigned);
    case TYPE_FLOAT:
        return type_create_float(type->bit_width);
    case TYPE_POINTER:
        return type_create_pointer(type_clone(type->base_type));
    case TYPE_ARRAY:
        return type_create_array(type_clone(type->base_type), type->array_size);
    case TYPE_FUNCTION:
    {
        Type **cloned_params = NULL;
        if (type->param_count > 0)
        {
            cloned_params = malloc(sizeof(Type *) * type->param_count);
            for (size_t i = 0; i < type->param_count; i++)
            {
                cloned_params[i] = type_clone(type->param_types[i]);
            }
        }
        Type *result = type_create_function(type_clone(type->return_type), cloned_params, type->param_count, type->is_variadic);
        free(cloned_params); // the function makes its own copy
        return result;
    }
    case TYPE_ERROR:
    default:
        return NULL;
    }
}

bool type_equals(Type *a, Type *b)
{
    if (!a || !b)
        return a == b;

    if (a->kind != b->kind)
        return false;

    switch (a->kind)
    {
    case TYPE_VOID:
        return true;
    case TYPE_INT:
        return a->bit_width == b->bit_width && a->is_unsigned == b->is_unsigned;
    case TYPE_FLOAT:
        return a->bit_width == b->bit_width;
    case TYPE_POINTER:
        return type_equals(a->base_type, b->base_type);
    case TYPE_ARRAY:
        return a->array_size == b->array_size && type_equals(a->base_type, b->base_type);
    case TYPE_FUNCTION:
        if (!type_equals(a->return_type, b->return_type) || a->param_count != b->param_count || a->is_variadic != b->is_variadic)
            return false;

        for (size_t i = 0; i < a->param_count; i++)
        {
            if (!type_equals(a->param_types[i], b->param_types[i]))
                return false;
        }
        return true;
    case TYPE_ERROR:
    default:
        return false;
    }
}

bool type_is_assignable(Type *from, Type *to)
{
    if (type_equals(from, to))
        return true;

    // allow some implicit conversions
    if (from->kind == TYPE_INT && to->kind == TYPE_INT)
    {
        // allow smaller to larger int types
        return from->bit_width <= to->bit_width;
    }

    if (from->kind == TYPE_INT && to->kind == TYPE_FLOAT)
    {
        // allow int to float conversion
        return true;
    }

    return false;
}

const char *type_to_string(Type *type)
{
    if (!type)
        return "null";

    // this is a simple implementation - in practice you'd want a proper string buffer
    static char buffer[256];

    switch (type->kind)
    {
    case TYPE_VOID:
        return "void";
    case TYPE_INT:
        snprintf(buffer, sizeof(buffer), "%s%zu", type->is_unsigned ? "u" : "i", type->bit_width);
        return buffer;
    case TYPE_FLOAT:
        snprintf(buffer, sizeof(buffer), "f%zu", type->bit_width);
        return buffer;
    case TYPE_POINTER:
        snprintf(buffer, sizeof(buffer), "*%s", type_to_string(type->base_type));
        return buffer;
    case TYPE_ARRAY:
        snprintf(buffer, sizeof(buffer), "[%zu]%s", type->array_size, type_to_string(type->base_type));
        return buffer;
    case TYPE_FUNCTION:
        return "function";
    case TYPE_ERROR:
    default:
        return "error";
    }
}

// resolve AST type node to semantic type
static Type *resolve_type_node(SemanticAnalyzer *analyzer, Node *type_node)
{
    if (!type_node)
        return type_create_void();

    switch (type_node->kind)
    {
    case NODE_IDENTIFIER:
    {
        const char *type_name = type_node->str_value;

        // basic integer types
        if (strcmp(type_name, "i8") == 0)
            return type_create_int(8, false);
        if (strcmp(type_name, "i16") == 0)
            return type_create_int(16, false);
        if (strcmp(type_name, "i32") == 0)
            return type_create_int(32, false);
        if (strcmp(type_name, "i64") == 0)
            return type_create_int(64, false);
        if (strcmp(type_name, "u8") == 0)
            return type_create_int(8, true);
        if (strcmp(type_name, "u16") == 0)
            return type_create_int(16, true);
        if (strcmp(type_name, "u32") == 0)
            return type_create_int(32, true);
        if (strcmp(type_name, "u64") == 0)
            return type_create_int(64, true);

        // float types
        if (strcmp(type_name, "f32") == 0)
            return type_create_float(32);
        if (strcmp(type_name, "f64") == 0)
            return type_create_float(64);

        // void type
        if (strcmp(type_name, "void") == 0)
            return type_create_void();

        semantic_error(analyzer, "Unknown type");
        return NULL;
    }

    case NODE_TYPE_POINTER:
    {
        Type *base_type = resolve_type_node(analyzer, type_node->single);
        if (!base_type)
            return NULL;
        return type_create_pointer(base_type);
    }

    case NODE_TYPE_ARRAY:
    {
        Type *element_type = resolve_type_node(analyzer, type_node->binary.right);
        if (!element_type)
            return NULL;

        // for now, assume dynamic arrays (size 0)
        return type_create_array(element_type, 0);
    }

    default:
        semantic_error(analyzer, "Unsupported type");
        return NULL;
    }
}

// symbol table implementation
void push_scope(SemanticAnalyzer *analyzer)
{
    analyzer->current_scope_level++;
}

void pop_scope(SemanticAnalyzer *analyzer)
{
    // remove symbols from current scope
    size_t symbols_to_remove = 0;
    for (int i = (int)analyzer->symbol_count - 1; i >= 0; i--)
    {
        if (analyzer->symbols[i].scope_level == analyzer->current_scope_level)
        {
            free(analyzer->symbols[i].name);
            type_destroy(analyzer->symbols[i].type);
            symbols_to_remove++;
        }
        else
        {
            break;
        }
    }

    analyzer->symbol_count -= symbols_to_remove;
    analyzer->current_scope_level--;
}

SemanticSymbol *add_symbol(SemanticAnalyzer *analyzer, const char *name, SymbolType symbol_type, Type *type, bool is_mutable, Node *declaration)
{
    // check for duplicate in current scope
    for (size_t i = 0; i < analyzer->symbol_count; i++)
    {
        if (analyzer->symbols[i].scope_level == analyzer->current_scope_level && strcmp(analyzer->symbols[i].name, name) == 0)
        {
            semantic_error(analyzer, "Symbol already defined in current scope");
            return NULL;
        }
    }

    // expand symbol table if needed
    if (analyzer->symbol_count >= analyzer->symbol_capacity)
    {
        analyzer->symbol_capacity *= 2;
        analyzer->symbols = realloc(analyzer->symbols, sizeof(SemanticSymbol) * analyzer->symbol_capacity);
    }

    SemanticSymbol *symbol = &analyzer->symbols[analyzer->symbol_count++];
    symbol->name           = strdup(name);
    symbol->symbol_type    = symbol_type;
    symbol->type           = type;
    symbol->is_mutable     = is_mutable;
    symbol->scope_level    = analyzer->current_scope_level;
    symbol->declaration    = declaration;

    return symbol;
}

SemanticSymbol *find_symbol(SemanticAnalyzer *analyzer, const char *name)
{
    // search from most recent to oldest
    for (int i = (int)analyzer->symbol_count - 1; i >= 0; i--)
    {
        if (strcmp(analyzer->symbols[i].name, name) == 0)
        {
            return &analyzer->symbols[i];
        }
    }
    return NULL;
}

// statement checking
bool check_statement(SemanticAnalyzer *analyzer, Node *stmt)
{
    if (!stmt)
        return true;

    switch (stmt->kind)
    {
    case NODE_BLOCK:
        if (stmt->children)
        {
            for (size_t i = 0; i < node_list_count(stmt->children); i++)
            {
                if (!check_statement(analyzer, stmt->children[i]))
                    return false;
            }
        }
        return true;

    case NODE_STMT_VAL:
    case NODE_STMT_VAR:
    {
        if (!stmt->decl.name || stmt->decl.name->kind != NODE_IDENTIFIER)
        {
            semantic_error_node(analyzer, stmt, "Invalid variable name");
            return false;
        }

        const char *var_name = stmt->decl.name->str_value;
        Type       *var_type = resolve_type_node(analyzer, stmt->decl.type);
        if (!var_type)
            return false;

        // check initializer if present
        if (stmt->decl.init)
        {
            Type *init_type = check_expression_type(analyzer, stmt->decl.init);
            if (!init_type)
                return false;

            if (!type_is_assignable(init_type, var_type))
            {
                semantic_error_node(analyzer, stmt, "Type mismatch in variable initialization");
                type_destroy(init_type);
                type_destroy(var_type);
                return false;
            }

            type_destroy(init_type);
        }
        else if (stmt->kind == NODE_STMT_VAL)
        {
            semantic_error_node(analyzer, stmt, "val declaration must have initializer");
            type_destroy(var_type);
            return false;
        }

        bool is_mutable = (stmt->kind == NODE_STMT_VAR);
        add_symbol(analyzer, var_name, SYMBOL_TYPE_VARIABLE, var_type, is_mutable, stmt);
        return true;
    }

    case NODE_STMT_RETURN:
    {
        if (stmt->single)
        {
            Type *return_type = check_expression_type(analyzer, stmt->single);
            if (!return_type)
                return false;

            if (analyzer->current_function_return_type)
            {
                if (!type_is_assignable(return_type, analyzer->current_function_return_type))
                {
                    semantic_error_node(analyzer, stmt, "Return type mismatch");
                    type_destroy(return_type);
                    return false;
                }
            }

            type_destroy(return_type);
        }
        return true;
    }

    case NODE_STMT_EXPRESSION:
        return check_expression_type(analyzer, stmt->single) != NULL;

    default:
        // other statements like if, for, etc. - implement as needed
        return true;
    }
}

static bool check_function(SemanticAnalyzer *analyzer, Node *func_node)
{
    if (func_node->kind != NODE_STMT_FUNCTION)
        return false;

    const char *func_name = "unknown";
    if (func_node->function.name && func_node->function.name->kind == NODE_IDENTIFIER)
    {
        func_name = func_node->function.name->str_value;
    }

    // resolve return type
    Type *return_type = resolve_type_node(analyzer, func_node->function.return_type);
    if (!return_type)
        return false;

    // resolve parameter types
    Type **param_types = NULL;
    size_t param_count = 0;

    if (func_node->function.params)
    {
        param_count = node_list_count(func_node->function.params);
        if (param_count > 0)
        {
            param_types = malloc(sizeof(Type *) * param_count);
            for (size_t i = 0; i < param_count; i++)
            {
                Node *param = func_node->function.params[i];
                if (param && param->kind == NODE_STMT_VAL && param->decl.type)
                {
                    param_types[i] = resolve_type_node(analyzer, param->decl.type);
                    if (!param_types[i])
                    {
                        // cleanup and return
                        for (size_t j = 0; j < i; j++)
                        {
                            type_destroy(param_types[j]);
                        }
                        free(param_types);
                        type_destroy(return_type);
                        return false;
                    }
                }
                else
                {
                    semantic_error_node(analyzer, func_node, "Invalid parameter declaration");
                    type_destroy(return_type);
                    free(param_types);
                    return false;
                }
            }
        }
    }

    // create function type
    Type *func_type = type_create_function(return_type, param_types, param_count, func_node->function.is_variadic);

    // add function to symbol table
    add_symbol(analyzer, func_name, SYMBOL_TYPE_FUNCTION, func_type, false, func_node);

    // check function body in new scope
    push_scope(analyzer);

    // add parameters to local scope
    if (func_node->function.params && param_count > 0)
    {
        for (size_t i = 0; i < param_count; i++)
        {
            Node *param = func_node->function.params[i];
            if (param && param->decl.name && param->decl.name->kind == NODE_IDENTIFIER)
            {
                const char *param_name = param->decl.name->str_value;
                add_symbol(analyzer, param_name, SYMBOL_TYPE_VARIABLE, type_clone(param_types[i]), false, param);
            }
        }
    }

    // set current function context
    Type *prev_return_type                 = analyzer->current_function_return_type;
    analyzer->current_function_return_type = return_type;

    // check function body
    bool result = true;
    if (func_node->function.body)
    {
        result = check_statement(analyzer, func_node->function.body);
    }

    // restore context
    analyzer->current_function_return_type = prev_return_type;
    pop_scope(analyzer);

    // cleanup param_types
    if (param_types)
    {
        for (size_t i = 0; i < param_count; i++)
        {
            type_destroy(param_types[i]);
        }
        free(param_types);
    }

    return result;
}

// expression type checking
Type *check_expression_type(SemanticAnalyzer *analyzer, Node *expr)
{
    if (!expr)
        return NULL;

    switch (expr->kind)
    {
    case NODE_LIT_INT:
        // for now, assume i32 for integer literals
        return type_create_int(32, false);

    case NODE_LIT_FLOAT:
        // assume f64 for float literals
        return type_create_float(64);

    case NODE_IDENTIFIER:
    {
        SemanticSymbol *symbol = find_symbol(analyzer, expr->str_value);
        if (!symbol)
        {
            semantic_error_node(analyzer, expr, "Undefined symbol");
            return NULL;
        }
        return type_clone(symbol->type);
    }

    case NODE_EXPR_BINARY:
        return check_binary_expression(analyzer, expr);

    case NODE_EXPR_UNARY:
        return check_unary_expression(analyzer, expr);

    case NODE_EXPR_CALL:
        return check_call_expression(analyzer, expr);

    default:
        semantic_error_node(analyzer, expr, "Unsupported expression");
        return NULL;
    }
}

static Type *check_binary_expression(SemanticAnalyzer *analyzer, Node *expr)
{
    // handle assignment specially
    if (expr->binary.op == OP_ASSIGN)
    {
        if (expr->binary.left->kind != NODE_IDENTIFIER)
        {
            semantic_error_node(analyzer, expr, "Left side of assignment must be a variable");
            return NULL;
        }

        SemanticSymbol *symbol = find_symbol(analyzer, expr->binary.left->str_value);
        if (!symbol)
        {
            semantic_error_node(analyzer, expr, "Undefined variable");
            return NULL;
        }

        if (!symbol->is_mutable)
        {
            semantic_error_node(analyzer, expr, "Cannot assign to immutable variable");
            return NULL;
        }

        Type *right_type = check_expression_type(analyzer, expr->binary.right);
        if (!right_type)
            return NULL;

        if (!type_is_assignable(right_type, symbol->type))
        {
            semantic_error_node(analyzer, expr, "Type mismatch in assignment");
            type_destroy(right_type);
            return NULL;
        }

        type_destroy(right_type);
        return type_clone(symbol->type);
    }

    // handle other binary operators
    Type *left_type  = check_expression_type(analyzer, expr->binary.left);
    Type *right_type = check_expression_type(analyzer, expr->binary.right);

    if (!left_type || !right_type)
    {
        if (left_type)
            type_destroy(left_type);
        if (right_type)
            type_destroy(right_type);
        return NULL;
    }

    // for now, require exact type match for binary operations
    // in a real compiler, you'd implement type promotion rules
    if (!type_equals(left_type, right_type))
    {
        semantic_error_node(analyzer, expr, "Type mismatch in binary operation");
        type_destroy(left_type);
        type_destroy(right_type);
        return NULL;
    }

    // determine result type based on operator
    Type *result_type = type_clone(left_type);

    switch (expr->binary.op)
    {
    case OP_EQUAL:
    case OP_NOT_EQUAL:
    case OP_LESS:
    case OP_GREATER:
    case OP_LESS_EQUAL:
    case OP_GREATER_EQUAL:
    case OP_LOGICAL_AND:
    case OP_LOGICAL_OR:
        // comparison and logical operators return boolean (i32 for now)
        type_destroy(result_type);
        result_type = type_create_int(32, false);
        break;

    default:
        // arithmetic operators return the same type as operands
        break;
    }

    type_destroy(left_type);
    type_destroy(right_type);
    return result_type;
}

static Type *check_unary_expression(SemanticAnalyzer *analyzer, Node *expr)
{
    Type *operand_type = check_expression_type(analyzer, expr->unary.target);
    if (!operand_type)
        return NULL;

    switch (expr->unary.op)
    {
    case OP_LOGICAL_NOT:
        // logical not returns boolean
        type_destroy(operand_type);
        return type_create_int(32, false);

    case OP_SUB:
    case OP_ADD:
    case OP_BITWISE_NOT:
        // these return the same type as operand
        return operand_type;

    default:
        semantic_error_node(analyzer, expr, "Unsupported unary operator");
        type_destroy(operand_type);
        return NULL;
    }
}

static Type *check_call_expression(SemanticAnalyzer *analyzer, Node *expr)
{
    if (!expr->call.target || expr->call.target->kind != NODE_IDENTIFIER)
    {
        semantic_error_node(analyzer, expr, "Invalid function call target");
        return NULL;
    }

    const char     *func_name   = expr->call.target->str_value;
    SemanticSymbol *func_symbol = find_symbol(analyzer, func_name);

    if (!func_symbol || func_symbol->symbol_type != SYMBOL_TYPE_FUNCTION)
    {
        semantic_error_node(analyzer, expr, "Undefined function");
        return NULL;
    }

    Type *func_type = func_symbol->type;
    if (func_type->kind != TYPE_FUNCTION)
    {
        semantic_error_node(analyzer, expr, "Symbol is not a function");
        return NULL;
    }

    // check argument count
    size_t arg_count = 0;
    if (expr->call.args)
    {
        arg_count = node_list_count(expr->call.args);
    }

    if (!func_type->is_variadic && arg_count != func_type->param_count)
    {
        semantic_error_node(analyzer, expr, "Wrong number of arguments");
        return NULL;
    }

    // check argument types
    for (size_t i = 0; i < arg_count && i < func_type->param_count; i++)
    {
        Type *arg_type = check_expression_type(analyzer, expr->call.args[i]);
        if (!arg_type)
            return NULL;

        if (!type_is_assignable(arg_type, func_type->param_types[i]))
        {
            semantic_error_node(analyzer, expr, "Argument type mismatch");
            type_destroy(arg_type);
            return NULL;
        }

        type_destroy(arg_type);
    }

    return type_clone(func_type->return_type);
}

// error handling
void semantic_error(SemanticAnalyzer *analyzer, const char *message)
{
    if (analyzer)
    {
        analyzer->has_error = true;

        // append to error buffer
        size_t remaining = sizeof(analyzer->error_messages) - analyzer->error_buffer_pos;
        int    written   = snprintf(analyzer->error_messages + analyzer->error_buffer_pos, remaining, "Semantic error: %s\n", message);
        if (written > 0 && (size_t)written < remaining)
        {
            analyzer->error_buffer_pos += written;
        }
    }
    fprintf(stderr, "Semantic error: %s\n", message);
}

void semantic_error_node(SemanticAnalyzer *analyzer, Node *node, const char *message)
{
    if (analyzer)
    {
        analyzer->has_error = true;

        // format error with position info if available
        if (node && node->token)
        {
            size_t remaining = sizeof(analyzer->error_messages) - analyzer->error_buffer_pos;
            int    written   = snprintf(analyzer->error_messages + analyzer->error_buffer_pos, remaining, "Semantic error at position %d: %s\n", node->token->pos, message);
            if (written > 0 && (size_t)written < remaining)
            {
                analyzer->error_buffer_pos += written;
            }

            fprintf(stderr, "Semantic error at position %d: %s\n", node->token->pos, message);
        }
        else
        {
            // fallback to simple error if no location info
            semantic_error(analyzer, message);
        }
    }
    else
    {
        fprintf(stderr, "Semantic error: %s\n", message);
    }
}
