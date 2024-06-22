#ifndef __NB_TASK_H
#define __NB_TASK_H

#include "dbpool.h"
#include "parser.h"

typedef struct {
    int fd;
    int uid;
    int* u_table;
    Command cmd;
    char** args;
    DBConnectionPool* dbpool;
    char* token;
} Task;

void freeTask(Task* task);

#endif
