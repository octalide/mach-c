#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "op.h"
#include "parser.h"

#define PARSER_DEBUG_TRACE

void parser_init(Parser *parser, Lexer *lexer)
{
#ifdef PARSER_DEBUG_TRACE
    printf("parser_init\n");
#endif

    parser->lexer = lexer;
    parser->current = next_token(lexer);
    parser->previous.type = TOKEN_EOF;
}

void parser_error(Parser *parser, const char *message)
{
#ifdef PARSER_DEBUG_TRACE
    printf("parser_error\n");
#endif

    fprintf(stderr, "error: %s\n", message);
    parser->error = message;
}

void parser_advance(Parser *parser)
{
#ifdef PARSER_DEBUG_TRACE
    printf("parser_advance\n");
#endif

    parser->previous = parser->current;
    parser->current = next_token(parser->lexer);

#ifdef PARSER_DEBUG_TRACE
    printf("L TOK: LN: %-3d T: %-19s: %.*s\n", parser->lexer->line, token_type_string(parser->current.type), parser->current.length, parser->current.start);
#endif
}

bool parser_peek(Parser *parser, TokenType type)
{
#ifdef PARSER_DEBUG_TRACE
    printf("parser_peek\n");
#endif

    return parser->current.type == type;
}

bool parser_match(Parser *parser, TokenType type)
{
#ifdef PARSER_DEBUG_TRACE
    printf("parser_match\n");
#endif

    if (parser_peek(parser, type))
    {
        parser_advance(parser);
        return true;
    }

    return false;
}

bool parser_consume(Parser *parser, TokenType type, const char *message)
{
#ifdef PARSER_DEBUG_TRACE
    printf("parser_consume\n");
#endif

    if (parser_match(parser, type))
    {
        return true;
    }

    parser_error(parser, message);

    return false;
}

Node *parse_expr_type_identifier(Parser *parser)
{
#ifdef PARSER_DEBUG_TRACE
    printf("parse_expr_type_identifier\n");
#endif

    if (parser->current.type != TOKEN_IDENTIFIER)
    {
        // err: expected an identifier (probably the parser's fault)
        parser_error(parser, "expected an identifier");
        return NULL;
    }

    Node *node = new_node(NODE_EXPR_TYPE);
    node->data.expr_type.identifier = new_node(NODE_EXPR_IDENTIFIER);
    node->data.expr_type.identifier->data.expr_identifier.name = parser->current.start;

    parser_advance(parser);

    return node;
}

Node *parse_expr_type_array(Parser *parser)
{
#ifdef PARSER_DEBUG_TRACE
    printf("parse_expr_type_array\n");
#endif

    if (!parser_consume(parser, TOKEN_LEFT_BRACKET, "expected '['"))
    {
        // err: expected left bracket (probably the parser's fault)
        return NULL;
    }

    Node *node = new_node(NODE_EXPR_TYPE_ARRAY);
    node->data.expr_type_array.size = parse_expr(parser);
    if (node->data.expr_type_array.size == NULL)
    {
        // err: size is missing
        free_node(node);
        return NULL;
    }

    if (!parser_consume(parser, TOKEN_RIGHT_BRACKET, "expected ']'"))
    {
        // err: no right bracket after array type
        free_node(node);
        return NULL;
    }

    node->data.expr_type_array.type = parse_expr_type(parser);
    if (node->data.expr_type_array.type == NULL)
    {
        // err: type is missing
        free_node(node);
        return NULL;
    }

    return node;
}

Node *parse_expr_type_ref(Parser *parser)
{
#ifdef PARSER_DEBUG_TRACE
    printf("parse_expr_type_ref\n");
#endif

    if (!parser_consume(parser, TOKEN_HASH, "expected '#'"))
    {
        // err: expected hash (probably the parser's fault)
        return NULL;
    }

    Node *node = new_node(NODE_EXPR_TYPE_REF);
    node->data.expr_type_ref.type = parse_expr_type(parser);
    if (node->data.expr_type_ref.type == NULL)
    {
        // err: type is missing
        free_node(node);
        return NULL;
    }

    return node;
}

