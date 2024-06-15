#include "thread_pool.h"
//主线程调用:处理客户端发过来的消息
void handleMessage(int sockfd, int epfd, task_queue_t * que)
{
    //消息格式：cmd content
    //1.1 获取消息长度
    int length = -1;
    int ret = recvn(sockfd, &length, sizeof(length));
    printf("recv length: %d\n", length);

    //1.2 获取消息类型
    int cmdType = -1;
    ret = recvn(sockfd, &cmdType, sizeof(cmdType));
    printf("recv cmd type: %d\n", cmdType);
    task_t *ptask = (task_t*)calloc(1, sizeof(task_t));
    ptask->peerfd = sockfd;
    ptask->type= cmdType;
    if(length > 0) {
        //1.3 获取消息内容
        ret = recvn(sockfd, ptask->data, length);
        if(ret > 0) {
            //往线程池中添加任务
            taskEnque(que, ptask);
        }
    } else if(length == 0){
        taskEnque(que, ptask);
    }

    if(ret == 0) {//连接断开的情况
        printf("\nconn %d is closed.\n", sockfd);
        delEpollReadfd(epfd, sockfd);
        close(sockfd);
    }
}

//注意：此函数可以根据实际的业务逻辑，进行相应的扩展
void doTask(task_t * task)
{
    assert(task);
    switch(task->type) {
    case CMD_TYPE_PWD:  
        pwdCommand(task);   break;
    case CMD_TYPE_CD:
        cdCommand(task);    break;
    case CMD_TYPE_LS:
        lsCommand(task);    break;
    case CMD_TYPE_MKDIR:
        mkdirCommand(task);  break;
    case CMD_TYPE_RMDIR:
        rmdirCommand(task);  break;
    case CMD_TYPE_NOTCMD:
        notCommand(task);   break;
    case CMD_TYPE_PUTS:
        putsCommand(task);   break;
    case CMD_TYPE_GETS:
        getsCommand(task);   break;
    }
}

//每一个具体任务的执行，交给一个成员来实现

void cdCommand(task_t * task)
{
    printf("execute cd command.\n");
}


void mkdirCommand(task_t * task)
{
    printf("execute mkdir command.\n");
}

void rmdirCommand(task_t * task)
{
    printf("execute rmdir command.\n");
}

void notCommand(task_t * task)
{
    printf("execute not command.\n");
}

void putsCommand(task_t * task) {
    printf("execute puts command.\n");
}

void getsCommand(task_t * task) {
    printf("execute gets command.\n");
}
