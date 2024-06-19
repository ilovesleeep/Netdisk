#ifndef __NB_THREAD_POOL_H
#define __NB_THREAD_POOL_H

#include "blockq.h"
#include "bussiness.h"
#include "head.h"
#include "log.h"
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
