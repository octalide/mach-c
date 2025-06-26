#include "parser.h"
#include "operator.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// core parser functions
void parser_init(Parser *parser, Lexer *lexer)
{
    parser->lexer     = lexer;
    parser->has_error = false;

    // initialize tokens
    token_init(&parser->current, TOKEN_ERROR, 0, 0);
    token_init(&parser->peek, TOKEN_ERROR, 0, 0);

    // advance to get first two tokens
    parser_advance(parser);
    parser_advance(parser);
}

void parser_dnit(Parser *parser)
{
    if (parser == NULL)
    {
        return;
    }

    token_dnit(&parser->current);
    token_dnit(&parser->peek);
    parser->lexer     = NULL;
    parser->has_error = false;
}

// utility functions
void parser_advance(Parser *parser)
{
    // move peek to current
    token_copy(&parser->peek, &parser->current);

    // get next token
    Token *token = lexer_next(parser->lexer);
    if (token != NULL)
    {
        token_copy(token, &parser->peek);
    }
    else
    {
        token_init(&parser->peek, TOKEN_EOF, parser->lexer->pos, 0);
    }

    // skip comments
    while (parser->peek.kind == TOKEN_COMMENT)
    {
        Token *next_token = lexer_next(parser->lexer);
        if (next_token != NULL)
        {
            token_copy(next_token, &parser->peek);
        }
        else
        {
            token_init(&parser->peek, TOKEN_EOF, parser->lexer->pos, 0);
            break;
        }
    }
}

bool parser_check(Parser *parser, TokenKind kind)
{
    return parser->current.kind == kind;
}

bool parser_match(Parser *parser, TokenKind kind)
{
    if (parser_check(parser, kind))
    {
        parser_advance(parser);
        return true;
    }
    return false;
}

bool parser_consume(Parser *parser, TokenKind kind, const char *message)
{
    if (parser_check(parser, kind))
    {
        parser_advance(parser);
        return true;
    }

    parser_error(parser, message);
    return false;
}

// error handling
void parser_error(Parser *parser, const char *message)
{
    parser->has_error = true;
    int line          = lexer_get_pos_line(parser->lexer, parser->current.pos);
    fprintf(stderr, "Parse error at line %d: %s\n", line + 1, message);
}

void parser_synchronize(Parser *parser)
{
    parser_advance(parser);

    while (!parser_check(parser, TOKEN_EOF))
    {
        if (parser->current.kind == TOKEN_SEMICOLON)
        {
            return;
        }

        switch (parser->peek.kind)
        {
        case TOKEN_KW_VAL:
        case TOKEN_KW_VAR:
        case TOKEN_KW_DEF:
        case TOKEN_KW_FUN:
        case TOKEN_KW_STR:
        case TOKEN_KW_UNI:
        case TOKEN_KW_EXT:
        case TOKEN_KW_USE:
        case TOKEN_KW_IF:
        case TOKEN_KW_OR:
        case TOKEN_KW_FOR:
        case TOKEN_KW_BRK:
        case TOKEN_KW_CNT:
        case TOKEN_KW_RET:
            return;
        default:
            break;
        }

        parser_advance(parser);
    }
}

// main parsing function
Node *parser_parse_program(Parser *parser)
{
    Node **statements = node_list_new();
    if (statements == NULL)
    {
        return NULL;
    }

    while (!parser_check(parser, TOKEN_EOF) && !parser->has_error)
    {
        Node *stmt = parser_parse_stmt(parser);
        if (stmt != NULL)
        {
            node_list_add(&statements, stmt);
        }
        else
        {
            parser_synchronize(parser);
        }
    }

    Node *program = malloc(sizeof(Node));
    if (program == NULL)
    {
        return NULL;
    }

    node_init(program, NODE_PROGRAM);
    program->children = statements;
    return program;
}

// identifier parsing
Node *parser_parse_identifier(Parser *parser)
{
    if (!parser_check(parser, TOKEN_IDENTIFIER))
    {
        parser_error(parser, "Expected identifier");
        return NULL;
    }

    char *value = lexer_raw_value(parser->lexer, &parser->current);
    Node *node  = node_identifier(value);
    free(value);
    parser_advance(parser);
    return node;
}

// literal parsing
Node *parser_parse_lit_int(Parser *parser)
{
    if (!parser_check(parser, TOKEN_LIT_INT))
    {
        parser_error(parser, "Expected integer literal");
        return NULL;
    }

    unsigned long long value = lexer_eval_lit_int(parser->lexer, &parser->current);
    Node              *node  = node_int(value);
    parser_advance(parser);
    return node;
}

Node *parser_parse_lit_float(Parser *parser)
{
    if (!parser_check(parser, TOKEN_LIT_FLOAT))
    {
        parser_error(parser, "Expected float literal");
        return NULL;
    }

    double value = lexer_eval_lit_float(parser->lexer, &parser->current);
    Node  *node  = node_float(value);
    parser_advance(parser);
    return node;
}

Node *parser_parse_lit_char(Parser *parser)
{
    if (!parser_check(parser, TOKEN_LIT_CHAR))
    {
        parser_error(parser, "Expected character literal");
        return NULL;
    }

    char  value = lexer_eval_lit_char(parser->lexer, &parser->current);
    Node *node  = node_char(value);
    parser_advance(parser);
    return node;
}

Node *parser_parse_lit_string(Parser *parser)
{
    if (!parser_check(parser, TOKEN_LIT_STRING))
    {
        parser_error(parser, "Expected string literal");
        return NULL;
    }

    char *value = lexer_eval_lit_string(parser->lexer, &parser->current);
    Node *node  = node_string(value);
    free(value);
    parser_advance(parser);
    return node;
}

// array literal: [size]type{elements...} or [_]type{elements...}
Node *parser_parse_lit_array(Parser *parser)
{
    if (!parser_check(parser, TOKEN_L_BRACKET))
    {
        parser_error(parser, "Expected '['");
        return NULL;
    }

    parser_advance(parser); // consume '['

    Node *size = NULL;
    if (parser_check(parser, TOKEN_UNDERSCORE))
    {
        parser_advance(parser); // dynamic size
    }
    else
    {
        size = parser_parse_expr(parser);
        if (size == NULL)
        {
            return NULL;
        }
    }

    if (!parser_consume(parser, TOKEN_R_BRACKET, "Expected ']'"))
    {
        if (size)
        {
            node_dnit(size);
            free(size);
        }
        return NULL;
    }

    Node *type = parser_parse_type(parser);
    if (type == NULL)
    {
        if (size)
        {
            node_dnit(size);
            free(size);
        }
        return NULL;
    }

    if (!parser_consume(parser, TOKEN_L_BRACE, "Expected '{'"))
    {
        if (size)
        {
            node_dnit(size);
            free(size);
        }
        node_dnit(type);
        free(type);
        return NULL;
    }

    Node **elements = node_list_new();
    if (elements == NULL)
    {
        if (size)
        {
            node_dnit(size);
            free(size);
        }
        node_dnit(type);
        free(type);
        return NULL;
    }

    while (!parser_check(parser, TOKEN_R_BRACE) && !parser_check(parser, TOKEN_EOF))
    {
        Node *element = parser_parse_expr(parser);
        if (element != NULL)
        {
            node_list_add(&elements, element);
        }

        if (!parser_check(parser, TOKEN_R_BRACE))
        {
            if (!parser_consume(parser, TOKEN_COMMA, "Expected ',' or '}'"))
            {
                break;
            }
        }
    }

    if (!parser_consume(parser, TOKEN_R_BRACE, "Expected '}'"))
    {
        if (size)
        {
            node_dnit(size);
            free(size);
        }
        node_dnit(type);
        free(type);
        for (size_t i = 0; i < node_list_count(elements); i++)
        {
            node_dnit(elements[i]);
            free(elements[i]);
        }
        free(elements);
        return NULL;
    }

    // create array literal node
    Node *node = malloc(sizeof(Node));
    if (node == NULL)
    {
        return NULL;
    }

    node_init(node, NODE_EXPR_CALL); // use call node for array literal
    node->call.target = type;
    node->call.args   = elements;

    if (size)
    {
        node_dnit(size);
        free(size);
    }
    return node;
}

