#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "op.h"
#include "parser.h"

Parser *parser_new(char *source)
{
    Parser *parser = calloc(1, sizeof(Parser));
    parser->lexer = lexer_new(source);
    parser->last = NULL;
    parser->curr = NULL;
    parser->next = NULL;
    parser->comments = calloc(1, sizeof(Token *));
    parser->comments[0] = NULL;

    return parser;
}

void parser_free(Parser *parser)
{
    if (parser == NULL)
    {
        return;
    }

    lexer_free(parser->lexer);
    parser->lexer = NULL;

    parser->last = NULL;
    parser->curr = NULL;
    parser->next = NULL;

    for (int i = 0; parser->comments[i] != NULL; i++)
    {
        token_free(parser->comments[i]);
        parser->comments[i] = NULL;
    }

    free(parser->comments);
    parser->comments = NULL;

    free(parser);
}

void parser_print_error(Parser *parser, Node *node, char *path)
{
    if (node->kind != NODE_ERROR)
    {
        return;
    }

    int offset = lexer_token_line_char_offset(parser->lexer, node->token);
    int line_num = lexer_token_line(parser->lexer, node->token);
    char *line = lexer_get_line(parser->lexer, line_num);
    if (line == NULL)
    {
        line = strdup("FAILED TO GET LINE TEXT");
    }

    printf("error: %s\n", node->error.message);
    printf("%s:%d:%d:\n", path, line_num, offset);
    printf("%4d | %s\n", line_num, line);
    printf("     | %*s^\n", offset, "");

    free(line);
}

void parser_add_comment(Parser *parser, Token *token)
{
    int i = 0;
    while (parser->comments[i] != NULL)
    {
        i++;
    }

    parser->comments = realloc(parser->comments, sizeof(Token *) * (i + 2));
    parser->comments[i] = token;
    parser->comments[i + 1] = NULL;
}

bool parser_has_comments(Parser *parser)
{
    return parser->comments[0] != NULL;
}

void parser_advance(Parser *parser)
{
    Token *next = lexer_next(parser->lexer);

    parser->last = parser->curr;
    parser->curr = parser->next;
    parser->next = next;

    if (parser->opt_verbose)
    {
        printf("[ PRSE ] [ TOKN ] %-20s `%s`\n", token_kind_string(parser->next->kind), parser->next->raw);
    }

    if (parser->curr == NULL)
    {
        parser_advance(parser);
    }

    if (parser->curr->kind == TOKEN_COMMENT)
    {
        parser_add_comment(parser, parser->curr);
        parser_advance(parser);
    }
}

bool parser_match(Parser *parser, TokenKind kind)
{
    return parser->curr->kind == kind;
}

bool parser_consume(Parser *parser, TokenKind kind)
{
    if (parser_match(parser, kind))
    {
        parser_advance(parser);
        return true;
    }

    return false;
}

// Utility function for creating an error node.
//
// The parser itself does not keep track of error nodes directly. They are
//   inserted into the tree by the function that creates the error and must be
//   found by crawling the tree. This is to preserve the partially parsed state
//   of a tree with errors as well as allowing for multiple errors to be
//   collected in one pass.
Node *parser_error(Token *token, char *message)
{
    Node *node = node_new(NODE_ERROR, token);
    node->error.message = strdup(message);

    return node;
}

Node *parse_program(Parser *parser)
{
    if (parser->opt_verbose)
    {
        printf("[ PRSE ] parse_program\n");
    }

    Node *node = node_new(NODE_PROGRAM, NULL);

    while (!parser_match(parser, TOKEN_EOF))
    {
        Node *stmt = parse_stmt(parser, node);
        node->program.statements = node_list_add(node->program.statements, stmt);
    }

    return node;
}

Node *parse_identifier(Parser *parser, Node *parent)
{
    if (parser->opt_verbose)
    {
        printf("[ PRSE ] parse_identifier\n");
    }

    if (!parser_consume(parser, TOKEN_IDENTIFIER))
    {
        return parser_error(parser->curr, "expected identifier");
    }

    Node *node = node_new(NODE_IDENTIFIER, parser->last);
    node->parent = parent;

    return node;
}