Node *parse_expr_type_member(Parser *parser, Node *target)
{
#ifdef PARSER_DEBUG_TRACE
    printf("parse_expr_type_member\n");
#endif

    Node *node = new_node(NODE_EXPR_TYPE_MEMBER);
    node->data.expr_type_member.target = target;

    if (parser->current.type != TOKEN_IDENTIFIER)
    {
        // err: expected an identifier
        free_node(node);
        return NULL;
    }

    Node *identifier = new_node(NODE_EXPR_IDENTIFIER);
    identifier->data.expr_identifier.name = parser->current.start;

    node->data.expr_type_member.member = identifier;

    parser_advance(parser);

    return node;
}

Node *parse_expr_type(Parser *parser)
{
#ifdef PARSER_DEBUG_TRACE
    printf("parse_expr_type\n");
#endif

    switch (parser->current.type)
    {
    case TOKEN_IDENTIFIER:
        return parse_expr_type_identifier(parser);
    case TOKEN_LEFT_BRACKET:
        return parse_expr_type_array(parser);
    case TOKEN_HASH:
        return parse_expr_type_ref(parser);
    default:
        // err: unexpected token (only type identifiers allowed)
        parser_error(parser, "expected a type");
        return NULL;
    }
}

Node *parse_expr_call(Parser *parser, Node *target)
{
#ifdef PARSER_DEBUG_TRACE
    printf("parse_expr_call\n");
#endif

    Node *node = new_node(NODE_EXPR_CALL);
    node->data.expr_call.target = target;

    if (!parser_consume(parser, TOKEN_LEFT_PAREN, "expected '('"))
    {
        // err: no left parenthesis after identifier
        free_node(node);
        return NULL;
    }

    while (true)
    {
        if (parser->current.type == TOKEN_RIGHT_PAREN)
        {
            break;
        }

        Node *argument = parse_expr(parser);
        if (argument == NULL)
        {
            // err: argument is missing
            free_node(node);
            return NULL;
        }

        if (node->data.expr_call.arguments == NULL)
        {
            node->data.expr_call.arguments = argument;
        }
        else
        {
            Node *last = node->data.expr_call.arguments;
            while (last->next != NULL)
            {
                last = last->next;
            }

            last->next = argument;
        }

        node->data.expr_call.argument_count++;

        if (!parser_match(parser, TOKEN_COMMA))
        {
            break;
        }
    }

    if (!parser_consume(parser, TOKEN_RIGHT_PAREN, "expected ')'"))
    {
        // err: no right parenthesis after arguments
        free_node(node);
        return NULL;
    }

    return node;
}

Node *parse_expr_index(Parser *parser, Node *target)
{
#ifdef PARSER_DEBUG_TRACE
    printf("parse_expr_index\n");
#endif

    Node *node = new_node(NODE_EXPR_INDEX);
    node->data.expr_index.target = target;

    if (!parser_consume(parser, TOKEN_LEFT_BRACKET, "expected '['"))
    {
        // err: no left bracket after identifier
        free_node(node);
        return NULL;
    }

    node->data.expr_index.index = parse_expr(parser);
    if (node->data.expr_index.index == NULL)
    {
        // err: index is missing
        free_node(node);
        return NULL;
    }

    if (!parser_consume(parser, TOKEN_RIGHT_BRACKET, "expected ']'"))
    {
        // err: no right bracket after index
        free_node(node);
        return NULL;
    }

    return node;
}

Node *parse_expr_array(Parser *parser)
{
#ifdef PARSER_DEBUG_TRACE
    printf("parse_expr_array\n");
#endif

    Node *node = new_node(NODE_EXPR_ARRAY);

    if (!parser_consume(parser, TOKEN_LEFT_BRACKET, "expected '['"))
    {
        // err: no left bracket after '['
        free_node(node);
        return NULL;
    }

    while (true)
    {
        if (parser->current.type == TOKEN_RIGHT_BRACKET)
        {
            break;
        }

        Node *element = parse_expr(parser);
        if (element == NULL)
        {
            // err: element is missing
            free_node(node);
            return NULL;
        }

        if (node->data.expr_array.elements == NULL)
        {
            node->data.expr_array.elements = element;
        }
        else
        {
            Node *last = node->data.expr_array.elements;
            while (last->next != NULL)
            {
                last = last->next;
            }

            last->next = element;
        }

        if (!parser_match(parser, TOKEN_COMMA))
        {
            break;
        }
    }

    if (!parser_consume(parser, TOKEN_RIGHT_BRACKET, "expected ']'"))
    {
        // err: no right bracket after array
        free_node(node);
        return NULL;
    }

    return node;
}

