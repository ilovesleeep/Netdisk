#ifndef __NB_BUSSINESS_H
#define __NB_BUSSINESS_H

#include "head.h"
#include "log.h"
#include "parser.h"
#include "shadow.h"

typedef struct {
    char name[32];
    char encrypted[128];
    char* path;
    int* index;
} WorkDir;

typedef struct {
    int fd;
    char** args;
    WorkDir** wd_table;  // connfd 作为索引
} Task;

void workdirInit(WorkDir** workdir_table, int connfd, char* username);
void workdirFree(WorkDir* workdir_tabled);

int sendn(int sockfd, void* buf, int length);
int recvn(int sockfd, void* buf, int length);

int sendFile(int sockfd, int fd);
int recvFile(int sockfd, char* path);

int cdCmd(Task* task);
void lsCmd(Task* task);
int deleteDir(const char* path);
void rmCmd(Task* task);
void pwdCmd(Task* task);
int getsCmd(Task* task);
int putsCmd(Task* task);
void mkdirCmd(Task* task);
void loginCheck1(Task* task);
void loginCheck2(Task* task);
void registerCmd(Task* task);
void unknownCmd(void);

int taskHandler(Task* task);
void taskFree(Task* task);

#endif