Node *parse_lit_char(Parser *parser, Node *parent)
{
    if (parser->opt_verbose)
    {
        printf("[ PRSE ] parse_lit_char\n");
    }

    if (!parser_consume(parser, TOKEN_CHARACTER))
    {
        return parser_error(parser->curr, "expected character literal");
    }

    Node *node = node_new(NODE_LIT_CHAR, parser->last);
    node->parent = parent;

    return node;
}

Node *parse_lit_int(Parser *parser, Node *parent)
{
    if (parser->opt_verbose)
    {
        printf("[ PRSE ] parse_lit_int\n");
    }

    if (!parser_consume(parser, TOKEN_INT))
    {
        return parser_error(parser->curr, "expected integer literal");
    }

    Node *node = node_new(NODE_LIT_INT, parser->last);
    node->parent = parent;

    return node;
}

Node *parse_lit_float(Parser *parser, Node *parent)
{
    if (parser->opt_verbose)
    {
        printf("[ PRSE ] parse_lit_float\n");
    }

    if (!parser_consume(parser, TOKEN_FLOAT))
    {
        return parser_error(parser->curr, "expected float literal");
    }

    Node *node = node_new(NODE_LIT_FLOAT, parser->last);
    node->parent = parent;

    return node;
}

Node *parse_lit_string(Parser *parser, Node *parent)
{
    if (parser->opt_verbose)
    {
        printf("[ PRSE ] parse_lit_string\n");
    }

    if (!parser_consume(parser, TOKEN_STRING))
    {
        return parser_error(parser->curr, "expected string literal");
    }

    Node *node = node_new(NODE_LIT_STRING, parser->last);
    node->parent = parent;

    return node;
}

Node *parse_expr_member(Parser *parser, Node *parent, Node *target)
{
    if (parser->opt_verbose)
    {
        printf("[ PRSE ] parse_expr_member\n");
    }

    if (!parser_consume(parser, TOKEN_DOT))
    {
        return parser_error(parser->curr, "expected '.'");
    }

    Node *node = node_new(NODE_EXPR_MEMBER, parser->last);
    node->parent = parent;
    node->expr_member.target = target;
    node->expr_member.identifier = parse_identifier(parser, node);

    return node;
}

Node *parse_expr_call(Parser *parser, Node *parent, Node *target)
{
    if (parser->opt_verbose)
    {
        printf("[ PRSE ] parse_expr_call\n");
    }

    if (!parser_consume(parser, TOKEN_L_PAREN))
    {
        return parser_error(parser->curr, "expected '('");
    }

    Node *node = node_new(NODE_EXPR_CALL, parser->last);
    node->parent = parent;
    node->expr_call.target = target;

    while (!parser_match(parser, TOKEN_R_PAREN) && !parser_match(parser, TOKEN_EOF))
    {
        Node *argument = parse_expr(parser, node);
        node->expr_call.arguments = node_list_add(node->expr_call.arguments, argument);

        if (!parser_consume(parser, TOKEN_COMMA))
        {
            if (!parser_match(parser, TOKEN_R_PAREN))
            {
                node_free(node);
                return parser_error(parser->curr, "expected ',' or ')'");
            }
        }
    }

    if (parser->last->kind == TOKEN_COMMA)
    {
        node_free(node);
        return parser_error(parser->curr, "unexpected ','");
    }

    if (!parser_consume(parser, TOKEN_R_PAREN))
    {
        node_free(node);
        return parser_error(parser->curr, "expected ')'");
    }

    return node;
}

Node *parse_expr_index(Parser *parser, Node *parent, Node *target)
{
    if (parser->opt_verbose)
    {
        printf("[ PRSE ] parse_expr_index\n");
    }

    if (!parser_consume(parser, TOKEN_L_BRACKET))
    {
        return parser_error(parser->curr, "expected '['");
    }

    Node *node = node_new(NODE_EXPR_INDEX, parser->last);
    node->parent = parent;
    node->expr_index.target = target;
    node->expr_index.index = parse_expr(parser, node);

    if (!parser_consume(parser, TOKEN_R_BRACKET))
    {
        node_free(node);
        return parser_error(parser->curr, "expected ']'");
    }

    return node;
}

Node *parse_expr_cast(Parser *parser, Node *parent, Node *target)
{
    if (parser->opt_verbose)
    {
        printf("[ PRSE ] parse_expr_cast\n");
    }

    if (!parser_consume(parser, TOKEN_COLON_COLON))
    {
        return parser_error(parser->curr, "expected '::'");
    }

    Node *node = node_new(NODE_EXPR_CAST, parser->last);
    node->parent = parent;
    node->expr_cast.target = target;
    node->expr_cast.type = parse_type(parser, node);

    return node;
}

