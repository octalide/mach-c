#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "lexer.h"
#include "token.h"

#include <stdbool.h>

typedef struct ParserError
{
    Token *token;
    char  *message;
} ParserError;

typedef struct ParserErrorList
{
    ParserError *errors;
    int          count;
    int          capacity;
} ParserErrorList;

typedef struct Parser
{
    Lexer          *lexer;
    Token          *current;
    Token          *previous;
    ParserErrorList errors;
    bool            panic_mode;
    bool            had_error;
} Parser;

// parser lifecycle
void parser_init(Parser *parser, Lexer *lexer);
void parser_dnit(Parser *parser);

// main parsing function
AstNode *parser_parse_program(Parser *parser);

// declaration parsing
AstNode *parser_parse_declaration(Parser *parser);
AstNode *parser_parse_use_decl(Parser *parser);
AstNode *parser_parse_ext_decl(Parser *parser);
AstNode *parser_parse_def_decl(Parser *parser);
AstNode *parser_parse_val_decl(Parser *parser);
AstNode *parser_parse_var_decl(Parser *parser);
AstNode *parser_parse_fun_decl(Parser *parser);
AstNode *parser_parse_str_decl(Parser *parser);
AstNode *parser_parse_uni_decl(Parser *parser);

// statement parsing
AstNode *parser_parse_statement(Parser *parser);
AstNode *parser_parse_block_stmt(Parser *parser);
AstNode *parser_parse_expr_stmt(Parser *parser);
AstNode *parser_parse_ret_stmt(Parser *parser);
AstNode *parser_parse_if_stmt(Parser *parser);
AstNode *parser_parse_for_stmt(Parser *parser);
AstNode *parser_parse_brk_stmt(Parser *parser);
AstNode *parser_parse_cnt_stmt(Parser *parser);

// expression parsing
AstNode *parser_parse_expression(Parser *parser);
AstNode *parser_parse_assignment(Parser *parser);
AstNode *parser_parse_logical_or(Parser *parser);
AstNode *parser_parse_logical_and(Parser *parser);
AstNode *parser_parse_equality(Parser *parser);
AstNode *parser_parse_comparison(Parser *parser);
AstNode *parser_parse_bit_shift(Parser *parser);
AstNode *parser_parse_addition(Parser *parser);
AstNode *parser_parse_multiplication(Parser *parser);
AstNode *parser_parse_bitwise(Parser *parser);
AstNode *parser_parse_unary(Parser *parser);
AstNode *parser_parse_postfix(Parser *parser);
AstNode *parser_parse_primary(Parser *parser);

// type parsing
AstNode *parser_parse_type(Parser *parser);
AstNode *parser_parse_type_name(Parser *parser);
AstNode *parser_parse_type_ptr(Parser *parser);
AstNode *parser_parse_type_array(Parser *parser);
AstNode *parser_parse_type_fun(Parser *parser);
AstNode *parser_parse_type_str(Parser *parser);
AstNode *parser_parse_type_uni(Parser *parser);

// helper functions
void parser_advance(Parser *parser);
bool parser_check(Parser *parser, TokenKind kind);
bool parser_match(Parser *parser, TokenKind kind);
bool parser_consume(Parser *parser, TokenKind kind, const char *message);
void parser_synchronize(Parser *parser);

// error handling
void parser_error(Parser *parser, Token *token, const char *message);
void parser_error_at_current(Parser *parser, const char *message);
void parser_error_at_previous(Parser *parser, const char *message);

// utility functions
bool     parser_is_at_end(Parser *parser);
bool     parser_is_type_start(Parser *parser);
bool     parser_is_declaration_start(Parser *parser);
char    *parser_parse_identifier(Parser *parser);
AstList *parser_parse_field_list(Parser *parser);
AstList *parser_parse_parameter_list(Parser *parser);
AstList *parser_parse_argument_list(Parser *parser);
AstNode *parser_parse_array_literal(Parser *parser);
AstNode *parser_parse_struct_literal(Parser *parser, AstNode *type);

// error list operations
void parser_error_list_init(ParserErrorList *list);
void parser_error_list_dnit(ParserErrorList *list);
void parser_error_list_add(ParserErrorList *list, Token *token, const char *message);
void parser_error_list_print(ParserErrorList *list, Lexer *lexer, const char *file_path);

#endif
