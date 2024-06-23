#ifndef __NB_TASK_H
#define __NB_TASK_H

#include "parser.h"

typedef struct {
    Command cmd;
    int uid;
    char* token;
    char* host;
    char* port;
    char** args;  // gets, puts 需要的参数

    // TODO: 多点下载的想法
    // 场景：一个主服务器
    // 客户端向主服务器发送 GETS1 命令，
    // GETS1 阶段成功后，主服务器会向客户端发送
    //
    // char* file;        // 文件名
    // int splice_start;  // 文件切片起始位置
    // int splice_size;   // 文件切片大小
} Task;

void freeTask(Task* task);

#endif