// struct literal: type{field1, field2, ...}
Node *parser_parse_lit_struct(Parser *parser)
{
    Node *type = parser_parse_type(parser);
    if (type == NULL)
    {
        return NULL;
    }

    if (!parser_consume(parser, TOKEN_L_BRACE, "Expected '{'"))
    {
        node_dnit(type);
        free(type);
        return NULL;
    }

    Node **fields = node_list_new();
    if (fields == NULL)
    {
        node_dnit(type);
        free(type);
        return NULL;
    }

    while (!parser_check(parser, TOKEN_R_BRACE) && !parser_check(parser, TOKEN_EOF))
    {
        Node *field = parser_parse_expr(parser);
        if (field != NULL)
        {
            node_list_add(&fields, field);
        }

        if (!parser_check(parser, TOKEN_R_BRACE))
        {
            if (!parser_consume(parser, TOKEN_COMMA, "Expected ',' or '}'"))
            {
                break;
            }
        }
    }

    if (!parser_consume(parser, TOKEN_R_BRACE, "Expected '}'"))
    {
        node_dnit(type);
        free(type);
        for (size_t i = 0; i < node_list_count(fields); i++)
        {
            node_dnit(fields[i]);
            free(fields[i]);
        }
        free(fields);
        return NULL;
    }

    // create struct literal node
    Node *node = malloc(sizeof(Node));
    if (node == NULL)
    {
        return NULL;
    }

    node_init(node, NODE_EXPR_CALL); // use call node for struct literal
    node->call.target = type;
    node->call.args   = fields;
    return node;
}

// expression parsing
Node *parser_parse_expr(Parser *parser)
{
    return parser_parse_expr_assignment(parser);
}

Node *parser_parse_expr_assignment(Parser *parser)
{
    Node *expr = parser_parse_expr_logical_or(parser);

    while (parser_check(parser, TOKEN_EQUAL))
    {
        Operator op = OP_ASSIGN;
        parser_advance(parser);
        Node *right = parser_parse_expr_logical_or(parser);
        expr        = node_binary(op, expr, right);
    }

    return expr;
}

Node *parser_parse_expr_logical_or(Parser *parser)
{
    Node *expr = parser_parse_expr_logical_and(parser);

    while (parser_check(parser, TOKEN_PIPE_PIPE))
    {
        Operator op = OP_LOGICAL_OR;
        parser_advance(parser);
        Node *right = parser_parse_expr_logical_and(parser);
        expr        = node_binary(op, expr, right);
    }

    return expr;
}

Node *parser_parse_expr_logical_and(Parser *parser)
{
    Node *expr = parser_parse_expr_bitwise_or(parser);

    while (parser_check(parser, TOKEN_AMPERSAND_AMPERSAND))
    {
        Operator op = OP_LOGICAL_AND;
        parser_advance(parser);
        Node *right = parser_parse_expr_bitwise_or(parser);
        expr        = node_binary(op, expr, right);
    }

    return expr;
}

Node *parser_parse_expr_bitwise_or(Parser *parser)
{
    Node *expr = parser_parse_expr_bitwise_xor(parser);

    while (parser_check(parser, TOKEN_PIPE))
    {
        Operator op = OP_BITWISE_OR;
        parser_advance(parser);
        Node *right = parser_parse_expr_bitwise_xor(parser);
        expr        = node_binary(op, expr, right);
    }

    return expr;
}

Node *parser_parse_expr_bitwise_xor(Parser *parser)
{
    Node *expr = parser_parse_expr_bitwise_and(parser);

    while (parser_check(parser, TOKEN_CARET))
    {
        Operator op = OP_BITWISE_XOR;
        parser_advance(parser);
        Node *right = parser_parse_expr_bitwise_and(parser);
        expr        = node_binary(op, expr, right);
    }

    return expr;
}

Node *parser_parse_expr_bitwise_and(Parser *parser)
{
    Node *expr = parser_parse_expr_equality(parser);

    while (parser_check(parser, TOKEN_AMPERSAND))
    {
        Operator op = OP_BITWISE_AND;
        parser_advance(parser);
        Node *right = parser_parse_expr_equality(parser);
        expr        = node_binary(op, expr, right);
    }

    return expr;
}

Node *parser_parse_expr_equality(Parser *parser)
{
    Node *expr = parser_parse_expr_comparison(parser);

    while (parser_check(parser, TOKEN_EQUAL_EQUAL) || parser_check(parser, TOKEN_BANG_EQUAL))
    {
        Operator op = parser->current.kind == TOKEN_EQUAL_EQUAL ? OP_EQUAL : OP_NOT_EQUAL;
        parser_advance(parser);
        Node *right = parser_parse_expr_comparison(parser);
        expr        = node_binary(op, expr, right);
    }

    return expr;
}

Node *parser_parse_expr_comparison(Parser *parser)
{
    Node *expr = parser_parse_expr_shift(parser);

    while (parser_check(parser, TOKEN_LESS) || parser_check(parser, TOKEN_LESS_EQUAL) || parser_check(parser, TOKEN_GREATER) || parser_check(parser, TOKEN_GREATER_EQUAL))
    {
        Operator op;
        switch (parser->current.kind)
        {
        case TOKEN_LESS:
            op = OP_LESS;
            break;
        case TOKEN_LESS_EQUAL:
            op = OP_LESS_EQUAL;
            break;
        case TOKEN_GREATER:
            op = OP_GREATER;
            break;
        case TOKEN_GREATER_EQUAL:
            op = OP_GREATER_EQUAL;
            break;
        default:
            op = OP_COUNT;
            break;
        }

        parser_advance(parser);
        Node *right = parser_parse_expr_shift(parser);
        expr        = node_binary(op, expr, right);
    }

    return expr;
}

Node *parser_parse_expr_shift(Parser *parser)
{
    Node *expr = parser_parse_expr_term(parser);

    while (parser_check(parser, TOKEN_LESS_LESS) || parser_check(parser, TOKEN_GREATER_GREATER))
    {
        Operator op = parser->current.kind == TOKEN_LESS_LESS ? OP_BITWISE_SHL : OP_BITWISE_SHR;
        parser_advance(parser);
        Node *right = parser_parse_expr_term(parser);
        expr        = node_binary(op, expr, right);
    }

    return expr;
}

