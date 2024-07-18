#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>
#include "token.h"
#include "ast.h"
#include "lexer.h"

typedef struct
{
    Lexer *lexer;
    Token current;
    Token previous;
} Parser;

void init_parser(Parser *parser, Lexer *lexer);
NodeProgram *parse(Parser *parser);

void parser_advance(Parser *parser);
bool parser_match(Parser *parser, TokenType type);
void parser_consume(Parser *parser, TokenType type, const char *message);

NodeIdentifier *parse_identifier(Parser *parser);
NodeComment *parse_comment(Parser *parser);
NodeStmt *parse_stmt(Parser *parser);
NodeExpr *parse_expr(Parser *parser);
NodeBlock *parse_block(Parser *parser);
NodeStmtUse *parse_stmt_use(Parser *parser);
NodeStmtFun *parse_stmt_fun(Parser *parser);
NodeStmtStr *parse_stmt_str(Parser *parser);
NodeStmtVal *parse_stmt_val(Parser *parser);
NodeStmtVar *parse_stmt_var(Parser *parser);
NodeStmtIf *parse_stmt_if(Parser *parser);
NodeStmtFor *parse_stmt_for(Parser *parser);
NodeStmtBrk *parse_stmt_brk(Parser *parser);
NodeStmtCnt *parse_stmt_cnt(Parser *parser);
NodeStmtRet *parse_stmt_ret(Parser *parser);
NodeStmtAssign *parse_stmt_assign(Parser *parser);
NodeStmtExpr *parse_stmt_expr(Parser *parser);
NodeExprLiteral *parse_expr_literal(Parser *parser);
NodeExprType *parse_expr_type(Parser *parser);
NodeExprUnary *parse_expr_unary(Parser *parser);
NodeExprBinary *parse_expr_binary(Parser *parser);
NodeExprCall *parse_expr_call(Parser *parser);
NodeExprIndex *parse_expr_index(Parser *parser);

#endif // PARSER_H
