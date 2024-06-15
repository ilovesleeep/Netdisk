#ifndef __K_STACK_H
#define __K_STACK_H

#include <func.h>

typedef char* E;

typedef struct node {
    E data;
    struct node* next;
} Node;

typedef struct {
    Node* top;
    int size;
} Stack;

// API
Stack* stackCreate(void);
void stackDestroy(Stack* s);

void stackPush(Stack* s, E val);
E stackPop(Stack* s);
E stackPeek(Stack* s);

bool stackEmpty(Stack* s);

#endif
