#ifndef OP_H
#define OP_H

#include "token.h"

bool op_is_binary(TokenType type);
bool op_is_unary(TokenType type);
bool op_is_right_associative(TokenType type);
int op_get_precedence(TokenType op);

#endif
