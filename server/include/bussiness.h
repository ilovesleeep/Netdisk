#ifndef __NB_BUSSINESS_H
#define __NB_BUSSINESS_H

#include <shadow.h>

#include "auth.h"
#include "dbpool.h"
#include "head.h"
#include "log.h"
#include "mysqloperate.h"
#include "parser.h"

typedef struct {
    char name[32];
    char encrypted[128];
    char* path;
    int* index;
} WorkDir;

typedef struct {
    int fd;
    int uid;
    int* u_table;
    Command cmd;
    char** args;
    DBConnectionPool* dbpool;
    WorkDir** wd_table;  // connfd 作为索引
} Task;

void workdirInit(WorkDir** workdir_table, int connfd, char* username);
void workdirFree(WorkDir* workdir_tabled);

int sendn(int sockfd, void* buf, int length);
int recvn(int sockfd, void* buf, int length);

int sendFile(int sockfd, int fd, off_t f_size);
int recvFile(int sockfd, MYSQL* mysql, int u_id);

int cdCmd(Task* task);
void lsCmd(Task* task);
int delFileOrDir(int pwdid);
int rmCmdHelper(int uid, int pwdid, char* name, char type);
void rmCmd(Task* task);
void pwdCmd(Task* task);
int getsCmd(Task* task);
int putsCmd(Task* task);
void mkdirCmd(Task* task);
void regCheck1(Task* task);
void regCheck2(Task* task);
void loginCheck1(Task* task);
void loginCheck2(Task* task);
void unknownCmd(void);

int taskHandler(Task* task);
void taskFree(Task* task);

#endif
