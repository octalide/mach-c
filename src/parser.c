#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "op.h"
#include "parser.h"

void init_parser(Parser *parser, Lexer *lexer)
{
    parser->lexer = lexer;
    parser->current = next_token(lexer);
    parser->previous.type = TOKEN_EOF;

    printf("P INIT: ln: %-3d type: %-19s len: %-3d val: %.*s\n", parser->lexer->line, token_type_string(parser->current.type), parser->current.length, parser->current.length, parser->current.start);
}

void parser_advance(Parser *parser)
{
    parser->previous = parser->current;
    parser->current = next_token(parser->lexer);

    printf("P ADVN: ln: %-3d type: %-19s len: %-3d val: %.*s\n", parser->lexer->line, token_type_string(parser->current.type), parser->current.length, parser->current.length, parser->current.start);
}

bool parser_match(Parser *parser, TokenType type)
{
    // printf("parser_match\n");
    if (parser->current.type == type)
    {
        parser_advance(parser);
        return true;
    }

    return false;
}

void parser_consume(Parser *parser, TokenType type, const char *message)
{
    printf("parser_consume\n");
    if (!parser_match(parser, type))
    {
        fprintf(stderr, "error: %s\n", message);
        exit(1);
    }
}

void parser_error(Parser *parser, const char *message)
{
    fprintf(stderr, "error: %s\n", message);
    exit(1);
}

NodeIdentifier *parse_identifier(Parser *parser)
{
    printf("parse_identifier\n");
    parser_consume(parser, TOKEN_IDENTIFIER, "expected identifier");

    return new_node_identifier(parser->previous);
}

NodeComment *parse_comment(Parser *parser)
{
    printf("parse_comment\n");
    parser_consume(parser, TOKEN_COMMENT, "expected comment");

    return new_node_comment(parser->previous);
}

NodeStmt *parse_stmt(Parser *parser)
{
    printf("parse_stmt\n");
    if (parser_match(parser, TOKEN_USE))
    {
        return parse_stmt_use(parser);
    }
    else if (parser_match(parser, TOKEN_FUN))
    {
        return parse_stmt_fun(parser);
    }
    else if (parser_match(parser, TOKEN_STR))
    {
        return parse_stmt_str(parser);
    }
    else if (parser_match(parser, TOKEN_VAL))
    {
        return parse_stmt_val(parser);
    }
    else if (parser_match(parser, TOKEN_VAR))
    {
        return parse_stmt_var(parser);
    }
    else if (parser_match(parser, TOKEN_IF))
    {
        return parse_stmt_if(parser);
    }
    else if (parser_match(parser, TOKEN_FOR))
    {
        return parse_stmt_for(parser);
    }
    else if (parser_match(parser, TOKEN_BRK))
    {
        return parse_stmt_brk(parser);
    }
    else if (parser_match(parser, TOKEN_CNT))
    {
        return parse_stmt_cnt(parser);
    }
    else if (parser_match(parser, TOKEN_RET))
    {
        return parse_stmt_ret(parser);
    }
    else if (parser_match(parser, TOKEN_LEFT_BRACE))
    {
        return parse_stmt_expr(parser);
    }

    return parse_stmt_expr(parser);
}

// examples:
// `use foo.bar;`
// format:
// `<TOKEN_FUN> <TOKEN_IDENTITY>[<TOKEN_DOT><TOKEN_IDENTITY>...]<TOKEN_SEMICOLON>`
NodeStmtUse *parse_stmt_use(Parser *parser)
{
    printf("parse_stmt_use\n");
    NodeIdentifier *identifiers = malloc(sizeof(NodeIdentifier));
    int count = 0;

    while (parser->current.type != TOKEN_SEMICOLON)
    {
        identifiers = realloc(identifiers, sizeof(NodeIdentifier) * (count + 1));
        NodeIdentifier *identifier = parse_identifier(parser);
        identifiers[count++] = *identifier;

        if (!parser_match(parser, TOKEN_DOT))
        {
            break;
        }
    }

    parser_consume(parser, TOKEN_SEMICOLON, "expected semicolon");

    return new_node_stmt_use(identifiers, count);
}

