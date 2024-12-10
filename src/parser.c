#include "parser.h"
#include "lexer.h"
#include "operator.h"

#include <stdio.h>

Parser *parser_new(Lexer *lexer)
{
    Parser *parser = calloc(1, sizeof(Parser));
    parser->comments = calloc(1, sizeof(Token *));
    parser->comments[0] = NULL;

    parser->lexer = lexer;

    return parser;
}

void parser_free(Parser *parser)
{
    while (parser->comments != NULL && *parser->comments != NULL)
    {
        free(*parser->comments);
        parser->comments++;
    }
    parser->comments = NULL;

    parser->last = NULL;
    parser->curr = NULL;
    parser->next = NULL;

    parser->lexer = NULL;

    node_free(parser->root);
    parser->root = NULL;

    free(parser);
}

void parser_print_error(Parser *parser, Node *node, char *path)
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

    printf("error: %s\n", node->data.error->message);
    printf("%s:%d:%d\n", path, line_num + 1, offset);
    printf("%5d | %s\n", line_num + 1, line);
    printf("      | %*s^\n", offset - 1, "");

    free(line);
}

void parser_add_comment(Parser *parser, Token *token)
{
    size_t count = 0;
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

    parser->last = parser->curr;
    parser->curr = parser->next;
    parser->next = next;

    if (parser->curr == NULL && parser->next != NULL)
    {
        parser_advance(parser);
    }
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

Node *parse_program(Parser *parser)
{
    Token *token = token_copy(parser->curr);

    Node **statements = node_list_new();

    while (!parser_match(parser, TOKEN_EOF))
    {
        Node *stmt = parse_stmt(parser);
        node_list_add(&statements, stmt);
    }

    Node *node = node_new(NODE_PROGRAM);
    node->token = token;

    if (statements != NULL)
    {
        for (size_t i = 0; statements[i] != NULL; i++)
        {
            statements[i]->parent = node;
        }
    }

    node->data.program->statements = statements;

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
    node->token = parser->last;
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
    node->token = parser->last;
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
    node->token = parser->last;
    node->data.lit_float->value = lexer_eval_lit_float(parser->lexer, parser->last);

    return node;
}

Node *parse_lit_char(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_LIT_CHAR))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected char literal");
    }

    Node *node = node_new(NODE_LIT_CHAR);
    node->token = parser->last;
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
    node->token = parser->last;
    node->data.lit_string->value = lexer_eval_lit_string(parser->lexer, parser->last);

    return node;
}

Node *parse_expr_member(Parser *parser, Node *target)
{
    if (!parser_consume(parser, TOKEN_DOT))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected '.'");
    }

    Token *token = token_copy(parser->last);

    Node *identifier = parse_identifier(parser);

    Node *node = node_new(NODE_EXPR_MEMBER);
    node->token = token;

    target->parent = node;
    identifier->parent = node;

    node->data.expr_member->target = target;
    node->data.expr_member->member = identifier;

    return node;
}

Node *parse_expr_call(Parser *parser, Node *target)
{
    if (!parser_consume(parser, TOKEN_L_PAREN))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected '('");
    }

    Token *token = token_copy(parser->last);

    Node **arguments = node_list_new();

    while (!parser_match(parser, TOKEN_R_PAREN))
    {
        Node *argument = parse_expr(parser);
        node_list_add(&arguments, argument);

        if (!parser_consume(parser, TOKEN_COMMA))
        {
            break;
        }
    }

    if (!parser_consume(parser, TOKEN_R_PAREN))
    {
        parser_advance(parser);

        Node *error = parser_new_error(parser->last, "expected ')'");
        node_list_free(arguments);
        return error;
    }

    Node *node = node_new(NODE_EXPR_CALL);
    node->token = token;

    if (arguments != NULL)
    {
        for (size_t i = 0; arguments[i] != NULL; i++)
        {
            arguments[i]->parent = node;
        }
    }

    target->parent = node;

    node->data.expr_call->arguments = arguments;
    node->data.expr_call->target = target;

    return node;
}

