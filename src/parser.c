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

// utility functions
bool parser_is_at_end(Parser *parser)
{
    return parser->current->kind == TOKEN_EOF;
}

bool parser_is_type_start(Parser *parser)
{
    switch (parser->current->kind)
    {
    case TOKEN_IDENTIFIER:
    case TOKEN_STAR:
    case TOKEN_L_BRACKET:
    case TOKEN_KW_FUN:
    case TOKEN_KW_STR:
    case TOKEN_KW_UNI:
    case TOKEN_TYPE_PTR:
    case TOKEN_TYPE_I8:
    case TOKEN_TYPE_I16:
    case TOKEN_TYPE_I32:
    case TOKEN_TYPE_I64:
    case TOKEN_TYPE_U8:
    case TOKEN_TYPE_U16:
    case TOKEN_TYPE_U32:
    case TOKEN_TYPE_U64:
    case TOKEN_TYPE_F16:
    case TOKEN_TYPE_F32:
    case TOKEN_TYPE_F64:
        return true;
    default:
        return false;
    }
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
    ast_node_init(program, AST_PROGRAM);
    program->program.decls = malloc(sizeof(AstList));
    ast_list_init(program->program.decls);

    while (!parser_is_at_end(parser))
    {
        AstNode *decl = parser_parse_declaration(parser);
        if (decl == NULL)
        {
            parser_synchronize(parser);
            continue;
        }

        ast_list_append(program->program.decls, decl);
    }

    return program;
}

// declaration parsing
AstNode *parser_parse_declaration(Parser *parser)
{
    AstNode *result = NULL;

    switch (parser->current->kind)
    {
    case TOKEN_KW_USE:
        result = parser_parse_use_decl(parser);
        break;
    case TOKEN_KW_EXT:
        result = parser_parse_ext_decl(parser);
        break;
    case TOKEN_KW_DEF:
        result = parser_parse_def_decl(parser);
        break;
    case TOKEN_KW_VAL:
        result = parser_parse_val_decl(parser);
        break;
    case TOKEN_KW_VAR:
        result = parser_parse_var_decl(parser);
        break;
    case TOKEN_KW_FUN:
        result = parser_parse_fun_decl(parser);
        break;
    case TOKEN_KW_STR:
        result = parser_parse_str_decl(parser);
        break;
    case TOKEN_KW_UNI:
        result = parser_parse_uni_decl(parser);
        break;
    default:
        parser_error_at_current(parser, "expected declaration");
        return NULL;
    }

    // synchronize after declaration parsing
    parser_synchronize(parser);

    return result;
}