// examples:
// `fun foo(): void {}`
// `fun foo(bar: #baz): bif {}`
// `fun foo(bar: #baz, bif: bam): #bop {}`
// format:
// `<TOKEN_FUN> <TOKEN_IDENTITY>([<NodeStmtFunParam>|<TOKEN_COMMA>|...])<NodeExprType> <NodeBlock>`
NodeStmtFun *parse_stmt_fun(Parser *parser)
{
    printf("parse_stmt_fun\n");
    NodeIdentifier *identifier = malloc(sizeof(NodeIdentifier));
    NodeStmtFunParam *parameters = malloc(sizeof(NodeStmtFunParam));
    NodeExprType *return_type = malloc(sizeof(NodeExprType));
    NodeBlock *body = malloc(sizeof(NodeBlock));

    identifier = parse_identifier(parser);

    parser_consume(parser, TOKEN_LEFT_PAREN, "expected left parenthesis");

    int count = 0;
    while (parser->current.type != TOKEN_RIGHT_PAREN)
    {
        parameters = realloc(parameters, sizeof(NodeStmtFunParam) * (count + 1));
        NodeIdentifier *param_identifier = parse_identifier(parser);
        NodeExprType *param_type = parse_expr_type(parser);
        parameters[count++] = *new_node_stmt_fun_param(param_identifier, param_type);

        if (!parser_match(parser, TOKEN_COMMA))
        {
            break;
        }
    }

    parser_consume(parser, TOKEN_RIGHT_PAREN, "expected right parenthesis");

    return_type = parse_expr_type(parser);
    body = parse_block(parser);

    return new_node_stmt_fun(identifier, parameters, count, return_type, body);
}

// examples:
// `str foo: {}`
// `str foo: { var bar: baz; }`
// `str foo: { var bar: baz; var bim: #bam; }`
// format:
// `<TOKEN_STR> <TOKEN_IDENTITY><TOKEN_COLON> <TOKEN_BRACE_LEFT> [<NodeStmtStrField>...] <TOKEN_BRACE_RIGHT>`
NodeStmtStr *parse_stmt_str(Parser *parser)
{
    printf("parse_stmt_str\n");
    NodeIdentifier *identifier = malloc(sizeof(NodeIdentifier));
    NodeStmtStrField *fields = malloc(sizeof(NodeStmtStrField));

    identifier = parse_identifier(parser);

    parser_consume(parser, TOKEN_COLON, "expected colon");
    parser_consume(parser, TOKEN_LEFT_BRACE, "expected left brace");

    int count = 0;
    while (true)
    {
        if (parser->current.type == TOKEN_RIGHT_BRACE)
        {
            break;
        }

        fields = realloc(fields, sizeof(NodeStmtStrField) * (count + 1));
        NodeIdentifier *field_identifier = parse_identifier(parser);
        NodeExprType *field_type = parse_expr_type(parser);
        fields[count++] = *new_node_stmt_str_field(field_identifier, field_type);

        if (!parser_match(parser, TOKEN_SEMICOLON))
        {
            break;
        }
    }

    parser_consume(parser, TOKEN_RIGHT_BRACE, "expected right brace");

    return new_node_stmt_str(identifier, fields, count);
}

