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
