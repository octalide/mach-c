#ifndef OP_H
#define OP_H

#include "token.h"

bool token_is_operator_binary(TokenType type);
bool token_is_operator_unary(TokenType type);
bool token_is_operator_right_associative(TokenType type);
int get_precedence(TokenType op);

#endif
