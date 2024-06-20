#include "../include/client.h"

#define MAXLINE 1024
#define BUFSIZE 1024
#define MAX_NAME_LENGTH 20

int main(int argc, char* argv[]) {
    if (argc != 3) {
        error(1, 0, "Usage: %s [host] [port]\n", argv[0]);
    }

    int sockfd = tcpConnect(argv[1], argv[2]);

    printMenu();

    char username[MAX_NAME_LENGTH + 1] = {0};

    welcome(sockfd, username);

    return sessionHandler(sockfd, argv[1], username);
}

void welcome(int sockfd, char* username) {
    int option = -1;
    while (option < 0) {
        printf("Enter option number: ");
        fflush(stdout);

        char input[3] = {0};
        fgets(input, sizeof(input), stdin);

        // input[0] for option, input[1] for check, input[2] for '\0'
        option = input[0] - '0';
        if (option < 1 || option > 3 || input[1] != '\n') {
            if (input[0] == '\n' || input[1] == '\n') {
                ;  // skip "\n" or "x\n"
            } else {
                char ch;
                while ((ch = getchar()) != '\n' && ch != EOF) {
                    ;  // 清空 stdin
                }
            }
            printf("Invalid input, please enter again.\n");
            option = -1;
            continue;
        }

        switch (option) {
            case 1:
                userLogin(sockfd, username);
                system("clear");
                printf(
                    "\033[47;30m Sir, this way! What can I do for you? "
                    "\033[0m\n\n");
                break;
            case 2:
                userRegister(sockfd);
                system("clear");
                printMenu();
                option = -1;
                break;
            // printf(
            //    "Registration requires v50 to us, please contact
            //    admin\n");
            // exit(EXIT_SUCCESS);
            case 3:
                printf("See you\n");
                exit(EXIT_SUCCESS);
        }
    }
}

int sessionHandler(int sockfd, char* host, char* user) {
    char buf[MAXLINE] = {0};
    char cwd[MAXLINE] = "~";
    while (1) {
        printf("\033[32m[%s@%s]:\033[33m%s\033[0m$ ", user, host, cwd);
        fflush(stdout);

        // 用户输入命令
        fgets(buf, MAXLINE, stdin);

        // 解析命令
        char** args = parseRequest(buf);
        Command cmd = getCommand(args[0]);
        if (cmd == CMD_UNKNOWN) {
            argsFree(args);
            continue;
        } else if (cmd == CMD_EXIT) {
            argsFree(args);
            close(sockfd);
            return 0;
        }

        // 发送命令到服务器
        // 先发长度
        int buf_len = strlen(buf);
        buf[--buf_len] = '\0';  // -1 for '\n'
        int data_len = sizeof(cmd) + buf_len;
        sendn(sockfd, &data_len, sizeof(int));
        // 再发内容
        sendn(sockfd, &cmd, sizeof(cmd));
        sendn(sockfd, buf, buf_len);

        // 接收服务器执行的结果
        bzero(buf, MAXLINE);
        switch (cmd) {
            case CMD_CD:
                cdCmd(sockfd, buf, cwd);
                break;
            case CMD_LS:
                lsCmd(sockfd);
                break;
            case CMD_RM:
                rmCmd(sockfd, buf);
                break;
            case CMD_PWD:
                recv(sockfd, buf, MAXLINE, 0);
                puts(buf);
                break;
            case CMD_GETS:
                getsCmd(sockfd);
                break;
            case CMD_PUTS:
                putsCmd(sockfd, args);
                break;
            case CMD_MKDIR:
                mkdirCmd(sockfd, buf);
                break;
            default:
                break;
        }
        // NOTE: 勿忘我
        argsFree(args);
    }
}

void printMenu(void) {
    system("clear");
    /*
    printf(
        "_________________________________\n"
        "|                               |\n"
        "|   Welcome to NewBee Netdisk!  |\n"
        "|                               |\n"
        "|      Menu:                    |\n"
        "|           1. Login            |\n"
        "|           2. Register         |\n"
        "|           3. Exit             |\n"
        "|                        v1.0   |\n"
        "|_______________________________|\n\n");
    */

    printf(
        "\033[1m\033[36m"
        "                                        \n"
        "         _   _      _      _ _     _    \n"
        " __/\\__ | \\ | | ___| |_ __| (_)___| | __\n"
        " \\ \033[31mN\033[36m  / |  \\| |/ _ \\ __/ _` | / __| |/ /\n"
        " /_ \033[31mB\033[36m_\\ | |\\  |  __/ || (_| | \\__ \\   <\n"
        "   \\/   |_| \\_|\\___|\\__\\__,_|.|___/_|\\_\\\n"
        "                                        \n"
        "            Menu:                       \n"
        "                 1. Login               \n"
        "                 2. Register            \n"
        "                 3. Exit                \n"
        "                                        \n"
        "                                     v1.0\n"
        "\033[0m"
        "                                        \n");
}
/*
int mainTest(int argc, char* argv[]) {
    int epfd = epoll_create(1);

    epollAdd(epfd, g_exit_pipe[0]);

    // 创建线程池
    ThreadPool* pool = createThreadPool(conf->num_threads, epfd);

    // 连接主服务器（路由）
    int sockfd = tcpConnect(argv[1], argv[2]);
    epollAdd(epfd, sockfd);

    struct epoll_event* ready_events =
        (struct epoll_event*)calloc(MAXEVENTS, sizeof(struct epoll_event));

    // 主循环
    while (1) {
        int nready = epoll_wait(epfd, ready_events, MAXEVENTS, -1);

        for (int i = 0; i < nready; i++) {
            if (ready_events[i].data.fd == sockfd) {
                // 路由服务器发来信息
                struct sockaddr_storage client_addr;
                socklen_t addrlen = sizeof(client_addr);

                int connfd =
                    accept(listenfd, (struct sockaddr*)&client_addr, &addrlen);

                // 添加到 epoll
                epollAdd(epfd, connfd);
                epollMod(epfd, connfd, EPOLLIN | EPOLLONESHOT);

                // 初始化 workdir
                char user_dir[] = "user";
                workdirInit(workdir_table, connfd, user_dir);

            } else if (ready_events[i].data.fd == g_exit_pipe[0]) {
                // 父进程传来退出信号
                pthread_cancel(monitor_dbpool);
                pthread_join(monitor_dbpool, NULL);
                destroyDBPool(dbpool);
                serverExit(pool);

            } else {
                // 客户端发过来请求
                requestHandler(ready_events[i].data.fd, pool, user_table,
                               dbpool, workdir_table);
            }
        }
    }

    return 0;
}
*/
