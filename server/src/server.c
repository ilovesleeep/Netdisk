#include "../include/server.h"

#define MAXEVENTS 1024
#define MAXLINE 1024
#define MAXUSER 1024
#define MAX_TOKEN_SIZE 512
#define TIMEOUT 1000

void serverInit(ServerConfig* conf, HashTable* ht) {
    char* port = (char*)find(ht, "port");
    if (port != NULL) {
        strcpy(conf->port, port);
    }
    const char* num_str = (const char*)find(ht, "num_threads");
    if (num_str != NULL) {
        conf->num_threads = atoi(num_str);
    }
}

Task* makeTask(int connfd, int* user_table, DBConnectionPool* dbpool,
               Command cmd, char* request_data) {
    Task* task = (Task*)malloc(sizeof(Task));
    task->fd = connfd;
    task->uid = user_table[connfd];
    task->u_table = user_table;
    task->cmd = cmd;
    task->args = getArgs(request_data);
    task->dbpool = dbpool;

    char** p = task->args;
    int i = 0, j = 0;
    switch (cmd) {
        case CMD_PUTS1:
        case CMD_GETS1:
            // 提取出 token
            while (p[i + 1] != NULL) {
                ++i;
            }  // p[i+1] == NULL, p[i]是最后一个元素
            task->token = strdup(p[i]);
            free(p[i]);
            p[i] = NULL;

            break;
        case CMD_PUTS2:
        case CMD_GETS2:
            // 提取出 uid
            while (p[i + 1] != NULL) {
                ++i;
            }  // p[i+1] == NULL, p[i]是最后一个元素
            task->uid = atoi(p[i]);
            free(p[i]);
            p[i] = NULL;

            // 提取出 token
            while (p[j + 1] != NULL) {
                ++j;
            }  // p[j+1] == NULL, p[j]是最后一个元素
            task->token = strdup(p[j]);
            free(p[j]);
            p[j] = NULL;

            break;
        default:
            task->token = NULL;
            break;
    }

    return task;
}

static int requestHandler(int connfd, ThreadPool* short_pool,
                          ThreadPool* long_pool, int* user_table,
                          DBConnectionPool* dbpool) {
    // 接收请求长度
    int request_len = -1;
    int ret = recv(connfd, &request_len, sizeof(int), MSG_WAITALL);
    log_debug("recv total len %d", request_len);

    if (request_len > 0) {  // 接收到了有效长度
        Command cmd = -1;
        char request_data[MAXLINE] = {0};
        int data_len = request_len - sizeof(cmd);
        ret = recv(connfd, &cmd, sizeof(cmd), MSG_WAITALL);
        log_debug("recv data len %d", data_len);
        ret = recv(connfd, request_data, data_len, MSG_WAITALL);
        log_debug("recv data %s", request_data);

        if (ret > 0) {          // 接收到了有效请求
            Task* task = NULL;  // 避免警告，在标签外声明 task
            switch (cmd) {
                case CMD_PUTS2:
                case CMD_GETS2:
                    task =
                        makeTask(connfd, user_table, dbpool, cmd, request_data);
                    // 放入长任务线程池
                    blockqPush(long_pool->task_queue, task);
                    return 0;

                default:
                    task =
                        makeTask(connfd, user_table, dbpool, cmd, request_data);
                    // 放入短任务线程池
                    blockqPush(short_pool->task_queue, task);
                    return 0;
            }
        }
    }

    if (ret == 0) {
        log_info("Say goodbye to connection %d", connfd);
        user_table[connfd] = 0;
        close(connfd);
    } else if (ret < 0) {
        error(0, errno, "recv");
    }

    return 0;
}

int g_exit_pipe[2];
static void exitHandler(int signo) {
    log_info("Exit order received.");
    write(g_exit_pipe[1], "1", 1);
}

int threadsExit(ThreadPool* pool) {
    // 父进程传来信号
    log_info("All the comrades, exit!");

    // 通知各个子线程退出
    for (int j = 0; j < pool->num_threads; j++) {
        Task exit_task = {-1, 0, NULL, 0, NULL, NULL, NULL};
        blockqPush(pool->task_queue, &exit_task);
    }
    // 等待各个子线程退出
    for (int j = 0; j < pool->num_threads; j++) {
        pthread_join(pool->threads[j], NULL);
    }
    return 0;
}