Node *parse_expr_unary(Parser *parser)
{
#ifdef PARSER_DEBUG_TRACE
    printf("parse_expr_unary\n");
#endif

    if (token_is_operator_unary(parser->current.type))
    {
        Token operator= parser->current;
        parser_advance(parser);

        Node *right = parse_expr_unary(parser);
        if (right == NULL)
        {
            // err: right side of unary operator is missing
            return NULL;
        }

        Node *node = new_node(NODE_EXPR_UNARY);
        node->data.expr_unary.right = right;
        node->data.expr_unary.operator= operator.type;

        return node;
    }

    return parse_expr_primary(parser);
}

Node *parse_expr_binary(Parser *parser, int precedence_parent)
{
#ifdef PARSER_DEBUG_TRACE
    printf("parse_expr_binary\n");
#endif

    Node *left = parse_expr_unary(parser);
    if (left == NULL)
    {
        // err: left side of binary operator is missing
        return NULL;
    }

    while (token_is_operator_binary(parser->current.type) && get_precedence(parser->current.type) >= precedence_parent)
    {
        Token operator= parser->current;
        parser_advance(parser);

        int precedence = get_precedence(operator.type);
        if (token_is_operator_right_associative(operator.type))
        {
            precedence++;
        }

        Node *right = parse_expr_binary(parser, precedence);
        if (right == NULL)
        {
            // err: right side of binary operator is missing
            free_node(left);
            return NULL;
        }

        Node *node = new_node(NODE_EXPR_BINARY);
        node->data.expr_binary.left = left;
        node->data.expr_binary.right = right;
        node->data.expr_binary.operator= operator.type;

        left = node;
    }

    return left;
}

Node *parse_expr_grouping(Parser *parser)
{
#ifdef PARSER_DEBUG_TRACE
    printf("parse_expr_grouping\n");
#endif

    if (parser_match(parser, TOKEN_LEFT_PAREN))
    {
        Node *node = parse_expr(parser);
        if (node == NULL)
        {
            // err: expression is missing
            return NULL;
        }

        if (!parser_consume(parser, TOKEN_RIGHT_PAREN, "expected ')'"))
        {
            // err: no right parenthesis after expression
            free_node(node);
            return NULL;
        }

        return node;
    }

    return parse_expr_primary(parser);
}

Node *parse_expr_primary(Parser *parser)
{
#ifdef PARSER_DEBUG_TRACE
    printf("parse_expr_primary\n");
#endif

    Node *node = NULL;

    switch (parser->current.type)
    {
    case TOKEN_NUMBER:
    case TOKEN_STRING:
        node = new_node(NODE_EXPR_LITERAL);
        node->data.expr_literal.value = parser->current;
        parser_advance(parser);
        break;
    case TOKEN_IDENTIFIER:
        Node *target = new_node(NODE_EXPR_IDENTIFIER);
        target->data.expr_identifier.name = parser->current.start;
        parser_advance(parser);

        if (parser_peek(parser, TOKEN_LEFT_PAREN))
        {
            node = parse_expr_call(parser, target);
        }
        else if (parser_peek(parser, TOKEN_LEFT_BRACKET))
        {
            node = parse_expr_index(parser, target);
        }
        else
        {
            node = target;
        }

        break;
    case TOKEN_LEFT_PAREN:
        node = parse_expr_grouping(parser);
        break;
    case TOKEN_LEFT_BRACKET:
        node = parse_expr_array(parser);
        break;
    default:
        // err: unexpected token (only literals and identifiers allowed)
        free_node(node);
        return NULL;
    }

    while (parser_match(parser, TOKEN_DOT))
    {
        node = parse_expr_type_member(parser, node);
        if (node == NULL)
        {
            // err: member is missing
            return NULL;
        }
    }

    return node;
}

