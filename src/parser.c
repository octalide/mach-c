#include "parser.h"
#include "lexer.h"
#include "operator.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Parser *parser_new(Lexer *lexer)
{
    Parser *parser = calloc(1, sizeof(Parser));
    parser->comments = calloc(1, sizeof(Token *));
    parser->comments[0] = NULL;

    parser->last = NULL;
    parser->curr = NULL;
    parser->next = NULL;

    parser->lexer = lexer;

    return parser;
}

void parser_free(Parser *parser)
{
    if (parser == NULL)
    {
        return;
    }

    token_free(parser->curr);
    parser->curr = NULL;

    token_free(parser->next);
    parser->next = NULL;

    if (parser->comments != NULL)
    {
        for (size_t i = 0; parser->comments[i] != NULL; i++)
        {
            token_free(parser->comments[i]);
            parser->comments[i] = NULL;
        }
        free(parser->comments);
        parser->comments = NULL;
    }

    lexer_free(parser->lexer);
    parser->lexer = NULL;

    free(parser);
}

void parser_print_error(Parser *parser, Node *node)
{
    if (node->kind != NODE_ERROR)
    {
        return;
    }

    if (node->token == NULL)
    {
        printf("error: %s\n", node->data.error->message);
        printf("! location unknown\n");
    }

    int line_num = lexer_get_pos_line(parser->lexer, node->token->pos);
    int offset = lexer_get_pos_line_offset(parser->lexer, node->token->pos);
    char *line = lexer_get_line_text(parser->lexer, line_num);
    if (line == NULL)
    {
        printf("error: %s\n", node->data.error->message);
        printf("! location unknown\n");
        return;
    }

    // find containing NODE_FILE
    // NOTE:
    //   This only works because the parser can safely assume that any parsed
    //   code is at some level contained inside a NODE_FILE. Breaking this
    //   convention for any reason would be silly and is not advised.
    //   It stands to reason then, that if an error is ever reported with
    //   "UNKNOWN_FILE_PATH", that something has been very borked.
    char *path = "UNKNOWN_FILE_PATH";
    Node *file = node_find_parent(node, NODE_FILE);
    if (file != NULL)
    {
        path = file->data.file->path;
    }

    printf("error: %s\n", node->data.error->message);
    printf("%s:%d:%d\n", path, line_num + 1, offset);
    printf("%5d | %s\n", line_num + 1, line);
    printf("      | %*s^\n", offset - 1, "");

    free(line);
}

void parser_add_comment(Parser *parser, Token *token)
{
    int count = 0;
    while (parser->comments[count] != NULL)
    {
        count++;
    }

    parser->comments = realloc(parser->comments, (count + 2) * sizeof(Token *));
    parser->comments[count] = token;
    parser->comments[count + 1] = NULL;
}

void parser_advance(Parser *parser)
{
    Token *next = lexer_next(parser->lexer);
    while (next->kind == TOKEN_COMMENT)
    {
        parser_add_comment(parser, next);
        next = lexer_next(parser->lexer);
    }

    token_free(parser->last);

    parser->last = parser->curr;
    parser->curr = parser->next;
    parser->next = next;

    // NOTE:
    //   Almost eclusively useful at the start of a parsing operation to
    //   pre-populate tokens.
    if (parser->curr == NULL && parser->next != NULL)
    {
        parser_advance(parser);
    }
}

bool parser_peek(Parser *parser, TokenKind type)
{
    return parser->next->kind == type;
}

bool parser_match(Parser *parser, TokenKind type)
{
    return parser->curr->kind == type;
}

bool parser_consume(Parser *parser, TokenKind type)
{
    if (parser_match(parser, type))
    {
        parser_advance(parser);
        return true;
    }

    return false;
}

Node *parser_new_error(Token *token, char *message)
{
    Node *node = node_new(NODE_ERROR);
    node->token = token_copy(token);
    node->data.error->message = strdup(message);

    return node;
}

Node *parse_file(Parser *parser, char *path)
{
    Node *node = node_new(NODE_FILE);
    node->token = token_copy(parser->curr);
    node->data.file->path = strdup(path);
    node->data.file->parser = parser;
    node->data.file->statements = node_list_new();

    while (!parser_match(parser, TOKEN_EOF))
    {
        Node *stmt = parse_stmt(parser);
        stmt->parent = node;

        node_list_add(&node->data.file->statements, stmt);
    }

    return node;
}

Node *parse_identifier(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_IDENTIFIER))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected identifier");
    }

    Node *node = node_new(NODE_IDENTIFIER);
    node->token = token_copy(parser->last);
    node->data.identifier->name = lexer_raw_value(parser->lexer, parser->last);

    return node;
}

Node *parse_lit_int(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_LIT_INT))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected integer literal");
    }

    Node *node = node_new(NODE_LIT_INT);
    node->token = token_copy(parser->last);
    node->data.lit_int->value = lexer_eval_lit_int(parser->lexer, parser->last);

    return node;
}

