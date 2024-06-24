#include "../include/client.h"

#define MAXLINE 1024
#define BUFSIZE 1024
#define MAX_NAME_LENGTH 20
#define MAX_HOST_LENGTH 64
#define MAX_TOKEN_LENGTH 512
#define NUM_THREADS 4
#define MAXEVENTS 1024

char g_user[MAX_NAME_LENGTH + 1] = {0};  // +1 for '\0'
char g_host[MAX_HOST_LENGTH] = "localhost";
char g_cwd[MAXLINE] = "~";

int g_uid = 0;
char g_new_host[MAX_HOST_LENGTH] = {0};
char g_new_port[8] = {0};
char g_token[MAX_TOKEN_LENGTH] = {0};

static void printPrompt(void) {
    printf("\033[32m[%s@%s]:\033[33m%s\033[0m$ ", g_user, g_host, g_cwd);
    fflush(stdout);
    return;
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

    // CMD_INFO_TOKEN:
    // 给调度服务器发获取token请求
    char tokenmsg[] = "getToken";
    int msg_len = strlen(tokenmsg);
    // 先发长度
    Command cmd_token = CMD_INFO_TOKEN;
    int total_len = sizeof(cmd_token) + msg_len;
    sendn(sockfd, &total_len, sizeof(int));
    // 再发内容
    sendn(sockfd, &cmd_token, sizeof(cmd_token));
    sendn(sockfd, tokenmsg, msg_len);
    printf("send tokenmsg ok\n");

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
                char buf[BUFSIZE] = {0};
                int buf_len = read(STDIN_FILENO, buf, BUFSIZE);
                buf[--buf_len] = '\0';  // -1 for '\n'

                log_debug("用户输入：'%s'", buf);

                // 解析请求
                char** args = parseRequest(buf);
                Command cmd = getCommand(args[0]);
                if (cmd == CMD_UNKNOWN) {
                    // 不响应
                    log_debug("未知命令，不响应");
                    freeStringArray(args);
                    printPrompt();
                    continue;
                } else if (cmd == CMD_EXIT) {
                    // TODO: 退出逻辑
                    freeStringArray(args);
                    close(sockfd);
                    exit(EXIT_SUCCESS);
                } else if (cmd == CMD_GETS1 || cmd == CMD_PUTS1) {
                    /*
                    // 更新 token
                    // CMD_INFO_TOKEN:
                    // 给调度服务器发获取token请求
                    // 先发长度
                    Command cmd_token = CMD_INFO_TOKEN;
                    int total_len = sizeof(cmd_token) + buf_len;
                    sendn(sockfd, &total_len, sizeof(int));
                    // 再发内容
                    sendn(sockfd, &cmd_token, sizeof(cmd_token));
                    sendn(sockfd, buf, buf_len);

                    // 接收
                    // getNewConnectionInfo(sockfd, g_new_host, g_new_port,
                    //                     g_token);
                    */

                    // 给请求加上 token
                    char new_buf[BUFSIZE + MAX_TOKEN_LENGTH] = {0};
                    sprintf(new_buf, "%s %s", buf, g_token);
                    int new_len = strlen(new_buf);
                    // 先发长度
                    int total_len = sizeof(cmd) + new_len;
                    int ret = sendn(sockfd, &total_len, sizeof(int));
                    // 再发内容
                    ret = sendn(sockfd, &cmd, sizeof(cmd));
                    ret = sendn(sockfd, new_buf, new_len);

                    freeStringArray(args);

                    continue;
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

static int optionCheck(char* input) {
    // input[0] for option, input[1] for check, input[2] for '\0'
    int option = input[0] - '0';
    if (option < 1 || option > 3 || input[1] != '\n') {
        if (input[0] == '\n' || input[1] == '\n') {
            ;  // skip "\n" or "x\n"
        } else {
            char ch;
            while ((ch = getchar()) != '\n' && ch != EOF) {
                ;  // flash stdin
            }
        }
        printf("Invalid input, please enter again.\n");
        option = -1;
    }
    return option;
}

void welcome(int sockfd, char* username) {
    int option = -1;
    while (option < 0) {
        printf("Enter option number: ");
        fflush(stdout);

        char input[3] = {0};
        fgets(input, sizeof(input), stdin);

        if ((option = optionCheck(input)) == -1) {
            continue;
        }

        switch (option) {
            case 1:
                userLogin(sockfd, username, g_cwd);
                // system("clear");
                printf(
                    "\033[47;30m Sir, this way! What can I do for you? "
                    "\033[0m\n\n");
                ;
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

int shortResponseHandler(int sockfd, Command cmd) {
    switch (cmd) {
        case CMD_CD:
            return cdCmd(sockfd, g_cwd);
        case CMD_LS:
            return lsCmd(sockfd);
        case CMD_RM:
            return rmCmd(sockfd);
        case CMD_PWD:
            return pwdCmd(sockfd);
        case CMD_MKDIR:
            return mkdirCmd(sockfd);
        default:
            return -1;
    }
}

char** getInfoList(char* data) { return parseRequest(data); }

int getNewConnectionInfo(int sockfd, int* uid, char* new_host, char* new_port,
                         char* token) {
    int data_len = 0;
    recv(sockfd, &data_len, sizeof(int), MSG_WAITALL);

    char conn_data[1024] = {0};
    recv(sockfd, conn_data, data_len, MSG_WAITALL);

    // printf("get token info:\n host: [%s]\n port: [%s]\n token[%s]\n"m )
    printf("获取到认证信息：\n %s \n", conn_data);

    //  info_list[0]: uid, info_list[1]: host
    //  info_list[2]: port, info_list[3]: token
    char** info_list = getInfoList(conn_data);
    *uid = atoi(info_list[0]);
    bzero(new_host, MAX_HOST_LENGTH);
    bzero(new_port, 8);
    bzero(token, MAX_TOKEN_LENGTH);
    strcpy(new_host, info_list[1]);
    strcpy(new_port, info_list[2]);
    strcpy(token, info_list[3]);

    freeStringArray(info_list);

    return 0;
}
/*
char** getNewTaskInfo(char* res_data) { return parseRequest(res_data); }

Task* getNewConnectionTask(Command cmd, char* res_data) {
    // 响应内容中包含了：资源服务器 host，端口 port 令牌 token 等其他信息
    // 解析响应内容，获取新的连接需要的信息，创建新连接(长命令)任务

    // info[0] host, info[1]: port, info[2]: token
    char** info = getNewTaskInfo(res_data);

    Task* task = (Task*)malloc(sizeof(Task));
    task->cmd = cmd;
    task->host = strdup(info[0]);
    task->port = strdup(info[1]);
    task->token = strdup(info[2]);

    freeStringArray(info);

    return task;
}
*/

Task* makeLongTask(Command cmd, char* res_data, int uid, char* new_host,
                   char* new_port, char* token) {
    Task* task = (Task*)malloc(sizeof(Task));
    task->cmd = cmd;
    task->uid = uid;
    task->host = strdup(new_host);
    task->port = strdup(new_port);
    task->token = strdup(token);
    task->args = parseRequest(res_data);

    return task;
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
        // log_debug("recv res_data: '%s'", res_data);

        if (ret > 0) {  // 接收到了有效响应
            // 分离长短命令, 主线程处理短命令响应
            Task* task = NULL;  // 避免警告，在标签外声明 task
            switch (cmd) {
                case CMD_INFO_TOKEN:
                    // 获取新连接信息和token
                    getNewConnectionInfo(sockfd, &g_uid, g_new_host, g_new_port,
                                         g_token);
                    return 0;
                case CMD_PUTS1:
                case CMD_GETS1:
                    printf("阶段 1 结束\n");
                    task = makeLongTask(cmd, res_data, g_uid, g_new_host,
                                        g_new_port, g_token);
                    blockqPush(pool->task_queue, task);
                    return 0;

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
        log_error("recv: %s", strerror(errno));
        close(sockfd);
        // 退出
        exit(EXIT_FAILURE);
    }

    return 0;
}

void printMenu(void) {
    system("clear");
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