Node *parse_expr(Parser *parser)
{
#ifdef PARSER_DEBUG_TRACE
    printf("parse_expr\n");
#endif

    return parse_expr_binary(parser, 0);
}

Node *parse_stmt_use(Parser *parser)
{
#ifdef PARSER_DEBUG_TRACE
    printf("parse_stmt_use\n");
#endif

    Node *node = new_node(NODE_STMT_USE);

    if (!parser_consume(parser, TOKEN_USE, "expected 'use`"))
    {
        // err: invalid token (probably the parser's fault)
        free_node(node);
        return NULL;
    }

    if (parser->current.type != TOKEN_IDENTIFIER)
    {
        // err: expected an identifier
        parser_error(parser, "expected an identifier");
        free_node(node);
        return NULL;
    }

    node->data.stmt_use.module = new_node(NODE_EXPR_IDENTIFIER);
    node->data.stmt_use.module->data.expr_identifier.name = parser->current.start;

    parser_advance(parser);

    while (true)
    {
        if (parser->current.type == TOKEN_SEMICOLON)
        {
            break;
        }

        if (!parser_consume(parser, TOKEN_DOT, "expected '.'"))
        {
            // err: no dot after part
            free_node(node);
            return NULL;
        }

        if (!parser_match(parser, TOKEN_IDENTIFIER))
        {
            // err: expected an identifier
            parser_error(parser, "expected an identifier");
            free_node(node);
            return NULL;
        }

        Node *part = new_node(NODE_EXPR_IDENTIFIER);
        part->data.expr_identifier.name = parser->previous.start;

        Node *last = node->data.stmt_use.module;
        if (last == NULL)
        {
            node->data.stmt_use.module = part;
        }
        else
        {
            while (last->next != NULL)
            {
                last = last->next;
            }

            last->next = part;
        }
    }

    if (!parser_consume(parser, TOKEN_SEMICOLON, "expected ';'"))
    {
        // err: no semicolon after use statement
        free_node(node);
        return NULL;
    }

    return node;
}

Node *parse_stmt_fun(Parser *parser)
{
#ifdef PARSER_DEBUG_TRACE
    printf("parse_stmt_fun\n");
#endif

    Node *node = new_node(NODE_STMT_FUN);

    if (!parser_consume(parser, TOKEN_FUN, "expected 'fun'"))
    {
        // err: invalid token (probably the parser's fault)
        free_node(node);
        return NULL;
    }

    if (parser->current.type != TOKEN_IDENTIFIER)
    {
        // err: next token after `fun` is not an identifier
        parser_error(parser, "expected an identifier");
        free_node(node);
        return NULL;
    }

    node->data.stmt_fun.identifier = new_node(NODE_EXPR_IDENTIFIER);
    node->data.stmt_fun.identifier->data.expr_identifier.name = parser->current.start;

    parser_advance(parser);

    if (!parser_consume(parser, TOKEN_LEFT_PAREN, "expected '('"))
    {
        // err: next token after identifier is not a left parenthesis
        free_node(node);
        return NULL;
    }

    while (true)
    {
        if (parser->current.type == TOKEN_RIGHT_PAREN)
        {
            break;
        }

        if (parser_match(parser, TOKEN_IDENTIFIER))
        {
            const char *name = parser->previous.start;

            if (!parser_consume(parser, TOKEN_COLON, "expected ':'"))
            {
                // err: no colon after parameter name
                free_node(node);
                return NULL;
            }

            Node *type = parse_expr_type(parser);
            if (type == NULL)
            {
                // err: type is missing
                free_node(node);
                return NULL;
            }

            if (node->data.stmt_fun.parameters == NULL)
            {
                node->data.stmt_fun.parameters = (fun_parameter *)malloc(sizeof(fun_parameter));
            }
            else
            {
                node->data.stmt_fun.parameters = (fun_parameter *)realloc(node->data.stmt_fun.parameters, (node->data.stmt_fun.parameter_count + 1) * sizeof(fun_parameter));
            }

            node->data.stmt_fun.parameters[node->data.stmt_fun.parameter_count].name = name;
            node->data.stmt_fun.parameters[node->data.stmt_fun.parameter_count].type = type;
            node->data.stmt_fun.parameter_count++;
        }

        if (!parser_match(parser, TOKEN_COMMA))
        {
            break;
        }
    }

    if (!parser_consume(parser, TOKEN_RIGHT_PAREN, "expected ')'"))
    {
        // err: no right parenthesis after parameters
        free_node(node);
        return NULL;
    }

    // optional return type
    if (parser_match(parser, TOKEN_COLON))
    {
        node->data.stmt_fun.return_type = parse_expr_type(parser);
        if (node->data.stmt_fun.return_type == NULL)
        {
            // err: return type is missing
            free_node(node);
            return NULL;
        }
    }

    node->data.stmt_fun.body = parse_stmt_block(parser);
    if (node->data.stmt_fun.body == NULL)
    {
        // err: function body is missing
        free_node(node);
        return NULL;
    }

    return node;
}