Node *parse_lit_float(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_LIT_FLOAT))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected float literal");
    }

    Node *node = node_new(NODE_LIT_FLOAT);
    node->token = token_copy(parser->last);
    node->data.lit_float->value = lexer_eval_lit_float(parser->lexer, parser->last);

    return node;
}

Node *parse_lit_char(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_LIT_CHAR))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected character literal");
    }

    Node *node = node_new(NODE_LIT_CHAR);
    node->token = token_copy(parser->last);
    node->data.lit_char->value = lexer_eval_lit_char(parser->lexer, parser->last);

    return node;
}

Node *parse_lit_string(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_LIT_STRING))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected string literal");
    }

    Node *node = node_new(NODE_LIT_STRING);
    node->token = token_copy(parser->last);
    node->data.lit_string->value = lexer_raw_value(parser->lexer, parser->last);

    return node;
}

Node *parse_expr_member(Parser *parser, Node *target)
{
    if (!parser_consume(parser, TOKEN_DOT))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected '.'");
    }

    Node *member = parse_identifier(parser);

    Node *node = node_new(NODE_EXPR_MEMBER);
    node->token = token_copy(parser->last);

    node->data.expr_member->target = target;
    node->data.expr_member->member = member;

    target->parent = node;
    member->parent = node;

    return node;
}

Node *parse_expr_call(Parser *parser, Node *target)
{
    if (!parser_consume(parser, TOKEN_L_PAREN))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected '('");
    }

    Node *node = node_new(NODE_EXPR_CALL);
    node->token = token_copy(parser->last);
    node->data.expr_call->arguments = node_list_new();
    node->data.expr_call->target = target;

    target->parent = node;

    while (!parser_match(parser, TOKEN_R_PAREN))
    {
        Node *argument = parse_expr(parser);
        if (argument->kind == NODE_ERROR)
        {
            node_free(node);
            return argument;
        }
        argument->parent = node;
        node_list_add(&node->data.expr_call->arguments, argument);

        if (!parser_consume(parser, TOKEN_COMMA))
        {
            break;
        }
    }

    if (!parser_consume(parser, TOKEN_R_PAREN))
    {
        parser_advance(parser);

        Node *error = parser_new_error(parser->last, "expected ')'");
        node_free(node);
        return error;
    }

    return node;
}

Node *parse_expr_index(Parser *parser, Node *target)
{
    if (!parser_consume(parser, TOKEN_L_BRACKET))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected '['");
    }

    Node *node = node_new(NODE_EXPR_INDEX);
    node->token = token_copy(parser->last);

    target->parent = node;
    node->data.expr_index->target = target;

    Node *node_expr = parse_expr(parser);
    if (node_expr->kind == NODE_ERROR)
    {
        node_free(node);
        return parser_new_error(parser->last, "expected expression");
    }
    node_expr->parent = node;    
    node->data.expr_index->index = node_expr;

    if (!parser_consume(parser, TOKEN_R_BRACKET))
    {
        parser_advance(parser);

        Node *error = parser_new_error(parser->last, "expected ']'");
        node_free(node);
        return error;
    }

    return node;
}

Node *parse_expr_cast(Parser *parser, Node *target)
{
    if (!parser_consume(parser, TOKEN_COLON_COLON))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected '::'");
    }

    Node *node = node_new(NODE_EXPR_CAST);
    node->token = token_copy(parser->last);

    target->parent = node;
    node->data.expr_cast->target = target;

    Node *type = parse_type(parser);
    if (type->kind == NODE_ERROR)
    {
        node_free(node);
        return parser_new_error(parser->last, "expected type");
    }
    type->parent = node;
    node->data.expr_cast->type = type;

    return node;
}

Node *parse_expr_unary(Parser *parser)
{
    if (op_is_unary(op_from_token_kind(parser->curr->kind)))
    {
        Operator op = op_from_token_kind(parser->curr->kind);
        parser_advance(parser);

        Node *node = node_new(NODE_EXPR_UNARY);
        node->token = token_copy(parser->last);
        node->data.expr_unary->op = op;

        Node *node_expr_unary = parse_expr_unary(parser);
        if (node_expr_unary->kind == NODE_ERROR)
        {
            node_free(node);
            return parser_new_error(parser->last, "expected unary expression");
        }
        node_expr_unary->parent = node;
        node->data.expr_unary->target = node_expr_unary;

        return node;
    }

    return parse_expr_primary(parser);
}

