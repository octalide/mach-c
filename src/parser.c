#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "op.h"
#include "parser.h"

void parser_print_error(Parser *parser)
{
    if (!parser->error)
    {
        return;
    }

    int err_line_index = lexer_get_token_line_index(parser->lexer, &parser->error->token);
    int err_char_index = lexer_get_token_line_char_index(parser->lexer, &parser->error->token);

    printf("%5d | %s\n", err_line_index, lexer_get_line(parser->lexer, err_line_index));
    for (int i = 0; i < err_char_index + 7; i++)
    {
        printf("-");
    }
    printf("^\n");
}

void parser_init(Parser *parser, Lexer *lexer)
{
    parser->lexer = lexer;

    // initialize all tokens as empty tokens
    parser->last = (Token){TOKEN_ERROR, NULL, 0};
    parser->curr = (Token){TOKEN_ERROR, NULL, 0};
    parser->next = (Token){TOKEN_ERROR, NULL, 0};

    parser->error = NULL;
}

void parser_free(Parser *parser)
{
    lexer_free(parser->lexer);

    free(parser->lexer);
    free(parser->error);
}

void parser_error(Parser *parser, Token token, const char *message)
{
    if (parser->error)
    {
        free(parser->error);
    }

    parser->error = malloc(sizeof(ParseError));
    parser->error->message = message;
    parser->error->token = token;
}

void parser_advance(Parser *parser)
{
    Token next = next_token(parser->lexer);

    parser->last = parser->curr;
    parser->curr = parser->next;
    parser->next = next;

    if (parser_match_next(parser, TOKEN_ERROR))
    {
        parser_error(parser, parser->curr, "unexpected token");
    }
}

bool parser_match_last(Parser *parser, TokenType type)
{
    return parser->last.type == type;
}

bool parser_match(Parser *parser, TokenType type)
{
    return parser->curr.type == type;
}

bool parser_match_next(Parser *parser, TokenType type)
{
    return parser->next.type == type;
}

bool parser_expect_last(Parser *parser, TokenType type, const char *message)
{
    if (!parser_match_last(parser, type))
    {
        parser_error(parser, parser->last, message);
        return false;
    }

    return true;
}

bool parser_expect(Parser *parser, TokenType type, const char *message)
{
    if (!parser_match(parser, type))
    {
        parser_error(parser, parser->curr, message);
        return false;
    }

    return true;
}

bool parser_expect_next(Parser *parser, TokenType type, const char *message)
{
    if (!parser_match_next(parser, type))
    {
        parser_error(parser, parser->next, message);
        return false;
    }

    return true;
}

bool parser_skip(Parser *parser, TokenType type)
{
    if (parser_match(parser, type))
    {
        parser_advance(parser);
        return true;
    }

    return false;
}

bool parser_consume(Parser *parser, TokenType type)
{
    if (parser_match(parser, type))
    {
        parser_advance(parser);
        return true;
    }

    return false;
}

bool parser_consume_required(Parser *parser, TokenType type, const char *message)
{
    if (parser_consume(parser, type))
    {
        return true;
    }

    parser_error(parser, parser->curr, message);
    return false;
}

// { ... }
Node *parse_block(Parser *parser)
{
    if (!parser_consume_required(parser, TOKEN_LEFT_BRACE, "expected '{'"))
    {
        return NULL;
    }

    Node *node = node_new(NODE_BLOCK);

    while (!parser_match(parser, TOKEN_RIGHT_BRACE) && !parser_match(parser, TOKEN_EOF))
    {
        Node *stmt = parse_stmt(parser);
        if (!stmt)
        {
            node_free(node);
            return NULL;
        }

        node_list_append(&node->data.node_block.statements, stmt);
    }

    if (!parser_consume_required(parser, TOKEN_RIGHT_BRACE, "expected '}'"))
    {
        node_free(node);
        return NULL;
    }

    return node;
}

// foo
// foo.bar
// ...
Node *parse_identifier(Parser *parser)
{
    if (!parser_consume_required(parser, TOKEN_IDENTIFIER, "expected identifier"))
    {
        return NULL;
    }

    Node *node = node_new(NODE_IDENTIFIER);
    node->data.node_identifier.value = parser->last;

    if (parser_consume(parser, TOKEN_DOT))
    {
        Node *member = parse_identifier(parser);
        if (!member)
        {
            node_free(node);
            return NULL;
        }

        node->data.node_identifier.member = member;
    }

    return node;
}

