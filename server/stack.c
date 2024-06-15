#include "stack.h"

Stack* stackCreate(void) { return calloc(1, sizeof(Stack)); }

void stackDestroy(Stack* s) {
    while (s->top) {
        Node* tmp = s->top;
        s->top = s->top->next;
        free(tmp);
    }
    free(s);
}

void stackPush(Stack* s, E val) {
    Node* new_node = malloc(sizeof(Node));
    if (!new_node) {
        printf("Error: malloc faild in stack_push\n");
        exit(1);
    }

    new_node->data = val;
    new_node->next = s->top;
    s->top = new_node;
    s->size++;
}

E stackPop(Stack* s) {
    if (stackEmpty(s)) {
        printf("Error: illegal operation in stack_pop\n");
        exit(1);
    }

    E e = s->top->data;

    Node* tmp = s->top;
    s->top = s->top->next;
    s->size--;
    free(tmp);

    return e;
}
E stackPeek(Stack* s) {
    if (stackEmpty(s)) {
        printf("Error: illegal operation in stack_peek\n");
        exit(1);
    }

    return s->top->data;
}

bool stackEmpty(Stack* s) { return s->size == 0; }
