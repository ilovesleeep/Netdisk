#include "../include/threadpool.h"

#define BUFSIZE 1024

static int touchTransferServer(int sockfd, Command cmd, Task* task) {
    // char data[BUFSIZE * 2] = {0};
    char old_data[BUFSIZE] = {0};

    char** p = task->args;
    for (int i = 0; p[i] != NULL; ++i) {
        strcat(old_data, p[i]);
        strcat(old_data, " ");
    }
    // printf("旧的请求： %s\n", old_data);
    //  sprintf(data, "%s %s %d", old_data, task->token, task->uid);
    // sprintf(data, "%s", old_data);
    // printf("阶段2请求： %s\n", data);
    int data_len = strlen(old_data);

    int res_len = sizeof(cmd) + data_len;
    sendn(sockfd, &res_len, sizeof(int));
    sendn(sockfd, &cmd, sizeof(cmd));
    sendn(sockfd, old_data, data_len);

    return 0;
}

void freeUnusedParameter(char** parameter) {
    int i = 0, j = 0;
    // 提取出 uid
    while (parameter[i + 1] != NULL) {
        ++i;
    }  // p[i+1] == NULL, p[i]是最后一个元素
    free(parameter[i]);
    parameter[i] = NULL;

    // 提取出 token
    while (parameter[j + 1] != NULL) {
        ++j;
    }  // p[j+1] == NULL, p[j]是最后一个元素
    free(parameter[j]);
    parameter[j] = NULL;
}

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
        log_debug("%lu Da! For mother China!", tid);

        int sockfd = tcpConnect(task->host, task->port);

        log_debug("start section 2");
        if (task->cmd == CMD_GETS1) {
            touchTransferServer(sockfd, CMD_GETS2, task);
            getsCmd(sockfd);
            log_debug("section 2 end, gets success");
        } else {
            touchTransferServer(sockfd, CMD_PUTS2, task);
            freeUnusedParameter(task->args);
            putsCmd(sockfd, task->args);
            log_debug("section 2 end, puts success");
        }
        freeTask(task);

        close(sockfd);

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