Node *parse_expr_new(Parser *parser, Node *parent)
{
    if (parser->opt_verbose)
    {
        printf("[ PRSE ] parse_expr_new\n");
    }

    if (!parser_consume(parser, TOKEN_KW_NEW))
    {
        return parser_error(parser->curr, "expected 'new'");
    }

    Node *node = node_new(NODE_EXPR_NEW, parser->last);
    node->parent = parent;
    node->expr_new.type = parse_type(parser, node);

    if (!parser_consume(parser, TOKEN_L_BRACE))
    {
        node_free(node);
        return parser_error(parser->curr, "expected '{'");
    }

    while (!parser_match(parser, TOKEN_R_BRACE) && !parser_match(parser, TOKEN_EOF))
    {
        Node *initializer = parse_expr(parser, node);
        node->expr_new.initializers = node_list_add(node->expr_new.initializers, initializer);

        if (!parser_consume(parser, TOKEN_SEMICOLON))
        {
            node_free(node);
            return parser_error(parser->curr, "expected ';'");
        }
    }

    if (!parser_consume(parser, TOKEN_R_BRACE))
    {
        node_free(node);
        return parser_error(parser->curr, "expected '}'");
    }

    return node;
}

Node *parse_expr_unary(Parser *parser, Node *parent)
{
    if (parser->opt_verbose)
    {
        printf("[ PRSE ] parse_expr_unary\n");
    }

    if (op_is_unary(parser->curr->kind))
    {
        Token *op = parser->curr;
        parser_advance(parser);

        Node *node = node_new(NODE_EXPR_UNARY, op);
        node->parent = parent;
        node->expr_unary.right = parse_expr_unary(parser, node);

        return node;
    }

    return parse_expr_primary(parser, parent);
}

Node *parse_expr_binary(Parser *parser, Node *parent, int parent_precedence)
{
    if (parser->opt_verbose)
    {
        printf("[ PRSE ] parse_expr_binary\n");
    }

    Node *lhs = parse_expr_postfix(parser, parent);

    while (op_is_binary(parser->curr->kind) && op_get_precedence(parser->curr->kind) >= parent_precedence)
    {
        Token *op = parser->curr;
        parser_advance(parser);

        int precedence = op_get_precedence(op->kind);
        if (op_is_right_associative(op->kind))
        {
            precedence++;
        }

        Node *node = node_new(NODE_EXPR_BINARY, op);
        node->parent = parent;
        node->expr_binary.left = lhs;
        node->expr_binary.right = parse_expr_binary(parser, node, precedence);

        lhs = node;
    }

    return lhs;
}

Node *parse_type_array(Parser *parser, Node *parent)
{
    if (parser->opt_verbose)
    {
        printf("[ PRSE ] parse_type_array\n");
    }

    if (!parser_consume(parser, TOKEN_L_BRACKET))
    {
        return parser_error(parser->curr, "expected '['");
    }

    Node *node = node_new(NODE_TYPE_ARRAY, parser->last);
    node->parent = parent;

    if (!parser_match(parser, TOKEN_R_BRACKET))
    {
        node->type_arr.size = parse_expr(parser, node);
    }

    if (!parser_consume(parser, TOKEN_R_BRACKET))
    {
        node_free(node);
        return parser_error(parser->curr, "expected ']'");
    }

    node->type_arr.type = parse_type(parser, node);

    return node;
}

Node *parse_type_pointer(Parser *parser, Node *parent)
{
    if (parser->opt_verbose)
    {
        printf("[ PRSE ] parse_type_pointer\n");
    }

    if (!parser_consume(parser, TOKEN_STAR))
    {
        return parser_error(parser->curr, "expected '*'");
    }

    Node *node = node_new(NODE_TYPE_POINTER, parser->last);
    node->parent = parent;
    node->type_ptr.type = parse_type(parser, node);

    return node;
}

