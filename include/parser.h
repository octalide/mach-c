#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>
#include "token.h"
#include "ast.h"
#include "lexer.h"

typedef struct Parser
{
    Lexer *lexer;
    Token current;
    Token previous;
    const char *error;
} Parser;


void parser_init(Parser *parser, Lexer *lexer);
void parser_error(Parser *parser, const char *message);
void parser_advance(Parser *parser);
bool parser_peek(Parser *parser, TokenType type);
bool parser_match(Parser *parser, TokenType type);
bool parser_consume(Parser *parser, TokenType type, const char *message);

Node *parse_expr_type_identifier(Parser *parser);
Node *parse_expr_type_array(Parser *parser);
Node *parse_expr_type_ref(Parser *parser);
Node *parse_expr_type_member(Parser *parser, Node *target);
Node *parse_expr_type(Parser *parser);
Node *parse_expr_call(Parser *parser, Node *target);
Node *parse_expr_index(Parser *parser, Node *target);
Node *parse_expr_array(Parser *parser);

Node *parse_expr_unary(Parser *parser);
Node *parse_expr_binary(Parser *parser, int precedence_parent);
Node *parse_expr_grouping(Parser *parser);
Node *parse_expr_primary(Parser *parser);

Node *parse_stmt_block(Parser *parser);
Node *parse_stmt_use(Parser *parser);
Node *parse_stmt_fun(Parser *parser);
Node *parse_stmt_str(Parser *parser);
Node *parse_stmt_val(Parser *parser);
Node *parse_stmt_var(Parser *parser);
Node *parse_stmt_if(Parser *parser);
Node *parse_stmt_for(Parser *parser);
Node *parse_stmt_brk(Parser *parser);
Node *parse_stmt_cnt(Parser *parser);
Node *parse_stmt_ret(Parser *parser);
Node *parse_stmt_expr(Parser *parser);

Node *parse_stmt(Parser *parser);
Node *parse_expr(Parser *parser);
Node *parse(Parser *parser);

#endif // PARSER_H