// examples:
// `val foo: bar;`
// `val foo: #bar;`
// `val foo: bar = baz;`
// format:
// `<TOKEN_VAL> <TOKEN_IDENTIFIER><NodeExprType> <TOKEN_EQUAL><NodeExpr>`
NodeStmtVal *parse_stmt_val(Parser *parser)
{
    printf("parse_stmt_val\n");
    NodeIdentifier *identifier = malloc(sizeof(NodeIdentifier));
    NodeExprType *type = malloc(sizeof(NodeExprType));
    NodeExpr *initializer = malloc(sizeof(NodeExpr));

    identifier = parse_identifier(parser);
    type = parse_expr_type(parser);

    if (parser_match(parser, TOKEN_EQUAL))
    {
        initializer = parse_expr(parser);
    }

    if (initializer == NULL)
    {
        parser_error(parser, "expected initializer");
    }

    return new_node_stmt_val(identifier, type, initializer);
}

// examples:
// `var foo: bar;`
// `var foo: #bar;`
// `var foo: bar = baz;`
// format:
// `<TOKEN_VAR> <TOKEN_IDENTIFIER><NodeExprType> [<TOKEN_EQUAL><NodeExpr>]`
NodeStmtVar *parse_stmt_var(Parser *parser)
{
    printf("parse_stmt_var\n");
    NodeIdentifier *identifier = malloc(sizeof(NodeIdentifier));
    NodeExprType *type = malloc(sizeof(NodeExprType));
    NodeExpr *initializer = malloc(sizeof(NodeExpr));

    identifier = parse_identifier(parser);
    type = parse_expr_type(parser);

    if (parser_match(parser, TOKEN_EQUAL))
    {
        initializer = parse_expr(parser);
    }

    return new_node_stmt_var(identifier, type, initializer);
}

// examples:
// `if (foo == bar) {}`
// `if (foo == bar) {} or {}`
// `if (foo == bar) {} or (bar == baz) {}`
// `if (foo == bar) {} or (bar == baz) {} or {}`
// format:
// `<TOKEN_IF> <TOKEN_RIGHT_PAREN><NodeExpr><TOKEN_RIGHT_PAREN> <NodeBlock> [<TOKEN_OR> [<TOKEN_RIGHT_PAREN><NodeExpr><TOKEN_PAREN_RIGHT>]<NodeBlock>]...`
NodeStmtIf *parse_stmt_if(Parser *parser)
{
    printf("parse_stmt_if\n");
    NodeExpr *condition = malloc(sizeof(NodeExpr));
    NodeBlock *body = malloc(sizeof(NodeBlock));
    NodeStmtIf *branch = NULL;

    parser_consume(parser, TOKEN_LEFT_PAREN, "expected left parenthesis");
    condition = parse_expr(parser);
    parser_consume(parser, TOKEN_RIGHT_PAREN, "expected right parenthesis");

    body = parse_block(parser);

    while (parser_match(parser, TOKEN_OR))
    {
        NodeExpr *condition = NULL;
        NodeBlock *body = malloc(sizeof(NodeBlock));

        if (parser_match(parser, TOKEN_LEFT_PAREN))
        {
            condition = parse_expr(parser);
            parser_consume(parser, TOKEN_RIGHT_PAREN, "expected right parenthesis");
        }

        body = parse_block(parser);

        branch = realloc(branch, sizeof(NodeStmtIf));
        branch = new_node_stmt_if(condition, body, branch);
    }

    return new_node_stmt_if(condition, body, branch);
}

// examples:
// `for (i < 1) {}`
// format:
// `<TOKEN_FOR> [<TOKEN_RIGHT_PAREN><NodeExpr><TOKEN_RIGHT_PAREN>] <NodeBlock>`
NodeStmtFor *parse_stmt_for(Parser *parser)
{
    printf("parse_stmt_for\n");
    NodeExpr *condition = malloc(sizeof(NodeExpr));
    NodeBlock *body = malloc(sizeof(NodeBlock));

    if (parser_match(parser, TOKEN_LEFT_PAREN))
    {
        condition = parse_expr(parser);
        parser_consume(parser, TOKEN_RIGHT_PAREN, "expected right parenthesis");
    }

    body = parse_block(parser);

    return new_node_stmt_for(condition, body);
}

