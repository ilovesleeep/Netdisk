#ifndef __NB_BUSSINESS_H
#define __NB_BUSSINESS_H

#include "head.h"
#include "parser.h"

typedef struct {
    Command cmd;
    char* token;
    char* host;
    char* port;

    // TODO: 方案1：分片+合并
    // char* file;        // 文件名
    // int splice_start;  // 文件切片起始位置
    // int splice_size;   // 文件切片大小
} Task;

Task* getNewConnectionTask(Command cmd, char* res_data);
void freeTask(Task* task);

int sendn(int sockfd, void* buf, int length);
int recvn(int sockfd, void* buf, int length);

int sendFile(int sockfd, int fd);
int recvFile(int sockfd);

int cdCmd(int sockfd, char* buf, char* cwd);
int lsCmd(int sockfd);
int pwdCmd(int sockfd);
int getsCmd(int sockfd);
int mkdirCmd(int sockfd, char* buf);
int rmCmd(int sockfd, char* buf);
int putsCmd(int sockfd, char** args);

void unknownCmd(char* buf);

#endif