Node *parse_expr_binary(Parser *parser, int parent_precedence)
{
    Node *left = parse_expr_postfix(parser);
    if (left->kind == NODE_ERROR)
    {
        return left;
    }

    while (true)
    {
        Operator op = op_from_token_kind(parser->curr->kind);
        if (!op_is_binary(op) || (op_precedence(op) < parent_precedence))
        {
            break;
        }

        int precedence = op_precedence(op);
        if (op_is_right_associative(op))
        {
            precedence++;
        }

        parser_advance(parser);

        Node *right = parse_expr_binary(parser, precedence);
        if (right->kind == NODE_ERROR)
        {
            node_free(left);
            return right;
        }

        Node *node = node_new(NODE_EXPR_BINARY);
        node->token = token_copy(parser->last);

        right->parent = node;
        left->parent = node;

        node->data.expr_binary->op = op;
        node->data.expr_binary->left = left;
        node->data.expr_binary->right = right;

        left = node;
    }

    return left;
}

Node *parse_expr_postfix(Parser *parser)
{
    Node *node = parse_expr_unary(parser);
    if (node->kind == NODE_ERROR)
    {
        return node;
    }

    while (true)
    {
        switch (parser->curr->kind)
        {
        case TOKEN_DOT:
            node = parse_expr_member(parser, node);
            break;
        case TOKEN_L_PAREN:
            node = parse_expr_call(parser, node);
            break;
        case TOKEN_L_BRACKET:
            node = parse_expr_index(parser, node);
            break;
        case TOKEN_COLON_COLON:
            node = parse_expr_cast(parser, node);
            break;
        default:
            return node;
        }
    }
}

Node *parse_expr_grouping(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_L_PAREN))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected '('");
    }

    Node *node = parse_expr(parser);
    if (node->kind == NODE_ERROR)
    {
        return node;
    }

    if (!parser_consume(parser, TOKEN_R_PAREN))
    {
        parser_advance(parser);

        Node *error = parser_new_error(parser->last, "expected ')'");
        node_free(node);
        return error;
    }

    return node;
}

Node *parse_expr_primary(Parser *parser)
{
    switch (parser->curr->kind)
    {
    case TOKEN_IDENTIFIER:
        return parse_identifier(parser);
    case TOKEN_LIT_INT:
        return parse_lit_int(parser);
    case TOKEN_LIT_FLOAT:
        return parse_lit_float(parser);
    case TOKEN_LIT_CHAR:
        return parse_lit_char(parser);
    case TOKEN_LIT_STRING:
        return parse_lit_string(parser);
    case TOKEN_L_PAREN:
        return parse_expr_grouping(parser);
    default:
        parser_advance(parser);
        return parser_new_error(parser->last, "expected primary expression");
    }
}

Node *parse_expr(Parser *parser)
{
    return parse_expr_binary(parser, 0);
}

Node *parse_type_array(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_L_BRACKET))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected '['");
    }
    Node *node = node_new(NODE_TYPE_ARRAY);
    node->token = token_copy(parser->last);

    if (!parser_match(parser, TOKEN_R_BRACKET))
    {
        Node *node_expr = parse_expr(parser);
        if (node_expr->kind == NODE_ERROR)
        {
            node_free(node);
            return parser_new_error(parser->last, "expected expression");
        }
        node_expr->parent = node;
        node->data.type_array->size = node_expr;
    }

    if (!parser_consume(parser, TOKEN_R_BRACKET))
    {
        parser_advance(parser);

        Node *error = parser_new_error(parser->last, "expected ']'");
        node_free(node);
        return error;
    }

    Node *node_type = parse_type(parser);
    if (node_type->kind == NODE_ERROR)
    {
        node_free(node);
        return parser_new_error(parser->last, "expected type");
    }
    node_type->parent = node;
    node->data.type_array->type = node_type;

    return node;
}

Node *parse_type_pointer(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_STAR))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected '*'");
    }

    Node *node = node_new(NODE_TYPE_POINTER);
    node->token = token_copy(parser->last);

    Node *type = parse_type(parser);
    if (type->kind == NODE_ERROR)
    {
        node_free(node);
        return parser_new_error(parser->last, "expected type");
    }
    type->parent = node;
    node->data.type_pointer->type = type;

    return node;
}

Node *parse_type_function(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_IDENTIFIER))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected 'fun'");
    }

    Node *node = node_new(NODE_TYPE_FUN);
    node->token = token_copy(parser->last);

    if (!parser_consume(parser, TOKEN_L_PAREN))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected '('");
    }

    while (!parser_match(parser, TOKEN_R_PAREN))
    {
        Node *parameter = parse_field(parser);
        if (parameter->kind == NODE_ERROR)
        {
            node_free(node);
            return parameter;
        }
        parameter->parent = node;
        node_list_add(&node->data.type_function->parameters, parameter);

        if (!parser_consume(parser, TOKEN_COMMA))
        {
            break;
        }
    }

    if (!parser_consume(parser, TOKEN_R_PAREN))
    {
        parser_advance(parser);

        Node *error = parser_new_error(parser->last, "expected ')'");
        node_free(node);
        return error;
    }

    if (parser_consume(parser, TOKEN_COLON))
    {
        Node *node_type = parse_type(parser);
        if (node_type->kind == NODE_ERROR)
        {
            node_free(node);
            return parser_new_error(parser->last, "expected type");
        }
        node_type->parent = node;
        node->data.type_function->return_type = node_type;
    }

    return node;
}