AstNode *parser_parse_use_decl(Parser *parser)
{
    parser_advance(parser); // consume 'use'

    AstNode *node = malloc(sizeof(AstNode));
    if (node == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for use declaration");
        return NULL;
    }
    ast_node_init(node, AST_USE_DECL);
    node->token = parser->previous;

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
        // it's an aliased import: `alias: module.path`
        alias = first;
        path  = parser_parse_identifier(parser);
        if (path == NULL)
        {
            parser_error_at_current(parser, "expected module path after alias");
            ast_node_dnit(node);
            free(node);
            return NULL;
        }

        // parse rest of module path
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
        // it's an unaliased import: `module.path`
        path = first;

        // parse rest of module path
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

    node->use_decl.module_path = path;
    node->use_decl.alias       = alias;

    if (!parser_consume(parser, TOKEN_SEMICOLON, "expected ';' after use declaration"))
    {
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    return node;
}

AstNode *parser_parse_ext_decl(Parser *parser)
{
    parser_advance(parser); // consume 'ext'

    AstNode *node = malloc(sizeof(AstNode));
    if (node == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for external declaration");
        return NULL;
    }
    ast_node_init(node, AST_EXT_DECL);
    node->token = parser->previous;

    node->ext_decl.name = parser_parse_identifier(parser);
    if (node->ext_decl.name == NULL)
    {
        parser_error_at_current(parser, "expected identifier after 'ext'");
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    if (!parser_consume(parser, TOKEN_COLON, "expected ':' after external name"))
    {
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    node->ext_decl.type = parser_parse_type(parser);
    if (node->ext_decl.type == NULL)
    {
        parser_error_at_current(parser, "expected type after ':' in external declaration");
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    if (!parser_consume(parser, TOKEN_SEMICOLON, "expected ';' after external declaration"))
    {
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    return node;
}

AstNode *parser_parse_def_decl(Parser *parser)
{
    parser_advance(parser); // consume 'def'

    AstNode *node = malloc(sizeof(AstNode));
    if (node == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for definition declaration");
        return NULL;
    }
    ast_node_init(node, AST_DEF_DECL);
    node->token = parser->previous;

    node->def_decl.name = parser_parse_identifier(parser);
    if (node->def_decl.name == NULL)
    {
        parser_error_at_current(parser, "expected identifier after 'def'");
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    if (!parser_consume(parser, TOKEN_COLON, "expected ':' after definition name"))
    {
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    node->def_decl.type = parser_parse_type(parser);
    if (node->def_decl.type == NULL)
    {
        parser_error_at_current(parser, "expected type after ':' in definition declaration");
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

AstNode *parser_parse_val_decl(Parser *parser)
{
    parser_advance(parser); // consume 'val'

    AstNode *node = malloc(sizeof(AstNode));
    if (node == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for val declaration");
        return NULL;
    }
    ast_node_init(node, AST_VAL_DECL);
    node->token           = parser->previous;
    node->var_decl.is_val = true;

    node->var_decl.name = parser_parse_identifier(parser);
    if (node->var_decl.name == NULL)
    {
        parser_error_at_current(parser, "expected identifier after 'val'");
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    if (!parser_consume(parser, TOKEN_COLON, "expected ':' after val name"))
    {
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    node->var_decl.type = parser_parse_type(parser);
    if (node->var_decl.type == NULL)
    {
        parser_error_at_current(parser, "expected type after ':' in val declaration");
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    if (!parser_consume(parser, TOKEN_EQUAL, "expected '=' in val declaration"))
    {
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    node->var_decl.init = parser_parse_expression(parser);
    if (node->var_decl.init == NULL)
    {
        parser_error_at_current(parser, "expected expression after '=' in val declaration");
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    if (!parser_consume(parser, TOKEN_SEMICOLON, "expected ';' after val declaration"))
    {
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    return node;
}

AstNode *parser_parse_var_decl(Parser *parser)
{
    parser_advance(parser); // consume 'var'

    AstNode *node = malloc(sizeof(AstNode));
    if (node == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for var declaration");
        return NULL;
    }
    ast_node_init(node, AST_VAR_DECL);
    node->token           = parser->previous;
    node->var_decl.is_val = false;

    node->var_decl.name = parser_parse_identifier(parser);
    if (node->var_decl.name == NULL)
    {
        parser_error_at_current(parser, "expected identifier after 'var'");
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    if (!parser_consume(parser, TOKEN_COLON, "expected ':' after var name"))
    {
        ast_node_dnit(node);
        free(node);
        return NULL;
    }
    node->var_decl.type = parser_parse_type(parser);
    if (node->var_decl.type == NULL)
    {
        parser_error_at_current(parser, "expected type after ':' in var declaration");
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    // optional initialization
    if (parser_match(parser, TOKEN_EQUAL))
    {
        node->var_decl.init = parser_parse_expression(parser);
        if (node->var_decl.init == NULL)
        {
            parser_error_at_current(parser, "expected expression after '=' in var declaration");
            ast_node_dnit(node);
            free(node);
            return NULL;
        }
    }
    else
    {
        node->var_decl.init = NULL;
    }

    if (!parser_consume(parser, TOKEN_SEMICOLON, "expected ';' after var declaration"))
    {
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    return node;
}

AstNode *parser_parse_fun_decl(Parser *parser)
{
    parser_advance(parser); // consume 'fun'

    AstNode *node = malloc(sizeof(AstNode));
    if (node == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for function declaration");
        return NULL;
    }
    ast_node_init(node, AST_FUN_DECL);
    node->token = parser->previous;

    node->fun_decl.name = parser_parse_identifier(parser);
    if (node->fun_decl.name == NULL)
    {
        parser_error_at_current(parser, "expected identifier after 'fun'");
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    if (!parser_consume(parser, TOKEN_L_PAREN, "expected '(' after function name"))
    {
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    node->fun_decl.params = parser_parse_parameter_list(parser);
    if (node->fun_decl.params == NULL)
    {
        // NOTE: no parameters is valid, but returns an empty list, not null
        parser_error_at_current(parser, "expected parameter list after '(' in function declaration");
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

    // check for '{' next. if not found, parse next tokens as return type
    node->fun_decl.return_type = NULL; // no return type
    if (!parser_check(parser, TOKEN_L_BRACE))
    {
        // parse return type
        node->fun_decl.return_type = parser_parse_type(parser);
        if (node->fun_decl.return_type == NULL)
        {
            parser_error_at_current(parser, "expected return type or '{' after function parameters");
            ast_node_dnit(node);
            free(node);
            return NULL;
        }
    }

    // function body
    node->fun_decl.body = parser_parse_block_stmt(parser);
    if (node->fun_decl.body == NULL)
    {
        // NOTE: empty body is valid, but not null
        parser_error_at_current(parser, "expected function body after '{'");
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    return node;
}

AstNode *parser_parse_str_decl(Parser *parser)
{
    parser_advance(parser); // consume 'str'

    AstNode *node = malloc(sizeof(AstNode));
    if (node == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for struct declaration");
        return NULL;
    }
    ast_node_init(node, AST_STR_DECL);
    node->token = parser->previous;

    node->str_decl.name = parser_parse_identifier(parser);
    if (node->str_decl.name == NULL)
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

    node->str_decl.fields = parser_parse_field_list(parser);
    if (node->str_decl.fields == NULL)
    {
        parser_error_at_current(parser, "expected field list after '{' in struct declaration");
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

AstNode *parser_parse_uni_decl(Parser *parser)
{
    parser_advance(parser); // consume 'uni'

    AstNode *node = malloc(sizeof(AstNode));
    if (node == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for union declaration");
        return NULL;
    }
    ast_node_init(node, AST_UNI_DECL);
    node->token = parser->previous;

    node->uni_decl.name = parser_parse_identifier(parser);
    if (node->uni_decl.name == NULL)
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
    node->uni_decl.fields = parser_parse_field_list(parser);
    if (node->uni_decl.fields == NULL)
    {
        parser_error_at_current(parser, "expected field list after '{' in union declaration");
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

// helper for parsing lists
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
            parser_error_at_current(parser, "memory allocation failed for field declaration");
            ast_list_dnit(list);
            free(list);
            return NULL;
        }
        ast_node_init(field, AST_FIELD_DECL);
        field->token = parser->current;

        field->field_decl.name = parser_parse_identifier(parser);
        if (field->field_decl.name == NULL)
        {
            parser_error_at_current(parser, "expected identifier after field declaration");
            ast_node_dnit(field);
            ast_list_dnit(list);
            free(field);
            free(list);
            return NULL;
        }

        if (!parser_consume(parser, TOKEN_COLON, "expected ':' after field name"))
        {
            ast_node_dnit(field);
            ast_list_dnit(list);
            free(field);
            free(list);
            return NULL;
        }

        field->field_decl.type = parser_parse_type(parser);
        if (field->field_decl.type == NULL)
        {
            parser_error_at_current(parser, "expected type after ':' in field declaration");
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
            parser_error_at_current(parser, "memory allocation failed for parameter declaration");
            ast_list_dnit(list);
            free(list);
            return NULL;
        }
        ast_node_init(param, AST_PARAM_DECL);
        param->token = parser->current;

        param->param_decl.name = parser_parse_identifier(parser);
        if (param->param_decl.name == NULL)
        {
            parser_error_at_current(parser, "expected identifier after parameter declaration");
            ast_node_dnit(param);
            ast_list_dnit(list);
            free(param);
            free(list);
            return NULL;
        }

        if (!parser_consume(parser, TOKEN_COLON, "expected ':' after parameter name"))
        {
            ast_node_dnit(param);
            ast_list_dnit(list);
            free(param);
            free(list);
            return NULL;
        }

        param->param_decl.type = parser_parse_type(parser);
        if (param->param_decl.type == NULL)
        {
            parser_error_at_current(parser, "expected type after ':' in parameter declaration");
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
        AstNode *arg = parser_parse_expression(parser);
        if (arg == NULL)
        {
            parser_error_at_current(parser, "expected expression in argument list");
            ast_list_dnit(list);
            free(list);
            return NULL;
        }

        ast_list_append(list, arg);
    } while (parser_match(parser, TOKEN_COMMA));

    return list;
}

// statement parsing
AstNode *parser_parse_statement(Parser *parser)
{
    switch (parser->current->kind)
    {
    case TOKEN_L_BRACE:
        return parser_parse_block_stmt(parser);
    case TOKEN_KW_RET:
        return parser_parse_ret_stmt(parser);
    case TOKEN_KW_IF:
        return parser_parse_if_stmt(parser);
    case TOKEN_KW_FOR:
        return parser_parse_for_stmt(parser);
    case TOKEN_KW_BRK:
        return parser_parse_brk_stmt(parser);
    case TOKEN_KW_CNT:
        return parser_parse_cnt_stmt(parser);
    default:
        return parser_parse_expr_stmt(parser);
    }
}

AstNode *parser_parse_block_stmt(Parser *parser)
{
    AstNode *node = malloc(sizeof(AstNode));
    if (node == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for block statement");
        return NULL;
    }
    ast_node_init(node, AST_BLOCK_STMT);
    node->token = parser->current;

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

    while (!parser_check(parser, TOKEN_R_BRACE) && !parser_is_at_end(parser))
    {
        // NOTE: not a huge fan of "declarations" being different than
        // statements, but this works for now. Main difference is cleaner
        // parsing and better errors during semantic analysis.
        AstNode *stmt;
        switch (parser->current->kind)
        {
        case TOKEN_KW_VAL:
            stmt = parser_parse_val_decl(parser);
            break;
        case TOKEN_KW_VAR:
            stmt = parser_parse_var_decl(parser);
            break;
        default:
            stmt = parser_parse_statement(parser);
            break;
        }

        if (stmt == NULL)
        {
            // if we failed to parse a statement, continue
            continue;
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

AstNode *parser_parse_expr_stmt(Parser *parser)
{
    AstNode *node = malloc(sizeof(AstNode));
    if (node == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for expression statement");
        return NULL;
    }
    ast_node_init(node, AST_EXPR_STMT);
    node->token = parser->current;

    node->expr_stmt.expr = parser_parse_expression(parser);
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

AstNode *parser_parse_ret_stmt(Parser *parser)
{
    parser_advance(parser); // consume 'ret'

    AstNode *node = malloc(sizeof(AstNode));
    if (node == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for return statement");
        return NULL;
    }
    ast_node_init(node, AST_RET_STMT);
    node->token = parser->previous;

    if (!parser_check(parser, TOKEN_SEMICOLON))
    {
        node->ret_stmt.expr = parser_parse_expression(parser);
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

AstNode *parser_parse_if_stmt(Parser *parser)
{
    AstNode *node = malloc(sizeof(AstNode));
    if (node == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for if statement");
        return NULL;
    }
    ast_node_init(node, AST_IF_STMT);
    node->token = parser->current;

    // NOTE: this logic is unique as the process for checking for 'or' chains
    // consumes the 'or' keyword. This is not the same behavior as 'if'
    // statements, so we check for 'if' and consume it first.
    parser_match(parser, TOKEN_KW_IF);

    if (!parser_consume(parser, TOKEN_L_PAREN, "expected '('"))
    {
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    node->if_stmt.cond = parser_parse_expression(parser);
    if (node->if_stmt.cond == NULL)
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

    node->if_stmt.then_stmt = parser_parse_block_stmt(parser);
    if (node->if_stmt.then_stmt == NULL)
    {
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    // handle 'or' (else) chains
    if (parser_match(parser, TOKEN_KW_OR))
    {
        // check if it's 'or (condition)' vs just 'or { block }'
        if (parser_check(parser, TOKEN_L_PAREN))
        {
            // has condition
            node->if_stmt.else_stmt = parser_parse_if_stmt(parser);
        }
        else
        {
            // no condition
            node->if_stmt.else_stmt = parser_parse_block_stmt(parser);
        }

        if (node->if_stmt.else_stmt == NULL)
        {
            parser_error_at_current(parser, "expected condition or block after 'or'");
            ast_node_dnit(node);
            free(node);
            return NULL;
        }
    }
    else
    {
        node->if_stmt.else_stmt = NULL;
    }

    return node;
}

AstNode *parser_parse_for_stmt(Parser *parser)
{
    parser_advance(parser); // consume 'for'

    AstNode *node = malloc(sizeof(AstNode));
    if (node == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for for statement");
        return NULL;
    }
    ast_node_init(node, AST_FOR_STMT);
    node->token = parser->previous;

    if (parser_match(parser, TOKEN_L_PAREN))
    {
        node->for_stmt.cond = parser_parse_expression(parser);
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

    node->for_stmt.body = parser_parse_block_stmt(parser);
    if (node->for_stmt.body == NULL)
    {
        parser_error_at_current(parser, "expected loop body after 'for'");
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    return node;
}

AstNode *parser_parse_brk_stmt(Parser *parser)
{
    parser_advance(parser); // consume 'brk'

    AstNode *node = malloc(sizeof(AstNode));
    if (node == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for break statement");
        return NULL;
    }
    ast_node_init(node, AST_BRK_STMT);
    node->token = parser->previous;

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
    parser_advance(parser); // consume 'cnt'

    AstNode *node = malloc(sizeof(AstNode));
    if (node == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for continue statement");
        return NULL;
    }
    ast_node_init(node, AST_CNT_STMT);
    node->token = parser->previous;

    if (!parser_consume(parser, TOKEN_SEMICOLON, "expected ';' after 'cnt'"))
    {
        ast_node_dnit(node);
        free(node);
        return NULL;
    }

    return node;
}

// expression parsing
AstNode *parser_parse_expression(Parser *parser)
{
    return parser_parse_assignment(parser);
}

AstNode *parser_parse_assignment(Parser *parser)
{
    AstNode *expr = parser_parse_logical_or(parser);

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
        ast_node_init(assign, AST_BINARY_EXPR);
        assign->token = parser->previous;

        assign->binary_expr.right = parser_parse_assignment(parser);
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

AstNode *parser_parse_logical_or(Parser *parser)
{
    AstNode *expr = parser_parse_logical_and(parser);

    while (parser_match(parser, TOKEN_PIPE_PIPE))
    {
        AstNode *binary = malloc(sizeof(AstNode));
        if (binary == NULL)
        {
            parser_error_at_current(parser, "memory allocation failed for binary expression");
            return NULL;
        }
        ast_node_init(binary, AST_BINARY_EXPR);
        binary->token = parser->previous;

        binary->binary_expr.op    = parser->previous->kind;
        binary->binary_expr.right = parser_parse_logical_and(parser);
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

AstNode *parser_parse_logical_and(Parser *parser)
{
    AstNode *expr = parser_parse_bitwise(parser);

    while (parser_match(parser, TOKEN_AMPERSAND_AMPERSAND))
    {
        AstNode *binary = malloc(sizeof(AstNode));
        if (binary == NULL)
        {
            parser_error_at_current(parser, "memory allocation failed for binary expression");
            return NULL;
        }
        ast_node_init(binary, AST_BINARY_EXPR);
        binary->token = parser->previous;

        binary->binary_expr.op    = parser->previous->kind;
        binary->binary_expr.right = parser_parse_bitwise(parser);
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

AstNode *parser_parse_bitwise(Parser *parser)
{
    AstNode *expr = parser_parse_equality(parser);

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
        ast_node_init(binary, AST_BINARY_EXPR);
        binary->token          = parser->previous;
        binary->binary_expr.op = parser->previous->kind;

        binary->binary_expr.right = parser_parse_equality(parser);
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

AstNode *parser_parse_equality(Parser *parser)
{
    AstNode *expr = parser_parse_comparison(parser);

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
        ast_node_init(binary, AST_BINARY_EXPR);
        binary->token          = parser->previous;
        binary->binary_expr.op = parser->previous->kind;

        binary->binary_expr.right = parser_parse_comparison(parser);
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

AstNode *parser_parse_comparison(Parser *parser)
{
    AstNode *expr = parser_parse_bit_shift(parser);

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
        ast_node_init(binary, AST_BINARY_EXPR);
        binary->token          = parser->previous;
        binary->binary_expr.op = parser->previous->kind;

        binary->binary_expr.right = parser_parse_bit_shift(parser);
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

AstNode *parser_parse_bit_shift(Parser *parser)
{
    AstNode *expr = parser_parse_addition(parser);

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
        ast_node_init(binary, AST_BINARY_EXPR);
        binary->token          = parser->previous;
        binary->binary_expr.op = parser->previous->kind;

        binary->binary_expr.right = parser_parse_addition(parser);
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

AstNode *parser_parse_addition(Parser *parser)
{
    AstNode *expr = parser_parse_multiplication(parser);

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
        ast_node_init(binary, AST_BINARY_EXPR);
        binary->token          = parser->previous;
        binary->binary_expr.op = parser->previous->kind;

        binary->binary_expr.right = parser_parse_multiplication(parser);
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

AstNode *parser_parse_multiplication(Parser *parser)
{
    AstNode *expr = parser_parse_unary(parser);

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
        ast_node_init(binary, AST_BINARY_EXPR);
        binary->token          = parser->previous;
        binary->binary_expr.op = parser->previous->kind;

        binary->binary_expr.right = parser_parse_unary(parser);
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

AstNode *parser_parse_unary(Parser *parser)
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
        ast_node_init(unary, AST_UNARY_EXPR);
        unary->token         = parser->previous;
        unary->unary_expr.op = parser->previous->kind;

        unary->unary_expr.expr = parser_parse_unary(parser);
        if (unary->unary_expr.expr == NULL)
        {
            parser_error_at_current(parser, "expected expression after unary operator");
            ast_node_dnit(unary);
            free(unary);
            return NULL;
        }

        return unary;
    }

    return parser_parse_postfix(parser);
}

AstNode *parser_parse_postfix(Parser *parser)
{
    AstNode *expr = parser_parse_primary(parser);

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
            ast_node_init(call, AST_CALL_EXPR);
            call->token          = parser->previous;
            call->call_expr.func = expr;

            // function call
            call->call_expr.args = parser_parse_argument_list(parser);
            if (call->call_expr.args == NULL)
            {
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
            ast_node_init(index_expr, AST_INDEX_EXPR);
            index_expr->token            = parser->previous;
            index_expr->index_expr.array = expr;

            // array indexing
            index_expr->index_expr.index = parser_parse_expression(parser);
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
            ast_node_init(field_expr, AST_FIELD_EXPR);
            field_expr->token             = parser->previous;
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
            ast_node_init(cast, AST_CAST_EXPR);
            cast->token          = parser->previous;
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

AstNode *parser_parse_primary(Parser *parser)
{
    // literals
    if (parser->current->kind == TOKEN_LIT_INT)
    {
        Token   *token = parser->current;
        AstNode *lit   = malloc(sizeof(AstNode));
        if (lit == NULL)
        {
            parser_error_at_current(parser, "memory allocation failed for integer literal");
            return NULL;
        }
        ast_node_init(lit, AST_LIT_EXPR);
        lit->token            = token;
        lit->lit_expr.kind    = TOKEN_LIT_INT;
        lit->lit_expr.int_val = lexer_eval_lit_int(parser->lexer, parser->current);
        parser_advance(parser);
        return lit;
    }

    if (parser->current->kind == TOKEN_LIT_FLOAT)
    {
        Token   *token = parser->current;
        AstNode *lit   = malloc(sizeof(AstNode));
        if (lit == NULL)
        {
            parser_error_at_current(parser, "memory allocation failed for float literal");
            return NULL;
        }
        ast_node_init(lit, AST_LIT_EXPR);
        lit->token              = token;
        lit->lit_expr.kind      = TOKEN_LIT_FLOAT;
        lit->lit_expr.float_val = lexer_eval_lit_float(parser->lexer, parser->current);
        parser_advance(parser);
        return lit;
    }

    if (parser->current->kind == TOKEN_LIT_CHAR)
    {
        Token   *token = parser->current;
        AstNode *lit   = malloc(sizeof(AstNode));
        if (lit == NULL)
        {
            parser_error_at_current(parser, "memory allocation failed for character literal");
            return NULL;
        }
        ast_node_init(lit, AST_LIT_EXPR);
        lit->token             = token;
        lit->lit_expr.kind     = TOKEN_LIT_CHAR;
        lit->lit_expr.char_val = lexer_eval_lit_char(parser->lexer, parser->current);
        parser_advance(parser);
        return lit;
    }

    if (parser->current->kind == TOKEN_LIT_STRING)
    {
        Token   *token = parser->current;
        AstNode *lit   = malloc(sizeof(AstNode));
        if (lit == NULL)
        {
            parser_error_at_current(parser, "memory allocation failed for string literal");
            return NULL;
        }
        ast_node_init(lit, AST_LIT_EXPR);
        lit->token               = token;
        lit->lit_expr.kind       = TOKEN_LIT_STRING;
        lit->lit_expr.string_val = lexer_eval_lit_string(parser->lexer, parser->current);
        parser_advance(parser);
        return lit;
    }

    // grouped expression
    if (parser_match(parser, TOKEN_L_PAREN))
    {
        AstNode *expr = parser_parse_expression(parser);
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
        return parser_parse_array_literal(parser);
    }

    // identifier (possibly followed by struct literal)
    if (parser->current->kind == TOKEN_IDENTIFIER)
    {
        Token *token = parser->current;
        char  *name  = parser_parse_identifier(parser);
        if (name == NULL)
        {
            parser_error_at_current(parser, "expected identifier after 'identifier'");
            return NULL;
        }

        // check for built-in functions that take types
        if (strcmp(name, "size_of") == 0 || strcmp(name, "align_of") == 0)
        {
            if (!parser_consume(parser, TOKEN_L_PAREN, "expected '(' after built-in function"))
            {
                return NULL;
            }

            AstNode *type_arg = parser_parse_type(parser);
            if (type_arg == NULL)
            {
                parser_error_at_current(parser, "expected type argument after '(' in built-in function");
                return NULL;
            }

            if (!parser_consume(parser, TOKEN_R_PAREN, "expected ')' after type argument"))
            {
                return NULL;
            }

            // create a special call node for built-ins
            AstNode *call = malloc(sizeof(AstNode));
            if (call == NULL)
            {
                parser_error_at_current(parser, "memory allocation failed for built-in function call");
                ast_node_dnit(type_arg);
                free(type_arg);
                return NULL;
            }
            ast_node_init(call, AST_CALL_EXPR);
            call->token = token;

            AstNode *func = malloc(sizeof(AstNode));
            if (func == NULL)
            {
                parser_error_at_current(parser, "memory allocation failed for built-in function node");
                ast_node_dnit(call);
                free(call);
                ast_node_dnit(type_arg);
                free(type_arg);
                return NULL;
            }
            ast_node_init(func, AST_IDENT_EXPR);
            func->token           = token;
            func->ident_expr.name = name;

            call->call_expr.func = func;
            call->call_expr.args = malloc(sizeof(AstList));
            if (call->call_expr.args == NULL)
            {
                parser_error_at_current(parser, "memory allocation failed for argument list in built-in function call");
                ast_node_dnit(func);
                free(func);
                ast_node_dnit(call);
                free(call);
                ast_node_dnit(type_arg);
                free(type_arg);
                return NULL;
            }

            ast_list_init(call->call_expr.args);
            ast_list_append(call->call_expr.args, type_arg);

            return call;
        }

        // check for offset_of which takes a field path
        if (strcmp(name, "offset_of") == 0)
        {
            if (!parser_consume(parser, TOKEN_L_PAREN, "expected '(' after offset_of"))
            {
                return NULL;
            }

            // parse type.field notation
            AstNode *type_name = parser_parse_type_name(parser);
            if (type_name == NULL)
            {
                parser_error_at_current(parser, "expected type name after '(' in offset_of");
                return NULL;
            }

            if (!parser_consume(parser, TOKEN_DOT, "expected '.' after type in offset_of"))
            {
                ast_node_dnit(type_name);
                free(type_name);
                return NULL;
            }

            char *field = parser_parse_identifier(parser);
            if (field == NULL)
            {
                parser_error_at_current(parser, "expected field name after '.' in offset_of");
                return NULL;
            }

            if (!parser_consume(parser, TOKEN_R_PAREN, "expected ')' after field"))
            {
                ast_node_dnit(type_name);
                free(type_name);
                free(field);
                return NULL;
            }

            // create field access node
            AstNode *field_expr = malloc(sizeof(AstNode));
            if (field_expr == NULL)
            {
                parser_error_at_current(parser, "memory allocation failed for field access node");
                ast_node_dnit(type_name);
                free(type_name);
                free(field);
                return NULL;
            }
            ast_node_init(field_expr, AST_FIELD_EXPR);
            field_expr->token             = token;
            field_expr->field_expr.object = type_name;
            field_expr->field_expr.field  = field;

            // create call node
            AstNode *call = malloc(sizeof(AstNode));
            if (call == NULL)
            {
                parser_error_at_current(parser, "memory allocation failed for call node");
                ast_node_dnit(field_expr);
                free(field_expr);
                return NULL;
            }
            ast_node_init(call, AST_CALL_EXPR);
            call->token = token;

            AstNode *func = malloc(sizeof(AstNode));
            if (func == NULL)
            {
                parser_error_at_current(parser, "memory allocation failed for built-in function node");
                ast_node_dnit(call);
                free(call);
                ast_node_dnit(field_expr);
                free(field_expr);
                return NULL;
            }
            ast_node_init(func, AST_IDENT_EXPR);
            func->token           = token;
            func->ident_expr.name = name;

            call->call_expr.func = func;
            call->call_expr.args = malloc(sizeof(AstList));
            if (call->call_expr.args == NULL)
            {
                parser_error_at_current(parser, "memory allocation failed for argument list in built-in function call");
                ast_node_dnit(func);
                free(func);
                ast_node_dnit(call);
                free(call);
                ast_node_dnit(field_expr);
                free(field_expr);
                return NULL;
            }

            ast_list_init(call->call_expr.args);
            ast_list_append(call->call_expr.args, field_expr);

            return call;
        }

        // check for len which can take either expression or type
        if (strcmp(name, "len") == 0)
        {
            if (!parser_consume(parser, TOKEN_L_PAREN, "expected '(' after len"))
            {
                return NULL;
            }

            AstNode *arg;
            if (parser_is_type_start(parser))
            {
                arg = parser_parse_type(parser);
            }
            else
            {
                arg = parser_parse_expression(parser);
            }
            if (arg == NULL)
            {
                parser_error_at_current(parser, "expected argument after '(' in len");
                return NULL;
            }

            if (!parser_consume(parser, TOKEN_R_PAREN, "expected ')' after argument"))
            {
                ast_node_dnit(arg);
                free(arg);
                return NULL;
            }

            AstNode *call = malloc(sizeof(AstNode));
            if (call == NULL)
            {
                parser_error_at_current(parser, "memory allocation failed for call node");
                ast_node_dnit(arg);
                free(arg);
                return NULL;
            }
            ast_node_init(call, AST_CALL_EXPR);
            call->token = token;

            AstNode *func = malloc(sizeof(AstNode));
            if (func == NULL)
            {
                parser_error_at_current(parser, "memory allocation failed for built-in function node");
                ast_node_dnit(call);
                free(call);
                ast_node_dnit(arg);
                free(arg);
                return NULL;
            }
            ast_node_init(func, AST_IDENT_EXPR);
            func->token           = token;
            func->ident_expr.name = name;

            call->call_expr.func = func;
            call->call_expr.args = malloc(sizeof(AstList));
            if (call->call_expr.args == NULL)
            {
                parser_error_at_current(parser, "memory allocation failed for argument list in built-in function call");
                ast_node_dnit(func);
                free(func);
                ast_node_dnit(call);
                free(call);
                ast_node_dnit(arg);
                free(arg);
                return NULL;
            }

            ast_list_init(call->call_expr.args);
            ast_list_append(call->call_expr.args, arg);

            return call;
        }

        // check for struct literal
        if (parser->current->kind == TOKEN_L_BRACE)
        {
            AstNode *type = malloc(sizeof(AstNode));
            if (type == NULL)
            {
                parser_error_at_current(parser, "memory allocation failed for struct type node");
                return NULL;
            }
            ast_node_init(type, AST_TYPE_NAME);
            type->token          = token;
            type->type_name.name = name;
            return parser_parse_struct_literal(parser, type);
        }

        AstNode *ident = malloc(sizeof(AstNode));
        if (ident == NULL)
        {
            parser_error_at_current(parser, "memory allocation failed for identifier node");
            return NULL;
        }
        ast_node_init(ident, AST_IDENT_EXPR);
        ident->token           = token;
        ident->ident_expr.name = name;
        return ident;
    }

    // NOTE: this is a fallback for when no valid primary expression is found
    // it will advance the parser and return NULL, which should be handled by
    // the caller. Note that this is one of those contingencies for something
    // that should never happen (the lexer should ensure valid tokens).
    parser_error_at_current(parser, "expected expression");
    parser_advance(parser);

    return NULL;
}

AstNode *parser_parse_array_literal(Parser *parser)
{
    AstNode *array = malloc(sizeof(AstNode));
    if (array == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for array literal");
        return NULL;
    }
    ast_node_init(array, AST_ARRAY_EXPR);
    array->token = parser->previous;

    // parse array type
    if (parser_match(parser, TOKEN_UNDERSCORE))
    {
        // unbound array
        array->array_expr.type = malloc(sizeof(AstNode));
        if (array->array_expr.type == NULL)
        {
            parser_error_at_current(parser, "memory allocation failed for array type");
            ast_node_dnit(array);
            free(array);
            return NULL;
        }
        ast_node_init(array->array_expr.type, AST_TYPE_ARRAY);
        array->array_expr.type->token           = parser->previous;
        array->array_expr.type->type_array.size = NULL;
    }
    else
    {
        // sized array
        AstNode *size = parser_parse_primary(parser);
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
        array->array_expr.type->token           = parser->previous;
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
            AstNode *elem = parser_parse_expression(parser);
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
    parser_advance(parser); // consume '{'

    AstNode *struct_lit = malloc(sizeof(AstNode));
    if (struct_lit == NULL)
    {
        parser_error_at_current(parser, "memory allocation failed for struct literal");
        return NULL;
    }
    ast_node_init(struct_lit, AST_STRUCT_EXPR);
    struct_lit->token            = parser->previous;
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
            ast_node_init(field, AST_FIELD_EXPR);
            field->token            = parser->previous;
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

            field->field_expr.object = parser_parse_expression(parser);
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
    type->token = parser->current;

    switch (parser->current->kind)
    {
    case TOKEN_TYPE_PTR:
        type->type_name.name = strdup("ptr");
        parser_advance(parser);
        break;
    case TOKEN_TYPE_I8:
        type->type_name.name = strdup("i8");
        parser_advance(parser);
        break;
    case TOKEN_TYPE_I16:
        type->type_name.name = strdup("i16");
        parser_advance(parser);
        break;
    case TOKEN_TYPE_I32:
        type->type_name.name = strdup("i32");
        parser_advance(parser);
        break;
    case TOKEN_TYPE_I64:
        type->type_name.name = strdup("i64");
        parser_advance(parser);
        break;
    case TOKEN_TYPE_U8:
        type->type_name.name = strdup("u8");
        parser_advance(parser);
        break;
    case TOKEN_TYPE_U16:
        type->type_name.name = strdup("u16");
        parser_advance(parser);
        break;
    case TOKEN_TYPE_U32:
        type->type_name.name = strdup("u32");
        parser_advance(parser);
        break;
    case TOKEN_TYPE_U64:
        type->type_name.name = strdup("u64");
        parser_advance(parser);
        break;
    case TOKEN_TYPE_F16:
        type->type_name.name = strdup("f16");
        parser_advance(parser);
        break;
    case TOKEN_TYPE_F32:
        type->type_name.name = strdup("f32");
        parser_advance(parser);
        break;
    case TOKEN_TYPE_F64:
        type->type_name.name = strdup("f64");
        parser_advance(parser);
        break;
    case TOKEN_IDENTIFIER:
        type->type_name.name = parser_parse_identifier(parser);
        if (type->type_name.name == NULL)
        {
            parser_error_at_current(parser, "expected identifier for type name");
            ast_node_dnit(type);
            free(type);
            return NULL;
        }
        break;
    default:
        parser_error_at_current(parser, "expected type name");
        type->type_name.name = strdup("error");
        break;
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
    ptr->token         = parser->current;
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
    array->token = parser->current;

    if (parser_match(parser, TOKEN_UNDERSCORE))
    {
        // unbound array
        array->type_array.size = NULL;
    }
    else
    {
        // sized array
        array->type_array.size = parser_parse_primary(parser);
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
    fun->token           = parser->previous;
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

    // parse return type (optional)
    if (parser_is_type_start(parser))
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
    str->token = parser->current;

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
    uni->token = parser->current;

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