// examples:
// `brk;`
// format:
// `<TOKEN_BRK><TOKEN_SEMICOLON>`
NodeStmtBrk *parse_stmt_brk(Parser *parser)
{
    printf("parse_stmt_brk\n");
    parser_consume(parser, TOKEN_SEMICOLON, "expected semicolon");

    return new_node_stmt_brk();
}

// examples:
// `cnt;`
// format:
// `<TOKEN_CNT><TOKEN_SEMICOLON>`
NodeStmtCnt *parse_stmt_cnt(Parser *parser)
{
    printf("parse_stmt_cnt\n");
    parser_consume(parser, TOKEN_SEMICOLON, "expected semicolon");

    return new_node_stmt_cnt();
}

// examples:
// `ret;`
// `ret foo;`
// format:
// `<TOKEN_RET> [<NodeExpr>]<TOKEN_SEMICOLON>`
NodeStmtRet *parse_stmt_ret(Parser *parser)
{
    printf("parse_stmt_ret\n");
    NodeExpr *value = malloc(sizeof(NodeExpr));

    if (parser->current.type != TOKEN_SEMICOLON)
    {
        value = parse_expr(parser);
    }

    parser_consume(parser, TOKEN_SEMICOLON, "expected semicolon");

    return new_node_stmt_ret(value);
}

// examples:
// `{ foo; }`
// `{ foo; bar; }`
// format:
// `<TOKEN_BRACE_LEFT> [<NodeStmt>...] <TOKEN_BRACE_RIGHT>`
NodeBlock *parse_block(Parser *parser)
{
    printf("parse_block\n");
    NodeStmt *statements = malloc(sizeof(NodeStmt));
    int count = 0;

    parser_consume(parser, TOKEN_LEFT_BRACE, "expected left brace");

    while (parser->current.type != TOKEN_RIGHT_BRACE)
    {
        statements = realloc(statements, sizeof(NodeStmt) * (count + 1));
        NodeStmt *statement = parse_stmt(parser);
        statements[count++] = *statement;
    }

    parser_consume(parser, TOKEN_RIGHT_BRACE, "expected right brace");

    return new_node_block(statements, count);
}

// examples:
// `foo;`
// `foo(bar);`
// `foo + bar;`
// format:
// `<NodeExpr><TOKEN_SEMICOLON>`
NodeStmtExpr *parse_stmt_expr(Parser *parser)
{
    printf("parse_stmt_expr\n");
    NodeExpr *expression = parse_expr(parser);

    parser_consume(parser, TOKEN_SEMICOLON, "expected semicolon");

    return new_node_stmt_expr(expression);
}

// examples:
// `123`
// `"foo"`
// `'a'`
// format:
// `<TOKEN_NUMBER>`
// `<TOKEN_CHARACTER>`
// `<TOKEN_STRING>`
NodeExprLiteral *parse_expr_literal(Parser *parser)
{
    printf("parse_expr_literal\n");
    parser_advance(parser);

    return new_node_expr_literal(parser->previous);
}

// examples:
// `+foo`
// `-foo`
// `!foo`
// format:
// `<OPERATOR><NodeExpr>`
NodeExprUnary *parse_expr_unary(Parser *parser)
{
    printf("parse_expr_unary\n");
    Token *op = NULL;

    switch (parser->current.type)
    {
    case TOKEN_PLUS:     // + positive
    case TOKEN_MINUS:    // - negative
    case TOKEN_TILDE:    // ~ bitwise not
    case TOKEN_BANG:     // ! logical not
    case TOKEN_QUESTION: // ? address
    case TOKEN_AT:       // @ dereference
        op = &parser->current;
        parser_advance(parser);
        break;
    default:
        break;
    }

    if (op == NULL)
    {
        return NULL;
    }

    NodeExpr *operand = parse_expr(parser);

    if (operand == NULL)
    {
        parser_error(parser, "expected expression");
    }

    return new_node_expr_unary(*op, operand);
}

