#include "parser.h"
#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// helper functions
static AstNode *parser_alloc_node(Parser *parser, AstKind kind, Token *token)
{
    AstNode *node = malloc(sizeof(AstNode));
    if (node == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed");
        return NULL;
    }
    ast_node_init(node, kind);

    if (token)
    {
        node->token = malloc(sizeof(Token));
        if (node->token == NULL)
        {
            parser_error_at_current(parser, "memory allocation failed for token");
            free(node);
            return NULL;
        }
        token_copy(token, node->token);
    }

    return node;
}

static AstList *parser_alloc_list(Parser *parser)
{
    AstList *list = malloc(sizeof(AstList));
    if (list == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for list");
        return NULL;
    }
    ast_list_init(list);
    return list;
}

// helper to safely free a node and return NULL
static AstNode *parser_free_node(AstNode *node)
{
    if (node)
    {
        ast_node_dnit(node);
        free(node);
    }
    return NULL;
}

// helper to safely free a list and return NULL
static AstList *parser_free_list(AstList *list)
{
    if (list)
    {
        ast_list_dnit(list);
        free(list);
    }
    return NULL;
}

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
    AstNode *program = parser_alloc_node(parser, AST_PROGRAM, NULL);
    if (program == NULL)
    {
        return NULL;
    }

    program->program.stmts = parser_alloc_list(parser);
    if (program->program.stmts == NULL)
    {
        return parser_free_node(program);
    }

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

    AstNode *node = parser_alloc_node(parser, AST_STMT_USE, parser->previous);
    if (node == NULL)
    {
        return NULL;
    }

    // check for alias first
    char *alias = NULL;
    char *path  = NULL;

    // parse first identifier
    char *first = parser_parse_identifier(parser);
    if (first == NULL)
    {
        parser_error_at_current(parser, "expected identifier after 'use'");
        return parser_free_node(node);
    }

    if (parser_match(parser, TOKEN_COLON))
    {
        // aliased import: `alias: module.path`
        alias = first;
        path  = parser_parse_identifier(parser);
        if (path == NULL)
        {
            parser_error_at_current(parser, "expected module path after alias");
            free(alias);
            return parser_free_node(node);
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
        return parser_free_node(node);
    }

    return node;
}

AstNode *parser_parse_stmt_ext(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_KW_EXT, "expected 'ext' keyword"))
    {
        return NULL;
    }

    AstNode *node = parser_alloc_node(parser, AST_STMT_EXT, parser->previous);
    if (node == NULL)
    {
        return NULL;
    }

    // identifier
    node->ext_stmt.name = parser_parse_identifier(parser);
    if (node->ext_stmt.name == NULL)
    {
        parser_error_at_current(parser, "expected identifier after 'ext'");
        return parser_free_node(node);
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

    AstNode *node = parser_alloc_node(parser, AST_STMT_DEF, parser->previous);
    if (node == NULL)
    {
        return NULL;
    }

    // identifier
    node->def_stmt.name = parser_parse_identifier(parser);
    if (node->def_stmt.name == NULL)
    {
        parser_error_at_current(parser, "expected identifier after 'def'");
        return parser_free_node(node);
    }

    // type
    if (!parser_consume(parser, TOKEN_COLON, "expected ':' after definition name"))
    {
        return parser_free_node(node);
    }

    node->def_stmt.type = parser_parse_type(parser);
    if (node->def_stmt.type == NULL)
    {
        parser_error_at_current(parser, "expected type after ':' in definition statement");
        return parser_free_node(node);
    }

    if (!parser_consume(parser, TOKEN_SEMICOLON, "expected ';' after type definition"))
    {
        return parser_free_node(node);
    }

    return node;
}