Node *parse_stmt_str(Parser *parser)
{
#ifdef PARSER_DEBUG_TRACE
    printf("parse_stmt_str\n");
#endif

    Node *node = new_node(NODE_STMT_STR);

    if (!parser_consume(parser, TOKEN_STR, "expected 'str'"))
    {
        // err: invalid token (probably the parser's fault)
        free_node(node);
        return NULL;
    }

    if (parser->current.type != TOKEN_IDENTIFIER)
    {
        // err: next token after `str` is not an identifier
        parser_error(parser, "expected an identifier");
        free_node(node);
        return NULL;
    }

    node->data.stmt_str.identifier = new_node(NODE_EXPR_IDENTIFIER);
    node->data.stmt_str.identifier->data.expr_identifier.name = parser->current.start;

    parser_advance(parser);

    if (!parser_consume(parser, TOKEN_COLON, "expected ':'"))
    {
        // err: no colon after identifier
        free_node(node);
        return NULL;
    }

    if (!parser_consume(parser, TOKEN_LEFT_BRACE, "expected '{'"))
    {
        // err: no right brace after colon
        free_node(node);
        return NULL;
    }

    if (!parser_peek(parser, TOKEN_IDENTIFIER))
    {
        // err: no fields after left brace
        parser_error(parser, "expected a field");
        free_node(node);
        return NULL;
    }

    while (true)
    {
        if (parser->current.type == TOKEN_RIGHT_BRACE)
        {
            break;
        }

        if (parser_match(parser, TOKEN_IDENTIFIER))
        {
            const char *name = parser->previous.start;

            if (!parser_consume(parser, TOKEN_COLON, "expected ':'"))
            {
                // err: no colon after field name
                free_node(node);
                return NULL;
            }

            Node *type = parse_expr_type(parser);
            if (type == NULL)
            {
                // err: type is missing
                free_node(node);
                return NULL;
            }

            if (node->data.stmt_str.fields == NULL)
            {
                node->data.stmt_str.fields = (str_field *)malloc(sizeof(str_field));
            }
            else
            {
                node->data.stmt_str.fields = (str_field *)realloc(node->data.stmt_str.fields, (node->data.stmt_str.field_count + 1) * sizeof(str_field));
            }

            node->data.stmt_str.fields[node->data.stmt_str.field_count].name = name;
            node->data.stmt_str.fields[node->data.stmt_str.field_count].type = type;
            node->data.stmt_str.field_count++;
        }

        if (!parser_consume(parser, TOKEN_SEMICOLON, "expected ';'"))
        {
            // err: no semicolon after field
            free_node(node);
            return NULL;
        }
    }

    if (!parser_consume(parser, TOKEN_RIGHT_BRACE, "expected '}'"))
    {
        // err: no right brace after fields
        free_node(node);
        return NULL;
    }

    return node;
}

