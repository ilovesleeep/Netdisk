#ifndef __NB_SERVER_H
#define __NB_SERVER_H

#include "dbpool.h"
#include "log.h"
#include "network.h"
#include "parser.h"
#include "threadpool.h"

typedef struct {
    char port[8];
    int num_threads;
} ServerConfig;

void serverInit(ServerConfig* conf, HashTable* ht);
int serverMain(ServerConfig* conf, HashTable* ht);
int serverExit(ThreadPool* pool);

#endif
