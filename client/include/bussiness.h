#ifndef __K_BUSSINESS_H
#define __K_BUSSINESS_H

#include <func.h>

#include "parser.h"

typedef struct {
    int fd;
    char** args;
} Task;

int sendn(int sockfd, void* buf, int length);
int recvn(int sockfd, void* buf, int length);

void sendFile(int sockfd, const char* path);
void recvFile(int sockfd);

int cdCmd(int sockfd, char* buf, char* cwd, int* recv_status);
void lsCmd(int sockfd);
void pwdCmd(char* buf);
void exitCmd(char* buf);
void unknownCmd(char* buf);

#endif