Node *parse_type_fun(Parser *parser, Node *parent)
{
    if (parser->opt_verbose)
    {
        printf("[ PRSE ] parse_type_fun\n");
    }

    if (!parser_consume(parser, TOKEN_KW_FUN))
    {
        return parser_error(parser->curr, "expected 'fun'");
    }

    Node *node = node_new(NODE_TYPE_FUN, parser->last);
    node->parent = parent;

    if (!parser_consume(parser, TOKEN_L_PAREN))
    {
        node_free(node);
        return parser_error(parser->curr, "expected '('");
    }

    while (!parser_match(parser, TOKEN_R_PAREN) && !parser_match(parser, TOKEN_EOF))
    {
        Node *param = parse_field(parser, node);
        node->type_fun.parameters = node_list_add(node->type_fun.parameters, param);

        if (!parser_consume(parser, TOKEN_COMMA))
        {
            if (!parser_match(parser, TOKEN_R_PAREN))
            {
                node_free(node);
                return parser_error(parser->curr, "expected ',' or ')'");
            }
        };
    }

    if (parser->last->kind == TOKEN_COMMA)
    {
        node_free(node);
        return parser_error(parser->curr, "unexpected ','");
    }

    if (!parser_consume(parser, TOKEN_R_PAREN))
    {
        node_free(node);
        return parser_error(parser->curr, "expected ')'");
    }

    if (parser_consume(parser, TOKEN_COLON))
    {
        node->type_fun.return_type = parse_type(parser, node);
    }

    return node;
}

Node *parse_type_str(Parser *parser, Node *parent)
{
    if (parser->opt_verbose)
    {
        printf("[ PRSE ] parse_type_str\n");
    }

    if (!parser_consume(parser, TOKEN_KW_STR))
    {
        return parser_error(parser->curr, "expected 'str'");
    }

    Node *node = node_new(NODE_TYPE_STR, parser->last);
    node->parent = parent;

    if (!parser_consume(parser, TOKEN_L_BRACE))
    {
        node_free(node);
        return parser_error(parser->curr, "expected '{'");
    }

    while (!parser_match(parser, TOKEN_R_BRACE) && !parser_match(parser, TOKEN_EOF))
    {
        Node *field = parse_field(parser, node);
        node->type_str.fields = node_list_add(node->type_str.fields, field);

        if (!parser_consume(parser, TOKEN_SEMICOLON))
        {
            node_free(node);
            return parser_error(parser->curr, "expected ';'");
        }
    }

    if (!parser_consume(parser, TOKEN_R_BRACE))
    {
        node_free(node);
        return parser_error(parser->curr, "expected '}'");
    }

    return node;
}

Node *parse_type_uni(Parser *parser, Node *parent)
{
    if (parser->opt_verbose)
    {
        printf("[ PRSE ] parse_type_uni\n");
    }

    if (!parser_consume(parser, TOKEN_KW_UNI))
    {
        return parser_error(parser->curr, "expected 'uni'");
    }

    Node *node = node_new(NODE_TYPE_UNI, parser->last);
    node->parent = parent;

    if (!parser_consume(parser, TOKEN_L_BRACE))
    {
        node_free(node);
        return parser_error(parser->curr, "expected '{'");
    }

    while (!parser_match(parser, TOKEN_R_BRACE) && !parser_match(parser, TOKEN_EOF))
    {
        Node *field = parse_field(parser, node);
        node->type_uni.fields = node_list_add(node->type_uni.fields, field);

        if (!parser_consume(parser, TOKEN_SEMICOLON))
        {
            node_free(node);
            return parser_error(parser->curr, "expected ';'");
        }
    }

    if (!parser_consume(parser, TOKEN_R_BRACE))
    {
        node_free(node);
        return parser_error(parser->curr, "expected '}'");
    }

    return node;
}

Node *parse_field(Parser *parser, Node *parent)
{
    if (parser->opt_verbose)
    {
        printf("[ PRSE ] parse_field\n");
    }

    Node *node = node_new(NODE_FIELD, parser->curr);
    node->parent = parent;
    node->field.identifier = parse_identifier(parser, node);

    if (!parser_consume(parser, TOKEN_COLON))
    {
        node_free(node);
        return parser_error(parser->curr, "expected ':'");
    }

    node->field.type = parse_type(parser, node);

    return node;
}

