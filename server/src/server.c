#include "../include/server.h"

#define MAXEVENTS 1024
#define MAXLINE 1024
#define MAXUSER 1024

static void requestHandler(int connfd, ThreadPool* pool, int* user_table,
                           DBConnectionPool* dbpool) {
    // 先接总长度
    int request_len = -1;
    int ret = recv(connfd, &request_len, sizeof(int), MSG_WAITALL);

    if (request_len > 0) {
        // 再接内容
        Command cmd = -1;
        char data[MAXLINE] = {0};
        int data_len = request_len - sizeof(cmd);
        ret = recv(connfd, &cmd, sizeof(cmd), MSG_WAITALL);
        ret = recv(connfd, data, data_len, MSG_WAITALL);

        if (ret > 0) {
            // 创建任务
            Task* task = (Task*)malloc(sizeof(Task));
            task->fd = connfd;
            task->uid = user_table[connfd];
            task->u_table = user_table;
            task->args = getArgs(data);
            task->cmd = cmd;
            task->dbpool = dbpool;

            // 把任务添加到任务队列
            blockqPush(pool->task_queue, task);
        }
    }

    if (ret == 0) {
        log_info("Say goodbye to connection %d", connfd);
        user_table[connfd] = 0;
        close(connfd);
    } else if (ret < 0) {
        error(0, errno, "recv");
    }
}

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

int g_exit_pipe[2];
static void exitHandler(int signo) {
    log_info("Exit order received.");
    write(g_exit_pipe[1], "1", 1);
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
    chdir("./user");
    log_info("%d Kirov process reporting", getpid());
    close(g_exit_pipe[1]);

    // epoll
    int epfd = epoll_create(1);

    // g_exit_pipe 读端加入 epoll
    epollAdd(epfd, g_exit_pipe[0]);

    // 创建线程池
    ThreadPool* pool = createThreadPool(conf->num_threads, epfd);

    // 初始化数据库连接池
    DBConnectionPool* dbpool = initDBPool(ht);

    // 创建连接池监视线程
    pthread_t monitor_dbpool;
    pthread_create(&monitor_dbpool, NULL, monitorDBPool, dbpool);

    // 控制命令端口
    int listenfd = tcpListen(conf->port);
    epollAdd(epfd, listenfd);

    // 数据传输端口
    int datafd = tcpListen("30002");
    epollAdd(epfd, datafd);

    // 就绪事件
    struct epoll_event* ready_events =
        (struct epoll_event*)calloc(MAXEVENTS, sizeof(struct epoll_event));

    // 存储用户 id
    int user_table[MAXUSER] = {0};

    // 主循环
    while (1) {
        int nready = epoll_wait(epfd, ready_events, MAXEVENTS, -1);

        for (int i = 0; i < nready; i++) {
            if (ready_events[i].data.fd == listenfd) {
                // 有新的连接
                struct sockaddr_storage client_addr;
                socklen_t addrlen = sizeof(client_addr);

                int connfd =
                    accept(listenfd, (struct sockaddr*)&client_addr, &addrlen);

                // 添加到 epoll
                epollAdd(epfd, connfd);
                epollMod(epfd, connfd, EPOLLIN | EPOLLONESHOT);

            } else if (ready_events[i].data.fd == datafd) {
                // GETS, PUTS
                //

            } else if (ready_events[i].data.fd == g_exit_pipe[0]) {
                // 父进程传来退出信号
                free(ready_events);
                pthread_cancel(monitor_dbpool);
                pthread_join(monitor_dbpool, NULL);
                destroyDBPool(dbpool);
                serverExit(pool);

            } else {
                // 客户端发过来请求
                requestHandler(ready_events[i].data.fd, pool, user_table,
                               dbpool);
            }
        }
    }

    return 0;
}

int serverExit(ThreadPool* pool) {
    // 父进程传来信号
    log_info("All the comrades, exit!");

    // 通知各个子线程退出
    for (int j = 0; j < pool->num_threads; j++) {
        Task exit_task = {-1, 0, NULL, 0, NULL, NULL};
        blockqPush(pool->task_queue, &exit_task);
    }
    // 等待各个子线程退出
    for (int j = 0; j < pool->num_threads; j++) {
        pthread_join(pool->threads[j], NULL);
    }

    // 主线程退出
    log_info("For home country, see you!");
    pthread_exit(0);
}
