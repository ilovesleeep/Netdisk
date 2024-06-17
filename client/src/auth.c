#include "../include/auth.h"

#define MAXLINE 1024
#define MAX_USERINFO_LENGTH 256

int userRegister(char* name, char* passwd) {
    printf(
        "name  : %s\n"
        "passwd: %s\n",
        name, passwd);
    return 0;
}

int userLogin(char* name, char* passwd) {
    bzero(name, MAX_USERINFO_LENGTH);
    bzero(passwd, MAX_USERINFO_LENGTH);

    printf("Enter username: ");
    fflush(stdout);
    int name_len = read(STDIN_FILENO, name, MAX_USERINFO_LENGTH);
    name[--name_len] = '\0';

    printf("Enter password: ");
    fflush(stdout);
    int passwd_len = read(STDIN_FILENO, passwd, MAX_USERINFO_LENGTH);
    passwd[--passwd_len] = '\0';

    printf("hello %s\n", name);
    return 0;
}
