#ifndef __NB_SERVER_H
#define __NB_SERVER_H

#include "network.h"
#include "parser.h"
#include "threadpool.h"

int serverMain(void);
int serverExit(ThreadPool* pool);
void addTask(int connfd, ThreadPool* pool);

#endif