Node *parse_expr_index(Parser *parser, Node *target)
{
    if (!parser_consume(parser, TOKEN_L_BRACKET))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected '['");
    }

    Token *token = token_copy(parser->last);

    Node *index = parse_expr(parser);

    if (!parser_consume(parser, TOKEN_R_BRACKET))
    {
        parser_advance(parser);

        Node *error = parser_new_error(parser->last, "expected ']'");
        node_free(index);
        return error;
    }

    Node *node = node_new(NODE_EXPR_INDEX);
    node->token = token;

    index->parent = node;
    target->parent = node;

    node->data.expr_index->target = target;
    node->data.expr_index->index = index;

    return node;
}

Node *parse_expr_cast(Parser *parser, Node *target)
{
    if (!parser_consume(parser, TOKEN_COLON_COLON))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected '::'");
    }

    Token *token = token_copy(parser->last);

    Node *type = parse_type(parser);

    Node *node = node_new(NODE_EXPR_CAST);
    node->token = token;

    type->parent = node;
    target->parent = node;

    node->data.expr_cast->target = target;
    node->data.expr_cast->type = type;

    return node;
}

// Node *parse_expr_new(Parser *parser)
// {
//     if (!parser_consume(parser, TOKEN_L_BRACE))
//     {
//         parser_advance(parser);
//         return parser_new_error(parser->last, "expected '{'");
//     }

//     Token *token = token_copy(parser->last);

//     Node **initializers = node_list_new();

//     while (!parser_match(parser, TOKEN_R_BRACE))
//     {
//         Node *initializer = parse_expr(parser);
//         node_list_add(&initializers, initializer);

//         if (!parser_consume(parser, TOKEN_SEMICOLON))
//         {
//             break;
//         }
//     }

//     if (!parser_consume(parser, TOKEN_R_BRACE))
//     {
//         parser_advance(parser);

//         Node *error = parser_new_error(parser->last, "expected '}'");
//         node_list_free(initializers);
//         return error;
//     }

//     Node *node = node_new(NODE_EXPR_NEW);
//     node->token = token;

//     if (initializers != NULL)
//     {
//         for (size_t i = 0; initializers[i] != NULL; i++)
//         {
//             initializers[i]->parent = node;
//         }
//     }

//     node->data.expr_new->initializers = initializers;

//     return node;
// }

Node *parse_expr_unary(Parser *parser)
{
    if (op_is_unary(op_from_token_kind(parser->curr->kind)))
    {
        Operator op = op_from_token_kind(parser->curr->kind);
        parser_advance(parser);

        Node *node = node_new(NODE_EXPR_UNARY);
        node->token = parser->last;
        node->data.expr_unary->op = op;
        node->data.expr_unary->target = parse_expr_unary(parser);

        return node;
    }

    return parse_expr_primary(parser);
}

Node *parse_expr_binary(Parser *parser, int parent_precedence)
{
    Node *left = parse_expr_postfix(parser);

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

        Node *node = node_new(NODE_EXPR_BINARY);
        node->token = parser->last;

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
    // case TOKEN_L_BRACE:
    //     return parse_expr_new(parser);
    default:
        return parser_new_error(parser->curr, "expected primary expression");
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

    Token *token = token_copy(parser->last);

    Node *size = NULL;

    if (!parser_match(parser, TOKEN_R_BRACKET))
    {
        size = parse_expr(parser);
    }

    if (!parser_consume(parser, TOKEN_R_BRACKET))
    {
        parser_advance(parser);

        Node *error = parser_new_error(parser->last, "expected ']'");
        node_free(size);
        return error;
    }

    Node *node = node_new(NODE_TYPE_ARRAY);
    node->token = token;

    if (size != NULL)
    {
        size->parent = node;
    }

    node->data.type_array->type = parse_type(parser);

    return node;
}