Node *parse_expr_identifier(Parser *parser)
{
    switch (keyword_token_ident_type(parser->curr))
    {
    case KW_STR:
        if (!parser_consume_required(parser, TOKEN_IDENTIFIER, "expected identifier"))
        {
            return NULL;
        }

        return parse_expr_def_str(parser);
    case KW_UNI:
        if (!parser_consume_required(parser, TOKEN_IDENTIFIER, "expected identifier"))
        {
            return NULL;
        }

        return parse_expr_def_uni(parser);
    default:
        break;
    }

    Node *identifier = parse_identifier(parser);
    if (!identifier)
    {
        return NULL;
    }

    if (parser_match(parser, TOKEN_LEFT_PAREN))
    {
        Node *node = parse_expr_call(parser);
        if (!node)
        {
            return NULL;
        }

        node->data.expr_call.target = identifier;

        return node;
    }

    if (parser_match(parser, TOKEN_LEFT_BRACKET))
    {
        Node *node = parse_expr_index(parser);
        if (!node)
        {
            return NULL;
        }

        node->data.expr_index.target = identifier;

        return node;
    }

    Node *node = node_new(NODE_EXPR_IDENTIFIER);
    node->data.expr_identifier.identifier = identifier;

    return node;
}

// 'a'
Node *parse_expr_lit_char(Parser *parser)
{
    if (!parser_consume_required(parser, TOKEN_CHARACTER, "expected character literal"))
    {
        return NULL;
    }

    Node *node = node_new(NODE_EXPR_LIT_CHAR);
    node->data.expr_lit_char.value = parser->last;

    return node;
}

// 1
// 1.1
// 0x1
// ...
Node *parse_expr_lit_number(Parser *parser)
{
    if (!parser_consume_required(parser, TOKEN_NUMBER, "expected number literal"))
    {
        return NULL;
    }

    Node *node = node_new(NODE_EXPR_LIT_NUMBER);
    node->data.expr_lit_number.value = parser->last;

    return node;
}

// "foo"
Node *parse_expr_lit_string(Parser *parser)
{
    if (!parser_consume_required(parser, TOKEN_STRING, "expected string literal"))
    {
        return NULL;
    }

    Node *node = node_new(NODE_EXPR_LIT_STRING);
    node->data.expr_lit_string.value = parser->last;

    return node;
}

// (...)
Node *parse_expr_call(Parser *parser)
{
    if (!parser_consume_required(parser, TOKEN_LEFT_PAREN, "expected '('"))
    {
        return NULL;
    }

    Node *node = node_new(NODE_EXPR_CALL);

    while (!parser_match(parser, TOKEN_RIGHT_PAREN) && !parser_match(parser, TOKEN_EOF))
    {
        Node *argument = parse_expr(parser);
        if (!argument)
        {
            node_free(node);
            return NULL;
        }

        node_list_append(&node->data.expr_call.arguments, argument);

        parser_consume(parser, TOKEN_COMMA);
    }

    if (!parser_consume_required(parser, TOKEN_RIGHT_PAREN, "expected ')'"))
    {
        node_free(node);
        return NULL;
    }

    return node;
}

// [...]
Node *parse_expr_index(Parser *parser)
{
    if (!parser_consume_required(parser, TOKEN_LEFT_BRACKET, "expected '['"))
    {
        return NULL;
    }

    Node *node = node_new(NODE_EXPR_INDEX);

    Node *index = parse_expr(parser);
    if (!index)
    {
        node_free(node);
        return NULL;
    }

    node->data.expr_index.index = index;

    if (!parser_consume_required(parser, TOKEN_RIGHT_BRACKET, "expected ']'"))
    {
        node_free(node);
        return NULL;
    }

    return node;
}