Node *parse_stmt_val(Parser *parser)
{
#ifdef PARSER_DEBUG_TRACE
    printf("parse_stmt_val\n");
#endif

    Node *node = new_node(NODE_STMT_VAL);

    if (!parser_consume(parser, TOKEN_VAL, "expected 'val'"))
    {
        // err: invalid token (probably the parser's fault)
        free_node(node);
        return NULL;
    }

    if (parser->current.type != TOKEN_IDENTIFIER)
    {
        // err: next token after `val` is not an identifier
        parser_error(parser, "expected an identifier");
        free_node(node);
        return NULL;
    }

    node->data.stmt_val.identifier = new_node(NODE_EXPR_IDENTIFIER);
    node->data.stmt_val.identifier->data.expr_identifier.name = parser->current.start;

    parser_advance(parser);

    if (!parser_consume(parser, TOKEN_COLON, "expected ':'"))
    {
        // err: no colon after val name
        free_node(node);
        return NULL;
    }

    node->data.stmt_val.type = parse_expr_type(parser);
    if (node->data.stmt_val.type == NULL)
    {
        // err: type is missing
        free_node(node);
        return NULL;
    }

    if (!parser_consume(parser, TOKEN_EQUAL, "expected '='"))
    {
        // err: no assignment operator after type
        free_node(node);
        return NULL;
    }

    node->data.stmt_val.initializer = parse_expr(parser);
    if (node->data.stmt_val.initializer == NULL)
    {
        // err: value is missing
        free_node(node);
        return NULL;
    }

    if (!parser_consume(parser, TOKEN_SEMICOLON, "expected ';'"))
    {
        // err: no semicolon after value
        free_node(node);
        return NULL;
    }

    return node;
}

Node *parse_stmt_var(Parser *parser)
{
#ifdef PARSER_DEBUG_TRACE
    printf("parse_stmt_var\n");
#endif

    Node *node = new_node(NODE_STMT_VAR);

    if (!parser_consume(parser, TOKEN_VAR, "expected 'var'"))
    {
        // err: invalid token (probably the parser's fault)
        free_node(node);
        return NULL;
    }

    if (parser->current.type != TOKEN_IDENTIFIER)
    {
        // err: next token after `var` is not an identifier
        parser_error(parser, "expected an identifier");
        free_node(node);
        return NULL;
    }

    node->data.stmt_var.identifier = new_node(NODE_EXPR_IDENTIFIER);
    node->data.stmt_var.identifier->data.expr_identifier.name = parser->current.start;

    parser_advance(parser);

    if (!parser_consume(parser, TOKEN_COLON, "expected ':'"))
    {
        // err: no colon after var name
        free_node(node);
        return NULL;
    }

    node->data.stmt_var.type = parse_expr_type(parser);
    if (node->data.stmt_var.type == NULL)
    {
        // err: type is missing
        free_node(node);
        return NULL;
    }

    // initializer is optional
    if (parser_match(parser, TOKEN_EQUAL))
    {
        node->data.stmt_var.initializer = parse_expr(parser);
        if (node->data.stmt_var.initializer == NULL)
        {
            // err: initializer is missing after assignment operator
            free_node(node);
            return NULL;
        }
    }

    if (!parser_consume(parser, TOKEN_SEMICOLON, "expected ';'"))
    {
        // err: no semicolon after type
        free_node(node);
        return NULL;
    }

    return node;
}

Node *parse_stmt_if(Parser *parser)
{
#ifdef PARSER_DEBUG_TRACE
    printf("parse_stmt_if\n");
#endif

    Node *node = new_node(NODE_STMT_IF);

    if (!parser_consume(parser, TOKEN_IF, "expected 'if'"))
    {
        // err: invalid token (probably the parser's fault)
        free_node(node);
        return NULL;
    }

    if (!parser_consume(parser, TOKEN_LEFT_PAREN, "expected '('"))
    {
        // err: no left parenthesis after `if`
        free_node(node);
        return NULL;
    }

    node->data.stmt_if.condition = parse_expr(parser);
    if (node->data.stmt_if.condition == NULL)
    {
        // err: condition is missing
        free_node(node);
        return NULL;
    }

    if (!parser_consume(parser, TOKEN_RIGHT_PAREN, "expected ')'"))
    {
        // err: no right parenthesis after condition
        free_node(node);
        return NULL;
    }

    node->data.stmt_if.body = parse_stmt(parser);
    if (node->data.stmt_if.body == NULL)
    {
        // err: body is missing
        free_node(node);
        return NULL;
    }

    return node;
}

