#ifndef __NB_BUSSINESS_H
#define __NB_BUSSINESS_H

#include "auth.h"
#include "head.h"
#include "log.h"
#include "mysqloperate.h"
#include "task.h"
#include "transfer.h"

typedef struct {
    int length;
    char data[4096];
} DataBlock;

int sendFile(int sockfd, int fd, off_t f_size);
int recvFile(int sockfd, MYSQL* mysql, int uid);

int cdCmd(Task* task);
void lsCmd(Task* task);
int delFileOrDir(MYSQL *mysql,int pwdid);
int rmCmdHelper(MYSQL *mysql,int uid,int pwdid,char *name,char type);
void rmCmd(Task* task);
void pwdCmd(Task* task);
int getsCmd(Task* task);
int putsCmd(Task* task);
void mkdirCmd(Task* task);
void unknownCmd(void);

int taskHandler(Task* task);

#endif