Node *parse_type_pointer(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_STAR))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected '*'");
    }

    Token *token = token_copy(parser->last);

    Node *type = parse_type(parser);

    Node *node = node_new(NODE_TYPE_POINTER);
    node->token = token;

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

    Token *token = token_copy(parser->last);

    if (!parser_consume(parser, TOKEN_L_PAREN))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected '('");
    }

    Node **parameters = node_list_new();

    while (!parser_match(parser, TOKEN_R_PAREN))
    {
        Node *parameter = parse_field(parser);
        node_list_add(&parameters, parameter);

        if (!parser_consume(parser, TOKEN_COMMA))
        {
            break;
        }
    }

    if (!parser_consume(parser, TOKEN_R_PAREN))
    {
        parser_advance(parser);

        Node *error = parser_new_error(parser->last, "expected ')'");
        node_list_free(parameters);
        return error;
    }

    Node *return_type = NULL;

    if (parser_consume(parser, TOKEN_COLON))
    {
        return_type = parse_type(parser);
    }

    Node *node = node_new(NODE_TYPE_FUN);
    node->token = token;

    if (parameters != NULL)
    {
        for (size_t i = 0; parameters[i] != NULL; i++)
        {
            parameters[i]->parent = node;
        }
    }

    if (return_type != NULL)
    {
        return_type->parent = node;
    }

    node->data.type_function->parameters = parameters;
    node->data.type_function->return_type = return_type;

    return node;
}

Node *parse_type_struct(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_IDENTIFIER))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected identifier");
    }

    Token *token = token_copy(parser->last);

    if (!parser_consume(parser, TOKEN_L_BRACE))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected '{'");
    }

    Node **fields = node_list_new();

    while (!parser_match(parser, TOKEN_R_BRACE))
    {
        Node *field = parse_field(parser);
        node_list_add(&fields, field);

        if (!parser_consume(parser, TOKEN_SEMICOLON))
        {
            break;
        }
    }

    if (!parser_consume(parser, TOKEN_R_BRACE))
    {
        parser_advance(parser);

        Node *error = parser_new_error(parser->last, "expected '}'");
        node_list_free(fields);
        return error;
    }

    Node *node = node_new(NODE_TYPE_STR);
    node->token = token;

    if (fields != NULL)
    {
        for (size_t i = 0; fields[i] != NULL; i++)
        {
            fields[i]->parent = node;
        }
    }

    node->data.type_struct->fields = fields;

    return node;
}

Node *parse_type_union(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_IDENTIFIER))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected identifier");
    }

    Token *token = token_copy(parser->last);

    if (!parser_consume(parser, TOKEN_L_BRACE))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected '{'");
    }

    Node **fields = node_list_new();

    while (!parser_match(parser, TOKEN_R_BRACE))
    {
        Node *field = parse_field(parser);
        node_list_add(&fields, field);

        if (!parser_consume(parser, TOKEN_SEMICOLON))
        {
            break;
        }
    }

    if (!parser_consume(parser, TOKEN_R_BRACE))
    {
        parser_advance(parser);

        Node *error = parser_new_error(parser->last, "expected '}'");
        node_list_free(fields);
        return error;
    }

    Node *node = node_new(NODE_TYPE_UNI);
    node->token = token;

    if (fields != NULL)
    {
        for (size_t i = 0; fields[i] != NULL; i++)
        {
            fields[i]->parent = node;
        }
    }

    node->data.type_union->fields = fields;

    return node;
}

