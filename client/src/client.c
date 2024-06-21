#include "../include/client.h"

#define MAXLINE 1024
#define BUFSIZE 1024
#define MAX_NAME_LENGTH 20
#define MAX_HOST_LENGTH 64
#define NUM_THREADS 4
#define MAXEVENTS 1024

int main_bak(int argc, char* argv[]) {
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
            freeStringArray(args);
            continue;
        } else if (cmd == CMD_EXIT) {
            freeStringArray(args);
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
        freeStringArray(args);
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

char g_user[MAX_NAME_LENGTH + 1] = {0};  // +1 for '\0'
char g_host[MAX_HOST_LENGTH] = "localhost";
char g_cwd[MAXLINE] = "~";

static void printPrompt(void) {
    printf("\033[32m[%s@%s]:\033[33m%s\033[0m$ ", g_user, g_host, g_cwd);
    fflush(stdout);
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        error(1, 0, "Usage: %s [host] [port]\n", argv[0]);
    }

    // 初始化客户端日志
    initLog();

    // 连接主服务器(调度)
    int sockfd = tcpConnect(argv[1], argv[2]);

    // 欢迎用户
    printMenu();
    welcome(sockfd, g_user);

    // 创建 epoll
    int epfd = epoll_create(1);

    // 将 sockfd 加入 epoll
    epollAdd(epfd, sockfd);

    // 创建线程池
    ThreadPool* pool = createThreadPool(NUM_THREADS, epfd);

    // 就绪事件
    struct epoll_event* ready_events =
        (struct epoll_event*)calloc(MAXEVENTS, sizeof(struct epoll_event));

    // 打印提示符
    strncpy(g_host, argv[1], MAX_HOST_LENGTH);
    printPrompt();

    // 将 STDIN_FILENO 加入 epoll
    epollAdd(epfd, STDIN_FILENO);

    // 主循环
    while (1) {
        int nready = epoll_wait(epfd, ready_events, MAXEVENTS, -1);

        for (int i = 0; i < nready; i++) {
            if (ready_events[i].data.fd == STDIN_FILENO) {
                // 读入请求
                log_debug("stdin");
                char buf[BUFSIZE] = {0};
                int buf_len = read(STDIN_FILENO, buf, BUFSIZE);
                buf[--buf_len] = '\0';  // -1 for '\n'

                log_debug("用户输入：'%s'", buf);

                // 解析请求
                char** args = parseRequest(buf);
                Command cmd = getCommand(args[0]);
                if (cmd == CMD_UNKNOWN) {
                    // 不响应
                    freeStringArray(args);
                    continue;
                } else if (cmd == CMD_EXIT) {
                    // TODO: 退出逻辑
                    freeStringArray(args);
                    close(sockfd);
                    exit(EXIT_SUCCESS);
                }

                // 给调度服务器发请求
                // 先发长度
                int data_len = sizeof(cmd) + buf_len;
                sendn(sockfd, &data_len, sizeof(int));
                // 再发内容
                sendn(sockfd, &cmd, sizeof(cmd));
                sendn(sockfd, buf, buf_len);

                freeStringArray(args);

            } else if (ready_events[i].data.fd == sockfd) {
                // 处理调度服务器响应
                responseHandler(sockfd, pool);

            } else {
                // 其他
                log_debug("other socket");
            }
        }
    }

    return 0;
}

char** getNewConnectionInfo(char* res_data) { return parseRequest(res_data); }

Task* getNewConnectionTask(Command cmd, char* res_data) {
    // 响应内容中包含了：令牌 token, 资源服务器 host，端口 port 等其他信息
    // 解析响应内容，获取新的连接需要的信息，创建新连接(长命令)任务

    // info[0] token, info[1]: host, info[2]: port
    char** info = getNewConnectionInfo(res_data);

    Task* task = (Task*)malloc(sizeof(Task));
    task->cmd = cmd;
    task->token = strdup(info[0]);
    task->host = strdup(info[1]);
    task->port = strdup(info[2]);

    freeStringArray(info);

    return task;
}

int shortResponseHandler(int sockfd, Command cmd) {
    char buf[BUFSIZE] = {0};
    switch (cmd) {
        case CMD_CD:
            return cdCmd(sockfd, buf, g_cwd);
        case CMD_LS:
            return lsCmd(sockfd);
        case CMD_RM:
            return rmCmd(sockfd, buf);
        case CMD_PWD:
            return pwdCmd(sockfd);
        case CMD_MKDIR:
            return mkdirCmd(sockfd, buf);
        default:
            return -1;
    }
}

int responseHandler(int sockfd, ThreadPool* pool) {
    // 接收响应长度
    int res_len = -1;
    int ret = recv(sockfd, &res_len, sizeof(int), MSG_WAITALL);

    if (res_len > 0) {  // 接收到了有效长度
        // 接收响应内容
        Command cmd = -1;
        char res_data[MAXLINE] = {0};
        int data_len = res_len - sizeof(cmd);
        ret = recv(sockfd, &cmd, sizeof(cmd), MSG_WAITALL);
        ret = recv(sockfd, res_data, data_len, MSG_WAITALL);

        if (ret > 0) {  // 接收到了有效响应
            // 分离长短命令, 主线程处理短命令响应
            Task* task = NULL;  // 避免警告，在标签外声明 task
            switch (cmd) {
                case CMD_PUTS:
                case CMD_GETS:
                    task = getNewConnectionTask(cmd, res_data);
                    blockqPush(pool->task_queue, task);
                    break;
                case CMD_CD:
                case CMD_LS:
                case CMD_RM:
                case CMD_PWD:
                case CMD_MKDIR:
                    shortResponseHandler(sockfd, cmd);
                    printPrompt();
                    return 0;
                default:
                    printPrompt();
                    return 0;
            }
            // 只有长命令才能走到这里
            blockqPush(pool->task_queue, task);
            return 0;
        }
    }

    if (ret == 0) {
        // 与服务器断开连接
        log_info("Say goodbye to [Main Server]");
        close(sockfd);
        // 退出
        exit(EXIT_FAILURE);
    } else if (ret < 0) {
        // 发生错误
        log_error("recv: %", strerror(errno));
        close(sockfd);
        // 退出
        exit(EXIT_FAILURE);
    }

    return 0;
}
