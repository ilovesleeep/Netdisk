#include "../include/threadpool.h"

#define MAXLINE 1024
#define MAX_TOKEN_SIZE 512

static int touchClient(Task* task, Command cmd) {
    char data[2] = "1";
    int res_len = sizeof(Command) + 1;
    sendn(task->fd, &res_len, sizeof(int));
    sendn(task->fd, &cmd, sizeof(Command));
    sendn(task->fd, data, 1);

    return 0;
}

static int tellClient(int connfd, int* user_table) {
    // 告诉客户端需要的连接信息
    // 生成 token
    char token[MAX_TOKEN_SIZE] = {0};
    makeToken(token, user_table[connfd]);

    // 发送连接信息 (host, port, token)
    char conn_data[1024] = {0};
    sprintf(conn_data, "%s %s %s", "localhost", "30002", token);
    int data_len = strlen(conn_data);
    sendn(connfd, &data_len, sizeof(int));
    sendn(connfd, conn_data, data_len);

    return 0;
}

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

        log_debug("%lu Da! For mother China!", tid);

        int retval = -1;
        switch (task->cmd) {
            case CMD_INFO_TOKEN:
                // 告知客户端新连接信息和token
                touchClient(task, CMD_INFO_TOKEN);
                tellClient(task->fd, task->u_table);
                break;
            case CMD_PUTS:
            case CMD_GETS:
                if (checkToken(task->token, task->uid) == 0) {
                    printf("认证成功\n");
                    taskHandler(task);
                    break;
                } else {
                    printf("认证失败\n");
                    break;
                }
                // taskHandler(task);
                break;
            default:
                taskHandler(task);
                break;
        }
        freeTask(task);

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
