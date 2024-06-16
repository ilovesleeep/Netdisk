#ifndef __K_BUSSINESS_H
#define __K_BUSSINESS_H

#include <func.h>

#include "parser.h"

typedef struct {
    char* path;
    int* index;
} WorkDir;

typedef struct {
    int fd;
    char** args;
    WorkDir** wd_table;  // connfd 作为索引
} Task;

void workdirInit(WorkDir** workdir_table, int connfd, char* username);
void workdirFree(Task* task);

int sendn(int sockfd, void* buf, int length);
int recvn(int sockfd, void* buf, int length);

void sendFile(int sockfd, const char* path);
void recvFile(int sockfd);

void cdCmd(Task* task);
void lsCmd(Task* task);
void rmCmd(Task* task);
void pwdCmd(Task* task);
void getsCmd(Task* task);
void putsCmd(Task* task);
void mkdirCmd(Task* task);
void exitCmd(Task* task);
void unknownCmd(Task* task);

void taskHandler(Task* task);

#endif