Node *parse_stmt_or(Parser *parser)
{
#ifdef PARSER_DEBUG_TRACE
    printf("parse_stmt_or\n");
#endif

    Node *node = new_node(NODE_STMT_OR);

    if (!parser_consume(parser, TOKEN_OR, "expected 'or'"))
    {
        // err: invalid token (probably the parser's fault)
        free_node(node);
        return NULL;
    }

    if (parser_match(parser, TOKEN_LEFT_PAREN))
    {
        node->data.stmt_or.condition = parse_expr(parser);
        if (node->data.stmt_or.condition == NULL)
        {
            // err: condition is missing
            free_node(node);
            return NULL;
        }

        if (!parser_consume(parser, TOKEN_RIGHT_PAREN, "expected ')'"))
        {
            // err: no right parenthesis after condition
            free_node(node);
            return NULL;
        }
    }

    node->data.stmt_or.body = parse_stmt(parser);
    if (node->data.stmt_or.body == NULL)
    {
        // err: body is missing
        free_node(node);
        return NULL;
    }

    return node;
}

Node *parse_stmt_for(Parser *parser)
{
#ifdef PARSER_DEBUG_TRACE
    printf("parse_stmt_for\n");
#endif

    Node *node = new_node(NODE_STMT_FOR);

    if (!parser_consume(parser, TOKEN_FOR, "expected 'for'"))
    {
        // err: invalid token (probably the parser's fault)
        free_node(node);
        return NULL;
    }

    // condition is an optional singular expression -- no C style bs
    if (parser_match(parser, TOKEN_LEFT_PAREN))
    {
        node->data.stmt_for.condition = parse_expr(parser);
        if (node->data.stmt_for.condition == NULL)
        {
            // err: condition is missing
            free_node(node);
            return NULL;
        }

        if (!parser_consume(parser, TOKEN_RIGHT_PAREN, "expected ')'"))
        {
            // err: no right parenthesis after condition
            free_node(node);
            return NULL;
        }
    }

    node->data.stmt_for.body = parse_stmt(parser);
    if (node->data.stmt_for.body == NULL)
    {
        // err: body is missing
        free_node(node);
        return NULL;
    }

    return node;
}

Node *parse_stmt_brk(Parser *parser)
{
#ifdef PARSER_DEBUG_TRACE
    printf("parse_stmt_brk\n");
#endif

    Node *node = new_node(NODE_STMT_BRK);

    if (!parser_consume(parser, TOKEN_BRK, "expected 'brk'"))
    {
        // err: invalid token (probably the parser's fault)
        free_node(node);
        return NULL;
    }

    if (!parser_consume(parser, TOKEN_SEMICOLON, "expected ';'"))
    {
        // err: no semicolon after `brk`
        free_node(node);
        return NULL;
    }

    return node;
}

Node *parse_stmt_cnt(Parser *parser)
{
#ifdef PARSER_DEBUG_TRACE
    printf("parse_stmt_cnt\n");
#endif

    Node *node = new_node(NODE_STMT_CNT);

    if (!parser_consume(parser, TOKEN_CNT, "expected 'cnt'"))
    {
        // err: invalid token (probably the parser's fault)
        free_node(node);
        return NULL;
    }

    if (!parser_consume(parser, TOKEN_SEMICOLON, "expected ';'"))
    {
        // err: no semicolon after `cnt`
        free_node(node);
        return NULL;
    }

    return node;
}

Node *parse_stmt_ret(Parser *parser)
{
#ifdef PARSER_DEBUG_TRACE
    printf("parse_stmt_ret\n");
#endif

    Node *node = new_node(NODE_STMT_RET);

    if (!parser_consume(parser, TOKEN_RET, "expected 'ret'"))
    {
        // err: invalid token (probably the parser's fault)
        free_node(node);
        return NULL;
    }

    if (!parser_peek(parser, TOKEN_SEMICOLON))
    {
        node->data.stmt_ret.value = parse_expr(parser);
        if (node->data.stmt_ret.value == NULL)
        {
            // err: value is missing
            free_node(node);
            return NULL;
        }
    }

    if (!parser_consume(parser, TOKEN_SEMICOLON, "expected ';'"))
    {
        // err: no semicolon after value
        free_node(node);
        return NULL;
    }

    return node;
}