AstNode *parser_parse_stmt_val(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_KW_VAL, "expected 'val' keyword"))
    {
        return NULL;
    }

    AstNode *node = parser_alloc_node(parser, AST_STMT_VAL, parser->previous);
    if (node == NULL)
    {
        return NULL;
    }

    // mark as read only constant
    node->var_stmt.is_val = true;

    // identifier
    node->var_stmt.name = parser_parse_identifier(parser);
    if (node->var_stmt.name == NULL)
    {
        parser_error_at_current(parser, "expected identifier after 'val'");
        return parser_free_node(node);
    }

    // type
    if (!parser_consume(parser, TOKEN_COLON, "expected ':' after val name"))
    {
        return parser_free_node(node);
    }

    node->var_stmt.type = parser_parse_type(parser);
    if (node->var_stmt.type == NULL)
    {
        parser_error_at_current(parser, "expected type after ':' in val statement");
        return parser_free_node(node);
    }

    // initialization
    if (!parser_consume(parser, TOKEN_EQUAL, "expected '=' in val statement"))
    {
        return parser_free_node(node);
    }

    node->var_stmt.init = parser_parse_expr(parser);
    if (node->var_stmt.init == NULL)
    {
        parser_error_at_current(parser, "expected expression after '=' in val statement");
        return parser_free_node(node);
    }

    if (!parser_consume(parser, TOKEN_SEMICOLON, "expected ';' after val statement"))
    {
        return parser_free_node(node);
    }

    return node;
}

