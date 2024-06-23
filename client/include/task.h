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
    // 场景：一个客户端，一个调度服务器，多个资源服务器
    //
    // GETS1 阶段
    //             客户端向调度服务器发送请求(GETS1)
    //             请求包含：要下载的文件，最大同时下载线程数
    //
    //             调度服务器收到请求，根据客户端发来的线程数（假设为4）
    //             向客户端发送 4 个响应信息
    //             每一次响应都包含了一个资源服务器信息和文件信息
    //             响应包含：host, port, 文件哈希值，第几个切片
    //
    //             客户端接收这些信息，放入任务队列
    // GETS1 结束
    //
    //
    // GETS2 阶段
    //             客户端的子线程从任务队列取出任务，得到新连接需要的信息
    //             子线程与资源服务器建立新的连接，发送 GETS2 请求
    //             请求包含：token，文件哈希，第几个切片
    //
    //             资源服务器收到请求，验证 token
    //             验证通过后，向客户端子线程发送确认信息
    //             然后根据文件哈希开始发送对应文件的切片数据
    //
    //             客户端子线程接收切片数据，在对应位置写入文件(已truncate)
    //             (或每个子线程写入不同的临时文件，最后进行合并)
    // GETS2 阶段
    //
    //
    //
    // int splice_start;  // 文件切片起始位置
    // int splice_size;   // 文件切片大小
} Task;

void freeTask(Task* task);

#endif
