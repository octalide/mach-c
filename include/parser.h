#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>

#include "token.h"
#include "ast.h"
#include "lexer.h"
#include "keywords.h"

typedef struct ParseError
{
    const char *message;
    Token token;
} ParseError;

typedef struct Parser
{
    Lexer *lexer;

    Token last;
    Token curr;
    Token next;

    Token **comments;
    ParseError **errors;

    bool opt_verbose;
} Parser;

void parser_init(Parser *parser, Lexer *lexer);
void parser_free(Parser *parser);

void parser_add_comment(Parser *parser, Token token);

void parser_add_error(Parser *parser, Token token, const char *message);
bool parser_has_errors(Parser *parser);
void parser_print_errors(Parser *parser);

void parser_advance(Parser *parser);

bool parser_match_last(Parser *parser, TokenType type);
bool parser_match(Parser *parser, TokenType type);
bool parser_match_next(Parser *parser, TokenType type);
bool parser_consume(Parser *parser, TokenType type);

Node *parse_block(Parser *parser);
Node *parse_identifier(Parser *parser);

Node *parse_postfix(Parser *parser);
Node *parse_expr_identifier(Parser *parser);
Node *parse_expr_lit_char(Parser *parser);
Node *parse_expr_lit_number(Parser *parser);
Node *parse_expr_lit_string(Parser *parser);
Node *parse_expr_call(Parser *parser);
Node *parse_expr_index(Parser *parser);
Node *parse_expr_def_str(Parser *parser);
Node *parse_expr_def_uni(Parser *parser);
Node *parse_expr_def_array(Parser *parser);
Node *parse_expr_unary(Parser *parser);
Node *parse_expr_binary(Parser *parser, int parent_precedence);
Node *parse_expr_grouping(Parser *parser);
Node *parse_expr_primary(Parser *parser);
Node *parse_expr(Parser *parser);

Node *parse_type_array(Parser *parser);
Node *parse_type_ref(Parser *parser);
Node *parse_type_fun(Parser *parser);
Node *parse_type_str(Parser *parser);
Node *parse_type_uni(Parser *parser);
Node *parse_field(Parser *parser);
Node *parse_type(Parser *parser);

Node *parse_stmt_use(Parser *parser);
Node *parse_stmt_if(Parser *parser);
Node *parse_stmt_or(Parser *parser);
Node *parse_stmt_for(Parser *parser);
Node *parse_stmt_brk(Parser *parser);
Node *parse_stmt_cnt(Parser *parser);
Node *parse_stmt_ret(Parser *parser);
Node *parse_stmt_decl_type(Parser *parser);
Node *parse_stmt_decl_val(Parser *parser);
Node *parse_stmt_decl_var(Parser *parser);
Node *parse_stmt_decl_fun(Parser *parser);
Node *parse_stmt_decl_str(Parser *parser);
Node *parse_stmt_decl_uni(Parser *parser);
Node *parse_stmt_expr(Parser *parser);
Node *parse_stmt(Parser *parser);

Node *parser_parse(Parser *parser);

#endif // PARSER_H
