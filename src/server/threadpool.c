#include "threadpool.h"

#define MAXLINE 1024

// void cleanup(void* arg) {
//     ThreadPool_t* pool = (ThreadPool_t*) arg;
// }

void* eventLoop(void* arg) {
    ThreadPool* pool = (ThreadPool*)arg;
    pthread_t tid = pthread_self();

    // 线程清理函数
    // pthread_cleanup_push(cleanup, pool);
    while (1) {
        Task* ptask = blockqPop(pool->task_queue);  // 阻塞点

        // 优雅退出
        if (ptask->fd == -1) {
            printf("[INFO] %lu Da! Moving out!\n", tid);
            pthread_exit(0);
        }

        // 处理业务
        printf("[INFO] %lu Da! For mother China!\n", tid);

        char buf[MAXLINE];
        bzero(buf, MAXLINE);
        taskHandler(ptask, buf);
        free(ptask);

        // sendFile(connfd, "file1");
        // close(connfd);

        printf("[INFO] %lu Ura! Waiting orders.\n", tid);
    }
    // pthread_cleanup_pop(0);
}

ThreadPool* createThreadPool(int n) {
    ThreadPool* pool = (ThreadPool*)malloc(sizeof(ThreadPool));
    pool->threads = (pthread_t*)calloc(n, sizeof(pthread_t));
    pool->num_threads = n;
    pool->task_queue = blockqCreate();

    // 创建线程
    for (int i = 0; i < n; i++) {
        pthread_create(&pool->threads[i], NULL, eventLoop, pool);
        printf("[INFO] %lu Conscript reporting\n", pool->threads[i]);
    }

    return pool;
}

void destroyThreadPool(ThreadPool* pool) {
    free(pool->threads);
    blockqDestroy(pool->task_queue);
    free(pool);
}