Node *parse_stmt_val(Parser *parser, Node *parent)
{
    if (parser->opt_verbose)
    {
        printf("[ PRSE ] parse_stmt_val\n");
    }

    if (!parser_consume(parser, TOKEN_KW_VAL))
    {
        return parser_error(parser->curr, "expected 'val'");
    }

    Node *node = node_new(NODE_STMT_VAL, parser->last);
    node->parent = parent;
    node->stmt_val.identifier = parse_identifier(parser, node);

    if (!parser_consume(parser, TOKEN_COLON))
    {
        node_free(node);
        return parser_error(parser->curr, "expected ':'");
    }

    node->stmt_val.type = parse_type(parser, node);

    if (!parser_consume(parser, TOKEN_EQUAL))
    {
        node_free(node);
        return parser_error(parser->curr, "expected '='");
    }

    node->stmt_val.initializer = parse_expr(parser, node);

    if (!parser_consume(parser, TOKEN_SEMICOLON))
    {
        node_free(node);
        return parser_error(parser->curr, "expected ';'");
    }

    return node;
}

Node *parse_stmt_var(Parser *parser, Node *parent)
{
    if (parser->opt_verbose)
    {
        printf("[ PRSE ] parse_stmt_var\n");
    }

    if (!parser_consume(parser, TOKEN_KW_VAR))
    {
        return parser_error(parser->curr, "expected 'var'");
    }

    Node *node = node_new(NODE_STMT_VAR, parser->last);
    node->parent = parent;
    node->stmt_var.identifier = parse_identifier(parser, node);

    if (!parser_consume(parser, TOKEN_COLON))
    {
        node_free(node);
        return parser_error(parser->curr, "expected ':'");
    }

    node->stmt_var.type = parse_type(parser, node);

    if (parser_consume(parser, TOKEN_EQUAL))
    {
        node->stmt_var.initializer = parse_expr(parser, node);
    }

    if (!parser_consume(parser, TOKEN_SEMICOLON))
    {
        node_free(node);
        return parser_error(parser->curr, "expected ';'");
    }

    return node;
}

Node *parse_stmt_def(Parser *parser, Node *parent)
{
    if (parser->opt_verbose)
    {
        printf("[ PRSE ] parse_stmt_def\n");
    }

    if (!parser_consume(parser, TOKEN_KW_DEF))
    {
        return parser_error(parser->curr, "expected 'def'");
    }

    Node *node = node_new(NODE_STMT_DEF, parser->last);
    node->parent = parent;
    node->stmt_def.identifier = parse_identifier(parser, node);

    if (!parser_consume(parser, TOKEN_COLON))
    {
        node_free(node);
        return parser_error(parser->curr, "expected ':'");
    }

    node->stmt_def.type = parse_type(parser, node);

    if (!parser_consume(parser, TOKEN_SEMICOLON))
    {
        node_free(node);
        return parser_error(parser->curr, "expected ';'");
    }

    return node;
}

Node *parse_stmt_use(Parser *parser, Node *parent)
{
    if (parser->opt_verbose)
    {
        printf("[ PRSE ] parse_stmt_use\n");
    }

    if (!parser_consume(parser, TOKEN_KW_USE))
    {
        return parser_error(parser->curr, "expected 'use'");
    }

    Node *node = node_new(NODE_STMT_USE, parser->last);
    node->parent = parent;
    node->stmt_use.path = parse_lit_string(parser, node);

    if (!parser_consume(parser, TOKEN_SEMICOLON))
    {
        node_free(node);
        return parser_error(parser->curr, "expected ';'");
    }

    return node;
}

Node *parse_stmt_fun(Parser *parser, Node *parent)
{
    if (parser->opt_verbose)
    {
        printf("[ PRSE ] parse_stmt_fun\n");
    }

    if (!parser_consume(parser, TOKEN_KW_FUN))
    {
        return parser_error(parser->curr, "expected 'fun'");
    }

    Node *node = node_new(NODE_STMT_FUN, parser->last);
    node->parent = parent;
    node->stmt_fun.identifier = parse_identifier(parser, node);

    if (!parser_consume(parser, TOKEN_L_PAREN))
    {
        node_free(node);
        return parser_error(parser->curr, "expected '('");
    }

    while (!parser_match(parser, TOKEN_R_PAREN) && !parser_match(parser, TOKEN_EOF))
    {
        Node *param = parse_field(parser, node);
        node->stmt_fun.parameters = node_list_add(node->stmt_fun.parameters, param);

        if (!parser_consume(parser, TOKEN_COMMA))
        {
            if (!parser_match(parser, TOKEN_R_PAREN))
            {
                node_free(node);
                return parser_error(parser->curr, "expected ',' or ')'");
            }
        }
    }

    if (parser->last->kind == TOKEN_COMMA)
    {
        node_free(node);
        return parser_error(parser->last, "unexpected ','");
    }

    if (!parser_consume(parser, TOKEN_R_PAREN))
    {
        node_free(node);
        return parser_error(parser->curr, "expected ')'");
    }

    if (parser_consume(parser, TOKEN_COLON))
    {
        node->stmt_fun.return_type = parse_type(parser, node);
    }

    node->stmt_fun.body = parse_stmt_block(parser, node);

    return node;
}

