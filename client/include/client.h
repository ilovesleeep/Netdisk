#ifndef __NB_CLIENT_H
#define __NB_CLIENT_H

#include <ctype.h>

#include "auth.h"
#include "bussiness.h"
#include "head.h"
#include "network.h"
#include "parser.h"
#include "threadpool.h"

int clientMain(int argc, char* argv[]);

void printMenu(void);

void welcome(int sockfd, char* username);

int sessionHandler(int sockfd, char* host, char* user);

int responseHandler(int sockfd, ThreadPool* pool);

int getNewConnectionInfo(int sockfd, int* uid, char* new_host, char* new_port,
                         char* token);
Task* getNewConnectionTask(Command cmd, char* res_data);

#endif
