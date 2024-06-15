#include "../include/server.h"

#define MAXEVENTS 1024
#define MAXLINE 1024

int g_exit_pipe[2];
void exitHandler(int signo) {
    printf("[INFO] Exit order received.\n");
    write(g_exit_pipe[1], "1", 1);
}

int serverMain(void) {
    pipe(g_exit_pipe);
    pid_t pid = fork();
    switch (pid) {
        case -1:
            error(1, errno, "fork");
        case 0:
            break;
        default:
            // 父进程
            printf("[INFO] %d MCV reporting\n", getpid());
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
    printf("[INFO] %d Kirov reporting\n", getpid());
    close(g_exit_pipe[1]);

    // 读取配置文件
    ServerConfig conf = {8080, 2};
    parseConfig(&conf);

    // 创建线程池
    ThreadPool* pool = createThreadPool(conf.num_threads);

    // epoll
    int epfd = epoll_create(1);

    // g_exit_pipe 读端加入 epoll
    epollAdd(epfd, g_exit_pipe[0]);

    // 监听端口
    int listenfd = tcpListen(conf.port);
    printf("[INFO] Location confirmed port %d\n", conf.port);
    epollAdd(epfd, listenfd);

    struct epoll_event* ready_events =
        (struct epoll_event*)calloc(MAXEVENTS, sizeof(struct epoll_event));

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

                // 打印 client 信息
                char ip_str[50];
                inet_ntop(client_addr.ss_family,
                          getIpAddr((struct sockaddr*)&client_addr), ip_str,
                          50);
                printf("[INFO] We're being attacked at %s\n", ip_str);

                // 添加到 epoll
                epollAdd(epfd, connfd);

            } else if (ready_events[i].data.fd == g_exit_pipe[0]) {
                // 父进程传来信号
                printf("[INFO] All the comrades, exit!\n");

                // 通知各个子线程退出
                for (int j = 0; j < pool->num_threads; j++) {
                    // 优雅退出
                    Task exit_task = {-1, NULL};
                    blockqPush(pool->task_queue, &exit_task);
                }
                // 等待各个子线程退出
                for (int j = 0; j < pool->num_threads; j++) {
                    pthread_join(pool->threads[j], NULL);
                }

                // 主线程退出
                printf("[INFO] For home country, see you!\n");
                pthread_exit(0);
            } else {
                // 客户端有数据可读
                int connfd = ready_events[i].data.fd;

                // 接收、处理请求
                char req[MAXLINE] = {0};
                int retval = recv(connfd, req, MAXLINE, 0);
                if (retval < 0) {
                    error(0, errno, "recv");
                    close(connfd);
                } else if (retval == 0) {
                    printf("[INFO] We are the champions, my friend!\n");
                    close(connfd);
                } else {
                    // 把任务添加到任务队列

                    Task* ptask = (Task*)malloc(sizeof(Task));
                    ptask->fd = connfd;
                    ptask->args = parseRequest(req);
#if DEBUG
                    for (char **p = ptask->args, i = 0; *p != NULL; ++p, ++i) {
                        printf("arg[%d] = %s\n", i, *p);
                    }
#endif
                    blockqPush(pool->task_queue, ptask);
                }
            }
        }
    }

    return 0;
}
