#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "lexer.h"
#include "token.h"

typedef struct Parser
{
    Lexer *lexer;
    Token  current;
    Token  peek;
    bool   has_error;
} Parser;

// core parser functions
void parser_init(Parser *parser, Lexer *lexer);
void parser_dnit(Parser *parser);

// parsing functions
Node *parser_parse_program(Parser *parser);

// utility functions
void parser_advance(Parser *parser);
bool parser_check(Parser *parser, TokenKind kind);
bool parser_match(Parser *parser, TokenKind kind);
bool parser_consume(Parser *parser, TokenKind kind, const char *message);

// expression parsing
Node *parser_parse_expr(Parser *parser);
Node *parser_parse_expr_assignment(Parser *parser);
Node *parser_parse_expr_logical_or(Parser *parser);
Node *parser_parse_expr_logical_and(Parser *parser);
Node *parser_parse_expr_bitwise_or(Parser *parser);
Node *parser_parse_expr_bitwise_xor(Parser *parser);
Node *parser_parse_expr_bitwise_and(Parser *parser);
Node *parser_parse_expr_equality(Parser *parser);
Node *parser_parse_expr_comparison(Parser *parser);
Node *parser_parse_expr_shift(Parser *parser);
Node *parser_parse_expr_term(Parser *parser);
Node *parser_parse_expr_factor(Parser *parser);
Node *parser_parse_expr_unary(Parser *parser);
Node *parser_parse_expr_postfix(Parser *parser);
Node *parser_parse_expr_primary(Parser *parser);
Node *parser_parse_expr_call(Parser *parser, Node *target);
Node *parser_parse_expr_index(Parser *parser, Node *target);
Node *parser_parse_expr_member(Parser *parser, Node *target);
Node *parser_parse_expr_cast(Parser *parser, Node *target);

// literal parsing
Node *parser_parse_lit_int(Parser *parser);
Node *parser_parse_lit_float(Parser *parser);
Node *parser_parse_lit_char(Parser *parser);
Node *parser_parse_lit_string(Parser *parser);
Node *parser_parse_lit_array(Parser *parser);
Node *parser_parse_lit_struct(Parser *parser);

// statement parsing
Node *parser_parse_stmt(Parser *parser);
Node *parser_parse_stmt_block(Parser *parser);
Node *parser_parse_stmt_val(Parser *parser);
Node *parser_parse_stmt_var(Parser *parser);
Node *parser_parse_stmt_def(Parser *parser);
Node *parser_parse_stmt_fun(Parser *parser);
Node *parser_parse_stmt_str(Parser *parser);
Node *parser_parse_stmt_uni(Parser *parser);
Node *parser_parse_stmt_ext(Parser *parser);
Node *parser_parse_stmt_use(Parser *parser);
Node *parser_parse_stmt_if(Parser *parser);
Node *parser_parse_stmt_or(Parser *parser);
Node *parser_parse_stmt_for(Parser *parser);
Node *parser_parse_stmt_break(Parser *parser);
Node *parser_parse_stmt_continue(Parser *parser);
Node *parser_parse_stmt_return(Parser *parser);
Node *parser_parse_stmt_expr(Parser *parser);

// type parsing
Node *parser_parse_type(Parser *parser);
Node *parser_parse_type_array(Parser *parser, Node *base_type);
Node *parser_parse_type_array_prefix(Parser *parser);
Node *parser_parse_type_pointer(Parser *parser, Node *base_type);
Node *parser_parse_type_function(Parser *parser);

// helper functions
Node **parser_parse_parameter_list(Parser *parser, bool *is_variadic);
Node **parser_parse_type_parameter_list(Parser *parser, bool *is_variadic);
Node **parser_parse_argument_list(Parser *parser);
Node **parser_parse_field_list(Parser *parser);
Node  *parser_parse_identifier(Parser *parser);

// error handling
void parser_error(Parser *parser, const char *message);
void parser_synchronize(Parser *parser);

#endif
