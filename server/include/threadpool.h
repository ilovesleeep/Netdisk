#ifndef __K_THREAD_POOL_H
#define __K_THREAD_POOL_H

#include <func.h>

#include "blockq.h"
#include "bussiness.h"
#include "network.h"

typedef struct {
    int epfd;            // 存储 epoll fd
    pthread_t* threads;  // 存储线程 tid
    int num_threads;
    BlockQ* task_queue;
} ThreadPool;

ThreadPool* createThreadPool(int num_threads, int epfd);
void destroyThreadPool(ThreadPool* pool);

#endif