// {
//    foo = bar;
//    ...
// }
Node *parse_expr_def_str(Parser *parser)
{
    if (!parser_consume_required(parser, TOKEN_LEFT_BRACE, "expected '{'"))
    {
        return NULL;
    }

    Node *node = node_new(NODE_EXPR_DEF_STR);

    while (!parser_match(parser, TOKEN_RIGHT_BRACE) && !parser_match(parser, TOKEN_EOF))
    {
        // NOTE: this is expected to be a binary expr with an assignment op
        Node *expr = parse_expr(parser);
        if (!expr)
        {
            node_free(node);
            return NULL;
        }

        node_list_append(&node->data.expr_def_str.initializers, expr);

        if (!parser_consume_required(parser, TOKEN_SEMICOLON, "expected ';'"))
        {
            node_free(node);
            return NULL;
        }
    }

    if (!parser_consume_required(parser, TOKEN_RIGHT_BRACE, "expected '}'"))
    {
        node_free(node);
        return NULL;
    }

    return node;
}

// {
//    foo = bar;
// }
Node *parse_expr_def_uni(Parser *parser)
{
    if (!parser_consume_required(parser, TOKEN_LEFT_BRACE, "expected '{'"))
    {
        return NULL;
    }

    Node *node = node_new(NODE_EXPR_DEF_UNI);

    // NOTE: expect exactly one initializer expression (same syntax as `str`)
    if (!parser_match(parser, TOKEN_RIGHT_BRACE))
    {
        Node *initializer = parse_expr(parser);
        if (!initializer)
        {
            node_free(node);
            return NULL;
        }

        node->data.expr_def_uni.initializer = initializer;

        if (!parser_consume_required(parser, TOKEN_SEMICOLON, "expected ';'"))
        {
            node_free(node);
            return NULL;
        }
    }

    if (!parser_consume_required(parser, TOKEN_RIGHT_BRACE, "expected '}'"))
    {
        node_free(node);
        return NULL;
    }

    return node;
}

// [ ... ]foo{ ... }
Node *parse_expr_def_array(Parser *parser)
{
    if (!parser_consume_required(parser, TOKEN_LEFT_BRACKET, "expected '['"))
    {
        return NULL;
    }

    Node *node = node_new(NODE_EXPR_DEF_ARRAY);

    if (!parser_match(parser, TOKEN_RIGHT_BRACKET))
    {
        Node *size = parse_expr(parser);
        if (!size)
        {
            node_free(node);
            return NULL;
        }

        node->data.expr_def_array.size = size;
    }

    if (!parser_consume_required(parser, TOKEN_RIGHT_BRACKET, "expected ']'"))
    {
        node_free(node);
        return NULL;
    }

    Node *type = parse_type(parser);
    if (!type)
    {
        node_free(node);
        return NULL;
    }

    node->data.expr_def_array.type = type;

    if (!parser_consume_required(parser, TOKEN_LEFT_BRACE, "expected '{'"))
    {
        node_free(node);
        return NULL;
    }

    if (parser_consume(parser, TOKEN_RIGHT_BRACE))
    {
        return node;
    }

    while (!parser_match(parser, TOKEN_RIGHT_BRACE) && !parser_match(parser, TOKEN_EOF))
    {
        Node *element = parse_expr(parser);
        if (!element)
        {
            node_free(node);
            return NULL;
        }

        node_list_append(&node->data.expr_def_array.elements, element);

        parser_consume(parser, TOKEN_COMMA);
    }

    if (!parser_consume_required(parser, TOKEN_RIGHT_BRACE, "expected '}'"))
    {
        node_free(node);
        return NULL;
    }

    return node;
}

Node *parse_postfix(Parser *parser)
{
    Node *node = parse_expr_primary(parser);
    if (!node)
    {
        return NULL;
    }

    if (parser_match(parser, TOKEN_DOT))
    {
        if (!parser_consume_required(parser, TOKEN_DOT, "expected '.'"))
        {
            node_free(node);
            return NULL;
        }

        Node *member = parse_identifier(parser);
        if (!member)
        {
            node_free(node);
            return NULL;
        }

        Node *node = node_new(NODE_EXPR_MEMBER);
        node->data.expr_member.target = node;
        node->data.expr_member.member = member;

        return node;
    }

    if (parser_match(parser, TOKEN_LEFT_PAREN))
    {
        Node *call = parse_expr_call(parser);
        if (!call)
        {
            node_free(node);
            return NULL;
        }

        call->data.expr_call.target = node;

        return call;
    }

    if (parser_match(parser, TOKEN_LEFT_BRACKET))
    {
        Node *index = parse_expr_index(parser);
        if (!index)
        {
            node_free(node);
            return NULL;
        }

        index->data.expr_index.target = node;

        return index;
    }

    return node;
}