Node *parse_type(Parser *parser)
{
    switch (parser->curr->kind)
    {
    case TOKEN_IDENTIFIER:
        char *value = lexer_raw_value(parser->lexer, parser->curr);

        if (strcmp(value, "fun") == 0)
            return parse_type_function(parser);
        if (strcmp(value, "str") == 0)
            return parse_type_struct(parser);
        if (strcmp(value, "uni") == 0)
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
    Token *token = token_copy(parser->curr);

    Node *identifier = parse_identifier(parser);

    if (!parser_consume(parser, TOKEN_COLON))
    {
        parser_advance(parser);

        Node *error = parser_new_error(parser->last, "expected ':'");
        node_free(identifier);
        return error;
    }

    Node *type = parse_type(parser);

    Node *node = node_new(NODE_FIELD);
    node->token = token;

    identifier->parent = node;
    type->parent = node;

    node->data.field->identifier = identifier;
    node->data.field->type = type;

    return node;
}

Node *parse_stmt_val(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_IDENTIFIER))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected identifier");
    }

    Token *token = token_copy(parser->last);

    Node *identifier = parse_identifier(parser);

    if (!parser_consume(parser, TOKEN_COLON))
    {
        parser_advance(parser);

        Node *error = parser_new_error(parser->last, "expected ':'");
        node_free(identifier);
        return error;
    }

    Node *type = parse_type(parser);

    if (!parser_consume(parser, TOKEN_EQUAL))
    {
        parser_advance(parser);

        Node *error = parser_new_error(parser->last, "expected '='");
        node_free(identifier);
        node_free(type);
        return error;
    }

    Node *initializer = parse_expr(parser);

    if (!parser_consume(parser, TOKEN_SEMICOLON))
    {
        parser_advance(parser);

        Node *error = parser_new_error(parser->last, "expected ';'");
        node_free(identifier);
        node_free(type);
        node_free(initializer);
        return error;
    }

    Node *node = node_new(NODE_STMT_VAL);
    identifier->parent = node;
    type->parent = node;
    initializer->parent = node;

    node->token = token;
    node->data.stmt_val->identifier = identifier;
    node->data.stmt_val->type = type;
    node->data.stmt_val->initializer = initializer;

    return node;
}

Node *parse_stmt_var(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_IDENTIFIER))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected identifier");
    }

    Token *token = token_copy(parser->last);

    Node *identifier = parse_identifier(parser);

    if (!parser_consume(parser, TOKEN_COLON))
    {
        parser_advance(parser);

        Node *error = parser_new_error(parser->last, "expected ':'");
        node_free(identifier);
        return error;
    }

    Node *type = parse_type(parser);

    Node *initializer = NULL;
    if (parser_consume(parser, TOKEN_EQUAL))
    {
        initializer = parse_expr(parser);
    }

    if (!parser_consume(parser, TOKEN_SEMICOLON))
    {
        parser_advance(parser);

        Node *error = parser_new_error(parser->last, "expected ';'");
        node_free(identifier);
        node_free(type);
        return error;
    }

    Node *node = node_new(NODE_STMT_VAR);
    identifier->parent = node;
    type->parent = node;

    if (initializer != NULL)
    {
        initializer->parent = node;
    }

    node->token = token;
    node->data.stmt_var->identifier = identifier;
    node->data.stmt_var->type = type;
    node->data.stmt_var->initializer = initializer;

    return node;
}

Node *parse_stmt_def(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_IDENTIFIER))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected identifier");
    }

    Token *token = token_copy(parser->last);

    Node *identifier = parse_identifier(parser);

    if (!parser_consume(parser, TOKEN_COLON))
    {
        parser_advance(parser);

        Node *error = parser_new_error(parser->last, "expected ':'");
        node_free(identifier);
        return error;
    }

    Node *type = parse_type(parser);

    if (!parser_consume(parser, TOKEN_SEMICOLON))
    {
        parser_advance(parser);

        Node *error = parser_new_error(parser->last, "expected ';'");
        node_free(identifier);
        node_free(type);
        return error;
    }

    Node *node = node_new(NODE_STMT_DEF);
    identifier->parent = node;
    type->parent = node;

    node->token = token;
    node->data.stmt_def->identifier = identifier;
    node->data.stmt_def->type = type;

    return node;
}

