#include <func.h>

#include "../include/bussiness.h"
#include "../include/network.h"
#include "../include/parser.h"

#define MAXLINE 1024

int main(int argc, char* argv[]) {
    // ./client host port
    if (argc != 3) {
        error(1, 0, "Usage: %s [host] [port]\n", argv[0]);
    }

    int sockfd = tcpConnect(argv[1], argv[2]);
    printf("[INFO] Established successfully\n\n");

    char buf[MAXLINE] = {0};
    char cwd[MAXLINE] = "~";
    while (1) {
        printf("\033[32m[user@%s]:\033[33m%s\033[0m$ ", argv[1], cwd);
        fflush(stdout);

        // 用户输入命令
        fgets(buf, MAXLINE, stdin);

        // 解析命令
        char** args = parseRequest(buf);
        Command cmd = getCommand(args[0]);
        if (cmd == CMD_UNKNOWN) {
            continue;
        } else if (cmd == CMD_EXIT) {
            argsFree(args);
            close(sockfd);
            return 0;
        }

        // 发送命令到服务器
        send(sockfd, buf, strlen(buf) - 1, 0);
        bzero(buf, MAXLINE);

        // 接收服务器执行的结果
        int recv_status = 0;
        switch (cmd) {
            case CMD_CD:
                cdCmd(sockfd, buf, cwd, &recv_status);
                break;
            case CMD_LS:
                lsCmd(sockfd);
                break;
            case CMD_RM:
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
