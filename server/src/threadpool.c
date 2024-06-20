#include "../include/threadpool.h"

#define MAXLINE 1024

void* eventLoop(void* arg) {
    ThreadPool* pool = (ThreadPool*)arg;
    pthread_t tid = pthread_self();

    while (1) {
        Task* task = blockqPop(pool->task_queue);  // 阻塞点

        // 优雅退出
        if (task->fd == -1) {
            log_debug("%lu Da! Moving out!\n", tid);
            pthread_exit(0);
        }

        // 处理业务

        int connfd = task->fd;
        // epollMod(pool->epfd, task->fd, 0);
        // epollDel(pool->epfd, task->fd);

        log_debug("%lu Da! For mother China!", tid);

        char buf[MAXLINE];
        bzero(buf, MAXLINE);
        int retval = taskHandler(task);
        taskFree(task);

        log_debug("%lu Ura! Waiting orders.\n", tid);

        if (retval != 1) {
            epollMod(pool->epfd, connfd, EPOLLIN | EPOLLONESHOT);
        } else {
            epollDel(pool->epfd, connfd);
            close(connfd);
        }
        // epollAdd(pool->epfd, connfd);
    }
}

ThreadPool* createThreadPool(int n, int epfd) {
    ThreadPool* pool = (ThreadPool*)malloc(sizeof(ThreadPool));
    pool->threads = (pthread_t*)calloc(n, sizeof(pthread_t));
    pool->num_threads = n;
    pool->task_queue = blockqCreate();
    pool->epfd = epfd;

    // 创建监视DB连接池的线程
    // TODO:该线程使用函数为 monitorPool

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