Node *parse_stmt_ext(Parser *parser, Node *parent)
{
    if (parser->opt_verbose)
    {
        printf("[ PRSE ] parse_stmt_ext\n");
    }

    if (!parser_consume(parser, TOKEN_KW_EXT))
    {
        return parser_error(parser->curr, "expected 'ext'");
    }

    Node *node = node_new(NODE_STMT_EXT, parser->last);
    node->parent = parent;
    node->stmt_ext.identifier = parse_identifier(parser, node);

    if (!parser_consume(parser, TOKEN_COLON))
    {
        node_free(node);
        return parser_error(parser->curr, "expected ':'");
    }

    node->stmt_ext.type = node_list_add(node->stmt_ext.type, parse_type(parser, node));

    if (!parser_consume(parser, TOKEN_SEMICOLON))
    {
        node_free(node);
        return parser_error(parser->curr, "expected ';'");
    }

    return node;
}

Node *parse_stmt_if(Parser *parser, Node *parent)
{
    if (parser->opt_verbose)
    {
        printf("[ PRSE ] parse_stmt_if\n");
    }

    if (!parser_consume(parser, TOKEN_KW_IF))
    {
        return parser_error(parser->curr, "expected 'if'");
    }

    Node *node = node_new(NODE_STMT_IF, parser->last);
    node->parent = parent;

    if (!parser_consume(parser, TOKEN_L_PAREN))
    {
        node_free(node);
        return parser_error(parser->curr, "expected '('");
    }

    node->stmt_if.condition = parse_expr(parser, node);

    if (!parser_consume(parser, TOKEN_R_PAREN))
    {
        node_free(node);
        return parser_error(parser->curr, "expected ')'");
    }

    node->stmt_if.body = parse_stmt_block(parser, node);

    return node;
}

Node *parse_stmt_or(Parser *parser, Node *parent)
{
    if (parser->opt_verbose)
    {
        printf("[ PRSE ] parse_stmt_or\n");
    }

    if (!parser_consume(parser, TOKEN_KW_OR))
    {
        return parser_error(parser->curr, "expected 'or'");
    }

    Node *node = node_new(NODE_STMT_OR, parser->last);
    node->parent = parent;

    if (parser_consume(parser, TOKEN_L_PAREN))
    {
        node->stmt_or.condition = parse_expr(parser, node);

        if (!parser_consume(parser, TOKEN_R_PAREN))
        {
            node_free(node);
            return parser_error(parser->curr, "expected ')'");
        }
    }

    node->stmt_or.body = parse_stmt_block(parser, node);

    return node;
}

Node *parse_stmt_for(Parser *parser, Node *parent)
{
    if (parser->opt_verbose)
    {
        printf("[ PRSE ] parse_stmt_for\n");
    }

    if (!parser_consume(parser, TOKEN_KW_FOR))
    {
        return parser_error(parser->curr, "expected 'for'");
    }

    Node *node = node_new(NODE_STMT_FOR, parser->last);
    node->parent = parent;

    if (parser_consume(parser, TOKEN_L_PAREN))
    {
        node->stmt_for.condition = parse_expr(parser, node);

        if (!parser_consume(parser, TOKEN_R_PAREN))
        {
            node_free(node);
            return parser_error(parser->curr, "expected ')'");
        }
    }

    node->stmt_for.body = parse_stmt_block(parser, node);

    return node;
}

Node *parse_stmt_brk(Parser *parser, Node *parent)
{
    if (parser->opt_verbose)
    {
        printf("[ PRSE ] parse_stmt_brk\n");
    }

    if (!parser_consume(parser, TOKEN_KW_BRK))
    {
        return parser_error(parser->curr, "expected 'brk'");
    }

    Node *node = node_new(NODE_STMT_BRK, parser->last);
    node->parent = parent;

    if (!parser_consume(parser, TOKEN_SEMICOLON))
    {
        node_free(node);
        return parser_error(parser->curr, "expected ';'");
    }

    return node;
}