Node *parse_stmt_block(Parser *parser)
{
#ifdef PARSER_DEBUG_TRACE
    printf("parse_stmt_block\n");
#endif

    Node *node = new_node(NODE_STMT_BLOCK);

    if (!parser_consume(parser, TOKEN_LEFT_BRACE, "expected '{'"))
    {
        // err: invalid token (probably the parser's fault)
        free_node(node);
        return NULL;
    }

    while (parser->current.type != TOKEN_RIGHT_BRACE && parser->current.type != TOKEN_EOF)
    {
        Node *stmt = parse_stmt(parser);
        if (stmt == NULL)
        {
            // err: statement is missing
            free_node(node);
            return NULL;
        }

        Node *last = node->data.stmt_block.statements;
        if (last == NULL)
        {
            node->data.stmt_block.statements = stmt;
        }
        else
        {
            while (last->next != NULL)
            {
                last = last->next;
            }

            last->next = stmt;
        }

        node->data.stmt_block.statement_count++;
    }

    if (!parser_consume(parser, TOKEN_RIGHT_BRACE, "expected '}'"))
    {
        // err: no right brace after statements
        free_node(node);
        return NULL;
    }

    return node;
}

Node *parse_stmt_expr(Parser *parser)
{
#ifdef PARSER_DEBUG_TRACE
    printf("parse_stmt_expr\n");
#endif

    Node *node = new_node(NODE_STMT_EXPR);

    node->data.stmt_expr.expression = parse_expr(parser);
    if (node->data.stmt_expr.expression == NULL)
    {
        // err: expression is missing
        free_node(node);
        return NULL;
    }

    if (!parser_consume(parser, TOKEN_SEMICOLON, "expected ';'"))
    {
        // err: no semicolon after expression
        free_node(node);
        return NULL;
    }

    return node;
}

Node *parse_stmt(Parser *parser)
{
#ifdef PARSER_DEBUG_TRACE
    printf("parse_stmt\n");
#endif

    Node *node = NULL;

    switch (parser->current.type)
    {
    case TOKEN_USE:
        node = parse_stmt_use(parser);
        break;
    case TOKEN_FUN:
        node = parse_stmt_fun(parser);
        break;
    case TOKEN_STR:
        node = parse_stmt_str(parser);
        break;
    case TOKEN_VAL:
        node = parse_stmt_val(parser);
        break;
    case TOKEN_VAR:
        node = parse_stmt_var(parser);
        break;
    case TOKEN_IF:
        node = parse_stmt_if(parser);
        break;
    case TOKEN_OR:
        node = parse_stmt_or(parser);
        break;
    case TOKEN_FOR:
        node = parse_stmt_for(parser);
        break;
    case TOKEN_BRK:
        node = parse_stmt_brk(parser);
        break;
    case TOKEN_CNT:
        node = parse_stmt_cnt(parser);
        break;
    case TOKEN_RET:
        node = parse_stmt_ret(parser);
        break;
    case TOKEN_LEFT_BRACE:
        node = parse_stmt_block(parser);
        break;
    default:
        node = parse_stmt_expr(parser);
        break;
    }

#ifdef PARSER_DEBUG_TRACE
    printf("P SMT: %s\n\n", node_type_string(node->type));
#endif

    return node;
}

Node *parse(Parser *parser)
{
#ifdef PARSER_DEBUG_TRACE
    printf("parse\n");
#endif

    Node *node = new_node(NODE_PROGRAM);

    while (parser->current.type != TOKEN_EOF)
    {
        Node *stmt = parse_stmt(parser);
        if (stmt == NULL)
        {
            // err: statement is missing
            free_node(node);
            return NULL;
        }

        Node *last = node->data.base_program.statements;
        if (last == NULL)
        {
            node->data.base_program.statements = stmt;
        }
        else
        {
            while (last->next != NULL)
            {
                last = last->next;
            }

            last->next = stmt;
        }

        node->data.base_program.statement_count++;
    }

    return node;
}
