#ifndef __NB_SERVER_H
#define __NB_SERVER_H

#include "log.h"
#include "network.h"
#include "parser.h"
#include "threadpool.h"

typedef struct {
    char port[8];
    int num_threads;
} ServerConfig;

int serverMain(int argc, char* argv[]);
int serverExit(ThreadPool* pool);
void setServerConfig(HashTable* ht, ServerConfig* conf);

void requestHandler(int connfd, ThreadPool* pool, WorkDir** workdir_table);

#endif