AstNode *parser_parse_stmt_var(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_KW_VAR, "expected 'var' keyword"))
    {
        return NULL;
    }

    AstNode *node = parser_alloc_node(parser, AST_STMT_VAR, parser->previous);
    if (node == NULL)
    {
        return NULL;
    }

    // mark as mutable variable
    node->var_stmt.is_val = false;

    // identifier
    node->var_stmt.name = parser_parse_identifier(parser);
    if (node->var_stmt.name == NULL)
    {
        parser_error_at_current(parser, "expected identifier after 'var'");
        return parser_free_node(node);
    }

    // type
    if (!parser_consume(parser, TOKEN_COLON, "expected ':' after var name"))
    {
        return parser_free_node(node);
    }

    node->var_stmt.type = parser_parse_type(parser);
    if (node->var_stmt.type == NULL)
    {
        parser_error_at_current(parser, "expected type after ':' in var statement");
        return parser_free_node(node);
    }

    // optional initialization
    if (parser_match(parser, TOKEN_EQUAL))
    {
        node->var_stmt.init = parser_parse_expr(parser);
        if (node->var_stmt.init == NULL)
        {
            parser_error_at_current(parser, "expected expression after '=' in var statement");
            return parser_free_node(node);
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

    AstNode *node = parser_alloc_node(parser, AST_STMT_FUN, parser->previous);
    if (node == NULL)
    {
        return NULL;
    }

    // identifier
    node->fun_stmt.name = parser_parse_identifier(parser);
    if (node->fun_stmt.name == NULL)
    {
        parser_error_at_current(parser, "expected identifier after 'fun'");
        return parser_free_node(node);
    }

    // parameters
    if (!parser_consume(parser, TOKEN_L_PAREN, "expected '(' after function name"))
    {
        return parser_free_node(node);
    }

    node->fun_stmt.params = parser_parse_parameter_list(parser);
    if (node->fun_stmt.params == NULL)
    {
        // NOTE: no parameters is valid, but returns an empty list, not null
        parser_error_at_current(parser, "expected parameter list after '(' in function statement");
        return parser_free_node(node);
    }

    if (!parser_consume(parser, TOKEN_R_PAREN, "expected ')' after parameters"))
    {
        return parser_free_node(node);
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

    AstNode *node = parser_alloc_node(parser, AST_STMT_STR, parser->previous);
    if (node == NULL)
    {
        return NULL;
    }

    // identifier
    node->str_stmt.name = parser_parse_identifier(parser);
    if (node->str_stmt.name == NULL)
    {
        parser_error_at_current(parser, "expected identifier after 'str'");
        return parser_free_node(node);
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

    AstNode *node = parser_alloc_node(parser, AST_STMT_UNI, parser->previous);
    if (node == NULL)
    {
        return NULL;
    }

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

    AstNode *node = parser_alloc_node(parser, AST_STMT_IF, parser->previous);
    if (node == NULL)
    {
        return NULL;
    }

    // condition
    if (!parser_consume(parser, TOKEN_L_PAREN, "expected '('"))
    {
        return parser_free_node(node);
    }

    node->cond_stmt.cond = parser_parse_expr(parser);
    if (node->cond_stmt.cond == NULL)
    {
        parser_error_at_current(parser, "expected condition expression after '('");
        return parser_free_node(node);
    }

    if (!parser_consume(parser, TOKEN_R_PAREN, "expected ')' after condition"))
    {
        return parser_free_node(node);
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
        AstNode *or_node = parser_alloc_node(parser, AST_STMT_OR, parser->previous);
        if (or_node == NULL)
        {
            return parser_free_node(node);
        }

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

    AstNode *node = parser_alloc_node(parser, AST_STMT_FOR, parser->previous);
    if (node == NULL)
    {
        return NULL;
    }

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

    AstNode *node = parser_alloc_node(parser, AST_STMT_BRK, parser->previous);
    if (node == NULL)
    {
        return NULL;
    }

    if (!parser_consume(parser, TOKEN_SEMICOLON, "expected ';' after 'brk'"))
    {
        return parser_free_node(node);
    }

    return node;
}

AstNode *parser_parse_cnt_stmt(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_KW_CNT, "expected 'cnt' keyword"))
    {
        return NULL;
    }

    AstNode *node = parser_alloc_node(parser, AST_STMT_CNT, parser->previous);
    if (node == NULL)
    {
        return NULL;
    }

    if (!parser_consume(parser, TOKEN_SEMICOLON, "expected ';' after 'cnt'"))
    {
        return parser_free_node(node);
    }

    return node;
}

AstNode *parser_parse_stmt_ret(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_KW_RET, "expected 'ret' keyword"))
    {
        return NULL;
    }

    AstNode *node = parser_alloc_node(parser, AST_STMT_RET, parser->previous);
    if (node == NULL)
    {
        return NULL;
    }

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
    AstNode *node = parser_alloc_node(parser, AST_STMT_BLOCK, parser->previous);
    if (node == NULL)
    {
        return NULL;
    }

    node->block_stmt.stmts = parser_alloc_list(parser);
    if (node->block_stmt.stmts == NULL)
    {
        return parser_free_node(node);
    }

    if (!parser_consume(parser, TOKEN_L_BRACE, "expected '{' at the start of block statement"))
    {
        return parser_free_node(node);
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
    AstNode *node = parser_alloc_node(parser, AST_STMT_EXPR, parser->previous);
    if (node == NULL)
    {
        return NULL;
    }

    node->expr_stmt.expr = parser_parse_expr(parser);
    if (node->expr_stmt.expr == NULL)
    {
        return parser_free_node(node);
    }

    if (!parser_consume(parser, TOKEN_SEMICOLON, "expected ';' after expression"))
    {
        return parser_free_node(node);
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
        AstNode *assign = parser_alloc_node(parser, AST_EXPR_BINARY, parser->previous);
        if (assign == NULL)
        {
            return parser_free_node(expr);
        }

        assign->binary_expr.right = parser_parse_expr_assignment(parser);
        if (assign->binary_expr.right == NULL)
        {
            parser_error_at_current(parser, "expected expression after '=' in assignment");
            parser_free_node(expr);
            return parser_free_node(assign);
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
        AstNode *binary = parser_alloc_node(parser, AST_EXPR_BINARY, parser->previous);
        if (binary == NULL)
        {
            return parser_free_node(expr);
        }

        binary->binary_expr.op = parser->previous->kind;

        binary->binary_expr.right = parser_parse_expr_logical_and(parser);
        if (binary->binary_expr.right == NULL)
        {
            parser_error_at_current(parser, "expected expression after '||'");
            parser_free_node(expr);
            return parser_free_node(binary);
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
        AstNode *binary = parser_alloc_node(parser, AST_EXPR_BINARY, parser->previous);
        if (binary == NULL)
        {
            return parser_free_node(expr);
        }

        binary->binary_expr.op = parser->previous->kind;

        binary->binary_expr.right = parser_parse_expr_bitwise(parser);
        if (binary->binary_expr.right == NULL)
        {
            parser_error_at_current(parser, "expected expression after '&&'");
            parser_free_node(expr);
            return parser_free_node(binary);
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
        AstNode *binary = parser_alloc_node(parser, AST_EXPR_BINARY, parser->previous);
        if (binary == NULL)
        {
            return parser_free_node(expr);
        }

        binary->binary_expr.op = parser->previous->kind;

        binary->binary_expr.right = parser_parse_expr_equality(parser);
        if (binary->binary_expr.right == NULL)
        {
            parser_error_at_current(parser, "expected expression after bitwise operator");
            parser_free_node(expr);
            return parser_free_node(binary);
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
        AstNode *binary = parser_alloc_node(parser, AST_EXPR_BINARY, parser->previous);
        if (binary == NULL)
        {
            return parser_free_node(expr);
        }

        binary->binary_expr.op = parser->previous->kind;

        binary->binary_expr.right = parser_parse_expr_comparison(parser);
        if (binary->binary_expr.right == NULL)
        {
            parser_error_at_current(parser, "expected expression after equality operator");
            parser_free_node(expr);
            return parser_free_node(binary);
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
        AstNode *binary = parser_alloc_node(parser, AST_EXPR_BINARY, parser->previous);
        if (binary == NULL)
        {
            return parser_free_node(expr);
        }

        binary->binary_expr.op = parser->previous->kind;

        binary->binary_expr.right = parser_parse_expr_bit_shift(parser);
        if (binary->binary_expr.right == NULL)
        {
            parser_error_at_current(parser, "expected expression after comparison operator");
            parser_free_node(expr);
            return parser_free_node(binary);
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
        AstNode *binary = parser_alloc_node(parser, AST_EXPR_BINARY, parser->previous);
        if (binary == NULL)
        {
            return parser_free_node(expr);
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
        AstNode *binary = parser_alloc_node(parser, AST_EXPR_BINARY, parser->previous);
        if (binary == NULL)
        {
            return parser_free_node(expr);
        }

        binary->binary_expr.op = parser->previous->kind;

        binary->binary_expr.right = parser_parse_expr_multiplication(parser);
        if (binary->binary_expr.right == NULL)
        {
            parser_error_at_current(parser, "expected expression after addition operator");
            parser_free_node(expr);
            return parser_free_node(binary);
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
        AstNode *binary = parser_alloc_node(parser, AST_EXPR_BINARY, parser->previous);
        if (binary == NULL)
        {
            return parser_free_node(expr);
        }

        binary->binary_expr.op = parser->previous->kind;

        binary->binary_expr.right = parser_parse_expr_unary(parser);
        if (binary->binary_expr.right == NULL)
        {
            parser_error_at_current(parser, "expected expression after multiplication operator");
            parser_free_node(expr);
            return parser_free_node(binary);
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

        AstNode *unary = parser_alloc_node(parser, AST_EXPR_UNARY, parser->previous);
        if (unary == NULL)
        {
            return NULL;
        }

        unary->unary_expr.op = parser->previous->kind;

        unary->unary_expr.expr = parser_parse_expr_unary(parser);
        if (unary->unary_expr.expr == NULL)
        {
            parser_error_at_current(parser, "expected expression after unary operator");
            return parser_free_node(unary);
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
            AstNode *call = parser_alloc_node(parser, AST_EXPR_CALL, parser->previous);
            if (call == NULL)
            {
                return parser_free_node(expr);
            }

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
            AstNode *index_expr = parser_alloc_node(parser, AST_EXPR_INDEX, parser->previous);
            if (index_expr == NULL)
            {
                return parser_free_node(expr);
            }

            index_expr->index_expr.array = expr;

            // array indexing
            index_expr->index_expr.index = parser_parse_expr(parser);
            if (index_expr->index_expr.index == NULL)
            {
                parser_error_at_current(parser, "expected expression for array index");
                return parser_free_node(index_expr);
            }

            if (!parser_consume(parser, TOKEN_R_BRACKET, "expected ']' after array index"))
            {
                return NULL;
            }

            expr = index_expr;
        }
        else if (parser_match(parser, TOKEN_DOT))
        {
            AstNode *field_expr = parser_alloc_node(parser, AST_EXPR_FIELD, parser->previous);
            if (field_expr == NULL)
            {
                return parser_free_node(expr);
            }

            field_expr->field_expr.object = expr;

            // field access
            field_expr->field_expr.field = parser_parse_identifier(parser);
            if (field_expr->field_expr.field == NULL)
            {
                parser_error_at_current(parser, "expected identifier after '.' for field access");
                return parser_free_node(field_expr);
            }

            expr = field_expr;
        }
        else if (parser_match(parser, TOKEN_COLON_COLON))
        {
            AstNode *cast = parser_alloc_node(parser, AST_EXPR_CAST, parser->previous);
            if (cast == NULL)
            {
                return parser_free_node(expr);
            }

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
        AstNode *lit = parser_alloc_node(parser, AST_EXPR_LIT, parser->current);
        if (lit == NULL)
        {
            return NULL;
        }

        lit->lit_expr.kind    = TOKEN_LIT_INT;
        lit->lit_expr.int_val = lexer_eval_lit_int(parser->lexer, parser->current);
        parser_advance(parser);
        return lit;
    }

    // floating-point literals
    if (parser->current->kind == TOKEN_LIT_FLOAT)
    {
        AstNode *lit = parser_alloc_node(parser, AST_EXPR_LIT, parser->current);
        if (lit == NULL)
        {
            return NULL;
        }

        lit->lit_expr.kind      = TOKEN_LIT_FLOAT;
        lit->lit_expr.float_val = lexer_eval_lit_float(parser->lexer, parser->current);
        parser_advance(parser);
        return lit;
    }

    // string literal
    if (parser->current->kind == TOKEN_LIT_CHAR)
    {
        AstNode *lit = parser_alloc_node(parser, AST_EXPR_LIT, parser->current);
        if (lit == NULL)
        {
            return NULL;
        }

        lit->lit_expr.kind     = TOKEN_LIT_CHAR;
        lit->lit_expr.char_val = lexer_eval_lit_char(parser->lexer, parser->current);
        parser_advance(parser);
        return lit;
    }

    // string literal
    if (parser->current->kind == TOKEN_LIT_STRING)
    {
        AstNode *lit = parser_alloc_node(parser, AST_EXPR_LIT, parser->current);
        if (lit == NULL)
        {
            return NULL;
        }

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
            AstNode *type = parser_alloc_node(parser, AST_TYPE_NAME, &token);
            if (type == NULL)
            {
                free(name);
                return NULL;
            }

            type->type_name.name = name;
            return parser_parse_struct_literal(parser, type);
        }

        AstNode *ident = parser_alloc_node(parser, AST_EXPR_IDENT, &token);
        if (ident == NULL)
        {
            free(name);
            return NULL;
        }

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
    AstNode *array = parser_alloc_node(parser, AST_EXPR_ARRAY, parser->current);
    if (array == NULL)
    {
        return NULL;
    }

    // parse array type
    if (parser_check(parser, TOKEN_R_BRACKET))
    {
        // unbound array - empty brackets []
        array->array_expr.type = parser_alloc_node(parser, AST_TYPE_ARRAY, parser->previous);
        if (array->array_expr.type == NULL)
        {
            return parser_free_node(array);
        }

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

        array->array_expr.type = parser_alloc_node(parser, AST_TYPE_ARRAY, parser->previous);
        if (array->array_expr.type == NULL)
        {
            return parser_free_node(array);
        }

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
    array->array_expr.elems = parser_alloc_list(parser);
    if (array->array_expr.elems == NULL)
    {
        return parser_free_node(array);
    }

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

    AstNode *struct_lit = parser_alloc_node(parser, AST_EXPR_STRUCT, parser->previous);
    if (struct_lit == NULL)
    {
        parser_free_node(type);
        return NULL;
    }

    struct_lit->struct_expr.type = type;

    struct_lit->struct_expr.fields = parser_alloc_list(parser);
    if (struct_lit->struct_expr.fields == NULL)
    {
        return parser_free_node(struct_lit);
    }

    if (!parser_check(parser, TOKEN_R_BRACE))
    {
        do
        {
            AstNode *field = parser_alloc_node(parser, AST_EXPR_FIELD, parser->previous);
            if (field == NULL)
            {
                return parser_free_node(struct_lit);
            }

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
    AstNode *type = parser_alloc_node(parser, AST_TYPE_NAME, parser->current);
    if (type == NULL)
    {
        return NULL;
    }

    type->type_name.name = parser_parse_identifier(parser);
    if (type->type_name.name == NULL)
    {
        parser_error_at_current(parser, "expected identifier for type name");
        return parser_free_node(type);
    }

    return type;
}

AstNode *parser_parse_type_ptr(Parser *parser)
{
    AstNode *ptr = parser_alloc_node(parser, AST_TYPE_PTR, parser->previous);
    if (ptr == NULL)
    {
        return NULL;
    }

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
    AstNode *array = parser_alloc_node(parser, AST_TYPE_ARRAY, parser->previous);
    if (array == NULL)
    {
        return NULL;
    }

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
            return parser_free_node(array);
        }
    }

    if (!parser_consume(parser, TOKEN_R_BRACKET, "expected ']' after array size"))
    {
        return parser_free_node(array);
    }

    array->type_array.elem_type = parser_parse_type(parser);
    if (array->type_array.elem_type == NULL)
    {
        return parser_free_node(array);
    }

    return array;
}

AstNode *parser_parse_type_fun(Parser *parser)
{
    if (!parser_consume(parser, TOKEN_L_PAREN, "expected '(' after 'fun'"))
    {
        return NULL;
    }

    AstNode *fun = parser_alloc_node(parser, AST_TYPE_FUN, parser->previous);
    if (fun == NULL)
    {
        return NULL;
    }

    fun->type_fun.params = parser_alloc_list(parser);
    if (fun->type_fun.params == NULL)
    {
        return parser_free_node(fun);
    }

    // parse parameter types
    if (!parser_check(parser, TOKEN_R_PAREN))
    {
        do
        {
            AstNode *param_type = parser_parse_type(parser);
            if (param_type == NULL)
            {
                return parser_free_node(fun);
            }

            ast_list_append(fun->type_fun.params, param_type);
        } while (parser_match(parser, TOKEN_COMMA));
    }

    if (!parser_consume(parser, TOKEN_R_PAREN, "expected ')' after function type parameters"))
    {
        return parser_free_node(fun);
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
    AstNode *str = parser_alloc_node(parser, AST_TYPE_STR, parser->previous);
    if (str == NULL)
    {
        return NULL;
    }

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
    AstNode *uni = parser_alloc_node(parser, AST_TYPE_UNI, parser->previous);
    if (uni == NULL)
    {
        return NULL;
    }

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
    AstList *list = parser_alloc_list(parser);
    if (list == NULL)
    {
        return NULL;
    }

    while (!parser_check(parser, TOKEN_R_BRACE) && !parser_is_at_end(parser))
    {
        AstNode *field = parser_alloc_node(parser, AST_STMT_FIELD, parser->current);
        if (field == NULL)
        {
            return parser_free_list(list);
        }

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
    AstList *list = parser_alloc_list(parser);
    if (list == NULL)
    {
        return NULL;
    }

    // check for empty parameter list
    if (parser_check(parser, TOKEN_R_PAREN))
    {
        return list;
    }

    do
    {
        AstNode *param = parser_alloc_node(parser, AST_STMT_PARAM, parser->current);
        if (param == NULL)
        {
            return parser_free_list(list);
        }

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
    AstList *list = parser_alloc_list(parser);
    if (list == NULL)
    {
        return NULL;
    }

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
