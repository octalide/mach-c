#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "ast.h"

typedef struct Parser
{
    Token **comments;
    Token *last;
    Token *curr;
    Token *next;

    Lexer *lexer;

    Node *root;
} Parser;

Parser *parser_new(Lexer *lexer);
void parser_free(Parser *parser);

void parser_print_error(Parser *parser, Node *node, char *path);

void parser_add_comment(Parser *parser, Token *token);

void parser_advance(Parser *parser);

bool parser_match(Parser *parser, TokenKind type);

Node *parser_new_error(Token *token, char *message);

Node *parse_program(Parser *parser);
Node *parse_identifier(Parser *parser);

Node *parse_lit_int(Parser *parser);
Node *parse_lit_float(Parser *parser);
Node *parse_lit_char(Parser *parser);
Node *parse_lit_string(Parser *parser);

Node *parse_expr_member(Parser *parser, Node *target);
Node *parse_expr_call(Parser *parser, Node *target);
Node *parse_expr_index(Parser *parser, Node *target);
Node *parse_expr_cast(Parser *parser, Node *target);
// Node *parse_expr_new(Parser *parser);
Node *parse_expr_unary(Parser *parser);
Node *parse_expr_binary(Parser *parser, int parent_precedence);
Node *parse_expr_postfix(Parser *parser);
Node *parse_expr_grouping(Parser *parser);
Node *parse_expr_primary(Parser *parser);
Node *parse_expr(Parser *parser);

Node *parse_type_array(Parser *parser);
Node *parse_type_pointer(Parser *parser);
Node *parse_type_function(Parser *parser);
Node *parse_type_struct(Parser *parser);
Node *parse_type_union(Parser *parser);
Node *parse_type(Parser *parser);

Node *parse_field(Parser *parser);

Node *parse_stmt_val(Parser *parser);
Node *parse_stmt_var(Parser *parser);
Node *parse_stmt_def(Parser *parser);
Node *parse_stmt_use(Parser *parser);
Node *parse_stmt_fun(Parser *parser);
Node *parse_stmt_ext(Parser *parser);
Node *parse_stmt_if(Parser *parser);
Node *parse_stmt_or(Parser *parser);
Node *parse_stmt_for(Parser *parser);
Node *parse_stmt_break(Parser *parser);
Node *parse_stmt_continue(Parser *parser);
Node *parse_stmt_return(Parser *parser);
Node *parse_stmt_asm(Parser *parser);
Node *parse_stmt_block(Parser *parser);
Node *parse_stmt_expr(Parser *parser);
Node *parse_stmt(Parser *parser);

Node *parser_parse(Parser *parser);

#endif