Node *parser_parse_expr_term(Parser *parser)
{
    Node *expr = parser_parse_expr_factor(parser);

    while (parser_check(parser, TOKEN_PLUS) || parser_check(parser, TOKEN_MINUS))
    {
        Operator op = parser->current.kind == TOKEN_PLUS ? OP_ADD : OP_SUB;
        parser_advance(parser);
        Node *right = parser_parse_expr_factor(parser);
        expr        = node_binary(op, expr, right);
    }

    return expr;
}

Node *parser_parse_expr_factor(Parser *parser)
{
    Node *expr = parser_parse_expr_unary(parser);

    while (parser_check(parser, TOKEN_STAR) || parser_check(parser, TOKEN_SLASH) || parser_check(parser, TOKEN_PERCENT))
    {
        Operator op;
        switch (parser->current.kind)
        {
        case TOKEN_STAR:
            op = OP_MUL;
            break;
        case TOKEN_SLASH:
            op = OP_DIV;
            break;
        case TOKEN_PERCENT:
            op = OP_MOD;
            break;
        default:
            op = OP_COUNT;
            break;
        }

        parser_advance(parser);
        Node *right = parser_parse_expr_unary(parser);
        expr        = node_binary(op, expr, right);
    }

    return expr;
}

Node *parser_parse_expr_unary(Parser *parser)
{
    if (parser_check(parser, TOKEN_MINUS) || parser_check(parser, TOKEN_PLUS) || parser_check(parser, TOKEN_BANG) || parser_check(parser, TOKEN_TILDE) || parser_check(parser, TOKEN_AT) || parser_check(parser, TOKEN_QUESTION))
    {
        Operator op;
        switch (parser->current.kind)
        {
        case TOKEN_MINUS:
            op = OP_SUB;
            break; // use SUB for unary minus
        case TOKEN_PLUS:
            op = OP_ADD;
            break; // use ADD for unary plus
        case TOKEN_BANG:
            op = OP_LOGICAL_NOT;
            break;
        case TOKEN_TILDE:
            op = OP_BITWISE_NOT;
            break;
        case TOKEN_AT:
            op = OP_DEREFERENCE;
            break;
        case TOKEN_QUESTION:
            op = OP_REFERENCE;
            break;
        default:
            op = OP_COUNT;
            break;
        }

        parser_advance(parser);
        Node *expr = parser_parse_expr_unary(parser);
        return node_unary(op, expr);
    }

    return parser_parse_expr_postfix(parser);
}

Node *parser_parse_expr_postfix(Parser *parser)
{
    Node *expr = parser_parse_expr_primary(parser);

    while (true)
    {
        if (parser_check(parser, TOKEN_L_PAREN))
        {
            expr = parser_parse_expr_call(parser, expr);
        }
        else if (parser_check(parser, TOKEN_L_BRACKET))
        {
            expr = parser_parse_expr_index(parser, expr);
        }
        else if (parser_check(parser, TOKEN_DOT))
        {
            expr = parser_parse_expr_member(parser, expr);
        }
        else if (parser_check(parser, TOKEN_COLON_COLON))
        {
            expr = parser_parse_expr_cast(parser, expr);
        }
        else
        {
            break;
        }
    }

    return expr;
}

Node *parser_parse_expr_primary(Parser *parser)
{
    switch (parser->current.kind)
    {
    case TOKEN_LIT_INT:
        return parser_parse_lit_int(parser);
    case TOKEN_LIT_FLOAT:
        return parser_parse_lit_float(parser);
    case TOKEN_LIT_CHAR:
        return parser_parse_lit_char(parser);
    case TOKEN_LIT_STRING:
        return parser_parse_lit_string(parser);
    case TOKEN_IDENTIFIER:
        // check if this might be a struct literal
        if (parser->peek.kind == TOKEN_L_BRACE)
        {
            return parser_parse_lit_struct(parser);
        }
        return parser_parse_identifier(parser);
    case TOKEN_TYPE_ANY:
    case TOKEN_TYPE_I8:
    case TOKEN_TYPE_I16:
    case TOKEN_TYPE_I32:
    case TOKEN_TYPE_I64:
    case TOKEN_TYPE_U8:
    case TOKEN_TYPE_U16:
    case TOKEN_TYPE_U32:
    case TOKEN_TYPE_U64:
    case TOKEN_TYPE_F32:
    case TOKEN_TYPE_F64:
    {
        // allow type tokens in expressions for builtin functions like size_of(i32)
        char *type_name = lexer_raw_value(parser->lexer, &parser->current);
        Node *node      = node_identifier(type_name);
        free(type_name);
        parser_advance(parser);
        return node;
    }
    case TOKEN_L_BRACKET:
        return parser_parse_lit_array(parser);
    case TOKEN_L_PAREN:
    {
        parser_advance(parser); // consume '('
        Node *expr = parser_parse_expr(parser);
        if (!parser_consume(parser, TOKEN_R_PAREN, "Expected ')'"))
        {
            if (expr)
            {
                node_dnit(expr);
                free(expr);
            }
            return NULL;
        }
        return expr;
    }
    default:
        parser_error(parser, "Unexpected token in expression");
        return NULL;
    }
}

// postfix expression parsing
Node *parser_parse_expr_call(Parser *parser, Node *target)
{
    parser_advance(parser); // consume '('

    Node **args = parser_parse_argument_list(parser);
    if (args == NULL && parser->has_error)
    {
        node_dnit(target);
        free(target);
        return NULL;
    }

    if (!parser_consume(parser, TOKEN_R_PAREN, "Expected ')'"))
    {
        node_dnit(target);
        free(target);
        if (args)
        {
            for (size_t i = 0; i < node_list_count(args); i++)
            {
                node_dnit(args[i]);
                free(args[i]);
            }
            free(args);
        }
        return NULL;
    }

    return node_call(target, args);
}

Node *parser_parse_expr_index(Parser *parser, Node *target)
{
    parser_advance(parser); // consume '['

    Node *index = parser_parse_expr(parser);
    if (index == NULL)
    {
        node_dnit(target);
        free(target);
        return NULL;
    }

    if (!parser_consume(parser, TOKEN_R_BRACKET, "Expected ']'"))
    {
        node_dnit(target);
        free(target);
        node_dnit(index);
        free(index);
        return NULL;
    }

    Node *node = malloc(sizeof(Node));
    if (node == NULL)
    {
        return NULL;
    }

    node_init(node, NODE_EXPR_INDEX);
    node->binary.left  = target;
    node->binary.right = index;
    return node;
}

Node *parser_parse_expr_member(Parser *parser, Node *target)
{
    parser_advance(parser); // consume '.'

    Node *member = parser_parse_identifier(parser);
    if (member == NULL)
    {
        node_dnit(target);
        free(target);
        return NULL;
    }

    Node *node = malloc(sizeof(Node));
    if (node == NULL)
    {
        return NULL;
    }

    node_init(node, NODE_EXPR_MEMBER);
    node->binary.left  = target;
    node->binary.right = member;
    return node;
}

