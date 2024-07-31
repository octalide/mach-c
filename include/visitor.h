#ifndef VISITOR_H
#define VISITOR_H

#include <stdbool.h>

#include "node.h"

typedef struct Visitor
{
    // user defined context
    void *context;

    void (*callback)(void *context, Node *node);

    NodeKind kind;
} Visitor;

Visitor *visitor_new(NodeKind kind);
void visitor_free(Visitor *visitor);

void visitor_visit(Visitor *visitor, Node *node);

void visitor_print_node_tree(void *context, Node *node);

#endif // VISITOR_H