int serverMain(ServerConfig* conf, HashTable* ht) {
    pipe(g_exit_pipe);
    pid_t pid = fork();
    switch (pid) {
        case -1:
            error(1, errno, "fork");
        case 0:
            break;
        default:
            // 父进程
            log_info("%d MCV porcess reporting", getpid());
            close(g_exit_pipe[0]);
            // 捕获 SIGUSR1 信号
            if (signal(SIGUSR1, exitHandler) == SIG_ERR) {
                error(1, errno, "signal");
            }
            // 等待子进程结束
            wait(NULL);
            exit(0);
    }
    // 子进程
    log_info("%d Kirov process reporting", getpid());
    close(g_exit_pipe[1]);

    // 更改工作目录为 ./user
    chdir("./user");

    // init timerwheel
    HashMap* hashmap = hashmapCreate();
    HashedWheelTimer* timer = hwtCreate(WHEEL_SIZE + 1);

    // epoll
    int epfd = epoll_create(1);

    // g_exit_pipe 读端加入 epoll
    epollAdd(epfd, g_exit_pipe[0]);

    // 创建短命令线程池
    ThreadPool* short_pool = createThreadPool(conf->num_threads, epfd);

    // 创建长命令线程池
    ThreadPool* long_pool = createThreadPool(conf->num_threads, epfd);

    // 初始化数据库连接池
    DBConnectionPool* dbpool = initDBPool(ht);

    // 创建连接池监视线程
    pthread_t monitor_tid;
    pthread_create(&monitor_tid, NULL, monitorDBPool, dbpool);

    // 控制命令端口
    int listenfd = tcpListen(conf->port);
    epollAdd(epfd, listenfd);

    // 数据传输端口
    int transferfd = tcpListen("30002");
    epollAdd(epfd, transferfd);

    // 就绪事件
    struct epoll_event* ready_events =
        (struct epoll_event*)calloc(MAXEVENTS, sizeof(struct epoll_event));

    // 存储用户 id
    int user_table[MAXUSER] = {0};

    // init timeout
    struct timespec start, end;
    int timeout = TIMEOUT;
    clock_gettime(CLOCK_MONOTONIC, &start);

    // 主循环
    while (1) {
        int nready = epoll_wait(epfd, ready_events, MAXEVENTS, timeout);

        if (nready == -1 && errno == EINTR) {
            error(0, errno, "epoll_wait");
            continue;
        } else if (nready == 0) {
            // 超时
            hwtClear(timer);
            timer->curr_idx = (timer->curr_idx + 1) % timer->size;
            timeout = TIMEOUT;
        } else {
            for (int i = 0; i < nready; i++) {
                if (ready_events[i].data.fd == listenfd) {
                    log_debug("listenfd new conn");
                    // 有新的客户端连接
                    struct sockaddr_storage client_addr;
                    socklen_t addrlen = sizeof(client_addr);

                    int connfd = accept(
                        listenfd, (struct sockaddr*)&client_addr, &addrlen);

                    // 添加到 epoll
                    epollAdd(epfd, connfd);
                    // epollMod(epfd, connfd, EPOLLIN | EPOLLONESHOT);

                    // connfd update
                    int slot_idx = hashmapSearch(
                        hashmap, connfd);  // 若存在返回slot，不存在返回-1
                    slot_idx = hwtUpdate(
                        timer, connfd,
                        slot_idx);  // 插入的新的slot，也就是上一个curr_idx的上一个
                    hashmapInsert(hashmap, connfd, slot_idx);  // 更新hashmap

                } else if (ready_events[i].data.fd == transferfd) {
                    log_debug("transferfd new conn");
                    // GETS, PUTS建立连接
                    struct sockaddr_storage client_addr;
                    socklen_t addrlen = sizeof(client_addr);

                    int connfd = accept(
                        transferfd, (struct sockaddr*)&client_addr, &addrlen);

                    // 添加到 epoll
                    epollAdd(epfd, connfd);
                    epollMod(epfd, connfd, EPOLLIN | EPOLLONESHOT);

                    // connfd update
                    int slot_idx = hashmapSearch(
                        hashmap, connfd);  // 若存在返回slot，不存在返回-1
                    slot_idx = hwtUpdate(
                        timer, connfd,
                        slot_idx);  // 插入的新的slot，也就是上一个curr_idx的上一个
                    hashmapInsert(hashmap, connfd, slot_idx);  // 更新hashmap

                } else if (ready_events[i].data.fd == g_exit_pipe[0]) {
                    // 父进程传来退出信号
                    free(ready_events);
                    pthread_cancel(monitor_tid);
                    pthread_join(monitor_tid, NULL);
                    destroyDBPool(dbpool);

                    hwtDestroy(timer);
                    hashmapDestroy(hashmap);

                    threadsExit(long_pool);
                    threadsExit(short_pool);

                    // 主线程退出
                    log_info("For home country, see you!");
                    pthread_exit(0);

                } else {
                    // 客户端发过来请求
                    requestHandler(ready_events[i].data.fd, short_pool,
                                   long_pool, user_table, dbpool);

                    // connfd update
                    int slot_idx = hashmapSearch(
                        hashmap, ready_events[i]
                                     .data.fd);  // 若存在返回slot，不存在返回-1
                    slot_idx = hwtUpdate(
                        timer, ready_events[i].data.fd,
                        slot_idx);  // 插入的新的slot，也就是上一个curr_idx的上一个
                    hashmapInsert(hashmap, ready_events[i].data.fd,
                                  slot_idx);  // 更新hashmap
                }
            }

            // 超时时间的相对计算
            clock_gettime(CLOCK_MONOTONIC, &end);
            timeout -= (end.tv_sec - start.tv_sec) * 1000 +
                       (end.tv_nsec - start.tv_nsec) / 1000000;
            if (timeout <= 0) {
                timeout = TIMEOUT;
            }
            clock_gettime(CLOCK_MONOTONIC, &start);
        }
    }

    return 0;
}