// examples:
// `foo + bar`
// `foo - bar`
// `foo * bar`
// `foo / bar`
// format:
// `<NodeExpr><OPERATOR><NodeExpr>`
NodeExprBinary *parse_expr_binary(Parser *parser)
{
    printf("parse_expr_binary\n");
    Token *op = NULL;
    NodeExpr *left = parse_expr(parser);

    switch (parser->current.type)
    {
    case TOKEN_PLUS:                // +  addition
    case TOKEN_MINUS:               // -  subtraction
    case TOKEN_STAR:                // *  multiplication
    case TOKEN_SLASH:               // /  division
    case TOKEN_PERCENT:             // %  modulus
    case TOKEN_AMPERSAND:           // &  bitwise and
    case TOKEN_PIPE:                // |  bitwise or
    case TOKEN_CARET:               // ^  bitwise xor
    case TOKEN_LESS:                // <  less than
    case TOKEN_GREATER:             // >  greater than
    case TOKEN_EQUAL_EQUAL:         // == equal
    case TOKEN_BANG_EQUAL:          // != not equal
    case TOKEN_LESS_EQUAL:          // <= less than or equal
    case TOKEN_GREATER_EQUAL:       // >= greater than or equal
    case TOKEN_LESS_LESS:           // << left shift
    case TOKEN_GREATER_GREATER:     // >> right shift
    case TOKEN_AMPERSAND_AMPERSAND: // && logical and
    case TOKEN_PIPE_PIPE:           // || logical or
        op = &parser->current;
        parser_advance(parser);
        break;
    default:
        break;
    }

    if (op == NULL)
    {
        return NULL;
    }

    NodeExpr *right = parse_expr(parser);
    if (right == NULL)
    {
        parser_error(parser, "expected expression");
    }

    return new_node_expr_binary(*op, left, right);
}

// examples:
// `foo()`
// `foo(bar)`
// `foo(bar, baz)`
// format:
// `<NodeExpr>([<NodeExpr>|<TOKEN_COMMA>|...])`
NodeExprCall *parse_expr_call(Parser *parser)
{
    printf("parse_expr_call\n");
    NodeExpr *callee = parse_expr(parser);
    NodeExpr *arguments = malloc(sizeof(NodeExpr));
    int count = 0;

    parser_consume(parser, TOKEN_LEFT_PAREN, "expected left parenthesis");

    while (parser->current.type != TOKEN_RIGHT_PAREN)
    {
        arguments = realloc(arguments, sizeof(NodeExpr) * (count + 1));
        NodeExpr *argument = parse_expr(parser);
        arguments[count++] = *argument;

        if (!parser_match(parser, TOKEN_COMMA))
        {
            break;
        }
    }

    parser_consume(parser, TOKEN_RIGHT_PAREN, "expected right parenthesis");

    return new_node_expr_call(callee, arguments, count);
}

// examples:
// `foo[0]`
// `foo[bar]`
// format:
// `<NodeExpr>[<NodeExpr>]`
NodeExprIndex *parse_expr_index(Parser *parser)
{
    printf("parse_expr_index\n");
    NodeExpr *target = parse_expr(parser);

    parser_consume(parser, TOKEN_LEFT_BRACKET, "expected left bracket");

    NodeExpr *index = parse_expr(parser);

    parser_consume(parser, TOKEN_RIGHT_BRACKET, "expected right bracket");

    return new_node_expr_index(target, index);
}

// examples:
// `: foo`
// `: #bar`
// format:
// `<TOKEN_COLON> [<TOKEN_HASH> || <TOKEN_RIGHT_BRACE><TOKEN_NUMBER><TOKEN_LEFT_BRACE>...]<TOKEN_IDENTITY>`
NodeExprType *parse_expr_type(Parser *parser)
{
    parser_consume(parser, TOKEN_COLON, "expected colon");

    if (parser_match(parser, TOKEN_HASH))
    {
        return new_node_expr_type(parse_identifier(parser), true);
    }

    return new_node_expr_type(parse_identifier(parser), false);
}