Node *parse_stmt_cnt(Parser *parser, Node *parent)
{
    if (parser->opt_verbose)
    {
        printf("[ PRSE ] parse_stmt_cnt\n");
    }

    if (!parser_consume(parser, TOKEN_KW_CNT))
    {
        return parser_error(parser->curr, "expected 'cnt'");
    }

    Node *node = node_new(NODE_STMT_CNT, parser->last);
    node->parent = parent;

    if (!parser_consume(parser, TOKEN_SEMICOLON))
    {
        node_free(node);
        return parser_error(parser->curr, "expected ';'");
    }

    return node;
}

Node *parse_stmt_ret(Parser *parser, Node *parent)
{
    if (parser->opt_verbose)
    {
        printf("[ PRSE ] parse_stmt_ret\n");
    }

    if (!parser_consume(parser, TOKEN_KW_RET))
    {
        return parser_error(parser->curr, "expected 'ret'");
    }

    Node *node = node_new(NODE_STMT_RET, parser->last);
    node->parent = parent;

    if (!parser_consume(parser, TOKEN_SEMICOLON))
    {
        node->stmt_ret.value = parse_expr(parser, node);

        if (!parser_consume(parser, TOKEN_SEMICOLON))
        {
            node_free(node);
            return parser_error(parser->curr, "expected ';'");
        }
    }

    return node;
}

Node *parse_stmt_asm(Parser *parser, Node *parent)
{
    if (parser->opt_verbose)
    {
        printf("[ PRSE ] parse_stmt_asm\n");
    }

    if (!parser_consume(parser, TOKEN_KW_ASM))
    {
        return parser_error(parser->curr, "expected 'asm'");
    }

    Node *node = node_new(NODE_STMT_ASM, parser->last);
    node->parent = parent;
    node->stmt_asm.code = parse_lit_string(parser, node);

    if (!parser_consume(parser, TOKEN_SEMICOLON))
    {
        node_free(node);
        return parser_error(parser->curr, "expected ';'");
    }

    return node;
}

Node *parse_stmt_block(Parser *parser, Node *parent)
{
    if (parser->opt_verbose)
    {
        printf("[ PRSE ] parse_stmt_block\n");
    }

    if (!parser_consume(parser, TOKEN_L_BRACE))
    {
        return parser_error(parser->curr, "expected '{'");
    }

    Node *node = node_new(NODE_STMT_BLOCK, parser->last);
    node->parent = parent;

    while (!parser_match(parser, TOKEN_R_BRACE) && !parser_match(parser, TOKEN_EOF))
    {
        Node *stmt = parse_stmt(parser, node);
        node->stmt_block.statements = node_list_add(node->stmt_block.statements, stmt);
    }

    if (!parser_consume(parser, TOKEN_R_BRACE))
    {
        node_free(node);
        return parser_error(parser->curr, "expected '}'");
    }

    return node;
}

Node *parse_stmt_expr(Parser *parser, Node *parent)
{
    if (parser->opt_verbose)
    {
        printf("[ PRSE ] parse_stmt_expr\n");
    }

    Node *node = parse_expr(parser, parent);

    if (!parser_consume(parser, TOKEN_SEMICOLON))
    {
        node_free(node);
        return parser_error(parser->curr, "expected ';'");
    }

    return node;
}

Node *parse_expr_postfix(Parser *parser, Node *parent)
{
    if (parser->opt_verbose)
    {
        printf("[ PRSE ] parse_expr_postfix\n");
    }

    Node *node = parse_expr_unary(parser, parent);

    while (true)
    {
        switch (parser->curr->kind)
        {
        case TOKEN_DOT:
            node = parse_expr_member(parser, parent, node);
            break;
        case TOKEN_L_PAREN:
            node = parse_expr_call(parser, parent, node);
            break;
        case TOKEN_L_BRACKET:
            node = parse_expr_index(parser, parent, node);
            break;
        case TOKEN_COLON_COLON:
            node = parse_expr_cast(parser, parent, node);
            break;
        default:
            return node;
        }
    }
}

