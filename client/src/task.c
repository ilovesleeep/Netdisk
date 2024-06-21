#include "../include/task.h"

char** getNewConnectionInfo(char* res_data) { return parseRequest(res_data); }

Task* getNewConnectionTask(Command cmd, char* res_data) {
    // 响应内容中包含了：令牌 token, 资源服务器 host，端口 port 等其他信息
    // 解析响应内容，获取新的连接需要的信息，创建新连接(长命令)任务

    // info[0] token, info[1]: host, info[2]: port
    char** info = getNewConnectionInfo(res_data);

    Task* task = (Task*)malloc(sizeof(Task));
    task->cmd = cmd;
    task->token = strdup(info[0]);
    task->host = strdup(info[1]);
    task->port = strdup(info[2]);

    freeStringArray(info);

    return task;
}

void freeTask(Task* task) {
    free(task->token);
    free(task->host);
    free(task->port);
    free(task);
}