Node *parse_stmt_use(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_IDENTIFIER))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected identifier");
    }

    Token *token = token_copy(parser->last);

    Node **module_parts = node_list_new();
    Node *alias = NULL;

    Node *module_part = parse_identifier(parser);
    node_list_add(&module_parts, module_part);

    if (parser_consume(parser, TOKEN_COLON))
    {
        // switch out the module part for the alias
        alias = module_part;
        module_parts = NULL;

        // prime the module parts list with the first part
        module_part = parse_identifier(parser);
        node_list_add(&module_parts, module_part);
    }

    while (parser_consume(parser, TOKEN_DOT))
    {
        module_part = parse_identifier(parser);
        node_list_add(&module_parts, module_part);
    }

    if (!parser_consume(parser, TOKEN_SEMICOLON))
    {
        parser_advance(parser);

        Node *error = parser_new_error(parser->last, "expected ';'");
        node_list_free(module_parts);
        node_free(alias);
        node_free(module_part);
        return error;
    }

    Node *node = node_new(NODE_STMT_USE);
    node->token = token;

    if (alias != NULL)
    {
        alias->parent = node;
    }

    if (module_parts != NULL)
    {
        for (size_t i = 0; module_parts[i] != NULL; i++)
        {
            module_parts[i]->parent = node;
        }
    }

    node->data.stmt_use->alias = alias;
    node->data.stmt_use->module_parts = module_parts;

    return node;
}

Node *parse_stmt_fun(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_IDENTIFIER))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected identifier");
    }

    Token *token = token_copy(parser->last);

    Node *identifier = parse_identifier(parser);

    if (!parser_consume(parser, TOKEN_L_PAREN))
    {
        parser_advance(parser);

        Node *error = parser_new_error(parser->last, "expected '('");
        node_free(identifier);
        return error;
    }

    Node **parameters = node_list_new();

    while (!parser_match(parser, TOKEN_R_PAREN))
    {
        Node *parameter = parse_field(parser);
        node_list_add(&parameters, parameter);

        if (!parser_consume(parser, TOKEN_COMMA))
        {
            break;
        }
    }

    if (!parser_consume(parser, TOKEN_R_PAREN))
    {
        parser_advance(parser);

        Node *error = parser_new_error(parser->last, "expected ')'");
        node_free(identifier);
        node_list_free(parameters);
        return error;
    }

    if (!parser_consume(parser, TOKEN_COLON))
    {
        parser_advance(parser);

        Node *error = parser_new_error(parser->last, "expected ':'");
        node_free(identifier);
        node_list_free(parameters);
        return error;
    }

    Node *return_type = parse_type(parser);

    Node *body = parse_stmt_block(parser);

    Node *node = node_new(NODE_STMT_FUN);
    node->token = token;

    identifier->parent = node;
    return_type->parent = node;

    if (parameters != NULL)
    {
        for (size_t i = 0; parameters[i] != NULL; i++)
        {
            parameters[i]->parent = node;
        }
    }

    body->parent = node;

    node->data.stmt_fun->identifier = identifier;
    node->data.stmt_fun->parameters = parameters;
    node->data.stmt_fun->return_type = return_type;
    node->data.stmt_fun->body = body;

    return node;
}

Node *parse_stmt_ext(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_IDENTIFIER))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected identifier");
    }

    Token *token = token_copy(parser->last);

    Node *identifier = parse_identifier(parser);

    if (!parser_consume(parser, TOKEN_COLON))
    {
        parser_advance(parser);

        Node *error = parser_new_error(parser->last, "expected ':'");
        node_free(identifier);
        return error;
    }

    Node *type = parse_type(parser);

    if (!parser_consume(parser, TOKEN_SEMICOLON))
    {
        parser_advance(parser);

        Node *error = parser_new_error(parser->last, "expected ';'");
        node_free(identifier);
        node_free(type);
        return error;
    }

    Node *node = node_new(NODE_STMT_EXT);
    node->token = token;

    identifier->parent = node;
    type->parent = node;

    node->data.stmt_ext->identifier = identifier;
    node->data.stmt_ext->type = type;

    return node;
}

