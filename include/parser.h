#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>

#include "token.h"
#include "node.h"
#include "lexer.h"

typedef struct Parser
{
    Lexer *lexer;

    Token *last;
    Token *curr;
    Token *next;

    Token **comments;

    bool opt_verbose;
} Parser;

Parser *parser_new(char *source);
void parser_free(Parser *parser);

void parser_print_error(Parser *parser, Node *node, char *path);

void parser_add_comment(Parser *parser, Token *token);
bool parser_has_comments(Parser *parser);

void parser_advance(Parser *parser);

bool parser_match(Parser *parser, TokenKind type);
bool parser_consume(Parser *parser, TokenKind type);

Node *parser_error(Token *token, char *message);

Node *parse_program(Parser *parser);
Node *parse_identifier(Parser *parser, Node *parent);
Node *parse_lit_int(Parser *parser, Node *parent);
Node *parse_lit_float(Parser *parser, Node *parent);
Node *parse_lit_char(Parser *parser, Node *parent);
Node *parse_lit_string(Parser *parser, Node *parent);
Node *parse_expr_member(Parser *parser, Node *parent, Node *target);
Node *parse_expr_call(Parser *parser, Node *parent, Node *target);
Node *parse_expr_index(Parser *parser, Node *parent, Node *target);
Node *parse_expr_cast(Parser *parser, Node *parent, Node *target);
Node *parse_expr_new(Parser *parser, Node *parent);
Node *parse_expr_unary(Parser *parser, Node *parent);
Node *parse_expr_binary(Parser *parser, Node *parent, int parent_precedence);
Node *parse_type_array(Parser *parser, Node *parent);
Node *parse_type_pointer(Parser *parser, Node *parent);
Node *parse_type_fun(Parser *parser, Node *parent);
Node *parse_type_str(Parser *parser, Node *parent);
Node *parse_type_uni(Parser *parser, Node *parent);
Node *parse_field(Parser *parser, Node *parent);
Node *parse_stmt_val(Parser *parser, Node *parent);
Node *parse_stmt_var(Parser *parser, Node *parent);
Node *parse_stmt_def(Parser *parser, Node *parent);
Node *parse_stmt_use(Parser *parser, Node *parent);
Node *parse_stmt_fun(Parser *parser, Node *parent);
Node *parse_stmt_ext(Parser *parser, Node *parent);
Node *parse_stmt_if(Parser *parser, Node *parent);
Node *parse_stmt_or(Parser *parser, Node *parent);
Node *parse_stmt_for(Parser *parser, Node *parent);
Node *parse_stmt_brk(Parser *parser, Node *parent);
Node *parse_stmt_cnt(Parser *parser, Node *parent);
Node *parse_stmt_ret(Parser *parser, Node *parent);
Node *parse_stmt_asm(Parser *parser, Node *parent);
Node *parse_stmt_block(Parser *parser, Node *parent);
Node *parse_stmt_expr(Parser *parser, Node *parent);

Node *parse_expr_postfix(Parser *parser, Node *parent);
Node *parse_expr_grouping(Parser *parser, Node *parent);
Node *parse_expr_primary(Parser *parser, Node *parent);
Node *parse_expr(Parser *parser, Node *parent);
Node *parse_type(Parser *parser, Node *parent);
Node *parse_stmt(Parser *parser, Node *parent);

Node *parser_parse(Parser *parser);

#endif // PARSER_H