// -1
// +1
// !1
// ...
Node *parse_expr_unary(Parser *parser)
{
    if (op_is_unary(parser->curr.type))
    {
        Token op = parser->curr;
        parser_advance(parser);

        Node *rhs = parse_expr_unary(parser);
        if (!rhs)
        {
            return NULL;
        }

        Node *node = node_new(NODE_EXPR_UNARY);
        node->data.expr_unary.op = op.type;
        node->data.expr_unary.rhs = rhs;

        return node;
    }

    return parse_postfix(parser);
}

// 1 + 2
// 1 - 2
// foo = bar
// ...
Node *parse_expr_binary(Parser *parser, int parent_precedence)
{
    Node *lhs = parse_expr_unary(parser);
    if (!lhs)
    {
        return NULL;
    }

    while (op_is_binary(parser->curr.type) && op_get_precedence(parser->curr.type) >= parent_precedence)
    {
        Token op = parser->curr;
        parser_advance(parser);

        int precedence = op_get_precedence(op.type);
        if (op_is_right_associative(op.type))
        {
            precedence++;
        }

        Node *rhs = parse_expr_binary(parser, precedence);
        if (!rhs)
        {
            node_free(lhs);
            return NULL;
        }

        Node *node = node_new(NODE_EXPR_BINARY);
        node->data.expr_binary.op = op.type;
        node->data.expr_binary.lhs = lhs;
        node->data.expr_binary.rhs = rhs;

        lhs = node;
    }

    return lhs;
}

// (...)
Node *parse_expr_grouping(Parser *parser)
{
    if (!parser_consume_required(parser, TOKEN_LEFT_PAREN, "expected '('"))
    {
        return NULL;
    }

    Node *node = parse_expr(parser);
    if (!node)
    {
        return NULL;
    }

    if (!parser_consume_required(parser, TOKEN_RIGHT_PAREN, "expected ')'"))
    {
        return NULL;
    }

    return node;
}

Node *parse_expr_primary(Parser *parser)
{
    if (parser_match(parser, TOKEN_IDENTIFIER))
    {
        return parse_expr_identifier(parser);
    }

    if (parser_match(parser, TOKEN_CHARACTER))
    {
        return parse_expr_lit_char(parser);
    }

    if (parser_match(parser, TOKEN_NUMBER))
    {
        return parse_expr_lit_number(parser);
    }

    if (parser_match(parser, TOKEN_STRING))
    {
        return parse_expr_lit_string(parser);
    }

    if (parser_match(parser, TOKEN_LEFT_PAREN))
    {
        return parse_expr_grouping(parser);
    }

    if (parser_match(parser, TOKEN_LEFT_BRACKET))
    {
        return parse_expr_def_array(parser);
    }

    return NULL;
}

Node *parse_expr(Parser *parser)
{
    return parse_expr_binary(parser, 0);
}

// [ ... ]foo
Node *parse_type_array(Parser *parser)
{
    if (!parser_consume_required(parser, TOKEN_LEFT_BRACKET, "expected '['"))
    {
        return NULL;
    }

    Node *node = node_new(NODE_TYPE_ARRAY);

    if (!parser_match(parser, TOKEN_RIGHT_BRACKET))
    {
        Node *size = parse_expr(parser);
        if (!size)
        {
            return NULL;
        }

        node->data.type_array.size = size;
    }

    if (!parser_consume_required(parser, TOKEN_RIGHT_BRACKET, "expected ']'"))
    {
        return NULL;
    }

    Node *type = parse_type(parser);
    if (!type)
    {
        return NULL;
    }

    node->data.type_array.type = type;

    return node;
}

// #foo
Node *parse_type_ref(Parser *parser)
{
    if (!parser_consume_required(parser, TOKEN_HASH, "expected '#'"))
    {
        return NULL;
    }

    Node *type = parse_type(parser);
    if (!type)
    {
        return NULL;
    }

    Node *node = node_new(NODE_TYPE_REF);
    node->data.type_ref.target = type;

    return node;
}