Node *parse_type_struct(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_IDENTIFIER))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected 'str'");
    }

    Node *node = node_new(NODE_TYPE_STR);
    node->token = token_copy(parser->last);

    if (!parser_consume(parser, TOKEN_L_BRACE))
    {
        parser_advance(parser);
        node_free(node);
        return parser_new_error(parser->last, "expected '{'");
    }

    node->data.type_struct->fields = node_list_new();

    while (!parser_match(parser, TOKEN_R_BRACE))
    {
        Node *field = parse_field(parser);
        if (field->kind == NODE_ERROR)
        {
            node_free(node);
            return field;
        }
        field->parent = node;
        node_list_add(&node->data.type_struct->fields, field);

        if (!parser_consume(parser, TOKEN_SEMICOLON))
        {
            parser_advance(parser);
            node_free(node);
            return parser_new_error(parser->last, "expected ';'");
        }
    }

    if (!parser_consume(parser, TOKEN_R_BRACE))
    {
        parser_advance(parser);
        node_free(node);
        return parser_new_error(parser->last, "expected '}'");
    }

    return node;
}

Node *parse_type_union(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_IDENTIFIER))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected 'uni'");
    }

    Node *node = node_new(NODE_TYPE_UNI);
    node->token = token_copy(parser->last);

    if (!parser_consume(parser, TOKEN_L_BRACE))
    {
        parser_advance(parser);
        node_free(node);
        return parser_new_error(parser->last, "expected '{'");
    }

    node->data.type_union->fields = node_list_new();

    while (!parser_match(parser, TOKEN_R_BRACE))
    {
        Node *field = parse_field(parser);
        if (field->kind == NODE_ERROR)
        {
            node_free(node);
            return field;
        }
        field->parent = node;
        node_list_add(&node->data.type_union->fields, field);

        if (!parser_consume(parser, TOKEN_SEMICOLON))
        {
            parser_advance(parser);
            node_free(node);
            return parser_new_error(parser->last, "expected ';'");
        }
    }

    if (!parser_consume(parser, TOKEN_R_BRACE))
    {
        parser_advance(parser);
        node_free(node);
        return parser_new_error(parser->last, "expected '}'");
    }

    return node;
}

Node *parse_type(Parser *parser)
{
    switch (parser->curr->kind)
    {
    case TOKEN_IDENTIFIER:
        char *token = lexer_raw_value(parser->lexer, parser->curr);
        
        if (strcmp(token, "fun") == 0)
            return parse_type_function(parser);
        if (strcmp(token, "str") == 0)
            return parse_type_struct(parser);
        if (strcmp(token, "uni") == 0)
            return parse_type_union(parser);

        return parse_identifier(parser);
    case TOKEN_L_BRACKET:
        return parse_type_array(parser);
    case TOKEN_STAR:
        return parse_type_pointer(parser);
    default:
        return parser_new_error(parser->curr, "expected type");
    }
}

Node *parse_field(Parser *parser)
{
    Node *node = node_new(NODE_FIELD);
    node->token = token_copy(parser->last);

    Node *node_identifier = parse_identifier(parser);
    if (node_identifier->kind == NODE_ERROR)
    {
        node_free(node);
        return node_identifier;
    }
    node_identifier->parent = node;
    node->data.field->identifier = node_identifier;

    if (!parser_consume(parser, TOKEN_COLON))
    {
        parser_advance(parser);

        Node *error = parser_new_error(parser->last, "expected ':'");
        node_free(node);
        return error;
    }

    Node *node_type = parse_type(parser);
    if (node_type->kind == NODE_ERROR)
    {
        node_free(node);
        return node_type;
    }
    node_type->parent = node;
    node->data.field->type = node_type;

    return node;
}

Node *parse_stmt_val(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_IDENTIFIER))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected identifier");
    }

    Node *node = node_new(NODE_STMT_VAL);
    node->token = token_copy(parser->last);

    Node *node_identifier = parse_identifier(parser);
    if (node_identifier->kind == NODE_ERROR)
    {
        node_free(node);
        return node_identifier;
    }
    node_identifier->parent = node;
    node->data.stmt_val->identifier = node_identifier;

    if (!parser_consume(parser, TOKEN_COLON))
    {
        parser_advance(parser);
        node_free(node);
        return parser_new_error(parser->last, "expected ':'");
    }

    Node *node_type = parse_type(parser);
    if (node_type->kind == NODE_ERROR)
    {
        node_free(node);
        return node_type;
    }
    node_type->parent = node;
    node->data.stmt_val->type = node_type;

    if (!parser_consume(parser, TOKEN_EQUAL))
    {
        parser_advance(parser);
        node_free(node);
        return parser_new_error(parser->last, "expected '='");
    }

    Node *node_expr = parse_expr(parser);
    if (node_expr->kind == NODE_ERROR)
    {
        node_free(node);
        return node_expr;
    }
    node_expr->parent = node;
    node->data.stmt_val->initializer = node_expr;

    if (!parser_consume(parser, TOKEN_SEMICOLON))
    {
        node_free(node);
        return parser_new_error(parser->last, "expected ';'");
    }

    return node;
}

