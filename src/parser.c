#include "parser.h"
#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// parser lifecycle
void parser_init(Parser *parser, Lexer *lexer)
{
    parser->lexer      = lexer;
    parser->current    = NULL;
    parser->previous   = NULL;
    parser->panic_mode = false;
    parser->had_error  = false;
    parser_error_list_init(&parser->errors);

    // prime the parser
    parser_advance(parser);
}

void parser_dnit(Parser *parser)
{
    if (parser->current)
    {
        token_dnit(parser->current);
        free(parser->current);
    }
    if (parser->previous)
    {
        token_dnit(parser->previous);
        free(parser->previous);
    }
    parser_error_list_dnit(&parser->errors);
}

// error list operations
void parser_error_list_init(ParserErrorList *list)
{
    list->errors   = NULL;
    list->count    = 0;
    list->capacity = 0;
}

void parser_error_list_dnit(ParserErrorList *list)
{
    for (int i = 0; i < list->count; i++)
    {
        free(list->errors[i].message);
        token_dnit(list->errors[i].token);
        free(list->errors[i].token);
    }
    free(list->errors);
}

void parser_error_list_add(ParserErrorList *list, Token *token, const char *message)
{
    if (list->count >= list->capacity)
    {
        list->capacity = list->capacity ? list->capacity * 2 : 8;
        list->errors   = realloc(list->errors, sizeof(ParserError) * list->capacity);
    }

    Token *error_token = malloc(sizeof(Token));
    if (error_token == NULL)
    {
        fprintf(stderr, "error: memory allocation failed for error token\n");
        exit(EXIT_FAILURE);
    }

    token_copy(token, error_token);

    list->errors[list->count].token   = error_token;
    list->errors[list->count].message = strdup(message);
    if (list->errors[list->count].message == NULL)
    {
        fprintf(stderr, "error: memory allocation failed for error message\n");
        exit(EXIT_FAILURE);
    }

    list->count++;
}

void parser_error_list_print(ParserErrorList *list, Lexer *lexer, const char *file_path)
{
    for (int i = 0; i < list->count; i++)
    {
        Token *token     = list->errors[i].token;
        int    line      = lexer_get_pos_line(lexer, token->pos);
        int    col       = lexer_get_pos_line_offset(lexer, token->pos);
        char  *line_text = lexer_get_line_text(lexer, line);
        if (line_text == NULL)
        {
            line_text = strdup("unable to retrieve line text");
        }

        printf("error: %s\n", list->errors[i].message);
        printf("%s:%d:%d\n", file_path, line + 1, col);
        printf("%5d | %-*s\n", line + 1, col > 1 ? col - 1 : 0, line_text);
        printf("      | %*s^\n", col - 1, "");

        free(line_text);
    }
}

// helper functions
void parser_advance(Parser *parser)
{
    // free tokens as they pass through. nodes will copy them as needed.
    if (parser->previous != NULL)
    {
        token_dnit(parser->previous);
        free(parser->previous);
    }

    parser->previous = parser->current;

    for (;;)
    {
        parser->current = lexer_next(parser->lexer);

        // skip comments
        if (parser->current->kind == TOKEN_COMMENT)
        {
            token_dnit(parser->current);
            free(parser->current);
            continue;
        }

        // if it's not an error token, we're done
        if (parser->current->kind != TOKEN_ERROR)
        {
            break;
        }

        parser_error_at_current(parser, "unexpected character");
        token_dnit(parser->current);
        free(parser->current);
    }
}

bool parser_check(Parser *parser, TokenKind kind)
{
    return parser->current->kind == kind;
}

bool parser_match(Parser *parser, TokenKind kind)
{
    if (!parser_check(parser, kind))
    {
        return false;
    }

    parser_advance(parser);
    return true;
}

bool parser_consume(Parser *parser, TokenKind kind, const char *message)
{
    if (parser->current->kind == kind)
    {
        parser_advance(parser);
        return true;
    }

    parser_error(parser, parser->previous, message);
    return false;
}

void parser_synchronize(Parser *parser)
{
    if (!parser->panic_mode)
    {
        return;
    }

    parser->panic_mode = false;

    while (!parser_is_at_end(parser))
    {
        // check current token for semicolon (statement terminator)
        if (parser->current->kind == TOKEN_SEMICOLON)
        {
            parser_advance(parser);
            return;
        }

        switch (parser->current->kind)
        {
        case TOKEN_KW_USE:
        case TOKEN_KW_EXT:
        case TOKEN_KW_DEF:
        case TOKEN_KW_STR:
        case TOKEN_KW_UNI:
        case TOKEN_KW_VAL:
        case TOKEN_KW_VAR:
        case TOKEN_KW_FUN:
            return;
        default:
            parser_advance(parser);
            break;
        }
    }
}

bool parser_is_at_end(Parser *parser)
{
    return parser->current->kind == TOKEN_EOF;
}

// error handling
void parser_error(Parser *parser, Token *token, const char *message)
{
    if (parser->panic_mode)
    {
        return;
    }

    parser->panic_mode = true;
    parser->had_error  = true;
    parser_error_list_add(&parser->errors, token, message);
}

void parser_error_at_current(Parser *parser, const char *message)
{
    parser_error(parser, parser->current, message);
}

void parser_error_at_previous(Parser *parser, const char *message)
{
    parser_error(parser, parser->previous, message);
}

char *parser_parse_identifier(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_IDENTIFIER, "expected identifier"))
    {
        return NULL;
    }

    return lexer_raw_value(parser->lexer, parser->previous);
}

// main parsing function
AstNode *parser_parse_program(Parser *parser)
{
    AstNode *program = malloc(sizeof(AstNode));
    if (program == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for program node");
        return NULL;
    }
    ast_node_init(program, AST_PROGRAM);

    program->program.stmts = malloc(sizeof(AstList));
    if (program->program.stmts == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for program statements list");
        ast_node_dnit(program);
        free(program);
        return NULL;
    }
    ast_list_init(program->program.stmts);

    while (!parser_is_at_end(parser))
    {
        AstNode *stmt = parser_parse_stmt_top(parser);
        if (stmt != NULL)
        {
            ast_list_append(program->program.stmts, stmt);
        }
    }

    return program;
}

// parse top-level statements
AstNode *parser_parse_stmt_top(Parser *parser)
{
    AstNode *result = NULL;

    switch (parser->current->kind)
    {
    case TOKEN_KW_USE:
        result = parser_parse_stmt_use(parser);
        break;
    case TOKEN_KW_EXT:
        result = parser_parse_stmt_ext(parser);
        break;
    case TOKEN_KW_DEF:
        result = parser_parse_stmt_def(parser);
        break;
    case TOKEN_KW_VAL:
        result = parser_parse_stmt_val(parser);
        break;
    case TOKEN_KW_VAR:
        result = parser_parse_stmt_var(parser);
        break;
    case TOKEN_KW_FUN:
        result = parser_parse_stmt_fun(parser);
        break;
    case TOKEN_KW_STR:
        result = parser_parse_stmt_str(parser);
        break;
    case TOKEN_KW_UNI:
        result = parser_parse_stmt_uni(parser);
        break;
    default:
        parser_error_at_current(parser, "expected statement");
        result = NULL;
        break;
    }

    // synchronize after each statement
    // NOTE: if not in panic mode, this does nothing. This is here to ensure
    // recovery is smooth between statements in the event that a statement
    // was unable to properly synchronize internally.
    parser_synchronize(parser);

    return result;
}

// parse statements allowed at the function level
AstNode *parser_parse_stmt(Parser *parser)
{
    switch (parser->current->kind)
    {
    case TOKEN_KW_VAL:
        return parser_parse_stmt_val(parser);
    case TOKEN_KW_VAR:
        return parser_parse_stmt_var(parser);
    case TOKEN_KW_IF:
        return parser_parse_stmt_if(parser);
    case TOKEN_KW_FOR:
        return parser_parse_stmt_for(parser);
    case TOKEN_KW_BRK:
        return parser_parse_stmt_brk(parser);
    case TOKEN_KW_CNT:
        return parser_parse_stmt_cnt(parser);
    case TOKEN_KW_RET:
        return parser_parse_stmt_ret(parser);
    case TOKEN_L_BRACE:
        return parser_parse_stmt_block(parser);
    default:
        return parser_parse_stmt_expr(parser);
    }
}