// ( ... ): foo
Node *parse_type_fun(Parser *parser)
{
    if (!parser_consume_required(parser, TOKEN_LEFT_PAREN, "expected '('"))
    {
        return NULL;
    }

    Node *node = node_new(NODE_TYPE_FUN);

    while (!parser_match(parser, TOKEN_RIGHT_PAREN) && !parser_match(parser, TOKEN_EOF))
    {
        Node *parameter_type = parse_type(parser);
        if (!parameter_type)
        {
            return NULL;
        }

        node_list_append(&node->data.type_fun.parameter_types, parameter_type);

        parser_consume(parser, TOKEN_COMMA);
    }

    if (!parser_consume_required(parser, TOKEN_RIGHT_PAREN, "expected ')'"))
    {
        return NULL;
    }

    if (!parser_consume_required(parser, TOKEN_COLON, "expected ':'"))
    {
        return NULL;
    }

    Node *ret = parse_type(parser);
    if (!ret)
    {
        return NULL;
    }

    node->data.type_fun.return_type = ret;

    return node;
}

// { ... }
Node *parse_type_str(Parser *parser)
{
    if (!parser_consume_required(parser, TOKEN_LEFT_BRACE, "expected '{'"))
    {
        return NULL;
    }

    Node *node = node_new(NODE_TYPE_STR);

    while (!parser_match(parser, TOKEN_RIGHT_BRACE) && !parser_match(parser, TOKEN_EOF))
    {
        Node *field = parse_field(parser);
        if (!field)
        {
            node_free(node);
            return NULL;
        }

        node_list_append(&node->data.type_str.fields, field);

        if (!parser_consume_required(parser, TOKEN_SEMICOLON, "expected ';'"))
        {
            node_free(node);
            return NULL;
        }
    }

    if (!parser_consume_required(parser, TOKEN_RIGHT_BRACE, "expected '}'"))
    {
        node_free(node);
        return NULL;
    }

    return node;
}

// { ... }
Node *parse_type_uni(Parser *parser)
{
    if (!parser_consume_required(parser, TOKEN_LEFT_BRACE, "expected '{'"))
    {
        return NULL;
    }

    Node *node = node_new(NODE_TYPE_UNI);

    while (!parser_match(parser, TOKEN_RIGHT_BRACE) && !parser_match(parser, TOKEN_EOF))
    {
        Node *field = parse_field(parser);
        if (!field)
        {
            node_free(node);
            return NULL;
        }

        node_list_append(&node->data.type_uni.fields, field);

        if (!parser_consume_required(parser, TOKEN_SEMICOLON, "expected ';'"))
        {
            node_free(node);
            return NULL;
        }
    }

    if (!parser_consume_required(parser, TOKEN_RIGHT_BRACE, "expected '}'"))
    {
        node_free(node);
        return NULL;
    }

    return node;
}

// foo: bar
Node *parse_field(Parser *parser)
{
    Node *identifier = parse_identifier(parser);
    if (!identifier)
    {
        return NULL;
    }

    Node *node = node_new(NODE_FIELD);
    node->data.node_field.identifier = identifier;

    if (!parser_consume_required(parser, TOKEN_COLON, "expected ':'"))
    {
        node_free(node);
        return NULL;
    }

    Node *type = parse_type(parser);
    if (!type)
    {
        node_free(node);
        return NULL;
    }

    node->data.node_field.type = type;

    return node;
}

Node *parse_type(Parser *parser)
{
    if (parser_match(parser, TOKEN_LEFT_BRACKET))
    {
        return parse_type_array(parser);
    }

    if (parser_match(parser, TOKEN_HASH))
    {
        return parse_type_ref(parser);
    }

    if (parser_match(parser, TOKEN_IDENTIFIER))
    {
        switch (keyword_token_ident_type(parser->curr))
        {
        case KW_FUN:
            if (!parser_consume_required(parser, TOKEN_IDENTIFIER, "expected identifier"))
            {
                return NULL;
            }

            return parse_type_fun(parser);
        case KW_STR:
            if (!parser_consume_required(parser, TOKEN_IDENTIFIER, "expected identifier"))
            {
                return NULL;
            }

            return parse_type_str(parser);
        case KW_UNI:
            if (!parser_consume_required(parser, TOKEN_IDENTIFIER, "expected identifier"))
            {
                return NULL;
            }

            return parse_type_uni(parser);
        default:
            return parse_identifier(parser);
        }
    }

    return NULL;
}