Node *parse_stmt_var(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_IDENTIFIER))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected identifier");
    }

    Node *node = node_new(NODE_STMT_VAR);
    node->token = token_copy(parser->last);

    Node *node_identifier = parse_identifier(parser);
    if (node_identifier->kind == NODE_ERROR)
    {
        node_free(node);
        return node_identifier;
    }
    node_identifier->parent = node;
    node->data.stmt_var->identifier = node_identifier;

    if (!parser_consume(parser, TOKEN_COLON))
    {
        parser_advance(parser);
        node_free(node);
        return parser_new_error(parser->last, "expected ':'");
    }

    Node *node_type = parse_type(parser);
    if (node_type->kind == NODE_ERROR)
    {
        node_free(node);
        return node_type;
    }
    node_type->parent = node;
    node->data.stmt_var->type = node_type;

    if (parser_consume(parser, TOKEN_EQUAL))
    {
        Node *node_expr = parse_expr(parser);
        if (node_expr->kind == NODE_ERROR)
        {
            node_free(node);
            return node_expr;
        }
        node_expr->parent = node;
        node->data.stmt_var->initializer = node_expr;
    }

    if (!parser_consume(parser, TOKEN_SEMICOLON))
    {
        node_free(node);
        return parser_new_error(parser->last, "expected ';'");
    }

    return node;
}

Node *parse_stmt_def(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_IDENTIFIER))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected identifier");
    }

    Node *node = node_new(NODE_STMT_DEF);
    node->token = token_copy(parser->last);

    Node *node_identifier = parse_identifier(parser);
    if (node_identifier->kind == NODE_ERROR)
    {
        node_free(node);
        return node_identifier;
    }
    node_identifier->parent = node;
    node->data.stmt_def->identifier = node_identifier;

    if (!parser_consume(parser, TOKEN_COLON))
    {
        parser_advance(parser);
        node_free(node);
        return parser_new_error(parser->last, "expected ':'");
    }

    Node *node_type = parse_type(parser);
    if (node_type->kind == NODE_ERROR)
    {
        node_free(node);
        return node_type;
    }
    node_type->parent = node;
    node->data.stmt_def->type = node_type;

    if (!parser_consume(parser, TOKEN_SEMICOLON))
    {
        node_free(node);
        return parser_new_error(parser->last, "expected ';'");
    }

    return node;
}

Node *parse_stmt_mod(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_IDENTIFIER))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected identifier");
    }

    Node *node = node_new(NODE_STMT_MOD);
    node->token = token_copy(parser->last);

    Node *node_module_path = parse_expr(parser);
    if (node_module_path->kind == NODE_ERROR)
    {
        node_free(node);
        node_module_path->data.error->message = "expected module path";
        return node_module_path;
    }

    // validate that module path follows the pattern of chained identifiers
    Node *current = node_module_path;
    while (current->kind == NODE_EXPR_MEMBER)
    {
        if (current->data.expr_member->member->kind != NODE_IDENTIFIER)
        {
            node_free(node);
            return parser_new_error(parser->last, "expected module path");
        }

        current = current->data.expr_member->target;
    }

    node_module_path->parent = node;
    node->data.stmt_mod->module_path = node_module_path;

    if (!parser_consume(parser, TOKEN_SEMICOLON))
    {
        node_free(node);
        return parser_new_error(parser->last, "expected ';'");
    }

    return node;
}

Node *parse_stmt_use(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_IDENTIFIER))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected identifier");
    }

    Node *node = node_new(NODE_STMT_USE);
    node->token = token_copy(parser->last);

    if (parser_peek(parser, TOKEN_COLON))
    {
        Node *node_identifier = parse_identifier(parser);
        if (node_identifier->kind == NODE_ERROR)
        {
            node_free(node);
            return node_identifier;
        }
        node_identifier->parent = node;
        node->data.stmt_use->alias = node_identifier;

        parser_advance(parser);
    }

    Node *node_module_path = parse_expr(parser);
    if (node_module_path->kind == NODE_ERROR)
    {
        node_free(node);
        node_module_path->data.error->message = "expected module path";
        return node_module_path;
    }

    // validate that module path follows the pattern of chained identifiers
    Node *current = node_module_path;
    while (current->kind == NODE_EXPR_MEMBER)
    {
        if (current->data.expr_member->member->kind != NODE_IDENTIFIER)
        {
            node_free(node);
            return parser_new_error(parser->last, "expected module path");
        }

        current = current->data.expr_member->target;
    }

    node_module_path->parent = node;
    node->data.stmt_use->module_path = node_module_path;

    if (!parser_consume(parser, TOKEN_SEMICOLON))
    {
        node_free(node);
        return parser_new_error(parser->last, "expected ';'");
    }

    return node;
}

