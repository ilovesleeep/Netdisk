#include "blockq.h"

BlockQ* blockqCreate(void) {
    BlockQ* q = (BlockQ*)calloc(1, sizeof(BlockQ));

    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);

    return q;
}

void blockqDestroy(BlockQ* q) {
    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->not_empty);
    pthread_cond_destroy(&q->not_full);
    free(q);
}

void blockqPush(BlockQ* q, Task* val) {
    pthread_mutex_lock(&q->mutex);
    while (q->size == N) {
        pthread_cond_wait(&q->not_full, &q->mutex);
    }

    q->elements[q->rear] = val;
    q->rear = (q->rear + 1) % N;
    q->size++;

    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mutex);
}

Task* blockqPop(BlockQ* q) {
    pthread_mutex_lock(&q->mutex);
    while (q->size == 0) {
        pthread_cond_wait(&q->not_empty, &q->mutex);
    }

    Task* retval = q->elements[q->front];
    q->front = (q->front + 1) % N;
    q->size--;

    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->mutex);

    return retval;
}

Task* blockqPeek(BlockQ* q) {
    pthread_mutex_lock(&q->mutex);
    while (q->size == 0) {
        pthread_cond_wait(&q->not_empty, &q->mutex);
    }

    Task* retval = q->elements[q->front];
    pthread_mutex_unlock(&q->mutex);

    return retval;
}

bool blockqEmpty(BlockQ* q) {
    pthread_mutex_lock(&q->mutex);
    int size = q->size;
    pthread_mutex_unlock(&q->mutex);

    return size == 0;
}

bool blockqFull(BlockQ* q) {
    pthread_mutex_lock(&q->mutex);
    int size = q->size;
    pthread_mutex_unlock(&q->mutex);

    return size == N;
}