// "foo";
// foo "foo";
// ...
Node *parse_stmt_use(Parser *parser)
{
    Node *identifier = parse_identifier(parser);

    Node *path = parse_expr_lit_string(parser);
    if (!path)
    {
        return NULL;
    }

    Node *node = node_new(NODE_STMT_USE);
    node->data.stmt_use.identifier = identifier;
    node->data.stmt_use.path = path;

    return node;
}

// (...) { ... }
Node *parse_stmt_if(Parser *parser)
{
    if (!parser_consume_required(parser, TOKEN_LEFT_PAREN, "expected '('"))
    {
        return NULL;
    }

    Node *condition = parse_expr(parser);
    if (!condition)
    {
        return NULL;
    }

    if (!parser_consume_required(parser, TOKEN_RIGHT_PAREN, "expected ')'"))
    {
        return NULL;
    }

    Node *body = parse_block(parser);
    if (!body)
    {
        return NULL;
    }

    Node *node = node_new(NODE_STMT_IF);
    node->data.stmt_if.condition = condition;
    node->data.stmt_if.body = body;

    return node;
}

// (...) { ... }
Node *parse_stmt_or(Parser *parser)
{
    Node *condition = NULL;
    if (parser_consume(parser, TOKEN_LEFT_PAREN))
    {
        condition = parse_expr(parser);
        if (!condition)
        {
            return NULL;
        }

        if (!parser_consume_required(parser, TOKEN_RIGHT_PAREN, "expected ')'"))
        {
            node_free(condition);
            return NULL;
        }
    }

    Node *body = parse_block(parser);
    if (!body)
    {
        node_free(condition);
        return NULL;
    }

    Node *node = node_new(NODE_STMT_OR);
    node->data.stmt_or.condition = condition;
    node->data.stmt_or.body = body;

    return node;
}

// (...) { ... }
Node *parse_stmt_for(Parser *parser)
{
    Node *condition = NULL;
    if (parser_consume(parser, TOKEN_LEFT_PAREN))
    {
        condition = parse_expr(parser);
        if (!condition)
        {
            return NULL;
        }

        if (!parser_consume_required(parser, TOKEN_RIGHT_PAREN, "expected ')'"))
        {
            node_free(condition);
            return NULL;
        }
    }

    Node *body = parse_block(parser);
    if (!body)
    {
        return NULL;
    }

    Node *node = node_new(NODE_STMT_FOR);
    node->data.stmt_for.condition = condition;
    node->data.stmt_for.body = body;

    return node;
}

// brk
Node *parse_stmt_brk(Parser *parser)
{
    // avoid unused parameter compiler warning
    (void)parser;

    Node *node = node_new(NODE_STMT_BRK);
    return node;
}

// cnt
Node *parse_stmt_cnt(Parser *parser)
{
    // avoid unused parameter compiler warning
    (void)parser;

    Node *node = node_new(NODE_STMT_CNT);
    return node;
}

// ret foo
Node *parse_stmt_ret(Parser *parser)
{
    Node *node = node_new(NODE_STMT_RET);

    if (parser_consume(parser, TOKEN_SEMICOLON))
    {
        return node;
    }

    Node *value = parse_expr(parser);
    if (!value)
    {
        node_free(node);
        return NULL;
    }

    node->data.stmt_ret.value = value;

    return node;
}

// def foo: bar
Node *parse_stmt_decl_type(Parser *parser)
{
    Node *identifier = parse_identifier(parser);
    if (!identifier)
    {
        return NULL;
    }

    if (!parser_consume_required(parser, TOKEN_COLON, "expected ':'"))
    {
        node_free(identifier);
        return NULL;
    }

    Node *type = parse_type(parser);
    if (!type)
    {
        node_free(identifier);
        return NULL;
    }

    Node *node = node_new(NODE_STMT_DECL_TYPE);
    node->data.stmt_decl_type.identifier = identifier;
    node->data.stmt_decl_type.type = type;

    return node;
}