Node *parser_parse_expr_cast(Parser *parser, Node *target)
{
    parser_advance(parser); // consume '::'

    Node *type = parser_parse_type(parser);
    if (type == NULL)
    {
        node_dnit(target);
        free(target);
        return NULL;
    }

    Node *node = malloc(sizeof(Node));
    if (node == NULL)
    {
        return NULL;
    }

    node_init(node, NODE_EXPR_CAST);
    node->binary.left  = target;
    node->binary.right = type;
    return node;
}

// statement parsing
Node *parser_parse_stmt(Parser *parser)
{
    switch (parser->current.kind)
    {
    case TOKEN_KW_USE:
        return parser_parse_stmt_use(parser);
    case TOKEN_KW_VAL:
        return parser_parse_stmt_val(parser);
    case TOKEN_KW_VAR:
        return parser_parse_stmt_var(parser);
    case TOKEN_KW_DEF:
        return parser_parse_stmt_def(parser);
    case TOKEN_KW_FUN:
        return parser_parse_stmt_fun(parser);
    case TOKEN_KW_STR:
        return parser_parse_stmt_str(parser);
    case TOKEN_KW_UNI:
        return parser_parse_stmt_uni(parser);
    case TOKEN_KW_EXT:
        return parser_parse_stmt_ext(parser);
    case TOKEN_KW_IF:
        return parser_parse_stmt_if(parser);
    case TOKEN_KW_OR:
        return parser_parse_stmt_or(parser);
    case TOKEN_KW_FOR:
        return parser_parse_stmt_for(parser);
    case TOKEN_KW_BRK:
        return parser_parse_stmt_break(parser);
    case TOKEN_KW_CNT:
        return parser_parse_stmt_continue(parser);
    case TOKEN_KW_RET:
        return parser_parse_stmt_return(parser);
    case TOKEN_L_BRACE:
        return parser_parse_stmt_block(parser);
    default:
        return parser_parse_stmt_expr(parser);
    }
}

Node *parser_parse_stmt_block(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_L_BRACE, "Expected '{'"))
    {
        return NULL;
    }

    Node **statements = node_list_new();
    if (statements == NULL)
    {
        return NULL;
    }

    while (!parser_check(parser, TOKEN_R_BRACE) && !parser_check(parser, TOKEN_EOF))
    {
        Node *stmt = parser_parse_stmt(parser);
        if (stmt != NULL)
        {
            node_list_add(&statements, stmt);
        }
        else
        {
            parser_synchronize(parser);
        }
    }

    if (!parser_consume(parser, TOKEN_R_BRACE, "Expected '}'"))
    {
        for (size_t i = 0; i < node_list_count(statements); i++)
        {
            node_dnit(statements[i]);
            free(statements[i]);
        }
        free(statements);
        return NULL;
    }

    return node_block(statements);
}

// rest of statement parsing functions

