#ifndef __NB_TASK_H
#define __NB_TASK_H

#include "parser.h"

typedef struct {
    Command cmd;
    char* token;
    char* host;
    char* port;

    // gets, puts 需要的参数

    // TODO: 方案1：分片+合并
    // char* file;        // 文件名
    // int splice_start;  // 文件切片起始位置
    // int splice_size;   // 文件切片大小
} Task;

void freeTask(Task* task);

#endif