Node *parse_stmt_if(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_IDENTIFIER))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected identifier");
    }

    Token *token = token_copy(parser->last);

    if (!parser_consume(parser, TOKEN_L_PAREN))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected '('");
    }

    Node *condition = parse_expr(parser);

    if (!parser_consume(parser, TOKEN_R_PAREN))
    {
        parser_advance(parser);

        Node *error = parser_new_error(parser->last, "expected ')'");
        node_free(condition);
        return error;
    }

    Node *body = parse_stmt_block(parser);

    Node *node = node_new(NODE_STMT_IF);
    node->token = token;

    condition->parent = node;
    body->parent = node;

    node->data.stmt_if->condition = condition;
    node->data.stmt_if->body = body;

    return node;
}

Node *parse_stmt_or(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_IDENTIFIER))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected identifier");
    }

    Token *token = token_copy(parser->last);

    Node *condition = NULL;

    if (parser_consume(parser, TOKEN_L_PAREN))
    {
        condition = parse_expr(parser);

        if (!parser_consume(parser, TOKEN_R_PAREN))
        {
            parser_advance(parser);

            Node *error = parser_new_error(parser->last, "expected ')'");
            node_free(condition);
            return error;
        }
    }

    Node *body = parse_stmt_block(parser);

    Node *node = node_new(NODE_STMT_OR);
    node->token = token;

    if (condition != NULL)
    {
        condition->parent = node;
    }

    body->parent = node;

    node->data.stmt_or->condition = condition;
    node->data.stmt_or->body = body;

    return node;
}

Node *parse_stmt_for(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_IDENTIFIER))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected identifier");
    }

    Token *token = token_copy(parser->last);

    Node *condition = NULL;
    if (parser_consume(parser, TOKEN_L_PAREN))
    {
        condition = parse_expr(parser);

        if (!parser_consume(parser, TOKEN_R_PAREN))
        {
            parser_advance(parser);

            Node *error = parser_new_error(parser->last, "expected ')'");
            node_free(condition);
            return error;
        }
    }

    Node *body = parse_stmt_block(parser);

    Node *node = node_new(NODE_STMT_FOR);
    node->token = token;

    if (condition != NULL)
    {
        condition->parent = node;
    }

    body->parent = node;

    node->data.stmt_for->condition = condition;
    node->data.stmt_for->body = body;

    return node;
}

Node *parse_stmt_break(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_IDENTIFIER))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected identifier");
    }

    Token *token = token_copy(parser->last);

    if (!parser_consume(parser, TOKEN_SEMICOLON))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected ';'");
    }

    Node *node = node_new(NODE_STMT_BRK);
    node->token = token;

    return node;
}

Node *parse_stmt_continue(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_IDENTIFIER))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected identifier");
    }

    Token *token = token_copy(parser->last);

    if (!parser_consume(parser, TOKEN_SEMICOLON))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected ';'");
    }

    Node *node = node_new(NODE_STMT_CNT);
    node->token = token;

    return node;
}

Node *parse_stmt_return(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_IDENTIFIER))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected 'ret'");
    }

    Token *token = token_copy(parser->last);

    Node *value = NULL;
    if (!parser_consume(parser, TOKEN_SEMICOLON))
    {
        value = parse_expr(parser);

        if (!parser_consume(parser, TOKEN_SEMICOLON))
        {
            parser_advance(parser);

            Node *error = parser_new_error(parser->last, "expected ';'");
            node_free(value);
            return error;
        }
    }

    Node *node = node_new(NODE_STMT_RET);
    node->token = token;

    if (value != NULL)
    {
        value->parent = node;
    }

    node->data.stmt_return->value = value;

    return node;
}

