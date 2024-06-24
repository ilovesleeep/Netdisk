#ifndef __NB_BUSSINESS_H
#define __NB_BUSSINESS_H

#include "head.h"
#include "parser.h"
#include "task.h"

int sendn(int sockfd, void* buf, int length);
int recvn(int sockfd, void* buf, int length);

int sendFile(int sockfd, int fd);
int recvFile(int sockfd);

int cdCmd(int sockfd, char* cwd);
int lsCmd(int sockfd);
int pwdCmd(int sockfd);
int getsCmd(int sockfd);
int mkdirCmd(int sockfd);
int rmCmd(int sockfd);
int putsCmd(int sockfd, char** args);

void unknownCmd(char* buf);

#endif
