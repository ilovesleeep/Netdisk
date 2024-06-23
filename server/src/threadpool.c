#include "../include/threadpool.h"

#define MAXLINE 1024
#define MAX_TOKEN_SIZE 512

static int touchClient(Task* task, Command cmd) {
    char data[MAXLINE * 2] = {0};
    char old_data[MAXLINE] = {0};

    char** p = task->args;
    for (int i = 0; p[i] != NULL; ++i) {
        strcat(old_data, p[i]);
        strcat(old_data, " ");
    }
    sprintf(data, "%s %s %d", old_data, task->token, task->uid);
    log_debug("touch client data: %s", data);
    int data_len = strlen(data);

    int res_len = sizeof(Command) + data_len;
    sendn(task->fd, &res_len, sizeof(int));
    sendn(task->fd, &cmd, sizeof(Command));
    sendn(task->fd, data, data_len);

    return 0;
}

void freeUnusedParameter(char** parameter) {
    int i = 0, j = 0;
    // free uid
    while (parameter[i + 1] != NULL) {
        ++i;
    }  // p[i+1] == NULL, p[i]是最后一个元素
    free(parameter[i]);
    parameter[i] = NULL;

    // free token
    while (parameter[j + 1] != NULL) {
        ++j;
    }  // p[j+1] == NULL, p[j]是最后一个元素
    free(parameter[j]);
    parameter[j] = NULL;
}

static int tellClient(int connfd, int* user_table) {
    // 告诉客户端需要的连接信息
    // 生成 token
    char token[MAX_TOKEN_SIZE] = {0};
    makeToken(token, user_table[connfd]);

    // 发送连接信息 (host, port, token, uid)
    // 多个服务器，比如有3个，(3，host+port, host+port, host+port, )
    //)
    // sprintf(conn_data, "%s %s %s %d", "localhost", "30002", token,
    //        user_table[connfd]);
    char conn_data[1024] = {0};
    sprintf(conn_data, "%d %s %s %s", user_table[connfd], "192.168.19.172",
            "30002", token);
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

        int ret = -1;
        switch (task->cmd) {
            case CMD_INFO_TOKEN:  // 告知客户端新连接信息和token
                touchClient(task, CMD_INFO_TOKEN);
                tellClient(connfd, task->u_table);
                break;
            case CMD_PUTS1:
            case CMD_GETS1:
                if (checkToken(task->token, task->uid) == 0) {
                    log_debug("[%d] Authentication section 1 success",
                              task->uid);
                    touchClient(task, task->cmd);
                    break;
                } else {
                    log_warn("[%d]Authentication section 1 failed", task->uid);
                    break;
                }
            case CMD_PUTS2:
            case CMD_GETS2:
                if (checkToken(task->token, task->uid) == 0) {
                    log_debug("[%d] Authentication section 2 success",
                              task->uid);
                    // freeUnusedParameter(task->args);
                    ret = taskHandler(task);
                    char* tmp = task->cmd == CMD_GETS2 ? "gets" : "puts";
                    log_debug("%s done", tmp);
                    if (ret != 1) {
                        epollMod(pool->epfd, connfd, EPOLLIN | EPOLLONESHOT);
                    } else {
                        epollDel(pool->epfd, connfd);
                        close(connfd);
                    }
                    break;
                } else {
                    log_warn("[%d] Authentication section 2 failed", task->uid);
                    break;
                }
            default:  // 短命令
                taskHandler(task);
                break;
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