// val foo: bar = baz
Node *parse_stmt_decl_val(Parser *parser)
{
    Node *identifier = parse_identifier(parser);
    if (!identifier)
    {
        return NULL;
    }

    if (!parser_consume_required(parser, TOKEN_COLON, "expected ':'"))
    {
        node_free(identifier);
        return NULL;
    }

    Node *type = parse_type(parser);
    if (!type)
    {
        node_free(identifier);
        return NULL;
    }

    if (!parser_consume_required(parser, TOKEN_EQUAL, "expected '='"))
    {
        node_free(identifier);
        return NULL;
    }

    Node *initializer = parse_expr(parser);
    if (!initializer)
    {
        node_free(identifier);
        return NULL;
    }

    Node *node = node_new(NODE_STMT_DECL_VAL);
    node->data.stmt_decl_val.identifier = identifier;
    node->data.stmt_decl_val.type = type;
    node->data.stmt_decl_val.initializer = initializer;

    return node;
}

// var foo: bar = baz
Node *parse_stmt_decl_var(Parser *parser)
{
    Node *identifier = parse_identifier(parser);
    if (!identifier)
    {
        return NULL;
    }

    if (!parser_consume_required(parser, TOKEN_COLON, "expected ':'"))
    {
        node_free(identifier);
        return NULL;
    }

    Node *type = parse_type(parser);
    if (!type)
    {
        node_free(identifier);
        return NULL;
    }

    Node *initializer = NULL;
    if (parser_consume(parser, TOKEN_EQUAL))
    {
        initializer = parse_expr(parser);
        if (!initializer)
        {
            node_free(identifier);
            return NULL;
        }
    }

    Node *node = node_new(NODE_STMT_DECL_VAR);
    node->data.stmt_decl_var.identifier = identifier;
    node->data.stmt_decl_var.type = type;
    node->data.stmt_decl_var.initializer = initializer;

    return node;
}

// foo(...): bar { ... }
Node *parse_stmt_decl_fun(Parser *parser)
{
    Node *identifier = parse_identifier(parser);
    if (!identifier)
    {
        return NULL;
    }

    if (!parser_consume_required(parser, TOKEN_LEFT_PAREN, "expected '('"))
    {
        node_free(identifier);
        return NULL;
    }

    Node *node = node_new(NODE_STMT_DECL_FUN);

    while (!parser_match(parser, TOKEN_RIGHT_PAREN) && !parser_match(parser, TOKEN_EOF))
    {
        Node *parameter = parse_field(parser);
        if (!parameter)
        {
            node_free(identifier);
            return NULL;
        }

        node_list_append(&node->data.stmt_decl_fun.parameters, parameter);

        parser_consume(parser, TOKEN_COMMA);
    }

    if (!parser_consume_required(parser, TOKEN_RIGHT_PAREN, "expected ')'"))
    {
        node_free(identifier);
        return NULL;
    }

    if (!parser_consume_required(parser, TOKEN_COLON, "expected ':'"))
    {
        node_free(identifier);
        return NULL;
    }

    node->data.stmt_decl_fun.identifier = identifier;

    Node *return_type = parse_type(parser);
    if (!return_type)
    {
        node_free(identifier);
        return NULL;
    }

    node->data.stmt_decl_fun.return_type = return_type;

    Node *body = parse_block(parser);
    if (!body)
    {
        node_free(identifier);
        return NULL;
    }

    node->data.stmt_decl_fun.body = body;

    return node;
}

// foo {
//   bar: baz;
//   ...
// }
Node *parse_stmt_decl_str(Parser *parser)
{
    Node *identifier = parse_identifier(parser);
    if (!identifier)
    {
        return NULL;
    }

    Node *node = node_new(NODE_STMT_DECL_STR);
    node->data.stmt_decl_str.identifier = identifier;

    if (!parser_consume_required(parser, TOKEN_LEFT_BRACE, "expected '{'"))
    {
        node_free(node);
        return NULL;
    }

    while (!parser_match(parser, TOKEN_RIGHT_BRACE) && !parser_match(parser, TOKEN_EOF))
    {
        Node *field = parse_field(parser);
        if (!field)
        {
            node_free(node);
            return NULL;
        }

        node_list_append(&node->data.stmt_decl_str.fields, field);

        if (!parser_consume_required(parser, TOKEN_SEMICOLON, "expected ';'"))
        {
            node_free(node);
            return NULL;
        }
    }

    if (!parser_consume_required(parser, TOKEN_RIGHT_BRACE, "expected '}'"))
    {
        node_free(node);
        return NULL;
    }

    return node;
}

