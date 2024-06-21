#include "../include/threadpool.h"

#define BUFSIZE 1024

void* eventLoop(void* arg) {
    ThreadPool* pool = (ThreadPool*)arg;
    pthread_t tid = pthread_self();

    while (1) {
        Task* task = blockqPop(pool->task_queue);  // 阻塞点

        // 优雅退出
        if (task->cmd == CMD_STOP) {
            log_debug("%lu Da! Moving out!\n", tid);
            pthread_exit(0);
        }

        // 处理业务

        int sockfd = tcpConnect(task->host, task->port);

        log_debug("%lu Da! For mother China!", tid);

        if (task->cmd == CMD_PUTS) {
            // putsHandler(task);
        } else {
            // getsHandler(task);
        }
        freeTask(task);

        log_debug("%lu Ura! Waiting orders.\n", tid);
    }
}

ThreadPool* createThreadPool(int n, int epfd) {
    ThreadPool* pool = (ThreadPool*)malloc(sizeof(ThreadPool));
    pool->threads = (pthread_t*)calloc(n, sizeof(pthread_t));
    pool->num_threads = n;
    pool->task_queue = blockqCreate();
    pool->epfd = epfd;

    // 创建线程
    for (int i = 0; i < n; i++) {
        pthread_create(&pool->threads[i], NULL, eventLoop, pool);
        log_debug("%lu Conscript reporting", pool->threads[i]);
    }

    return pool;
}

void destroyThreadPool(ThreadPool* pool) {
    free(pool->threads);
    blockqDestroy(pool->task_queue);
    free(pool);
}