Node *parse_stmt_str(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_IDENTIFIER))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected identifier");
    }

    Node *node = node_new(NODE_STMT_STR);
    node->token = token_copy(parser->last);

    Node *node_identifier = parse_identifier(parser);
    if (node_identifier->kind == NODE_ERROR)
    {
        node_free(node);
        return node_identifier;
    }
    node_identifier->parent = node;
    node->data.stmt_str->identifier = node_identifier;

    if (!parser_consume(parser, TOKEN_L_BRACE))
    {
        parser_advance(parser);
        node_free(node);
        return parser_new_error(parser->last, "expected '{'");
    }

    node->data.stmt_str->fields = node_list_new();

    while (!parser_match(parser, TOKEN_R_BRACE))
    {
        Node *field = parse_field(parser);
        if (field->kind == NODE_ERROR)
        {
            node_free(node);
            return field;
        }
        field->parent = node;
        node_list_add(&node->data.stmt_str->fields, field);

        if (!parser_consume(parser, TOKEN_SEMICOLON))
        {
            parser_advance(parser);
            node_free(node);
            return parser_new_error(parser->last, "expected ';'");
        }
    }

    if (!parser_consume(parser, TOKEN_R_BRACE))
    {
        parser_advance(parser);
        node_free(node);
        return parser_new_error(parser->last, "expected '}'");
    }

    return node;
}

Node *parse_stmt_uni(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_IDENTIFIER))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected identifier");
    }

    Node *node = node_new(NODE_STMT_UNI);
    node->token = token_copy(parser->last);

    Node *node_identifier = parse_identifier(parser);
    if (node_identifier->kind == NODE_ERROR)
    {
        node_free(node);
        return node_identifier;
    }
    node_identifier->parent = node;
    node->data.stmt_uni->identifier = node_identifier;

    if (!parser_consume(parser, TOKEN_L_BRACE))
    {
        parser_advance(parser);
        node_free(node);
        return parser_new_error(parser->last, "expected '{'");
    }

    node->data.stmt_uni->fields = node_list_new();

    while (!parser_match(parser, TOKEN_R_BRACE))
    {
        Node *field = parse_field(parser);
        if (field->kind == NODE_ERROR)
        {
            node_free(node);
            return field;
        }
        field->parent = node;
        node_list_add(&node->data.stmt_uni->fields, field);

        if (!parser_consume(parser, TOKEN_SEMICOLON))
        {
            parser_advance(parser);
            node_free(node);
            return parser_new_error(parser->last, "expected ';'");
        }
    }

    if (!parser_consume(parser, TOKEN_R_BRACE))
    {
        parser_advance(parser);
        node_free(node);
        return parser_new_error(parser->last, "expected '}'");
    }

    return node;
}

Node *parse_stmt_fun(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_IDENTIFIER))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected identifier");
    }

    Node *node = node_new(NODE_STMT_FUN);
    node->token = token_copy(parser->last);

    Node *node_identifier = parse_identifier(parser);
    if (node_identifier->kind == NODE_ERROR)
    {
        node_free(node);
        return node_identifier;
    }
    node_identifier->parent = node;
    node->data.stmt_fun->identifier = node_identifier;

    if (!parser_consume(parser, TOKEN_L_PAREN))
    {
        parser_advance(parser);
        node_free(node);
        return parser_new_error(parser->last, "expected '('");
    }

    while (!parser_match(parser, TOKEN_R_PAREN))
    {
        Node *parameter = parse_field(parser);
        if (parameter->kind == NODE_ERROR)
        {
            node_free(node);
            return parameter;
        }
        parameter->parent = node;
        node_list_add(&node->data.stmt_fun->parameters, parameter);

        if (!parser_consume(parser, TOKEN_COMMA))
        {
            break;
        }
    }

    if (!parser_consume(parser, TOKEN_R_PAREN))
    {
        parser_advance(parser);
        node_free(node);
        return parser_new_error(parser->last, "expected ')'");
    }

    if (!parser_consume(parser, TOKEN_COLON))
    {
        parser_advance(parser);
        node_free(node);
        return parser_new_error(parser->last, "expected ':'");
    }

    Node *node_type = parse_type(parser);
    if (node_type->kind == NODE_ERROR)
    {
        node_free(node);
        return node_type;
    }
    node_type->parent = node;
    node->data.stmt_fun->return_type = node_type;

    Node *node_body = parse_stmt_block(parser);
    if (node_body->kind == NODE_ERROR)
    {
        node_free(node);
        return node_body;
    }
    node_body->parent = node;
    node->data.stmt_fun->body = node_body;

    return node;
}

