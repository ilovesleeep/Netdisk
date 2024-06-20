#include "../include/client.h"

#define MAXLINE 1024
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

int sessionHandler(int sockfd, char* host, char* username) {
    char buf[MAXLINE] = {0};
    char cwd[MAXLINE] = "~";
    while (1) {
        printf("\033[32m[%s@%s]:\033[33m%s\033[0m$ ", username, host, cwd);
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
        int buf_len = strlen(buf);
        buf[--buf_len] = '\0';  // -1 for '\n'
        sendn(sockfd, &buf_len, sizeof(int));
        sendn(sockfd, buf, buf_len);
        bzero(buf, MAXLINE);

        // 接收服务器执行的结果
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
