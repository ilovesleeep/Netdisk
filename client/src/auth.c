#include "../include/auth.h"

#include <crypt.h>

#define MAXLINE 1024
#define MAX_NAME_LENGTH 20

static int userRegister1(int sockfd, char* username, char* salt);
static int userRegister2(int sockfd, char* username, char* salt);

int userRegister(int sockfd) {
    char username[MAX_NAME_LENGTH + 1];
    char salt[MAXLINE];
    // 发送用户名给服务器
    userRegister1(sockfd, username, salt);
    // 根据盐值和密码生成密文，发送密文给服务器
    userRegister2(sockfd, username, salt);

    return 0;
}

static int userRegister1(int sockfd, char* username, char* salt) {
    while (1) {
        printf("Username: ");
        fflush(stdout);
        char name[MAX_NAME_LENGTH + 1] = {0};
        int name_len = read(STDIN_FILENO, name, MAX_NAME_LENGTH + 1);
        if (name_len > MAX_NAME_LENGTH) {
            printf("Max name length is 20, please re-enter\n");
            continue;
        } else if (name_len < 1) {  // name == "\n"
            printf("Min name length is 1, please re-enter\n");
            continue;
        }
        for (int i = 0; i < name_len; ++i) {
            if (name[i] == ' ') {
                printf("Space is not allowed in usrename, please re-enter\n");
                continue;
            }
        }
        name[--name_len] = '\0';

        // reg1 for section 1
        char buf[MAXLINE] = {0};
        sprintf(buf, "reg1 %s", name);

        // 先发长度
        Command cmd = CMD_REG1;
        int buf_len = strlen(buf);
        int data_len = sizeof(cmd) + buf_len;
        sendn(sockfd, &data_len, sizeof(int));
        // 再发内容
        sendn(sockfd, &cmd, sizeof(cmd));
        sendn(sockfd, buf, buf_len);

        // 接收状态
        // 0: 用户名可用, 1: 用户名已存在
        int status_code = -1;
        recv(sockfd, &status_code, sizeof(int), MSG_WAITALL);

        if (status_code == 1) {
            printf("Sorry, the username does exist, please re-enter\n");
            continue;
        }

        // 用户名可用，接收 salt
        bzero(buf, MAXLINE);
        int salt_len = -1;
        recv(sockfd, &salt_len, sizeof(int), MSG_WAITALL);
        recv(sockfd, &buf, salt_len, MSG_WAITALL);

        bzero(salt, MAXLINE);
        strcpy(salt, buf);

        strcpy(username, name);
        break;
    }
    return 0;
}

static int userRegister2(int sockfd, char* username, char* salt) {
    while (1) {
        char* passwd = NULL;
        char* confirm_passwd = NULL;
        int count = 0;
        do {
            if (++count == 3) {
                printf("Too many tries, exit.\n");
                exit(EXIT_SUCCESS);
            }

            if (passwd != NULL) {
                printf("The two entries are invalid, please re-enter\n");
            }
            passwd = getpass("Password: ");
            confirm_passwd = getpass("Confirm password: ");
        } while (strcmp(passwd, confirm_passwd) != 0 || passwd[0] == '\0');

        // 加密
        char* encrytped = crypt(passwd, salt);

        // reg2 for section 2
        char buf[MAXLINE] = {0};
        sprintf(buf, "reg2 %s %s", username, encrytped);

        // 先发长度
        Command cmd = CMD_REG2;
        int buf_len = strlen(buf);
        int data_len = sizeof(cmd) + buf_len;
        sendn(sockfd, &data_len, sizeof(int));
        // 再发内容
        sendn(sockfd, &cmd, sizeof(cmd));
        sendn(sockfd, buf, buf_len);

        // 接收状态
        // 0: 注册成功, 1: 注册失败
        int status_code = -1;
        recv(sockfd, &status_code, sizeof(int), MSG_WAITALL);
        if (status_code == 0) {
            // 1秒后返回主界面
            printf(
                "Registration successful, returning to the main menu "
                "shortly.\n");
            sleep(1);
            break;
        } else {
            printf("Register failed, exit\n");
            exit(EXIT_FAILURE);
        }
    }
    return 0;
}