Node *parse_stmt_ext(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_IDENTIFIER))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected identifier");
    }

    Node *node = node_new(NODE_STMT_EXT);
    node->token = token_copy(parser->last);

    Node *node_identifier = parse_identifier(parser);
    if (node_identifier->kind == NODE_ERROR)
    {
        node_free(node);
        return node_identifier;
    }
    node_identifier->parent = node;
    node->data.stmt_ext->identifier = node_identifier;

    if (!parser_consume(parser, TOKEN_COLON))
    {
        parser_advance(parser);
        node_free(node);
        return parser_new_error(parser->last, "expected ':'");
    }

    Node *node_type = parse_type(parser);
    if (node_type->kind == NODE_ERROR)
    {
        node_free(node);
        return node_type;
    }
    node_type->parent = node;
    node->data.stmt_ext->type = node_type;

    if (!parser_consume(parser, TOKEN_SEMICOLON))
    {
        node_free(node);
        return parser_new_error(parser->last, "expected ';'");
    }

    return node;
}

Node *parse_stmt_if(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_IDENTIFIER))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected 'if'");
    }

    Node *node = node_new(NODE_STMT_IF);
    node->token = token_copy(parser->last);

    if (!parser_consume(parser, TOKEN_L_PAREN))
    {
        parser_advance(parser);
        node_free(node);
        return parser_new_error(parser->last, "expected '('");
    }

    Node *node_expr = parse_expr(parser);
    if (node_expr->kind == NODE_ERROR)
    {
        node_free(node);
        return node_expr;
    }
    node_expr->parent = node;
    node->data.stmt_if->condition = node_expr;

    if (!parser_consume(parser, TOKEN_R_PAREN))
    {
        parser_advance(parser);
        node_free(node);
        return parser_new_error(parser->last, "expected ')'");
    }

    Node *node_body = parse_stmt_block(parser);
    if (node_body->kind == NODE_ERROR)
    {
        node_free(node);
        return node_body;
    }
    node_body->parent = node;
    node->data.stmt_if->body = node_body;

    return node;
}

Node *parse_stmt_or(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_IDENTIFIER))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected 'or'");
    }

    Node *node = node_new(NODE_STMT_OR);
    node->token = token_copy(parser->last);

    if (parser_consume(parser, TOKEN_L_PAREN))
    {
        Node *node_expr = parse_expr(parser);
        if (node_expr->kind == NODE_ERROR)
        {
            node_free(node);
            return node_expr;
        }
        node_expr->parent = node;
        node->data.stmt_or->condition = node_expr;

        if (!parser_consume(parser, TOKEN_R_PAREN))
        {
            parser_advance(parser);
            node_free(node);
            return parser_new_error(parser->last, "expected ')'");
        }
    }

    Node *node_body = parse_stmt_block(parser);
    if (node_body->kind == NODE_ERROR)
    {
        node_free(node);
        return node_body;
    }
    node_body->parent = node;
    node->data.stmt_or->body = node_body;

    return node;
}

Node *parse_stmt_for(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_IDENTIFIER))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected identifier");
    }

    Node *node = node_new(NODE_STMT_FOR);
    node->token = token_copy(parser->last);

    if (parser_consume(parser, TOKEN_L_PAREN))
    {
        Node *node_expr = parse_expr(parser);
        if (node_expr->kind == NODE_ERROR)
        {
            node_free(node);
            return node_expr;
        }
        node_expr->parent = node;
        node->data.stmt_for->condition = node_expr;

        if (!parser_consume(parser, TOKEN_R_PAREN))
        {
            parser_advance(parser);
            node_free(node);
            return parser_new_error(parser->last, "expected ')'");
        }
    }

    Node *node_body = parse_stmt_block(parser);
    if (node_body->kind == NODE_ERROR)
    {
        node_free(node);
        return node_body;
    }
    node_body->parent = node;
    node->data.stmt_for->body = node_body;

    return node;
}

Node *parse_stmt_break(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_IDENTIFIER))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected 'break'");
    }

    Node *node = node_new(NODE_STMT_BRK);
    node->token = token_copy(parser->last);

    if (!parser_consume(parser, TOKEN_SEMICOLON))
    {
        node_free(node);
        return parser_new_error(parser->last, "expected ';'");
    }

    return node;
}

Node *parse_stmt_continue(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_IDENTIFIER))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected 'continue'");
    }

    Node *node = node_new(NODE_STMT_CNT);
    node->token = token_copy(parser->last);

    if (!parser_consume(parser, TOKEN_SEMICOLON))
    {
        node_free(node);
        return parser_new_error(parser->last, "expected ';'");
    }

    return node;
}

