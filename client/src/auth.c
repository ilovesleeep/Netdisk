#include "../include/auth.h"

#include <crypt.h>
#include <shadow.h>

#define MAXLINE 1024
#define MAX_NAME_LENGTH 32

void getSetting(char* salt, char* passwd) {
    int i, j;
    // 取出salt,i 记录密码字符下标，j记录$出现次数
    for (i = 0, j = 0; passwd[i] && j != 4; ++i) {
        if (passwd[i] == '$') {
            ++j;
        }
    }
    strncpy(salt, passwd, i);
}

int userRegister(int sockfd, char* name) {
    printf("name  : %s\n", name);
    return 0;
}

static int userLogin1(int sockfd, char* name, char* setting);
static int userLogin2(int sockfd, char* setting);

int userLogin(int sockfd, char* name) {
    bzero(name, MAX_NAME_LENGTH);

    char setting[MAXLINE] = {0};

    // 发送登录请求
    // 发送用户名
    userLogin1(sockfd, name, setting);
    // 发送密码
    userLogin2(sockfd, setting);

    return 0;
}

static int userLogin1(int sockfd, char* name, char* setting) {
    while (1) {
        printf("Enter username: ");
        fflush(stdout);
        int name_len = read(STDIN_FILENO, name, MAX_NAME_LENGTH);
        name[--name_len] = '\0';

        char buf[MAXLINE] = {0};
        // login1 for section 1
        sprintf(buf, "login1 %s", name);

        // 先发长度
        int buf_len = strlen(buf);
        sendn(sockfd, &buf_len, sizeof(int));
        // 再发内容
        sendn(sockfd, buf, buf_len);

        // 接收状态
        // 0: ok, 1: not ok
        int user_stat = -1;
        int ret = recv(sockfd, &user_stat, sizeof(int), MSG_WAITALL);

        if (user_stat == 1) {
            printf("Sorry, the user does not exist, please re-enter\n");
            continue;
        }

        // 用户名正确，读取 setting
        bzero(buf, MAXLINE);
        int setting_len = -1;
        ret = recv(sockfd, &setting_len, sizeof(int), MSG_WAITALL);
        ret = recv(sockfd, &buf, setting_len, MSG_WAITALL);

        bzero(setting, MAXLINE);
        strcpy(setting, buf);

        break;
    }
    return 0;
}

static int userLogin2(int sockfd, char* setting) {
    while (1) {
        // printf("Enter password: ");
        // fflush(stdout);
        // int passwd_len = read(STDIN_FILENO, passwd, MAX_USERINFO_LENGTH);
        // passwd[--passwd_len] = '\0';

        char* passwd = getpass("Enter password: ");
        char* encrytped = crypt(passwd, setting);

        char buf[MAXLINE] = {0};
        // login2 for section 2
        sprintf(buf, "login2 %s", encrytped);

        // 先发长度
        int buf_len = strlen(buf);
        sendn(sockfd, &buf_len, sizeof(int));
        // 再发内容
        sendn(sockfd, buf, buf_len);

        // 接收状态
        // 0: ok, 1: not ok
        int user_stat = -1;
        int ret = recv(sockfd, &user_stat, sizeof(int), MSG_WAITALL);

        if (user_stat == 1) {
            printf("Password error, please re-enter\n");
            continue;
        } else {
            system("clear");
            printf("\033[47;30m Okay sir, what can I do for you? \033[0m\n\n");
            break;
        }
    }
    return 0;
}