// foo {
//   bar: baz;
//   ...
// }
Node *parse_stmt_decl_uni(Parser *parser)
{
    Node *identifier = parse_identifier(parser);
    if (!identifier)
    {
        return NULL;
    }

    Node *node = node_new(NODE_STMT_DECL_UNI);
    node->data.stmt_decl_uni.identifier = identifier;

    if (!parser_consume_required(parser, TOKEN_LEFT_BRACE, "expected '{'"))
    {
        node_free(node);
        return NULL;
    }

    while (!parser_match(parser, TOKEN_RIGHT_BRACE) && !parser_match(parser, TOKEN_EOF))
    {
        Node *field = parse_field(parser);
        if (!field)
        {
            node_free(node);
            return NULL;
        }

        node_list_append(&node->data.stmt_decl_uni.fields, field);

        if (!parser_consume_required(parser, TOKEN_SEMICOLON, "expected ';'"))
        {
            node_free(node);
            return NULL;
        }
    }

    if (!parser_consume_required(parser, TOKEN_RIGHT_BRACE, "expected '}'"))
    {
        node_free(node);
        return NULL;
    }

    return node;
}

// foo;
// foo();
// 1 + 1;
// ...
Node *parse_stmt_expr(Parser *parser)
{
    Node *node = parse_expr(parser);
    if (!node)
    {
        return NULL;
    }

    return node;
}

Node *parse_stmt(Parser *parser)
{
    KeywordType keyword = keyword_token_ident_type(parser->curr);
    Node *(*parse_function)(Parser *);

    bool semicolon_terminated = true;

    switch (keyword)
    {
    case KW_USE:
        parse_function = parse_stmt_use;
        semicolon_terminated = true;
        break;
    case KW_IF:
        parse_function = parse_stmt_if;
        semicolon_terminated = false;
        break;
    case KW_OR:
        parse_function = parse_stmt_or;
        semicolon_terminated = false;
        break;
    case KW_FOR:
        parse_function = parse_stmt_for;
        semicolon_terminated = false;
        break;
    case KW_BRK:
        parse_function = parse_stmt_brk;
        semicolon_terminated = true;
        break;
    case KW_CNT:
        parse_function = parse_stmt_cnt;
        semicolon_terminated = true;
        break;
    case KW_RET:
        parse_function = parse_stmt_ret;
        semicolon_terminated = true;
        break;
    case KW_DEF:
        parse_function = parse_stmt_decl_type;
        semicolon_terminated = true;
        break;
    case KW_VAL:
        parse_function = parse_stmt_decl_val;
        semicolon_terminated = true;
        break;
    case KW_VAR:
        parse_function = parse_stmt_decl_var;
        semicolon_terminated = true;
        break;
    case KW_FUN:
        parse_function = parse_stmt_decl_fun;
        semicolon_terminated = false;
        break;
    case KW_STR:
        parse_function = parse_stmt_decl_str;
        semicolon_terminated = false;
        break;
    case KW_UNI:
        parse_function = parse_stmt_decl_uni;
        semicolon_terminated = false;
        break;
    default:
        parse_function = parse_stmt_expr;
        semicolon_terminated = true;
        break;
    }

    // consume keyword if it was matched
    // NOTE: none of the above statement functions prepended with a keyword
    //   expect to parse their own keyword.
    if (keyword != KW_UNK)
    {
        if (!parser_consume_required(parser, TOKEN_IDENTIFIER, "expected identifier"))
        {
            return NULL;
        }
    }

    Node *node = parse_function(parser);
    if (!node)
    {
        return NULL;
    }

    if (semicolon_terminated && !parser_consume_required(parser, TOKEN_SEMICOLON, "expected ';'"))
    {
        node_free(node);
        return NULL;
    }

#ifdef DEBUG_PARSER_PRINT
    printf("%-3d | STMT: %s\n", parser->lexer->line, node_type_string(node->type));
#endif

    return node;
}

Node *parse(Parser *parser)
{
    // prime the parser (assume just initialized)
    parser_advance(parser);
    parser_advance(parser);

    Node *node = node_new(NODE_BLOCK);

    while (!parser_match(parser, TOKEN_EOF))
    {
        Node *stmt = parse_stmt(parser);
        if (!stmt)
        {
            node_free(node);
            return NULL;
        }

        node_list_append(&node->data.node_block.statements, stmt);
    }

    return node;
}