Node *parse_stmt_return(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_IDENTIFIER))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected 'return'");
    }

    Node *node = node_new(NODE_STMT_RET);
    node->token = token_copy(parser->last);

    if (!parser_consume(parser, TOKEN_SEMICOLON))
    {
        Node *node_expr = parse_expr(parser);
        if (node_expr->kind == NODE_ERROR)
        {
            node_free(node);
            return node_expr;
        }
        node_expr->parent = node;
        node->data.stmt_return->value = node_expr;

        if (!parser_consume(parser, TOKEN_SEMICOLON))
        {
            parser_advance(parser);
            node_free(node);
            return parser_new_error(parser->last, "expected ';'");
        }
    }

    return node;
}

Node *parse_stmt_asm(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_IDENTIFIER))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected 'asm'");
    }

    Node *node = node_new(NODE_STMT_ASM);
    node->token = token_copy(parser->last);

    if (!parser_consume(parser, TOKEN_LIT_STRING))
    {
        parser_advance(parser);
        node_free(node);
        return parser_new_error(parser->last, "expected string literal");
    }

    Node *code = node_new(NODE_LIT_STRING);
    code->data.lit_string->value = lexer_raw_value(parser->lexer, parser->last);
    code->token = token_copy(parser->last);

    node->data.stmt_asm->code = code;
    node->data.stmt_asm->code->parent = node;

    if (!parser_consume(parser, TOKEN_SEMICOLON))
    {
        node_free(node);
        return parser_new_error(parser->last, "expected ';'");
    }

    return node;
}

Node *parse_stmt_block(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_L_BRACE))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected '{'");
    }

    Node *node = node_new(NODE_STMT_BLOCK);
    node->token = token_copy(parser->last);
    node->data.stmt_block->statements = node_list_new();

    while (!parser_match(parser, TOKEN_R_BRACE))
    {
        Node *stmt = parse_stmt(parser);
        if (stmt->kind == NODE_ERROR)
        {
            node_free(node);
            return stmt;
        }
        stmt->parent = node;
        node_list_add(&node->data.stmt_block->statements, stmt);
    }

    if (!parser_consume(parser, TOKEN_R_BRACE))
    {
        parser_advance(parser);
        node_free(node);
        return parser_new_error(parser->last, "expected '}'");
    }

    return node;
}

Node *parse_stmt_expr(Parser *parser)
{
    Node *node = node_new(NODE_STMT_EXPR);
    node->token = token_copy(parser->curr);

    Node *expr = parse_expr(parser);
    if (expr->kind == NODE_ERROR)
    {
        node_free(node);
        return expr;
    }
    expr->parent = node;
    node->data.stmt_expr->expression = expr;

    if (!parser_consume(parser, TOKEN_SEMICOLON))
    {
        node_free(node);
        return parser_new_error(parser->last, "expected ';'");
    }

    return node;
}

Node *parse_stmt(Parser *parser)
{
    if (parser->curr->kind == TOKEN_IDENTIFIER)
    {
        char *raw = lexer_raw_value(parser->lexer, parser->curr);

        if (strcmp(raw, "val") == 0)
        {
            free(raw);
            return parse_stmt_val(parser);
        }
        else if (strcmp(raw, "var") == 0)
        {
            free(raw);
            return parse_stmt_var(parser);
        }
        else if (strcmp(raw, "def") == 0)
        {
            free(raw);
            return parse_stmt_def(parser);
        }
        else if (strcmp(raw, "mod") == 0)
        {
            free(raw);
            return parse_stmt_mod(parser);
        }
        else if (strcmp(raw, "use") == 0)
        {
            free(raw);
            return parse_stmt_use(parser);
        }
        else if (strcmp(raw, "str") == 0)
        {
            free(raw);
            return parse_stmt_str(parser);
        }
        else if (strcmp(raw, "uni") == 0)
        {
            free(raw);
            return parse_stmt_uni(parser);
        }
        else if (strcmp(raw, "fun") == 0)
        {
            free(raw);
            return parse_stmt_fun(parser);
        }
        else if (strcmp(raw, "ext") == 0)
        {
            free(raw);
            return parse_stmt_ext(parser);
        }
        else if (strcmp(raw, "if") == 0)
        {
            free(raw);
            return parse_stmt_if(parser);
        }
        else if (strcmp(raw, "or") == 0)
        {
            free(raw);
            return parse_stmt_or(parser);
        }
        else if (strcmp(raw, "for") == 0)
        {
            free(raw);
            return parse_stmt_for(parser);
        }
        else if (strcmp(raw, "brk") == 0)
        {
            free(raw);
            return parse_stmt_break(parser);
        }
        else if (strcmp(raw, "cnt") == 0)
        {
            free(raw);
            return parse_stmt_continue(parser);
        }
        else if (strcmp(raw, "ret") == 0)
        {
            free(raw);
            return parse_stmt_return(parser);
        }
        else if (strcmp(raw, "asm") == 0)
        {
            free(raw);
            return parse_stmt_asm(parser);
        }
    }

    return parse_stmt_expr(parser);
}

Node *parser_parse(Parser *parser, char *path)
{
    parser_advance(parser);

    return parse_file(parser, path);
}