static int userLogin1(int sockfd, char* name, char* salt);
static int userLogin2(int sockfd, char* cwd, char* salt);

int userLogin(int sockfd, char* name, char* cwd) {
    char salt[MAXLINE] = {0};
    // 发送用户名
    userLogin1(sockfd, name, salt);
    // 发送密码
    userLogin2(sockfd, cwd, salt);

    return 0;
}

static int userLogin1(int sockfd, char* name, char* salt) {
    while (1) {
        printf("Login: ");
        fflush(stdout);
        bzero(name, MAX_NAME_LENGTH + 1);
        int name_len = read(STDIN_FILENO, name, MAX_NAME_LENGTH + 1);
        if (name_len > MAX_NAME_LENGTH) {
            printf("Max name length is 20, please re-enter\n");
            continue;
        } else if (name_len < 1) {  // name == "\n"
            printf("Min name length is 1, please re-enter\n");
            continue;
        }
        for (int i = 0; i < name_len; ++i) {
            if (name[i] == ' ') {
                printf("Space is not allowed in usrename, please re-enter\n");
                continue;
            }
        }
        name[--name_len] = '\0';

        // login1 for section 1
        char buf[MAXLINE] = {0};
        sprintf(buf, "login1 %s", name);

        // 先发长度
        Command cmd = CMD_LOGIN1;
        int buf_len = strlen(buf);
        int data_len = sizeof(cmd) + buf_len;
        sendn(sockfd, &data_len, sizeof(int));
        // 再发内容
        sendn(sockfd, &cmd, sizeof(cmd));
        sendn(sockfd, buf, buf_len);

        // 接收状态
        // 0: 成功, 1: 失败
        int status_code = -1;
        recv(sockfd, &status_code, sizeof(int), MSG_WAITALL);

        if (status_code == 1) {
            printf("Sorry, the user does not exist, please re-enter\n");
            continue;
        }

        // 用户存在，接收 salt
        bzero(buf, MAXLINE);
        int salt_len = -1;
        recv(sockfd, &salt_len, sizeof(int), MSG_WAITALL);
        recv(sockfd, &buf, salt_len, MSG_WAITALL);

        bzero(salt, MAXLINE);
        strcpy(salt, buf);

        break;
    }
    return 0;
}

static int userLogin2(int sockfd, char* cwd, char* salt) {
    int count = 0;
    while (1) {
        if (++count == 3) {
            printf("Too many incorrect password attempts, exit.\n");
            exit(EXIT_SUCCESS);
        }

        char* passwd = getpass("Password: ");
        char* encrytped = crypt(passwd, salt);

        // login2 for section 2
        char buf[MAXLINE] = {0};
        sprintf(buf, "login2 %s", encrytped);

        // 先发长度
        Command cmd = CMD_LOGIN2;
        int buf_len = strlen(buf);
        int data_len = sizeof(cmd) + buf_len;
        sendn(sockfd, &data_len, sizeof(int));
        // 再发内容
        sendn(sockfd, &cmd, sizeof(cmd));
        sendn(sockfd, buf, buf_len);

        // 接收状态
        // 0: 成功, 1: 失败
        int user_stat = -1;
        recv(sockfd, &user_stat, sizeof(int), MSG_WAITALL);

        if (user_stat == 1) {
            printf("Password error, please re-enter\n");
            continue;
        } else {
            // 登录成功，获取用户上一次的工作目录
            int cwd_len = 0;
            bzero(cwd, MAXLINE);
            recv(sockfd, &cwd_len, sizeof(cwd_len), MSG_WAITALL);
            recv(sockfd, cwd, cwd_len, MSG_WAITALL);
            printf("cwd='%s'\n", cwd);
            break;
        }
    }
    return 0;
}
