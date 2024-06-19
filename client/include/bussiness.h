#ifndef __NB_BUSSINESS_H
#define __NB_BUSSINESS_H

#include "head.h"
#include "parser.h"

typedef struct {
    int fd;
    char** args;
} Task;

int sendn(int sockfd, void* buf, int length);
int recvn(int sockfd, void* buf, int length);

void sendFile(int sockfd, int fd);
void recvFile(int sockfd);

int cdCmd(int sockfd, char* buf, char* cwd);
void lsCmd(int sockfd);
void pwdCmd(char* buf);
void getsCmd(int sockfd);
void mkdirCmd(int sockfd, char* buf);
void rmCmd(int sockfd, char* buf);
void putsCmd(int sockfd, char** args);
void exitCmd(char* buf);

void unknownCmd(char* buf);

#endif
