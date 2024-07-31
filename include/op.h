#ifndef OP_H
#define OP_H

#include "token.h"

bool op_is_binary(TokenKind type);
bool op_is_unary(TokenKind type);
bool op_is_right_associative(TokenKind type);
int op_get_precedence(TokenKind op);

#endif
