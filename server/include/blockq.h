#ifndef __NB_BLOCKQ_H
#define __NB_BLOCKQ_H

#include "bussiness.h"
#include "head.h"

#define N 1024

typedef struct {
    Task* elements[N];
    int front;
    int rear;
    int size;

    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} BlockQ;

BlockQ* blockqCreate(void);
void blockqDestroy(BlockQ* q);

void blockqPush(BlockQ* q, Task* val);
Task* blockqPop(BlockQ* q);
Task* blockqPeek(BlockQ* q);
bool blockqEmpty(BlockQ* q);
bool blockqFull(BlockQ* q);

#endif