NodeExpr *parse_primary(Parser *parser);
NodeExpr *parse_unary(Parser *parser);
NodeExpr *parse_binary(Parser *parser, int parentPrecedence);
NodeExpr *parse_grouping(Parser *parser);
NodeExpr *parse_expr_identifier(Parser *parser);

NodeExpr *parse_primary(Parser *parser)
{
    printf("parse_primary\n");
    switch (parser->current.type)
    {
    case TOKEN_NUMBER:
    case TOKEN_CHARACTER:
    case TOKEN_STRING:
        return parse_expr_literal(parser);
    case TOKEN_IDENTIFIER:
        return parse_expr_identifier(parser);
    case TOKEN_LEFT_PAREN:
        return parse_grouping(parser);
    default:
        return NULL;
    }
}

NodeExpr *parse_expr_identifier(Parser *parser)
{
    printf("parse_expr_identifier\n");
    NodeIdentifier *identifier = parse_identifier(parser);

    if (parser_match(parser, TOKEN_LEFT_PAREN))
    {
        return parse_expr_call(parser);
    }

    if (parser_match(parser, TOKEN_LEFT_BRACKET))
    {
        return parse_expr_index(parser);
    }

    if (parser_match(parser, TOKEN_EQUAL))
    {
        NodeExpr *value = parse_expr(parser);

        return new_node_expr_assign(identifier, value);
    }

    return new_node_expr_identifier(identifier);
}

NodeExpr *parse_unary(Parser *parser)
{
    printf("parse_unary\n");
    if (parser_match(parser, TOKEN_PLUS) || parser_match(parser, TOKEN_MINUS) || parser_match(parser, TOKEN_TILDE) || parser_match(parser, TOKEN_BANG) || parser_match(parser, TOKEN_QUESTION) || parser_match(parser, TOKEN_AT))
    {
        Token operator= parser->previous;
        NodeExpr *right = parse_unary(parser);

        return new_node_expr_unary(operator, right);
    }

    return parse_primary(parser);
}

NodeExpr *parse_binary(Parser *parser, int parentPrecedence)
{
    printf("parse_binary\n");
    NodeExpr *left = parse_unary(parser);

    while (true)
    {
        int precedence = get_precedence(parser->current.type);
        if (precedence <= parentPrecedence)
        {
            break;
        }

        parser_advance(parser);
        NodeExpr *right = parse_binary(parser, precedence);
        left = new_node_expr_binary(parser->previous, left, right);
    }

    return left;
}

NodeExpr *parse_grouping(Parser *parser)
{
    printf("parse_grouping\n");
    NodeExpr *expr = parse_expr(parser);
    parser_consume(parser, TOKEN_RIGHT_PAREN, "expected right parenthesis");

    return expr;
}

NodeExpr *parse_expr(Parser *parser)
{
    printf("parse_expr\n");
    return parse_binary(parser, 0);
}

NodeProgram *parse(Parser *parser)
{
    printf("parse\n");
    NodeStmt *statements = malloc(sizeof(NodeStmt));

    int count = 0;
    while (parser->current.type != TOKEN_EOF)
    {
        statements = realloc(statements, sizeof(NodeStmt) * (count + 1));
        NodeStmt *statement = parse_stmt(parser);

        switch (statement->base.type)
        {
        case NODE_STMT_USE:
        case NODE_STMT_FUN:
        case NODE_STMT_STR:
        case NODE_STMT_VAL:
            break;
        default:
            parser_error(parser, "illegal top-level statement: expected use, fun, str, or val");
        }

        statements[count++] = *statement;

        printf("P STMT: %d\n", count);
    }

    return new_node_program(statements, count);
}