Node *parse_stmt_asm(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_IDENTIFIER))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected 'asm'");
    }

    Token *token = token_copy(parser->last);

    if (!parser_consume(parser, TOKEN_LIT_STRING))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected string literal");
    }

    Node *code = node_new(NODE_LIT_STRING);
    code->data.lit_string->value = lexer_raw_value(parser->lexer, parser->last);

    if (!parser_consume(parser, TOKEN_SEMICOLON))
    {
        parser_advance(parser);

        Node *error = parser_new_error(parser->last, "expected ';'");
        node_free(code);
        return error;
    }

    Node *node = node_new(NODE_STMT_ASM);
    node->token = token;

    code->parent = node;

    node->data.stmt_asm->code = code;

    return node;
}

Node *parse_stmt_block(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_L_BRACE))
    {
        parser_advance(parser);
        return parser_new_error(parser->last, "expected '{'");
    }

    Token *token = token_copy(parser->last);

    Node **statements = node_list_new();

    while (!parser_match(parser, TOKEN_R_BRACE))
    {
        Node *stmt = parse_stmt(parser);
        node_list_add(&statements, stmt);
    }

    if (!parser_consume(parser, TOKEN_R_BRACE))
    {
        parser_advance(parser);

        Node *error = parser_new_error(parser->last, "expected '}'");
        node_list_free(statements);
        return error;
    }

    Node *node = node_new(NODE_STMT_BLOCK);
    node->token = token;

    for (size_t i = 0; statements[i] != NULL; i++)
    {
        statements[i]->parent = node;
    }

    node->data.stmt_block->statements = statements;

    return node;
}

Node *parse_stmt_expr(Parser *parser)
{
    Token *token = token_copy(parser->curr);

    Node *expr = parse_expr(parser);

    if (!parser_consume(parser, TOKEN_SEMICOLON))
    {
        parser_advance(parser);

        Node *error = parser_new_error(parser->last, "expected ';'");
        node_free(expr);
        return error;
    }

    Node *node = node_new(NODE_STMT_EXPR);
    node->token = token;

    expr->parent = node;

    node->data.stmt_expr->expression = expr;

    return node;
}

Node *parse_stmt(Parser *parser)
{
    if (parser->curr->kind == TOKEN_IDENTIFIER)
    {
        char *raw = lexer_raw_value(parser->lexer, parser->curr);

        if (strcmp(raw, "val") == 0)
            return parse_stmt_val(parser);
        else if (strcmp(raw, "var") == 0)
            return parse_stmt_var(parser);
        else if (strcmp(raw, "def") == 0)
            return parse_stmt_def(parser);
        else if (strcmp(raw, "use") == 0)
            return parse_stmt_use(parser);
        else if (strcmp(raw, "fun") == 0)
            return parse_stmt_fun(parser);
        else if (strcmp(raw, "ext") == 0)
            return parse_stmt_ext(parser);
        else if (strcmp(raw, "if") == 0)
            return parse_stmt_if(parser);
        else if (strcmp(raw, "or") == 0)
            return parse_stmt_or(parser);
        else if (strcmp(raw, "for") == 0)
            return parse_stmt_for(parser);
        else if (strcmp(raw, "brk") == 0)
            return parse_stmt_break(parser);
        else if (strcmp(raw, "cnt") == 0)
            return parse_stmt_continue(parser);
        else if (strcmp(raw, "ret") == 0)
            return parse_stmt_return(parser);
        else if (strcmp(raw, "asm") == 0)
            return parse_stmt_asm(parser);
    }

    return parse_stmt_expr(parser);
}

Node *parser_parse(Parser *parser)
{
    parser_advance(parser);

    Node *node = parse_program(parser);

    parser->root = node;

    return parser->root;
}