Node *parser_parse_stmt_use(Parser *parser)
{
    parser_advance(parser); // consume 'use'

    // first parse identifier (could be alias or module)
    Node *first = parser_parse_identifier(parser);
    if (first == NULL)
    {
        return NULL;
    }

    // check for alias syntax: use alias: module
    if (parser_check(parser, TOKEN_COLON))
    {
        parser_advance(parser); // consume ':'

        // first was alias, now parse module (can be dotted)
        Node *module = parser_parse_identifier(parser);
        if (module == NULL)
        {
            node_dnit(first);
            free(first);
            return NULL;
        }

        // handle dotted module names like std.math
        while (parser_check(parser, TOKEN_DOT))
        {
            parser_advance(parser); // consume '.'

            Node *next_part = parser_parse_identifier(parser);
            if (next_part == NULL)
            {
                node_dnit(first);
                free(first);
                node_dnit(module);
                free(module);
                return NULL;
            }

            // create a member access node for the dotted name
            Node *dotted = malloc(sizeof(Node));
            if (dotted == NULL)
            {
                node_dnit(first);
                free(first);
                node_dnit(module);
                free(module);
                node_dnit(next_part);
                free(next_part);
                return NULL;
            }

            node_init(dotted, NODE_EXPR_MEMBER);
            dotted->binary.left  = module;
            dotted->binary.right = next_part;
            module               = dotted;
        }

        // create use node with alias and module
        Node *node = malloc(sizeof(Node));
        if (node == NULL)
        {
            return NULL;
        }

        node_init(node, NODE_STMT_USE);
        node->binary.left  = first;  // alias
        node->binary.right = module; // module

        if (!parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after use statement"))
        {
            node_dnit(node);
            free(node);
            return NULL;
        }

        return node;
    }

    // no colon, so first is the module - handle dotted names
    Node *module = first;
    while (parser_check(parser, TOKEN_DOT))
    {
        parser_advance(parser); // consume '.'

        Node *next_part = parser_parse_identifier(parser);
        if (next_part == NULL)
        {
            node_dnit(module);
            free(module);
            return NULL;
        }

        // create a member access node for the dotted name
        Node *dotted = malloc(sizeof(Node));
        if (dotted == NULL)
        {
            node_dnit(module);
            free(module);
            node_dnit(next_part);
            free(next_part);
            return NULL;
        }

        node_init(dotted, NODE_EXPR_MEMBER);
        dotted->binary.left  = module;
        dotted->binary.right = next_part;
        module               = dotted;
    }

    // simple use statement (no alias)
    Node *node = malloc(sizeof(Node));
    if (node == NULL)
    {
        return NULL;
    }

    node_init(node, NODE_STMT_USE);
    node->binary.left  = NULL;   // no alias
    node->binary.right = module; // module

    if (!parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after use statement"))
    {
        node_dnit(node);
        free(node);
        return NULL;
    }

    return node;
}

Node *parser_parse_stmt_val(Parser *parser)
{
    parser_advance(parser); // consume 'val'

    Node *identifier = parser_parse_identifier(parser);
    if (identifier == NULL)
    {
        return NULL;
    }

    Node *type = NULL;
    if (parser_match(parser, TOKEN_COLON))
    {
        type = parser_parse_type(parser);
        if (type == NULL)
        {
            node_dnit(identifier);
            free(identifier);
            return NULL;
        }
    }

    Node *init = NULL;
    if (parser_match(parser, TOKEN_EQUAL))
    {
        init = parser_parse_expr(parser);
        if (init == NULL)
        {
            node_dnit(identifier);
            free(identifier);
            if (type)
            {
                node_dnit(type);
                free(type);
            }
            return NULL;
        }
    }

    if (!parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after val declaration"))
    {
        node_dnit(identifier);
        free(identifier);
        if (type)
        {
            node_dnit(type);
            free(type);
        }
        if (init)
        {
            node_dnit(init);
            free(init);
        }
        return NULL;
    }

    Node *node = malloc(sizeof(Node));
    if (node == NULL)
    {
        return NULL;
    }

    node_init(node, NODE_STMT_VAL);
    node->decl.name = identifier;
    node->decl.type = type;
    node->decl.init = init;
    return node;
}

Node *parser_parse_stmt_var(Parser *parser)
{
    parser_advance(parser); // consume 'var'

    Node *identifier = parser_parse_identifier(parser);
    if (identifier == NULL)
    {
        return NULL;
    }

    Node *type = NULL;
    if (parser_match(parser, TOKEN_COLON))
    {
        type = parser_parse_type(parser);
        if (type == NULL)
        {
            node_dnit(identifier);
            free(identifier);
            return NULL;
        }
    }

    Node *init = NULL;
    if (parser_match(parser, TOKEN_EQUAL))
    {
        init = parser_parse_expr(parser);
        if (init == NULL)
        {
            node_dnit(identifier);
            free(identifier);
            if (type)
            {
                node_dnit(type);
                free(type);
            }
            return NULL;
        }
    }

    if (!parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after var declaration"))
    {
        node_dnit(identifier);
        free(identifier);
        if (type)
        {
            node_dnit(type);
            free(type);
        }
        if (init)
        {
            node_dnit(init);
            free(init);
        }
        return NULL;
    }

    Node *node = malloc(sizeof(Node));
    if (node == NULL)
    {
        return NULL;
    }

    node_init(node, NODE_STMT_VAR);
    node->decl.name = identifier;
    node->decl.type = type;
    node->decl.init = init;
    return node;
}

Node *parser_parse_stmt_def(Parser *parser)
{
    parser_advance(parser); // consume 'def'

    Node *identifier = parser_parse_identifier(parser);
    if (identifier == NULL)
    {
        return NULL;
    }

    if (!parser_consume(parser, TOKEN_COLON, "Expected ':' after def name"))
    {
        node_dnit(identifier);
        free(identifier);
        return NULL;
    }

    Node *type = parser_parse_type(parser);
    if (type == NULL)
    {
        node_dnit(identifier);
        free(identifier);
        return NULL;
    }

    if (!parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after def declaration"))
    {
        node_dnit(identifier);
        free(identifier);
        node_dnit(type);
        free(type);
        return NULL;
    }

    Node *node = malloc(sizeof(Node));
    if (node == NULL)
    {
        return NULL;
    }

    node_init(node, NODE_STMT_DEF);
    node->decl.name = identifier;
    node->decl.type = type;
    node->decl.init = NULL;
    return node;
}

Node *parser_parse_stmt_fun(Parser *parser)
{
    parser_advance(parser); // consume 'fun'

    Node *name = parser_parse_identifier(parser);
    if (name == NULL)
    {
        return NULL;
    }

    if (!parser_consume(parser, TOKEN_L_PAREN, "Expected '(' after function name"))
    {
        node_dnit(name);
        free(name);
        return NULL;
    }

    bool   is_variadic = false;
    Node **params      = parser_parse_parameter_list(parser, &is_variadic);

    if (!parser_consume(parser, TOKEN_R_PAREN, "Expected ')' after parameters"))
    {
        node_dnit(name);
        free(name);
        if (params)
        {
            for (size_t i = 0; i < node_list_count(params); i++)
            {
                node_dnit(params[i]);
                free(params[i]);
            }
            free(params);
        }
        return NULL;
    }

    Node *return_type = NULL;
    if (!parser_check(parser, TOKEN_L_BRACE))
    {
        return_type = parser_parse_type(parser);
        if (return_type == NULL)
        {
            node_dnit(name);
            free(name);
            if (params)
            {
                for (size_t i = 0; i < node_list_count(params); i++)
                {
                    node_dnit(params[i]);
                    free(params[i]);
                }
                free(params);
            }
            return NULL;
        }
    }

    Node *body = parser_parse_stmt_block(parser);
    if (body == NULL)
    {
        node_dnit(name);
        free(name);
        if (params)
        {
            for (size_t i = 0; i < node_list_count(params); i++)
            {
                node_dnit(params[i]);
                free(params[i]);
            }
            free(params);
        }
        if (return_type)
        {
            node_dnit(return_type);
            free(return_type);
        }
        return NULL;
    }

    Node *node = malloc(sizeof(Node));
    if (node == NULL)
    {
        return NULL;
    }

    node_init(node, NODE_STMT_FUNCTION);
    node->function.params      = params;
    node->function.return_type = return_type;
    node->function.body        = body;
    node->function.is_variadic = is_variadic;
    return node;
}

Node *parser_parse_stmt_str(Parser *parser)
{
    parser_advance(parser); // consume 'str'

    // check if this is direct struct definition: str name { ... }
    if (parser_check(parser, TOKEN_IDENTIFIER))
    {
        Node *name = parser_parse_identifier(parser);
        if (name == NULL)
        {
            return NULL;
        }

        if (!parser_consume(parser, TOKEN_L_BRACE, "Expected '{' after struct name"))
        {
            node_dnit(name);
            free(name);
            return NULL;
        }

        Node **fields = parser_parse_field_list(parser);

        if (!parser_consume(parser, TOKEN_R_BRACE, "Expected '}' after struct fields"))
        {
            node_dnit(name);
            free(name);
            if (fields)
            {
                for (size_t i = 0; i < node_list_count(fields); i++)
                {
                    node_dnit(fields[i]);
                    free(fields[i]);
                }
                free(fields);
            }
            return NULL;
        }

        // create struct type node
        Node *struct_type = malloc(sizeof(Node));
        if (struct_type == NULL)
        {
            node_dnit(name);
            free(name);
            if (fields)
            {
                for (size_t i = 0; i < node_list_count(fields); i++)
                {
                    node_dnit(fields[i]);
                    free(fields[i]);
                }
                free(fields);
            }
            return NULL;
        }

        node_init(struct_type, NODE_STMT_STRUCT);
        struct_type->composite.fields = fields;

        // create def statement node (same AST as def name: str { ... };)
        Node *node = malloc(sizeof(Node));
        if (node == NULL)
        {
            node_dnit(name);
            free(name);
            node_dnit(struct_type);
            free(struct_type);
            return NULL;
        }

        node_init(node, NODE_STMT_DEF);
        node->decl.name = name;
        node->decl.type = struct_type;
        node->decl.init = NULL;
        return node;
    }

    // anonymous struct definition for type context: str { ... }
    if (!parser_consume(parser, TOKEN_L_BRACE, "Expected '{' after str"))
    {
        return NULL;
    }

    Node **fields = parser_parse_field_list(parser);

    if (!parser_consume(parser, TOKEN_R_BRACE, "Expected '}' after struct fields"))
    {
        if (fields)
        {
            for (size_t i = 0; i < node_list_count(fields); i++)
            {
                node_dnit(fields[i]);
                free(fields[i]);
            }
            free(fields);
        }
        return NULL;
    }

    Node *node = malloc(sizeof(Node));
    if (node == NULL)
    {
        return NULL;
    }

    node_init(node, NODE_STMT_STRUCT);
    node->composite.fields = fields;
    return node;
}

Node *parser_parse_stmt_uni(Parser *parser)
{
    parser_advance(parser); // consume 'uni'

    // check if this is direct union definition: uni name { ... }
    if (parser_check(parser, TOKEN_IDENTIFIER))
    {
        Node *name = parser_parse_identifier(parser);
        if (name == NULL)
        {
            return NULL;
        }

        if (!parser_consume(parser, TOKEN_L_BRACE, "Expected '{' after union name"))
        {
            node_dnit(name);
            free(name);
            return NULL;
        }

        Node **fields = parser_parse_field_list(parser);

        if (!parser_consume(parser, TOKEN_R_BRACE, "Expected '}' after union fields"))
        {
            node_dnit(name);
            free(name);
            if (fields)
            {
                for (size_t i = 0; i < node_list_count(fields); i++)
                {
                    node_dnit(fields[i]);
                    free(fields[i]);
                }
                free(fields);
            }
            return NULL;
        }

        // create union type node
        Node *union_type = malloc(sizeof(Node));
        if (union_type == NULL)
        {
            node_dnit(name);
            free(name);
            if (fields)
            {
                for (size_t i = 0; i < node_list_count(fields); i++)
                {
                    node_dnit(fields[i]);
                    free(fields[i]);
                }
                free(fields);
            }
            return NULL;
        }

        node_init(union_type, NODE_STMT_UNION);
        union_type->composite.fields = fields;

        // create def statement node (same AST as def name: uni { ... };)
        Node *node = malloc(sizeof(Node));
        if (node == NULL)
        {
            node_dnit(name);
            free(name);
            node_dnit(union_type);
            free(union_type);
            return NULL;
        }

        node_init(node, NODE_STMT_DEF);
        node->decl.name = name;
        node->decl.type = union_type;
        node->decl.init = NULL;
        return node;
    }

    // anonymous union definition for type context: uni { ... }
    if (!parser_consume(parser, TOKEN_L_BRACE, "Expected '{' after uni"))
    {
        return NULL;
    }

    Node **fields = parser_parse_field_list(parser);

    if (!parser_consume(parser, TOKEN_R_BRACE, "Expected '}' after union fields"))
    {
        if (fields)
        {
            for (size_t i = 0; i < node_list_count(fields); i++)
            {
                node_dnit(fields[i]);
                free(fields[i]);
            }
            free(fields);
        }
        return NULL;
    }

    Node *node = malloc(sizeof(Node));
    if (node == NULL)
    {
        return NULL;
    }

    node_init(node, NODE_STMT_UNION);
    node->composite.fields = fields;
    return node;
}

Node *parser_parse_stmt_ext(Parser *parser)
{
    parser_advance(parser); // consume 'ext'

    Node *name = parser_parse_identifier(parser);
    if (name == NULL)
    {
        return NULL;
    }

    if (!parser_consume(parser, TOKEN_COLON, "Expected ':' after external name"))
    {
        node_dnit(name);
        free(name);
        return NULL;
    }

    Node *type = parser_parse_type(parser);
    if (type == NULL)
    {
        node_dnit(name);
        free(name);
        return NULL;
    }

    if (!parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after external declaration"))
    {
        node_dnit(name);
        free(name);
        node_dnit(type);
        free(type);
        return NULL;
    }

    Node *node = malloc(sizeof(Node));
    if (node == NULL)
    {
        return NULL;
    }

    node_init(node, NODE_STMT_EXTERNAL);
    node->decl.name = name;
    node->decl.type = type;
    node->decl.init = NULL;
    return node;
}

Node *parser_parse_stmt_if(Parser *parser)
{
    parser_advance(parser); // consume 'if'

    if (!parser_consume(parser, TOKEN_L_PAREN, "Expected '(' after if"))
    {
        return NULL;
    }

    Node *condition = parser_parse_expr(parser);
    if (condition == NULL)
    {
        return NULL;
    }

    if (!parser_consume(parser, TOKEN_R_PAREN, "Expected ')' after if condition"))
    {
        node_dnit(condition);
        free(condition);
        return NULL;
    }

    Node *body = parser_parse_stmt_block(parser);
    if (body == NULL)
    {
        node_dnit(condition);
        free(condition);
        return NULL;
    }

    Node *node = malloc(sizeof(Node));
    if (node == NULL)
    {
        return NULL;
    }

    node_init(node, NODE_STMT_IF);
    node->conditional.condition = condition;
    node->conditional.body      = body;
    return node;
}

Node *parser_parse_stmt_or(Parser *parser)
{
    parser_advance(parser); // consume 'or'

    Node *condition = NULL;
    if (parser_check(parser, TOKEN_L_PAREN))
    {
        parser_advance(parser); // consume '('
        condition = parser_parse_expr(parser);
        if (condition == NULL)
        {
            return NULL;
        }

        if (!parser_consume(parser, TOKEN_R_PAREN, "Expected ')' after or condition"))
        {
            node_dnit(condition);
            free(condition);
            return NULL;
        }
    }

    Node *body = parser_parse_stmt_block(parser);
    if (body == NULL)
    {
        if (condition)
        {
            node_dnit(condition);
            free(condition);
        }
        return NULL;
    }

    Node *node = malloc(sizeof(Node));
    if (node == NULL)
    {
        return NULL;
    }

    node_init(node, NODE_STMT_OR);
    node->conditional.condition = condition;
    node->conditional.body      = body;
    return node;
}

Node *parser_parse_stmt_for(Parser *parser)
{
    parser_advance(parser); // consume 'for'

    Node *condition = NULL;
    if (parser_check(parser, TOKEN_L_PAREN))
    {
        parser_advance(parser); // consume '('
        condition = parser_parse_expr(parser);
        if (condition == NULL)
        {
            return NULL;
        }

        if (!parser_consume(parser, TOKEN_R_PAREN, "Expected ')' after for condition"))
        {
            node_dnit(condition);
            free(condition);
            return NULL;
        }
    }

    Node *body = parser_parse_stmt_block(parser);
    if (body == NULL)
    {
        if (condition)
        {
            node_dnit(condition);
            free(condition);
        }
        return NULL;
    }

    Node *node = malloc(sizeof(Node));
    if (node == NULL)
    {
        return NULL;
    }

    node_init(node, NODE_STMT_FOR);
    node->conditional.condition = condition;
    node->conditional.body      = body;
    return node;
}

Node *parser_parse_stmt_break(Parser *parser)
{
    parser_advance(parser); // consume 'brk'

    if (!parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after break"))
    {
        return NULL;
    }

    Node *node = malloc(sizeof(Node));
    if (node == NULL)
    {
        return NULL;
    }

    node_init(node, NODE_STMT_BREAK);
    return node;
}

Node *parser_parse_stmt_continue(Parser *parser)
{
    parser_advance(parser); // consume 'cnt'

    if (!parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after continue"))
    {
        return NULL;
    }

    Node *node = malloc(sizeof(Node));
    if (node == NULL)
    {
        return NULL;
    }

    node_init(node, NODE_STMT_CONTINUE);
    return node;
}

Node *parser_parse_stmt_return(Parser *parser)
{
    parser_advance(parser); // consume 'ret'

    Node *expr = NULL;
    if (!parser_check(parser, TOKEN_SEMICOLON))
    {
        expr = parser_parse_expr(parser);
        if (expr == NULL)
        {
            return NULL;
        }
    }

    if (!parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after return"))
    {
        if (expr)
        {
            node_dnit(expr);
            free(expr);
        }
        return NULL;
    }

    Node *node = malloc(sizeof(Node));
    if (node == NULL)
    {
        return NULL;
    }

    node_init(node, NODE_STMT_RETURN);
    node->single = expr;
    return node;
}

Node *parser_parse_stmt_expr(Parser *parser)
{
    Node *expr = parser_parse_expr(parser);
    if (expr == NULL)
    {
        return NULL;
    }

    if (!parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after expression"))
    {
        node_dnit(expr);
        free(expr);
        return NULL;
    }

    Node *node = malloc(sizeof(Node));
    if (node == NULL)
    {
        return NULL;
    }

    node_init(node, NODE_STMT_EXPRESSION);
    node->single = expr;
    return node;
}

// type parsing
Node *parser_parse_type(Parser *parser)
{
    Node *base_type = NULL;

    // handle prefix modifiers (pointers)
    int pointer_count = 0;
    while (parser_check(parser, TOKEN_STAR))
    {
        parser_advance(parser); // consume '*'
        pointer_count++;
    }

    // handle base types
    switch (parser->current.kind)
    {
    case TOKEN_TYPE_ANY:
    case TOKEN_TYPE_I8:
    case TOKEN_TYPE_I16:
    case TOKEN_TYPE_I32:
    case TOKEN_TYPE_I64:
    case TOKEN_TYPE_U8:
    case TOKEN_TYPE_U16:
    case TOKEN_TYPE_U32:
    case TOKEN_TYPE_U64:
    case TOKEN_TYPE_F32:
    case TOKEN_TYPE_F64:
    {
        char *type_name = lexer_raw_value(parser->lexer, &parser->current);
        base_type       = node_identifier(type_name);
        free(type_name);
        parser_advance(parser);
    }
    break;
    case TOKEN_IDENTIFIER:
        base_type = parser_parse_identifier(parser);
        break;
    case TOKEN_KW_FUN:
        base_type = parser_parse_type_function(parser);
        break;
    case TOKEN_KW_STR:
        base_type = parser_parse_stmt_str(parser); // reuse struct parsing
        break;
    case TOKEN_KW_UNI:
        base_type = parser_parse_stmt_uni(parser); // reuse union parsing
        break;
    case TOKEN_L_BRACKET:
        // handle array types that start with [, like [_]u8 or [10]i32
        base_type = parser_parse_type_array_prefix(parser);
        break;
    default:
        parser_error(parser, "Expected type");
        return NULL;
    }

    if (base_type == NULL)
    {
        return NULL;
    }

    // apply pointer prefixes (in reverse order since we counted them)
    for (int i = 0; i < pointer_count; i++)
    {
        Node *pointer_node = malloc(sizeof(Node));
        if (pointer_node == NULL)
        {
            node_dnit(base_type);
            free(base_type);
            return NULL;
        }

        node_init(pointer_node, NODE_TYPE_POINTER);
        pointer_node->single = base_type;
        base_type            = pointer_node;
    }

    // handle postfix modifiers (arrays, additional pointers)
    while (true)
    {
        if (parser_check(parser, TOKEN_L_BRACKET))
        {
            base_type = parser_parse_type_array(parser, base_type);
            if (base_type == NULL)
            {
                return NULL;
            }
        }
        else if (parser_check(parser, TOKEN_STAR))
        {
            base_type = parser_parse_type_pointer(parser, base_type);
            if (base_type == NULL)
            {
                return NULL;
            }
        }
        else
        {
            break;
        }
    }

    return base_type;
}

Node *parser_parse_type_array(Parser *parser, Node *base_type)
{
    parser_advance(parser); // consume '['

    Node *size = NULL;
    if (!parser_check(parser, TOKEN_UNDERSCORE) && !parser_check(parser, TOKEN_R_BRACKET))
    {
        size = parser_parse_expr(parser);
        if (size == NULL)
        {
            node_dnit(base_type);
            free(base_type);
            return NULL;
        }
    }
    else if (parser_check(parser, TOKEN_UNDERSCORE))
    {
        parser_advance(parser); // consume '_' for dynamic array
    }

    if (!parser_consume(parser, TOKEN_R_BRACKET, "Expected ']'"))
    {
        node_dnit(base_type);
        free(base_type);
        if (size)
        {
            node_dnit(size);
            free(size);
        }
        return NULL;
    }

    Node *node = malloc(sizeof(Node));
    if (node == NULL)
    {
        return NULL;
    }

    node_init(node, NODE_TYPE_ARRAY);
    node->binary.left  = size;
    node->binary.right = base_type;
    return node;
}

Node *parser_parse_type_pointer(Parser *parser, Node *base_type)
{
    parser_advance(parser); // consume '*'

    Node *node = malloc(sizeof(Node));
    if (node == NULL)
    {
        return NULL;
    }

    node_init(node, NODE_TYPE_POINTER);
    node->single = base_type;
    return node;
}

Node *parser_parse_type_function(Parser *parser)
{
    parser_advance(parser); // consume 'fun'

    if (!parser_consume(parser, TOKEN_L_PAREN, "Expected '(' after fun"))
    {
        return NULL;
    }

    bool   is_variadic = false;
    Node **params      = parser_parse_type_parameter_list(parser, &is_variadic);

    if (!parser_consume(parser, TOKEN_R_PAREN, "Expected ')' after parameters"))
    {
        if (params)
        {
            for (size_t i = 0; i < node_list_count(params); i++)
            {
                node_dnit(params[i]);
                free(params[i]);
            }
            free(params);
        }
        return NULL;
    }

    Node *return_type = NULL;
    if (!parser_check(parser, TOKEN_SEMICOLON) && !parser_check(parser, TOKEN_R_PAREN) && !parser_check(parser, TOKEN_COMMA) && !parser_check(parser, TOKEN_EOF))
    {
        return_type = parser_parse_type(parser);
        if (return_type == NULL)
        {
            if (params)
            {
                for (size_t i = 0; i < node_list_count(params); i++)
                {
                    node_dnit(params[i]);
                    free(params[i]);
                }
                free(params);
            }
            return NULL;
        }
    }

    Node *node = malloc(sizeof(Node));
    if (node == NULL)
    {
        return NULL;
    }

    node_init(node, NODE_TYPE_FUNCTION);
    node->function.params      = params;
    node->function.return_type = return_type;
    node->function.body        = NULL;
    node->function.is_variadic = is_variadic;
    return node;
}

// parameter list parsing for type declarations (type-only)
Node **parser_parse_type_parameter_list(Parser *parser, bool *is_variadic)
{
    Node **params = node_list_new();
    if (params == NULL)
    {
        return NULL;
    }

    *is_variadic = false;

    while (!parser_check(parser, TOKEN_R_PAREN) && !parser_check(parser, TOKEN_EOF))
    {
        // check for variadic parameters
        if (parser_check(parser, TOKEN_DOT) && parser->peek.kind == TOKEN_DOT)
        {
            parser_advance(parser);              // consume first '.'
            parser_advance(parser);              // consume second '.'
            if (parser_match(parser, TOKEN_DOT)) // consume third '.'
            {
                *is_variadic = true;
                break;
            }
            else
            {
                parser_error(parser, "Expected '...'");
                goto error;
            }
        }

        // for type parameter lists, just parse the type directly
        Node *type = parser_parse_type(parser);
        if (type == NULL)
        {
            goto error;
        }

        // create a parameter node without name
        Node *param = malloc(sizeof(Node));
        if (param == NULL)
        {
            node_dnit(type);
            free(type);
            goto error;
        }

        node_init(param, NODE_STMT_VAL); // use VAL for parameters
        param->decl.name = NULL;         // no name for type-only parameters
        param->decl.type = type;
        param->decl.init = NULL;

        node_list_add(&params, param);

        if (!parser_check(parser, TOKEN_R_PAREN))
        {
            if (!parser_consume(parser, TOKEN_COMMA, "Expected ',' or ')'"))
            {
                goto error;
            }
        }
    }

    return params;

error:
    if (params)
    {
        for (size_t i = 0; i < node_list_count(params); i++)
        {
            node_dnit(params[i]);
            free(params[i]);
        }
        free(params);
    }
    return NULL;
}

// helper functions
Node **parser_parse_parameter_list(Parser *parser, bool *is_variadic)
{
    Node **params = node_list_new();
    if (params == NULL)
    {
        return NULL;
    }

    *is_variadic = false;

    while (!parser_check(parser, TOKEN_R_PAREN) && !parser_check(parser, TOKEN_EOF))
    {
        // check for variadic parameters at start (anonymous: ...)
        if (parser_check(parser, TOKEN_DOT) && parser->peek.kind == TOKEN_DOT)
        {
            parser_advance(parser);              // consume first '.'
            parser_advance(parser);              // consume second '.'
            if (parser_match(parser, TOKEN_DOT)) // consume third '.'
            {
                *is_variadic = true;
                break;
            }
            else
            {
                parser_error(parser, "Expected '...'");
                goto error;
            }
        }

        Node *name = parser_parse_identifier(parser);
        if (name == NULL)
        {
            goto error;
        }

        if (!parser_consume(parser, TOKEN_COLON, "Expected ':' after parameter name"))
        {
            node_dnit(name);
            free(name);
            goto error;
        }

        // check for named variadic parameter: name: ...
        if (parser_check(parser, TOKEN_DOT) && parser->peek.kind == TOKEN_DOT)
        {
            parser_advance(parser);              // consume first '.'
            parser_advance(parser);              // consume second '.'
            if (parser_match(parser, TOKEN_DOT)) // consume third '.'
            {
                // create variadic parameter node with name but no type
                Node *param = malloc(sizeof(Node));
                if (param == NULL)
                {
                    node_dnit(name);
                    free(name);
                    goto error;
                }

                node_init(param, NODE_STMT_VAL); // use VAL for parameters
                param->decl.name = name;
                param->decl.type = NULL; // no type for variadic parameters
                param->decl.init = NULL;

                node_list_add(&params, param);
                *is_variadic = true;
                break; // variadic parameter must be last
            }
            else
            {
                parser_error(parser, "Expected '...'");
                node_dnit(name);
                free(name);
                goto error;
            }
        }

        Node *type = parser_parse_type(parser);
        if (type == NULL)
        {
            node_dnit(name);
            free(name);
            goto error;
        }

        // create parameter node
        Node *param = malloc(sizeof(Node));
        if (param == NULL)
        {
            node_dnit(name);
            free(name);
            node_dnit(type);
            free(type);
            goto error;
        }

        node_init(param, NODE_STMT_VAL); // use VAL for parameters
        param->decl.name = name;
        param->decl.type = type;
        param->decl.init = NULL;

        node_list_add(&params, param);

        if (!parser_check(parser, TOKEN_R_PAREN))
        {
            if (!parser_consume(parser, TOKEN_COMMA, "Expected ',' or ')'"))
            {
                goto error;
            }
        }
    }

    return params;

error:
    if (params)
    {
        for (size_t i = 0; i < node_list_count(params); i++)
        {
            node_dnit(params[i]);
            free(params[i]);
        }
        free(params);
    }
    return NULL;
}

Node **parser_parse_argument_list(Parser *parser)
{
    Node **args = node_list_new();
    if (args == NULL)
    {
        return NULL;
    }

    while (!parser_check(parser, TOKEN_R_PAREN) && !parser_check(parser, TOKEN_EOF))
    {
        Node *arg = parser_parse_expr(parser);
        if (arg != NULL)
        {
            node_list_add(&args, arg);
        }

        if (!parser_check(parser, TOKEN_R_PAREN))
        {
            if (!parser_consume(parser, TOKEN_COMMA, "Expected ',' or ')'"))
            {
                break;
            }
        }
    }

    return args;
}

Node **parser_parse_field_list(Parser *parser)
{
    Node **fields = node_list_new();
    if (fields == NULL)
    {
        return NULL;
    }

    while (!parser_check(parser, TOKEN_R_BRACE) && !parser_check(parser, TOKEN_EOF))
    {
        Node *name = parser_parse_identifier(parser);
        if (name == NULL)
        {
            goto error;
        }

        if (!parser_consume(parser, TOKEN_COLON, "Expected ':' after field name"))
        {
            node_dnit(name);
            free(name);
            goto error;
        }

        Node *type = parser_parse_type(parser);
        if (type == NULL)
        {
            node_dnit(name);
            free(name);
            goto error;
        }

        if (!parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after field"))
        {
            node_dnit(name);
            free(name);
            node_dnit(type);
            free(type);
            goto error;
        }

        // create field node
        Node *field = malloc(sizeof(Node));
        if (field == NULL)
        {
            node_dnit(name);
            free(name);
            node_dnit(type);
            free(type);
            goto error;
        }

        node_init(field, NODE_STMT_VAL); // use VAL for fields
        field->decl.name = name;
        field->decl.type = type;
        field->decl.init = NULL;

        node_list_add(&fields, field);
    }

    return fields;

error:
    if (fields)
    {
        for (size_t i = 0; i < node_list_count(fields); i++)
        {
            node_dnit(fields[i]);
            free(fields[i]);
        }
        free(fields);
    }
    return NULL;
}

Node *parser_parse_type_array_prefix(Parser *parser)
{
    parser_advance(parser); // consume '['

    Node *size = NULL;
    if (!parser_check(parser, TOKEN_UNDERSCORE) && !parser_check(parser, TOKEN_R_BRACKET))
    {
        size = parser_parse_expr(parser);
        if (size == NULL)
        {
            return NULL;
        }
    }
    else if (parser_check(parser, TOKEN_UNDERSCORE))
    {
        parser_advance(parser); // consume '_' for dynamic array
    }

    if (!parser_consume(parser, TOKEN_R_BRACKET, "Expected ']'"))
    {
        if (size)
        {
            node_dnit(size);
            free(size);
        }
        return NULL;
    }

    Node *base_type = parser_parse_type(parser);
    if (base_type == NULL)
    {
        if (size)
        {
            node_dnit(size);
            free(size);
        }
        return NULL;
    }

    Node *node = malloc(sizeof(Node));
    if (node == NULL)
    {
        if (size)
        {
            node_dnit(size);
            free(size);
        }
        node_dnit(base_type);
        free(base_type);
        return NULL;
    }

    node_init(node, NODE_TYPE_ARRAY);
    node->binary.left  = size;
    node->binary.right = base_type;
    return node;
}