Node *parse_expr_grouping(Parser *parser, Node *parent)
{
    if (parser->opt_verbose)
    {
        printf("[ PRSE ] parse_expr_grouping\n");
    }

    if (!parser_consume(parser, TOKEN_L_PAREN))
    {
        return parser_error(parser->curr, "expected '('");
    }

    Node *node = parse_expr(parser, parent);

    if (!parser_consume(parser, TOKEN_R_PAREN))
    {
        node_free(node);
        return parser_error(parser->curr, "expected ')'");
    }

    return node;
}

Node *parse_expr_primary(Parser *parser, Node *parent)
{
    if (parser->opt_verbose)
    {
        printf("[ PRSE ] parse_expr_primary\n");
    }

    switch (parser->curr->kind)
    {
    case TOKEN_IDENTIFIER:
        return parse_identifier(parser, parent);
    case TOKEN_CHARACTER:
        return parse_lit_char(parser, parent);
    case TOKEN_INT:
        return parse_lit_int(parser, parent);
    case TOKEN_FLOAT:
        return parse_lit_float(parser, parent);
    case TOKEN_STRING:
        return parse_lit_string(parser, parent);
    case TOKEN_KW_NEW:
        return parse_expr_new(parser, parent);
    case TOKEN_L_PAREN:
        return parse_expr_grouping(parser, parent);
    default:
        return parser_error(parser->curr, "expected primary expression");
    }
}

Node *parse_expr(Parser *parser, Node *parent)
{
    if (parser->opt_verbose)
    {
        printf("[ PRSE ] parse_expr\n");
    }

    return parse_expr_binary(parser, parent, 0);
}

Node *parse_type(Parser *parser, Node *parent)
{
    if (parser->opt_verbose)
    {
        printf("[ PRSE ] parse_type\n");
    }

    switch (parser->curr->kind)
    {
    case TOKEN_IDENTIFIER:
        return parse_identifier(parser, parent);
    case TOKEN_L_BRACKET:
        return parse_type_array(parser, parent);
    case TOKEN_STAR:
        return parse_type_pointer(parser, parent);
    case TOKEN_KW_FUN:
        return parse_type_fun(parser, parent);
    case TOKEN_KW_STR:
        return parse_type_str(parser, parent);
    case TOKEN_KW_UNI:
        return parse_type_uni(parser, parent);
    default:
        return parser_error(parser->curr, "expected type");
    }
}

Node *parse_stmt(Parser *parser, Node *parent)
{
    if (parser->opt_verbose)
    {
        printf("[ PRSE ] parse_stmt\n");
    }

    Node *node;

    switch (parser->curr->kind)
    {
    case TOKEN_KW_VAL:
        node = parse_stmt_val(parser, parent);
        break;
    case TOKEN_KW_VAR:
        node = parse_stmt_var(parser, parent);
        break;
    case TOKEN_KW_DEF:
        node = parse_stmt_def(parser, parent);
        break;
    case TOKEN_KW_USE:
        node = parse_stmt_use(parser, parent);
        break;
    case TOKEN_KW_FUN:
        node = parse_stmt_fun(parser, parent);
        break;
    case TOKEN_KW_EXT:
        node = parse_stmt_ext(parser, parent);
        break;
    case TOKEN_KW_IF:
        node = parse_stmt_if(parser, parent);
        break;
    case TOKEN_KW_OR:
        node = parse_stmt_or(parser, parent);
        break;
    case TOKEN_KW_FOR:
        node = parse_stmt_for(parser, parent);
        break;
    case TOKEN_KW_BRK:
        node = parse_stmt_brk(parser, parent);
        break;
    case TOKEN_KW_CNT:
        node = parse_stmt_cnt(parser, parent);
        break;
    case TOKEN_KW_RET:
        node = parse_stmt_ret(parser, parent);
        break;
    case TOKEN_KW_ASM:
        node = parse_stmt_asm(parser, parent);
        break;
    case TOKEN_L_BRACE:
        node = parse_stmt_block(parser, parent);
        break;
    default:
        node = parse_stmt_expr(parser, parent);
        break;
    }

    if (parser->opt_verbose)
    {
        printf("[ PRSE ] [ STMT ] %s\n", node_kind_string(node->kind));
    }

    if (node->kind == NODE_ERROR)
    {
        parser_advance(parser);
    }

    return node;
}

Node *parser_parse(Parser *parser)
{
    parser_advance(parser);

    return parse_program(parser);
}