AstNode *parser_parse_stmt_use(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_KW_USE, "expected 'use' keyword"))
    {
        return NULL;
    }

    AstNode *node = malloc(sizeof(AstNode));
    if (node == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for use statement");
        return NULL;
    }
    ast_node_init(node, AST_STMT_USE);

    node->token = malloc(sizeof(Token));
    if (node->token == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for use statement token");
        ast_node_dnit(node);
        free(node);
        return NULL;
    }
    token_copy(parser->previous, node->token);

    // check for alias first
    char *alias = NULL;
    char *path  = NULL;

    // parse first identifier
    char *first = parser_parse_identifier(parser);
    if (first == NULL)
    {
        parser_error_at_current(parser, "expected identifier after 'use'");
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    if (parser_match(parser, TOKEN_COLON))
    {
        // aliased import: `alias: module.path`
        alias = first;
        path  = parser_parse_identifier(parser);
        if (path == NULL)
        {
            parser_error_at_current(parser, "expected module path after alias");
            ast_node_dnit(node);
            free(node);
            return NULL;
        }

        // parse rest of module path like `.bar.baz`
        while (parser_match(parser, TOKEN_DOT))
        {
            char *next     = parser_parse_identifier(parser);
            char *new_path = malloc(strlen(path) + strlen(next) + 2);
            sprintf(new_path, "%s.%s", path, next);
            free(path);
            free(next);
            path = new_path;
        }
    }
    else
    {
        // unaliased import: `module.path`
        path = first;

        // parse rest of module path like `.bar.baz`
        while (parser_match(parser, TOKEN_DOT))
        {
            char *next     = parser_parse_identifier(parser);
            char *new_path = malloc(strlen(path) + strlen(next) + 2);
            sprintf(new_path, "%s.%s", path, next);
            free(path);
            free(next);
            path = new_path;
        }
    }

    node->use_stmt.module_path = path;
    node->use_stmt.alias       = alias;

    if (!parser_consume(parser, TOKEN_SEMICOLON, "expected ';' after use statement"))
    {
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    return node;
}

AstNode *parser_parse_stmt_ext(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_KW_EXT, "expected 'ext' keyword"))
    {
        return NULL;
    }

    AstNode *node = malloc(sizeof(AstNode));
    if (node == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for external statement");
        return NULL;
    }
    ast_node_init(node, AST_STMT_EXT);

    node->token = malloc(sizeof(Token));
    if (node->token == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for external statement token");
        ast_node_dnit(node);
        free(node);
        return NULL;
    }
    token_copy(parser->previous, node->token);

    // identifier
    node->ext_stmt.name = parser_parse_identifier(parser);
    if (node->ext_stmt.name == NULL)
    {
        parser_error_at_current(parser, "expected identifier after 'ext'");
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    // type
    if (!parser_consume(parser, TOKEN_COLON, "expected ':' after external name"))
    {
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    node->ext_stmt.type = parser_parse_type(parser);
    if (node->ext_stmt.type == NULL)
    {
        parser_error_at_current(parser, "expected type after ':' in external statement");
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    if (!parser_consume(parser, TOKEN_SEMICOLON, "expected ';' after external statement"))
    {
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    return node;
}

AstNode *parser_parse_stmt_def(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_KW_DEF, "expected 'def' keyword"))
    {
        return NULL;
    }

    AstNode *node = malloc(sizeof(AstNode));
    if (node == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for definition statement");
        return NULL;
    }
    ast_node_init(node, AST_STMT_DEF);

    node->token = malloc(sizeof(Token));
    if (node->token == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for definition statement token");
        ast_node_dnit(node);
        free(node);
        return NULL;
    }
    token_copy(parser->previous, node->token);

    // identifier
    node->def_stmt.name = parser_parse_identifier(parser);
    if (node->def_stmt.name == NULL)
    {
        parser_error_at_current(parser, "expected identifier after 'def'");
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    // type
    if (!parser_consume(parser, TOKEN_COLON, "expected ':' after definition name"))
    {
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    node->def_stmt.type = parser_parse_type(parser);
    if (node->def_stmt.type == NULL)
    {
        parser_error_at_current(parser, "expected type after ':' in definition statement");
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    if (!parser_consume(parser, TOKEN_SEMICOLON, "expected ';' after type definition"))
    {
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    return node;
}

AstNode *parser_parse_stmt_val(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_KW_VAL, "expected 'val' keyword"))
    {
        return NULL;
    }

    AstNode *node = malloc(sizeof(AstNode));
    if (node == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for val statement");
        return NULL;
    }
    ast_node_init(node, AST_STMT_VAL);

    node->token = malloc(sizeof(Token));
    if (node->token == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for val statement token");
        ast_node_dnit(node);
        free(node);
        return NULL;
    }
    token_copy(parser->previous, node->token);

    // mark as read only constant
    node->var_stmt.is_val = true;

    // identifier
    node->var_stmt.name = parser_parse_identifier(parser);
    if (node->var_stmt.name == NULL)
    {
        parser_error_at_current(parser, "expected identifier after 'val'");
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    // type
    if (!parser_consume(parser, TOKEN_COLON, "expected ':' after val name"))
    {
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    node->var_stmt.type = parser_parse_type(parser);
    if (node->var_stmt.type == NULL)
    {
        parser_error_at_current(parser, "expected type after ':' in val statement");
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    // initialization
    if (!parser_consume(parser, TOKEN_EQUAL, "expected '=' in val statement"))
    {
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    node->var_stmt.init = parser_parse_expr(parser);
    if (node->var_stmt.init == NULL)
    {
        parser_error_at_current(parser, "expected expression after '=' in val statement");
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    if (!parser_consume(parser, TOKEN_SEMICOLON, "expected ';' after val statement"))
    {
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    return node;
}

AstNode *parser_parse_stmt_var(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_KW_VAR, "expected 'var' keyword"))
    {
        return NULL;
    }

    AstNode *node = malloc(sizeof(AstNode));
    if (node == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for var statement");
        return NULL;
    }
    ast_node_init(node, AST_STMT_VAR);

    node->token = malloc(sizeof(Token));
    if (node->token == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for var statement token");
        ast_node_dnit(node);
        free(node);
        return NULL;
    }
    token_copy(parser->previous, node->token);

    // mark as mutable variable
    node->var_stmt.is_val = false;

    // identifier
    node->var_stmt.name = parser_parse_identifier(parser);
    if (node->var_stmt.name == NULL)
    {
        parser_error_at_current(parser, "expected identifier after 'var'");
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    // type
    if (!parser_consume(parser, TOKEN_COLON, "expected ':' after var name"))
    {
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    node->var_stmt.type = parser_parse_type(parser);
    if (node->var_stmt.type == NULL)
    {
        parser_error_at_current(parser, "expected type after ':' in var statement");
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    // optional initialization
    if (parser_match(parser, TOKEN_EQUAL))
    {
        node->var_stmt.init = parser_parse_expr(parser);
        if (node->var_stmt.init == NULL)
        {
            parser_error_at_current(parser, "expected expression after '=' in var statement");
            ast_node_dnit(node);
            free(node);
            return NULL;
        }
    }
    else
    {
        node->var_stmt.init = NULL;
    }

    if (!parser_consume(parser, TOKEN_SEMICOLON, "expected ';' after var statement"))
    {
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    return node;
}

AstNode *parser_parse_stmt_fun(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_KW_FUN, "expected 'fun' keyword"))
    {
        return NULL;
    }

    AstNode *node = malloc(sizeof(AstNode));
    if (node == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for function statement");
        return NULL;
    }
    ast_node_init(node, AST_STMT_FUN);

    node->token = malloc(sizeof(Token));
    if (node->token == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for function statement token");
        ast_node_dnit(node);
        free(node);
        return NULL;
    }
    token_copy(parser->previous, node->token);

    // identifier
    node->fun_stmt.name = parser_parse_identifier(parser);
    if (node->fun_stmt.name == NULL)
    {
        parser_error_at_current(parser, "expected identifier after 'fun'");
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    // parameters
    if (!parser_consume(parser, TOKEN_L_PAREN, "expected '(' after function name"))
    {
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    node->fun_stmt.params = parser_parse_parameter_list(parser);
    if (node->fun_stmt.params == NULL)
    {
        // NOTE: no parameters is valid, but returns an empty list, not null
        parser_error_at_current(parser, "expected parameter list after '(' in function statement");
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    if (!parser_consume(parser, TOKEN_R_PAREN, "expected ')' after parameters"))
    {
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    // optional return type
    if (!parser_check(parser, TOKEN_L_BRACE))
    {
        // parse return type
        node->fun_stmt.return_type = parser_parse_type(parser);
        if (node->fun_stmt.return_type == NULL)
        {
            parser_error_at_current(parser, "expected return type or '{' after function parameters");
            ast_node_dnit(node);
            free(node);
            return NULL;
        }
    }
    else
    {
        node->fun_stmt.return_type = NULL;
    }

    // function body
    node->fun_stmt.body = parser_parse_stmt_block(parser);
    if (node->fun_stmt.body == NULL)
    {
        // NOTE: a null block node does not mean it's empty, it means it failed
        // to parse.
        parser_error_at_current(parser, "expected function body after '{'");
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    return node;
}

AstNode *parser_parse_stmt_str(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_KW_STR, "expected 'str' keyword"))
    {
        return NULL;
    }

    AstNode *node = malloc(sizeof(AstNode));
    if (node == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for struct statement");
        return NULL;
    }
    ast_node_init(node, AST_STMT_STR);

    node->token = malloc(sizeof(Token));
    if (node->token == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for struct statement token");
        ast_node_dnit(node);
        free(node);
        return NULL;
    }
    token_copy(parser->previous, node->token);

    // identifier
    node->str_stmt.name = parser_parse_identifier(parser);
    if (node->str_stmt.name == NULL)
    {
        parser_error_at_current(parser, "expected identifier after 'str'");
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    if (!parser_consume(parser, TOKEN_L_BRACE, "expected '{' after struct name"))
    {
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    // field list
    node->str_stmt.fields = parser_parse_field_list(parser);
    if (node->str_stmt.fields == NULL)
    {
        parser_error_at_current(parser, "expected field list after '{' in struct statement");
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    if (!parser_consume(parser, TOKEN_R_BRACE, "expected '}' after struct fields"))
    {
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    return node;
}

AstNode *parser_parse_stmt_uni(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_KW_UNI, "expected 'uni' keyword"))
    {
        return NULL;
    }

    AstNode *node = malloc(sizeof(AstNode));
    if (node == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for union statement");
        return NULL;
    }
    ast_node_init(node, AST_STMT_UNI);

    node->token = malloc(sizeof(Token));
    if (node->token == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for union statement token");
        ast_node_dnit(node);
        free(node);
        return NULL;
    }
    token_copy(parser->previous, node->token);

    // identifier
    node->uni_stmt.name = parser_parse_identifier(parser);
    if (node->uni_stmt.name == NULL)
    {
        parser_error_at_current(parser, "expected identifier after 'uni'");
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    if (!parser_consume(parser, TOKEN_L_BRACE, "expected '{' after union name"))
    {
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    // field list
    node->uni_stmt.fields = parser_parse_field_list(parser);
    if (node->uni_stmt.fields == NULL)
    {
        parser_error_at_current(parser, "expected field list after '{' in union statement");
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    if (!parser_consume(parser, TOKEN_R_BRACE, "expected '}' after union fields"))
    {
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    return node;
}

AstNode *parser_parse_stmt_if(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_KW_IF, "expected 'if' keyword"))
    {
        return NULL;
    }

    AstNode *node = malloc(sizeof(AstNode));
    if (node == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for if statement");
        return NULL;
    }
    ast_node_init(node, AST_STMT_IF);

    node->token = malloc(sizeof(Token));
    if (node->token == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for if statement token");
        ast_node_dnit(node);
        free(node);
        return NULL;
    }
    token_copy(parser->previous, node->token);

    // condition
    if (!parser_consume(parser, TOKEN_L_PAREN, "expected '('"))
    {
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    node->cond_stmt.cond = parser_parse_expr(parser);
    if (node->cond_stmt.cond == NULL)
    {
        parser_error_at_current(parser, "expected condition expression after '('");
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    if (!parser_consume(parser, TOKEN_R_PAREN, "expected ')' after condition"))
    {
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    // body
    node->cond_stmt.body = parser_parse_stmt_block(parser);
    if (node->cond_stmt.body == NULL)
    {
        parser_error_at_current(parser, "expected block statement after 'if' condition");
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    // handle `or` chains
    // `or` optionally has a conditional. The `or` keyword functions as both an
    // `else if` and an `else` statement, depending on whether or not it has a
    // condition. Only the last `or` in a chain can be without a condition.
    while (parser_match(parser, TOKEN_KW_OR))
    {
        AstNode *or_node = malloc(sizeof(AstNode));
        if (or_node == NULL)
        {
            parser_error_at_current(parser, "memory allocation failed for or statement");
            ast_node_dnit(node);
            free(node);
            return NULL;
        }
        ast_node_init(or_node, AST_STMT_OR);

        or_node->token = malloc(sizeof(Token));
        if (or_node->token == NULL)
        {
            parser_error_at_current(parser, "memory allocation failed for or statement token");
            ast_node_dnit(or_node);
            free(or_node);
            return NULL;
        }
        token_copy(parser->previous, or_node->token);

        // condition
        if (parser_match(parser, TOKEN_L_PAREN))
        {
            or_node->cond_stmt.cond = parser_parse_expr(parser);
            if (or_node->cond_stmt.cond == NULL)
            {
                parser_error_at_current(parser, "expected condition expression after '(' in 'or'");
                ast_node_dnit(or_node);
                free(or_node);
                return NULL;
            }

            if (!parser_consume(parser, TOKEN_R_PAREN, "expected ')' after 'or' condition"))
            {
                ast_node_dnit(or_node);
                free(or_node);
                return NULL;
            }
        }
        else
        {
            or_node->cond_stmt.cond = NULL;
        }

        // body
        or_node->cond_stmt.body = parser_parse_stmt_block(parser);
        if (or_node->cond_stmt.body == NULL)
        {
            parser_error_at_current(parser, "expected block statement after 'or' condition");
            ast_node_dnit(or_node);
            free(or_node);
            return NULL;
        }

        // append to the end of the list
        if (node->cond_stmt.stmt_or == NULL)
        {
            node->cond_stmt.stmt_or = or_node;
        }
        else
        {
            AstNode *last = node->cond_stmt.stmt_or;
            while (last->cond_stmt.stmt_or != NULL)
            {
                last = last->cond_stmt.stmt_or;
            }
            last->cond_stmt.stmt_or = or_node;
        }
    }

    return node;
}

AstNode *parser_parse_for_stmt(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_KW_FOR, "expected 'for' keyword"))
    {
        return NULL;
    }

    AstNode *node = malloc(sizeof(AstNode));
    if (node == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for for statement");
        return NULL;
    }
    ast_node_init(node, AST_STMT_FOR);

    node->token = malloc(sizeof(Token));
    if (node->token == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for for statement token");
        ast_node_dnit(node);
        free(node);
        return NULL;
    }
    token_copy(parser->previous, node->token);

    // optional condition
    if (parser_match(parser, TOKEN_L_PAREN))
    {
        node->for_stmt.cond = parser_parse_expr(parser);
        if (node->for_stmt.cond == NULL)
        {
            parser_error_at_current(parser, "expected loop condition after '('");
            ast_node_dnit(node);
            free(node);
            return NULL;
        }

        if (!parser_consume(parser, TOKEN_R_PAREN, "expected ')' after loop condition"))
        {
            ast_node_dnit(node);
            free(node);
            return NULL;
        }
    }
    else
    {
        node->for_stmt.cond = NULL; // infinite loop
    }

    // body
    node->for_stmt.body = parser_parse_stmt_block(parser);
    if (node->for_stmt.body == NULL)
    {
        parser_error_at_current(parser, "expected block statement after 'for' condition");
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    return node;
}

AstNode *parser_parse_brk_stmt(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_KW_BRK, "expected 'brk' keyword"))
    {
        return NULL;
    }

    AstNode *node = malloc(sizeof(AstNode));
    if (node == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for break statement");
        return NULL;
    }
    ast_node_init(node, AST_STMT_BRK);

    node->token = malloc(sizeof(Token));
    if (node->token == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for break statement token");
        ast_node_dnit(node);
        free(node);
        return NULL;
    }
    token_copy(parser->previous, node->token);

    if (!parser_consume(parser, TOKEN_SEMICOLON, "expected ';' after 'brk'"))
    {
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    return node;
}

AstNode *parser_parse_cnt_stmt(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_KW_CNT, "expected 'cnt' keyword"))
    {
        return NULL;
    }

    AstNode *node = malloc(sizeof(AstNode));
    if (node == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for continue statement");
        return NULL;
    }
    ast_node_init(node, AST_STMT_CNT);

    node->token = malloc(sizeof(Token));
    if (node->token == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for continue statement token");
        ast_node_dnit(node);
        free(node);
        return NULL;
    }
    token_copy(parser->previous, node->token);

    if (!parser_consume(parser, TOKEN_SEMICOLON, "expected ';' after 'cnt'"))
    {
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    return node;
}

AstNode *parser_parse_stmt_ret(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_KW_RET, "expected 'ret' keyword"))
    {
        return NULL;
    }

    AstNode *node = malloc(sizeof(AstNode));
    if (node == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for return statement");
        return NULL;
    }
    ast_node_init(node, AST_STMT_RET);

    node->token = malloc(sizeof(Token));
    if (node->token == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for return statement token");
        ast_node_dnit(node);
        free(node);
        return NULL;
    }
    token_copy(parser->previous, node->token);

    if (!parser_check(parser, TOKEN_SEMICOLON))
    {
        node->ret_stmt.expr = parser_parse_expr(parser);
        if (node->ret_stmt.expr == NULL)
        {
            parser_error_at_current(parser, "expected return value after 'ret'");
            ast_node_dnit(node);
            free(node);
            return NULL;
        }
    }
    else
    {
        node->ret_stmt.expr = NULL;
    }

    if (!parser_consume(parser, TOKEN_SEMICOLON, "expected ';' after return value"))
    {
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    return node;
}

// statement parsing
AstNode *parser_parse_stmt_block(Parser *parser)
{
    AstNode *node = malloc(sizeof(AstNode));
    if (node == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for block statement");
        return NULL;
    }
    ast_node_init(node, AST_STMT_BLOCK);

    node->token = malloc(sizeof(Token));
    if (node->token == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for block statement token");
        ast_node_dnit(node);
        free(node);
        return NULL;
    }
    token_copy(parser->previous, node->token);

    node->block_stmt.stmts = malloc(sizeof(AstList));
    if (node->block_stmt.stmts == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for block statement");
        ast_node_dnit(node);
        free(node);
        return NULL;
    }
    ast_list_init(node->block_stmt.stmts);

    if (!parser_consume(parser, TOKEN_L_BRACE, "expected '{' at the start of block statement"))
    {
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    // parse statements until we reach the end of the block
    while (!parser_check(parser, TOKEN_R_BRACE) && !parser_is_at_end(parser))
    {
        AstNode *stmt = parser_parse_stmt(parser);
        if (stmt == NULL)
        {
            parser_error_at_current(parser, "expected statement in block");
            ast_node_dnit(node);
            free(node);
            return NULL;
        }

        ast_list_append(node->block_stmt.stmts, stmt);
    }

    if (!parser_consume(parser, TOKEN_R_BRACE, "expected '}' after block"))
    {
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    return node;
}

AstNode *parser_parse_stmt_expr(Parser *parser)
{
    AstNode *node = malloc(sizeof(AstNode));
    if (node == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for expression statement");
        return NULL;
    }
    ast_node_init(node, AST_STMT_EXPR);

    node->token = malloc(sizeof(Token));
    if (node->token == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for expression statement token");
        ast_node_dnit(node);
        free(node);
        return NULL;
    }
    token_copy(parser->previous, node->token);

    node->expr_stmt.expr = parser_parse_expr(parser);
    if (node->expr_stmt.expr == NULL)
    {
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    if (!parser_consume(parser, TOKEN_SEMICOLON, "expected ';' after expression"))
    {
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    return node;
}

// expression parsing
AstNode *parser_parse_expr(Parser *parser)
{
    return parser_parse_expr_assignment(parser);
}

AstNode *parser_parse_expr_assignment(Parser *parser)
{
    AstNode *expr = parser_parse_expr_logical_or(parser);

    if (parser_match(parser, TOKEN_EQUAL))
    {
        AstNode *assign = malloc(sizeof(AstNode));
        if (assign == NULL)
        {
            parser_error_at_current(parser, "memory allocation failed for assignment expression");
            ast_node_dnit(expr);
            free(expr);
            return NULL;
        }
        ast_node_init(assign, AST_EXPR_BINARY);

        assign->token = malloc(sizeof(Token));
        if (assign->token == NULL)
        {
            parser_error_at_current(parser, "memory allocation failed for assignment expression token");
            ast_node_dnit(assign);
            ast_node_dnit(expr);
            free(assign);
            free(expr);
            return NULL;
        }
        token_copy(parser->previous, assign->token);

        assign->binary_expr.right = parser_parse_expr_assignment(parser);
        if (assign->binary_expr.right == NULL)
        {
            parser_error_at_current(parser, "expected expression after '=' in assignment");
            ast_node_dnit(assign);
            ast_node_dnit(expr);
            free(assign);
            free(expr);
            return NULL;
        }

        assign->binary_expr.left = expr;
        assign->binary_expr.op   = TOKEN_EQUAL;

        return assign;
    }

    return expr;
}

AstNode *parser_parse_expr_logical_or(Parser *parser)
{
    AstNode *expr = parser_parse_expr_logical_and(parser);

    while (parser_match(parser, TOKEN_PIPE_PIPE))
    {
        AstNode *binary = malloc(sizeof(AstNode));
        if (binary == NULL)
        {
            parser_error_at_current(parser, "memory allocation failed for binary expression");
            return NULL;
        }
        ast_node_init(binary, AST_EXPR_BINARY);

        binary->token = malloc(sizeof(Token));
        if (binary->token == NULL)
        {
            parser_error_at_current(parser, "memory allocation failed for binary expression token");
            ast_node_dnit(expr);
            free(binary);
            free(expr);
            return NULL;
        }
        token_copy(parser->previous, binary->token);

        binary->binary_expr.op = parser->previous->kind;

        binary->binary_expr.right = parser_parse_expr_logical_and(parser);
        if (binary->binary_expr.right == NULL)
        {
            parser_error_at_current(parser, "expected expression after '||'");
            ast_node_dnit(expr);
            ast_node_dnit(binary);
            free(binary);
            free(expr);
            return NULL;
        }

        binary->binary_expr.left = expr;
        expr                     = binary;
    }

    return expr;
}

AstNode *parser_parse_expr_logical_and(Parser *parser)
{
    AstNode *expr = parser_parse_expr_bitwise(parser);

    while (parser_match(parser, TOKEN_AMPERSAND_AMPERSAND))
    {
        AstNode *binary = malloc(sizeof(AstNode));
        if (binary == NULL)
        {
            parser_error_at_current(parser, "memory allocation failed for binary expression");
            return NULL;
        }
        ast_node_init(binary, AST_EXPR_BINARY);

        binary->token = malloc(sizeof(Token));
        if (binary->token == NULL)
        {
            parser_error_at_current(parser, "memory allocation failed for binary expression token");
            ast_node_dnit(expr);
            free(binary);
            free(expr);
            return NULL;
        }
        token_copy(parser->previous, binary->token);

        binary->binary_expr.op = parser->previous->kind;

        binary->binary_expr.right = parser_parse_expr_bitwise(parser);
        if (binary->binary_expr.right == NULL)
        {
            parser_error_at_current(parser, "expected expression after '&&'");
            ast_node_dnit(expr);
            ast_node_dnit(binary);
            free(binary);
            free(expr);
            return NULL;
        }

        binary->binary_expr.left = expr;
        expr                     = binary;
    }

    return expr;
}

AstNode *parser_parse_expr_bitwise(Parser *parser)
{
    AstNode *expr = parser_parse_expr_equality(parser);

    while (parser->current->kind == TOKEN_PIPE || parser->current->kind == TOKEN_CARET || parser->current->kind == TOKEN_AMPERSAND)
    {
        parser_advance(parser);
        AstNode *binary = malloc(sizeof(AstNode));
        if (binary == NULL)
        {
            parser_error_at_current(parser, "memory allocation failed for binary expression");
            ast_node_dnit(expr);
            free(expr);
            return NULL;
        }
        ast_node_init(binary, AST_EXPR_BINARY);

        binary->token = malloc(sizeof(Token));
        if (binary->token == NULL)
        {
            parser_error_at_current(parser, "memory allocation failed for binary expression token");
            ast_node_dnit(expr);
            free(binary);
            free(expr);
            return NULL;
        }
        token_copy(parser->previous, binary->token);

        binary->binary_expr.op = parser->previous->kind;

        binary->binary_expr.right = parser_parse_expr_equality(parser);
        if (binary->binary_expr.right == NULL)
        {
            parser_error_at_current(parser, "expected expression after bitwise operator");
            ast_node_dnit(expr);
            ast_node_dnit(binary);
            free(binary);
            free(expr);
            return NULL;
        }

        binary->binary_expr.left = expr;
        expr                     = binary;
    }

    return expr;
}

AstNode *parser_parse_expr_equality(Parser *parser)
{
    AstNode *expr = parser_parse_expr_comparison(parser);

    while (parser->current->kind == TOKEN_EQUAL_EQUAL || parser->current->kind == TOKEN_BANG_EQUAL)
    {
        parser_advance(parser);
        AstNode *binary = malloc(sizeof(AstNode));
        if (binary == NULL)
        {
            parser_error_at_current(parser, "memory allocation failed for binary expression");
            ast_node_dnit(expr);
            free(expr);
            return NULL;
        }
        ast_node_init(binary, AST_EXPR_BINARY);

        binary->token = malloc(sizeof(Token));
        if (binary->token == NULL)
        {
            parser_error_at_current(parser, "memory allocation failed for binary expression token");
            ast_node_dnit(expr);
            free(binary);
            free(expr);
            return NULL;
        }
        token_copy(parser->previous, binary->token);

        binary->binary_expr.op = parser->previous->kind;

        binary->binary_expr.right = parser_parse_expr_comparison(parser);
        if (binary->binary_expr.right == NULL)
        {
            parser_error_at_current(parser, "expected expression after equality operator");
            ast_node_dnit(expr);
            ast_node_dnit(binary);
            free(binary);
            free(expr);
            return NULL;
        }

        binary->binary_expr.left = expr;
        expr                     = binary;
    }

    return expr;
}

AstNode *parser_parse_expr_comparison(Parser *parser)
{
    AstNode *expr = parser_parse_expr_bit_shift(parser);

    while (parser->current->kind == TOKEN_LESS || parser->current->kind == TOKEN_GREATER || parser->current->kind == TOKEN_LESS_EQUAL || parser->current->kind == TOKEN_GREATER_EQUAL)
    {
        parser_advance(parser);
        AstNode *binary = malloc(sizeof(AstNode));
        if (binary == NULL)
        {
            parser_error_at_current(parser, "memory allocation failed for binary expression");
            ast_node_dnit(expr);
            free(expr);
            return NULL;
        }
        ast_node_init(binary, AST_EXPR_BINARY);

        binary->token = malloc(sizeof(Token));
        if (binary->token == NULL)
        {
            parser_error_at_current(parser, "memory allocation failed for binary expression token");
            ast_node_dnit(expr);
            free(binary);
            free(expr);
            return NULL;
        }
        token_copy(parser->previous, binary->token);

        binary->binary_expr.op = parser->previous->kind;

        binary->binary_expr.right = parser_parse_expr_bit_shift(parser);
        if (binary->binary_expr.right == NULL)
        {
            parser_error_at_current(parser, "expected expression after comparison operator");
            ast_node_dnit(expr);
            ast_node_dnit(binary);
            free(binary);
            free(expr);
            return NULL;
        }

        binary->binary_expr.left = expr;
        expr                     = binary;
    }

    return expr;
}

AstNode *parser_parse_expr_bit_shift(Parser *parser)
{
    AstNode *expr = parser_parse_expr_addition(parser);

    while (parser->current->kind == TOKEN_LESS_LESS || parser->current->kind == TOKEN_GREATER_GREATER)
    {
        parser_advance(parser);
        AstNode *binary = malloc(sizeof(AstNode));
        if (binary == NULL)
        {
            parser_error_at_current(parser, "memory allocation failed for binary expression");
            ast_node_dnit(expr);
            free(expr);
            return NULL;
        }
        ast_node_init(binary, AST_EXPR_BINARY);

        binary->token = malloc(sizeof(Token));
        if (binary->token == NULL)
        {
            parser_error_at_current(parser, "memory allocation failed for binary expression token");
            ast_node_dnit(expr);
            free(binary);
            free(expr);
            return NULL;
        }
        token_copy(parser->previous, binary->token);

        binary->binary_expr.op = parser->previous->kind;

        binary->binary_expr.right = parser_parse_expr_addition(parser);
        if (binary->binary_expr.right == NULL)
        {
            parser_error_at_current(parser, "expected expression after bit shift operator");
            ast_node_dnit(expr);
            ast_node_dnit(binary);
            free(binary);
            free(expr);
            return NULL;
        }

        binary->binary_expr.left = expr;
        expr                     = binary;
    }

    return expr;
}

AstNode *parser_parse_expr_addition(Parser *parser)
{
    AstNode *expr = parser_parse_expr_multiplication(parser);

    while (parser->current->kind == TOKEN_PLUS || parser->current->kind == TOKEN_MINUS)
    {
        parser_advance(parser);
        AstNode *binary = malloc(sizeof(AstNode));
        if (binary == NULL)
        {
            parser_error_at_current(parser, "memory allocation failed for binary expression");
            ast_node_dnit(expr);
            free(expr);
            return NULL;
        }
        ast_node_init(binary, AST_EXPR_BINARY);

        binary->token = malloc(sizeof(Token));
        if (binary->token == NULL)
        {
            parser_error_at_current(parser, "memory allocation failed for binary expression token");
            ast_node_dnit(expr);
            free(binary);
            free(expr);
            return NULL;
        }
        token_copy(parser->previous, binary->token);

        binary->binary_expr.op = parser->previous->kind;

        binary->binary_expr.right = parser_parse_expr_multiplication(parser);
        if (binary->binary_expr.right == NULL)
        {
            parser_error_at_current(parser, "expected expression after addition operator");
            ast_node_dnit(expr);
            ast_node_dnit(binary);
            free(binary);
            free(expr);
            return NULL;
        }

        binary->binary_expr.left = expr;
        expr                     = binary;
    }

    return expr;
}

AstNode *parser_parse_expr_multiplication(Parser *parser)
{
    AstNode *expr = parser_parse_expr_unary(parser);

    while (parser->current->kind == TOKEN_STAR || parser->current->kind == TOKEN_SLASH || parser->current->kind == TOKEN_PERCENT)
    {
        parser_advance(parser);
        AstNode *binary = malloc(sizeof(AstNode));
        if (binary == NULL)
        {
            parser_error_at_current(parser, "memory allocation failed for binary expression");
            ast_node_dnit(expr);
            free(expr);
            return NULL;
        }
        ast_node_init(binary, AST_EXPR_BINARY);

        binary->token = malloc(sizeof(Token));
        if (binary->token == NULL)
        {
            parser_error_at_current(parser, "memory allocation failed for binary expression token");
            ast_node_dnit(expr);
            free(binary);
            free(expr);
            return NULL;
        }
        token_copy(parser->previous, binary->token);

        binary->binary_expr.op = parser->previous->kind;

        binary->binary_expr.right = parser_parse_expr_unary(parser);
        if (binary->binary_expr.right == NULL)
        {
            parser_error_at_current(parser, "expected expression after multiplication operator");
            ast_node_dnit(expr);
            ast_node_dnit(binary);
            free(binary);
            free(expr);
            return NULL;
        }

        binary->binary_expr.left = expr;
        expr                     = binary;
    }

    return expr;
}

AstNode *parser_parse_expr_unary(Parser *parser)
{
    if (parser->current->kind == TOKEN_BANG || parser->current->kind == TOKEN_MINUS || parser->current->kind == TOKEN_PLUS || parser->current->kind == TOKEN_TILDE || parser->current->kind == TOKEN_QUESTION || parser->current->kind == TOKEN_AT ||
        parser->current->kind == TOKEN_STAR)
    {
        parser_advance(parser);

        AstNode *unary = malloc(sizeof(AstNode));
        if (unary == NULL)
        {
            parser_error_at_current(parser, "memory allocation failed for unary expression");
            return NULL;
        }
        ast_node_init(unary, AST_EXPR_UNARY);

        unary->token = malloc(sizeof(Token));
        if (unary->token == NULL)
        {
            parser_error_at_current(parser, "memory allocation failed for unary expression token");
            ast_node_dnit(unary);
            free(unary);
            return NULL;
        }
        token_copy(parser->previous, unary->token);

        unary->unary_expr.op = parser->previous->kind;

        unary->unary_expr.expr = parser_parse_expr_unary(parser);
        if (unary->unary_expr.expr == NULL)
        {
            parser_error_at_current(parser, "expected expression after unary operator");
            ast_node_dnit(unary);
            free(unary);
            return NULL;
        }

        return unary;
    }

    return parser_parse_expr_postfix(parser);
}

AstNode *parser_parse_expr_postfix(Parser *parser)
{
    AstNode *expr = parser_parse_expr_primary(parser);

    for (;;)
    {
        if (parser_match(parser, TOKEN_L_PAREN))
        {
            AstNode *call = malloc(sizeof(AstNode));
            if (call == NULL)
            {
                parser_error_at_current(parser, "memory allocation failed for function call expression");
                return NULL;
            }
            ast_node_init(call, AST_EXPR_CALL);

            call->token = malloc(sizeof(Token));
            if (call->token == NULL)
            {
                parser_error_at_current(parser, "memory allocation failed for function call expression token");
                ast_node_dnit(call);
                free(call);
                return NULL;
            }
            token_copy(parser->previous, call->token);

            call->call_expr.func = expr;

            // argument list
            call->call_expr.args = parser_parse_argument_list(parser);
            if (call->call_expr.args == NULL)
            {
                // NOTE: a null argument list does not mean it's empty, it means
                // it failed to parse.
                parser_error_at_current(parser, "expected argument list after '(' in function call");
                ast_node_dnit(call);
                free(call);
                return NULL;
            }

            if (!parser_consume(parser, TOKEN_R_PAREN, "expected ')' after arguments"))
            {
                return NULL;
            }

            expr = call;
        }
        else if (parser_match(parser, TOKEN_L_BRACKET))
        {
            AstNode *index_expr = malloc(sizeof(AstNode));
            if (index_expr == NULL)
            {
                parser_error_at_current(parser, "memory allocation failed for index expression");
                return NULL;
            }
            ast_node_init(index_expr, AST_EXPR_INDEX);

            index_expr->token = malloc(sizeof(Token));
            if (index_expr->token == NULL)
            {
                parser_error_at_current(parser, "memory allocation failed for index expression token");
                ast_node_dnit(index_expr);
                free(index_expr);
                return NULL;
            }
            token_copy(parser->previous, index_expr->token);

            index_expr->index_expr.array = expr;

            // array indexing
            index_expr->index_expr.index = parser_parse_expr(parser);
            if (index_expr->index_expr.index == NULL)
            {
                parser_error_at_current(parser, "expected expression for array index");
                ast_node_dnit(index_expr);
                free(index_expr);
                return NULL;
            }

            if (!parser_consume(parser, TOKEN_R_BRACKET, "expected ']' after array index"))
            {
                return NULL;
            }

            expr = index_expr;
        }
        else if (parser_match(parser, TOKEN_DOT))
        {
            AstNode *field_expr = malloc(sizeof(AstNode));
            if (field_expr == NULL)
            {
                parser_error_at_current(parser, "memory allocation failed for field expression");
                return NULL;
            }
            ast_node_init(field_expr, AST_EXPR_FIELD);

            field_expr->token = malloc(sizeof(Token));
            if (field_expr->token == NULL)
            {
                parser_error_at_current(parser, "memory allocation failed for field expression token");
                ast_node_dnit(field_expr);
                free(field_expr);
                return NULL;
            }
            token_copy(parser->previous, field_expr->token);

            field_expr->field_expr.object = expr;

            // field access
            field_expr->field_expr.field = parser_parse_identifier(parser);
            if (field_expr->field_expr.field == NULL)
            {
                parser_error_at_current(parser, "expected identifier after '.' for field access");
                ast_node_dnit(field_expr);
                free(field_expr);
                return NULL;
            }

            expr = field_expr;
        }
        else if (parser_match(parser, TOKEN_COLON_COLON))
        {
            AstNode *cast = malloc(sizeof(AstNode));
            if (cast == NULL)
            {
                parser_error_at_current(parser, "memory allocation failed for cast expression");
                return NULL;
            }
            ast_node_init(cast, AST_EXPR_CAST);

            cast->token = malloc(sizeof(Token));
            if (cast->token == NULL)
            {
                parser_error_at_current(parser, "memory allocation failed for cast expression token");
                ast_node_dnit(cast);
                free(cast);
                return NULL;
            }
            token_copy(parser->previous, cast->token);

            cast->cast_expr.expr = expr;

            // type cast
            cast->cast_expr.type = parser_parse_type(parser);
            if (cast->cast_expr.type == NULL)
            {
                parser_error_at_current(parser, "expected type after '::' for cast");
                ast_node_dnit(cast);
                free(cast);
                return NULL;
            }

            expr = cast;
        }
        else
        {
            break;
        }
    }

    return expr;
}

AstNode *parser_parse_expr_primary(Parser *parser)
{
    // literal integer
    if (parser->current->kind == TOKEN_LIT_INT)
    {
        AstNode *lit = malloc(sizeof(AstNode));
        if (lit == NULL)
        {
            parser_error_at_current(parser, "memory allocation failed for integer literal");
            return NULL;
        }
        ast_node_init(lit, AST_EXPR_LIT);

        lit->token = malloc(sizeof(Token));
        if (lit->token == NULL)
        {
            parser_error_at_current(parser, "memory allocation failed for integer literal token");
            ast_node_dnit(lit);
            free(lit);
            return NULL;
        }
        token_copy(parser->current, lit->token);

        lit->lit_expr.kind    = TOKEN_LIT_INT;
        lit->lit_expr.int_val = lexer_eval_lit_int(parser->lexer, parser->current);
        parser_advance(parser);
        return lit;
    }

    // floating-point literals
    if (parser->current->kind == TOKEN_LIT_FLOAT)
    {
        AstNode *lit = malloc(sizeof(AstNode));
        if (lit == NULL)
        {
            parser_error_at_current(parser, "memory allocation failed for float literal");
            return NULL;
        }
        ast_node_init(lit, AST_EXPR_LIT);

        lit->token = malloc(sizeof(Token));
        if (lit->token == NULL)
        {
            parser_error_at_current(parser, "memory allocation failed for float literal token");
            ast_node_dnit(lit);
            free(lit);
            return NULL;
        }
        token_copy(parser->current, lit->token);

        lit->lit_expr.kind      = TOKEN_LIT_FLOAT;
        lit->lit_expr.float_val = lexer_eval_lit_float(parser->lexer, parser->current);
        parser_advance(parser);
        return lit;
    }

    // string literal
    if (parser->current->kind == TOKEN_LIT_CHAR)
    {
        AstNode *lit = malloc(sizeof(AstNode));
        if (lit == NULL)
        {
            parser_error_at_current(parser, "memory allocation failed for character literal");
            return NULL;
        }
        ast_node_init(lit, AST_EXPR_LIT);

        lit->token = malloc(sizeof(Token));
        if (lit->token == NULL)
        {
            parser_error_at_current(parser, "memory allocation failed for character literal token");
            ast_node_dnit(lit);
            free(lit);
            return NULL;
        }
        token_copy(parser->current, lit->token);

        lit->lit_expr.kind     = TOKEN_LIT_CHAR;
        lit->lit_expr.char_val = lexer_eval_lit_char(parser->lexer, parser->current);
        parser_advance(parser);
        return lit;
    }

    // string literal
    if (parser->current->kind == TOKEN_LIT_STRING)
    {
        AstNode *lit = malloc(sizeof(AstNode));
        if (lit == NULL)
        {
            parser_error_at_current(parser, "memory allocation failed for string literal");
            return NULL;
        }
        ast_node_init(lit, AST_EXPR_LIT);

        lit->token = malloc(sizeof(Token));
        if (lit->token == NULL)
        {
            parser_error_at_current(parser, "memory allocation failed for string literal token");
            ast_node_dnit(lit);
            free(lit);
            return NULL;
        }
        token_copy(parser->current, lit->token);

        lit->lit_expr.kind       = TOKEN_LIT_STRING;
        lit->lit_expr.string_val = lexer_eval_lit_string(parser->lexer, parser->current);
        parser_advance(parser);
        return lit;
    }

    // grouped expression
    if (parser_match(parser, TOKEN_L_PAREN))
    {
        AstNode *expr = parser_parse_expr(parser);
        if (expr == NULL)
        {
            parser_error_at_current(parser, "expected expression after '('");
            return NULL;
        }

        if (!parser_consume(parser, TOKEN_R_PAREN, "expected ')' after expression"))
        {
            return NULL;
        }

        return expr;
    }

    // array literal
    if (parser_match(parser, TOKEN_L_BRACKET))
    {
        return parser_parse_lit_array(parser);
    }

    // identifier and struct/union literal
    if (parser->current->kind == TOKEN_IDENTIFIER)
    {
        Token token;
        token_copy(parser->current, &token);

        char *name = parser_parse_identifier(parser);
        if (name == NULL)
        {
            parser_error_at_current(parser, "expected identifier");
            return NULL;
        }

        // check for struct or union literal
        if (parser->current->kind == TOKEN_L_BRACE)
        {
            AstNode *type = malloc(sizeof(AstNode));
            if (type == NULL)
            {
                parser_error_at_current(parser, "memory allocation failed for struct type node");
                return NULL;
            }
            ast_node_init(type, AST_TYPE_NAME);

            type->token = malloc(sizeof(Token));
            if (type->token == NULL)
            {
                parser_error_at_current(parser, "memory allocation failed for struct type token");
                ast_node_dnit(type);
                free(type);
                return NULL;
            }
            token_copy(&token, type->token);

            type->type_name.name = name;
            return parser_parse_struct_literal(parser, type);
        }

        AstNode *ident = malloc(sizeof(AstNode));
        if (ident == NULL)
        {
            parser_error_at_current(parser, "memory allocation failed for identifier node");
            return NULL;
        }
        ast_node_init(ident, AST_EXPR_IDENT);

        ident->token = malloc(sizeof(Token));
        if (ident->token == NULL)
        {
            parser_error_at_current(parser, "memory allocation failed for identifier token");
            ast_node_dnit(ident);
            free(ident);
            return NULL;
        }
        token_copy(&token, ident->token);

        ident->ident_expr.name = name;
        return ident;
    }

    // NOTE: this is a fallback for when no valid primary expression is found
    // it will advance the parser and return NULL, which should be handled by
    // the caller.
    parser_error_at_current(parser, "expected expression");
    parser_advance(parser);

    return NULL;
}

AstNode *parser_parse_lit_array(Parser *parser)
{
    AstNode *array = malloc(sizeof(AstNode));
    if (array == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for array literal");
        return NULL;
    }
    ast_node_init(array, AST_EXPR_ARRAY);

    array->token = malloc(sizeof(Token));
    if (array->token == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for array literal token");
        ast_node_dnit(array);
        free(array);
        return NULL;
    }
    token_copy(parser->current, array->token);

    // parse array type
    if (parser_check(parser, TOKEN_R_BRACKET))
    {
        // unbound array - empty brackets []
        array->array_expr.type = malloc(sizeof(AstNode));
        if (array->array_expr.type == NULL)
        {
            parser_error_at_current(parser, "memory allocation failed for array type");
            ast_node_dnit(array);
            free(array);
            return NULL;
        }
        ast_node_init(array->array_expr.type, AST_TYPE_ARRAY);

        array->array_expr.type->token = malloc(sizeof(Token));
        if (array->array_expr.type->token == NULL)
        {
            parser_error_at_current(parser, "memory allocation failed for array type token");
            ast_node_dnit(array);
            free(array);
            return NULL;
        }
        token_copy(parser->previous, array->array_expr.type->token);

        array->array_expr.type->type_array.size = NULL;
    }
    else
    {
        // fixed size array
        AstNode *size = parser_parse_expr_primary(parser);
        if (size == NULL)
        {
            ast_node_dnit(array);
            free(array);
            return NULL;
        }

        array->array_expr.type = malloc(sizeof(AstNode));
        if (array->array_expr.type == NULL)
        {
            parser_error_at_current(parser, "memory allocation failed for array type");
            ast_node_dnit(array);
            free(array);
            return NULL;
        }
        ast_node_init(array->array_expr.type, AST_TYPE_ARRAY);

        array->array_expr.type->token = malloc(sizeof(Token));
        if (array->array_expr.type->token == NULL)
        {
            parser_error_at_current(parser, "memory allocation failed for array type token");
            ast_node_dnit(array);
            free(array->array_expr.type);
            return NULL;
        }
        token_copy(parser->previous, array->array_expr.type->token);

        array->array_expr.type->type_array.size = size;
    }

    if (!parser_consume(parser, TOKEN_R_BRACKET, "expected ']' in array type"))
    {
        ast_node_dnit(array);
        free(array);
        return NULL;
    }

    // parse element type
    array->array_expr.type->type_array.elem_type = parser_parse_type(parser);
    if (array->array_expr.type->type_array.elem_type == NULL)
    {
        parser_error_at_current(parser, "expected element type after array size");
        ast_node_dnit(array);
        free(array);
        return NULL;
    }

    if (!parser_consume(parser, TOKEN_L_BRACE, "expected '{' to start array literal"))
    {
        ast_node_dnit(array);
        free(array);
        return NULL;
    }

    // parse elements
    array->array_expr.elems = malloc(sizeof(AstList));
    if (array->array_expr.elems == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for array elements");
        ast_node_dnit(array);
        free(array);
        return NULL;
    }
    ast_list_init(array->array_expr.elems);

    if (!parser_check(parser, TOKEN_R_BRACE))
    {
        do
        {
            AstNode *elem = parser_parse_expr(parser);
            if (elem == NULL)
            {
                parser_error_at_current(parser, "expected expression for array element");
                ast_node_dnit(array);
                free(array);
                return NULL;
            }
            ast_list_append(array->array_expr.elems, elem);
        } while (parser_match(parser, TOKEN_COMMA));
    }

    if (!parser_consume(parser, TOKEN_R_BRACE, "expected '}' after array elements"))
    {
        ast_node_dnit(array);
        free(array);
        return NULL;
    }

    return array;
}

AstNode *parser_parse_struct_literal(Parser *parser, AstNode *type)
{
    if (!parser_consume(parser, TOKEN_L_BRACE, "expected '{' to start struct literal"))
    {
        ast_node_dnit(type);
        free(type);
        return NULL;
    }

    AstNode *struct_lit = malloc(sizeof(AstNode));
    if (struct_lit == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for struct literal");
        return NULL;
    }
    ast_node_init(struct_lit, AST_EXPR_STRUCT);

    struct_lit->token = malloc(sizeof(Token));
    if (struct_lit->token == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for struct literal token");
        ast_node_dnit(struct_lit);
        free(struct_lit);
        return NULL;
    }
    token_copy(parser->previous, struct_lit->token);

    struct_lit->struct_expr.type = type;

    struct_lit->struct_expr.fields = malloc(sizeof(AstList));
    if (struct_lit->struct_expr.fields == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for struct fields");
        ast_node_dnit(struct_lit);
        free(struct_lit);
        return NULL;
    }
    ast_list_init(struct_lit->struct_expr.fields);

    if (!parser_check(parser, TOKEN_R_BRACE))
    {
        do
        {
            AstNode *field = malloc(sizeof(AstNode));
            if (field == NULL)
            {
                parser_error_at_current(parser, "memory allocation failed for struct field");
                ast_node_dnit(struct_lit);
                free(struct_lit);
                return NULL;
            }
            ast_node_init(field, AST_EXPR_FIELD);

            field->token = malloc(sizeof(Token));
            if (field->token == NULL)
            {
                parser_error_at_current(parser, "memory allocation failed for struct field token");
                ast_node_dnit(field);
                free(field);
                ast_node_dnit(struct_lit);
                free(struct_lit);
                return NULL;
            }
            token_copy(parser->previous, field->token);

            field->field_expr.field = parser_parse_identifier(parser);
            if (field->field_expr.field == NULL)
            {
                parser_error_at_current(parser, "expected field name");
                ast_node_dnit(struct_lit);
                free(struct_lit);
                return NULL;
            }

            if (!parser_consume(parser, TOKEN_COLON, "expected ':' after field name"))
            {
                ast_node_dnit(struct_lit);
                free(struct_lit);
                return NULL;
            }

            field->field_expr.object = parser_parse_expr(parser);
            if (field->field_expr.object == NULL)
            {
                ast_node_dnit(struct_lit);
                free(struct_lit);
                return NULL;
            }

            ast_list_append(struct_lit->struct_expr.fields, field);
        } while (parser_match(parser, TOKEN_COMMA));
    }

    if (!parser_consume(parser, TOKEN_R_BRACE, "expected '}' after struct fields"))
    {
        ast_node_dnit(struct_lit);
        free(struct_lit);
        return NULL;
    }

    return struct_lit;
}

// type parsing
AstNode *parser_parse_type(Parser *parser)
{
    // function type
    if (parser_match(parser, TOKEN_KW_FUN))
    {
        return parser_parse_type_fun(parser);
    }

    // struct type
    if (parser_match(parser, TOKEN_KW_STR))
    {
        return parser_parse_type_str(parser);
    }

    // union type
    if (parser_match(parser, TOKEN_KW_UNI))
    {
        return parser_parse_type_uni(parser);
    }

    // pointer type
    if (parser_match(parser, TOKEN_STAR))
    {
        return parser_parse_type_ptr(parser);
    }

    // array type
    if (parser_match(parser, TOKEN_L_BRACKET))
    {
        return parser_parse_type_array(parser);
    }

    // built-in or named type
    return parser_parse_type_name(parser);
}

AstNode *parser_parse_type_name(Parser *parser)
{
    AstNode *type = malloc(sizeof(AstNode));
    if (type == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for type name");
        return NULL;
    }
    ast_node_init(type, AST_TYPE_NAME);

    type->token = malloc(sizeof(Token));
    if (type->token == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for type name token");
        ast_node_dnit(type);
        free(type);
        return NULL;
    }
    token_copy(parser->current, type->token);

    type->type_name.name = parser_parse_identifier(parser);
    if (type->type_name.name == NULL)
    {
        parser_error_at_current(parser, "expected identifier for type name");
        ast_node_dnit(type);
        free(type);
        return NULL;
    }

    return type;
}

AstNode *parser_parse_type_ptr(Parser *parser)
{
    AstNode *ptr = malloc(sizeof(AstNode));
    if (ptr == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for pointer type");
        return NULL;
    }
    ast_node_init(ptr, AST_TYPE_PTR);

    ptr->token = malloc(sizeof(Token));
    if (ptr->token == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for pointer type token");
        ast_node_dnit(ptr);
        free(ptr);
        return NULL;
    }
    token_copy(parser->previous, ptr->token);

    ptr->type_ptr.base = parser_parse_type(parser);
    if (ptr->type_ptr.base == NULL)
    {
        ast_node_dnit(ptr);
        free(ptr);
        return NULL;
    }

    return ptr;
}

AstNode *parser_parse_type_array(Parser *parser)
{
    AstNode *array = malloc(sizeof(AstNode));
    if (array == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for array type");
        return NULL;
    }
    ast_node_init(array, AST_TYPE_ARRAY);

    array->token = malloc(sizeof(Token));
    if (array->token == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for array type token");
        ast_node_dnit(array);
        free(array);
        return NULL;
    }
    token_copy(parser->previous, array->token);

    if (parser_check(parser, TOKEN_R_BRACKET))
    {
        // unbound array - empty brackets []
        array->type_array.size = NULL;
    }
    else
    {
        // sized array
        array->type_array.size = parser_parse_expr_primary(parser);
        if (array->type_array.size == NULL)
        {
            ast_node_dnit(array);
            free(array);
            return NULL;
        }
    }

    if (!parser_consume(parser, TOKEN_R_BRACKET, "expected ']' after array size"))
    {
        ast_node_dnit(array);
        free(array);
        return NULL;
    }

    array->type_array.elem_type = parser_parse_type(parser);
    if (array->type_array.elem_type == NULL)
    {
        ast_node_dnit(array);
        free(array);
        return NULL;
    }

    return array;
}

AstNode *parser_parse_type_fun(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_L_PAREN, "expected '(' after 'fun'"))
    {
        return NULL;
    }

    AstNode *fun = malloc(sizeof(AstNode));
    if (fun == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for function type");
        return NULL;
    }
    ast_node_init(fun, AST_TYPE_FUN);

    fun->token = malloc(sizeof(Token));
    if (fun->token == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for function type token");
        ast_node_dnit(fun);
        free(fun);
        return NULL;
    }
    token_copy(parser->previous, fun->token);

    fun->type_fun.params = malloc(sizeof(AstList));
    if (fun->type_fun.params == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for function parameters");
        free(fun);
        return NULL;
    }
    ast_list_init(fun->type_fun.params);

    // parse parameter types
    if (!parser_check(parser, TOKEN_R_PAREN))
    {
        do
        {
            AstNode *param_type = parser_parse_type(parser);
            if (param_type == NULL)
            {
                ast_node_dnit(fun);
                free(fun);
                return NULL;
            }

            ast_list_append(fun->type_fun.params, param_type);
        } while (parser_match(parser, TOKEN_COMMA));
    }

    if (!parser_consume(parser, TOKEN_R_PAREN, "expected ')' after function type parameters"))
    {
        ast_node_dnit(fun);
        free(fun);
        return NULL;
    }

    // parse optional return type
    if (!parser_match(parser, TOKEN_L_PAREN))
    {
        fun->type_fun.return_type = parser_parse_type(parser);
        if (fun->type_fun.return_type == NULL)
        {
            ast_node_dnit(fun);
            free(fun);
            return NULL;
        }
    }
    else
    {
        fun->type_fun.return_type = NULL;
    }

    return fun;
}

AstNode *parser_parse_type_str(Parser *parser)
{
    AstNode *str = malloc(sizeof(AstNode));
    if (str == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for string type");
        return NULL;
    }
    ast_node_init(str, AST_TYPE_STR);

    str->token = malloc(sizeof(Token));
    if (str->token == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for string type token");
        ast_node_dnit(str);
        free(str);
        return NULL;
    }
    token_copy(parser->previous, str->token);

    if (parser->current->kind == TOKEN_IDENTIFIER)
    {
        str->type_str.name = parser_parse_identifier(parser);
        if (str->type_str.name == NULL)
        {
            ast_node_dnit(str);
            free(str);
            return NULL;
        }
    }
    else
    {
        str->type_str.name = NULL;
        if (!parser_consume(parser, TOKEN_L_BRACE, "expected '{' for anonymous struct"))
        {
            ast_node_dnit(str);
            free(str);
            return NULL;
        }

        str->type_str.fields = parser_parse_field_list(parser);
        if (str->type_str.fields == NULL)
        {
            ast_node_dnit(str);
            free(str);
            return NULL;
        }

        if (!parser_consume(parser, TOKEN_R_BRACE, "expected '}' after struct fields"))
        {
            ast_node_dnit(str);
            free(str);
            return NULL;
        }
    }

    return str;
}

AstNode *parser_parse_type_uni(Parser *parser)
{
    AstNode *uni = malloc(sizeof(AstNode));
    if (uni == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for union type");
        return NULL;
    }
    ast_node_init(uni, AST_TYPE_UNI);

    uni->token = malloc(sizeof(Token));
    if (uni->token == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for union type token");
        ast_node_dnit(uni);
        free(uni);
        return NULL;
    }
    token_copy(parser->previous, uni->token);

    if (parser->current->kind == TOKEN_IDENTIFIER)
    {
        uni->type_uni.name = parser_parse_identifier(parser);
        if (uni->type_uni.name == NULL)
        {
            ast_node_dnit(uni);
            free(uni);
            return NULL;
        }
    }
    else
    {
        uni->type_uni.name = NULL;
        if (!parser_consume(parser, TOKEN_L_BRACE, "expected '{' for anonymous union"))
        {
            ast_node_dnit(uni);
            free(uni);
            return NULL;
        }
        uni->type_uni.fields = parser_parse_field_list(parser);
        if (uni->type_uni.fields == NULL)
        {
            ast_node_dnit(uni);
            free(uni);
            return NULL;
        }

        if (!parser_consume(parser, TOKEN_R_BRACE, "expected '}' after union fields"))
        {
            ast_node_dnit(uni);
            free(uni);
            return NULL;
        }
    }

    return uni;
}

AstList *parser_parse_field_list(Parser *parser)
{
    AstList *list = malloc(sizeof(AstList));
    if (list == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for field list");
        return NULL;
    }
    ast_list_init(list);

    while (!parser_check(parser, TOKEN_R_BRACE) && !parser_is_at_end(parser))
    {
        AstNode *field = malloc(sizeof(AstNode));
        if (field == NULL)
        {
            parser_error_at_current(parser, "memory allocation failed for field statement");
            ast_list_dnit(list);
            free(list);
            return NULL;
        }
        ast_node_init(field, AST_STMT_FIELD);

        field->token = malloc(sizeof(Token));
        if (field->token == NULL)
        {
            parser_error_at_current(parser, "memory allocation failed for field token");
            ast_node_dnit(field);
            ast_list_dnit(list);
            free(field);
            free(list);
            return NULL;
        }
        token_copy(field->token, parser->current);

        // field name
        field->field_stmt.name = parser_parse_identifier(parser);
        if (field->field_stmt.name == NULL)
        {
            parser_error_at_current(parser, "expected identifier");
            ast_node_dnit(field);
            ast_list_dnit(list);
            free(field);
            free(list);
            return NULL;
        }

        // field type
        if (!parser_consume(parser, TOKEN_COLON, "expected ':' after field name"))
        {
            ast_node_dnit(field);
            ast_list_dnit(list);
            free(field);
            free(list);
            return NULL;
        }

        field->field_stmt.type = parser_parse_type(parser);
        if (field->field_stmt.type == NULL)
        {
            parser_error_at_current(parser, "expected type after ':' in field statement");
            ast_node_dnit(field);
            ast_list_dnit(list);
            free(field);
            free(list);
            return NULL;
        }

        if (!parser_consume(parser, TOKEN_SEMICOLON, "expected ';' after field"))
        {
            ast_node_dnit(field);
            ast_list_dnit(list);
            free(field);
            free(list);
            return NULL;
        }

        ast_list_append(list, field);
    }

    return list;
}

AstList *parser_parse_parameter_list(Parser *parser)
{
    AstList *list = malloc(sizeof(AstList));
    if (list == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for parameter list");
        return NULL;
    }
    ast_list_init(list);

    // check for empty parameter list
    if (parser_check(parser, TOKEN_R_PAREN))
    {
        return list;
    }

    do
    {
        AstNode *param = malloc(sizeof(AstNode));
        if (param == NULL)
        {
            parser_error_at_current(parser, "memory allocation failed for parameter statement");
            ast_list_dnit(list);
            free(list);
            return NULL;
        }
        ast_node_init(param, AST_STMT_PARAM);

        param->token = malloc(sizeof(Token));
        if (param->token == NULL)
        {
            parser_error_at_current(parser, "memory allocation failed for parameter token");
            ast_node_dnit(param);
            ast_list_dnit(list);
            free(param);
            free(list);
            return NULL;
        }
        token_copy(param->token, parser->current);

        // parameter name
        param->param_stmt.name = parser_parse_identifier(parser);
        if (param->param_stmt.name == NULL)
        {
            parser_error_at_current(parser, "expected identifier");
            ast_node_dnit(param);
            ast_list_dnit(list);
            free(param);
            free(list);
            return NULL;
        }

        // parameter type
        if (!parser_consume(parser, TOKEN_COLON, "expected ':' after parameter name"))
        {
            ast_node_dnit(param);
            ast_list_dnit(list);
            free(param);
            free(list);
            return NULL;
        }

        param->param_stmt.type = parser_parse_type(parser);
        if (param->param_stmt.type == NULL)
        {
            parser_error_at_current(parser, "expected type after ':' in parameter statement");
            ast_node_dnit(param);
            ast_list_dnit(list);
            free(param);
            free(list);
            return NULL;
        }

        ast_list_append(list, param);
    } while (parser_match(parser, TOKEN_COMMA));

    return list;
}

AstList *parser_parse_argument_list(Parser *parser)
{
    AstList *list = malloc(sizeof(AstList));
    if (list == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for argument list");
        return NULL;
    }
    ast_list_init(list);

    if (parser_check(parser, TOKEN_R_PAREN))
    {
        return list;
    }

    do
    {
        AstNode *arg = parser_parse_expr(parser);
        if (arg == NULL)
        {
            parser_error_at_current(parser, "expected expression");
            ast_list_dnit(list);
            free(list);
            return NULL;
        }

        ast_list_append(list, arg);
    } while (parser_match(parser, TOKEN_COMMA));

    return list;
}
