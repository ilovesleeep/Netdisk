#include "../include/bussiness.h"

#define MAXLINE 1024

int kcdCmd(Task* task) {
    WorkDir* wd = task->wd_table[task->fd];
    char msg[MAXLINE] = {0};

    // 检查参数个数
    if (task->args[2] != NULL) {
        // 参数至多有一个
        // 发给 buf
        strcpy(msg, "Parameter at most 1");
        sendn(task->fd, msg, strlen(msg));
        return -1;
    } else {
        // 合法，发送一个长度为 0 的消息
        send(task->fd, "", 0, 0);
    }

    // 没有参数，回到家目录
    if (task->args[1] == NULL) {
        wd->path[wd->index[1] + 1] = '\0';
        wd->index[0] = 1;
        strcpy(msg, "~");
        sendn(task->fd, msg, strlen(msg));
        return 0;
    }

    // 获取当前路径
    char current_path[MAXLINE];
    strcpy(current_path, wd->path);
    current_path[wd->index[0] + 1] = '\0';

    // 检查目标路径是否有效
    char target_path[MAXLINE];
    sprintf(target_path, "%s/%s", current_path, task->args[1]);
    DIR* pdir;
    if ((pdir = opendir(target_path)) == NULL) {
        strcpy(msg, strerror(errno));
        sendn(task->fd, msg, strlen(msg));
        return -1;
    }

    // 更新 WorkDir
    // 初始路径为: user，客户端看到的：~
    char* tmp = strdup(task->args[1]);
    char* token = strtok(tmp, "/");
    while (token) {
        if (strcmp(token, ".") == 0) {
            continue;
        } else if (strcmp(token, "..") == 0) {
            if (wd->index[0] == 1) {
                // 没有上一级了
                strcpy(msg, "Permission denied");
                sendn(task->fd, msg, strlen(msg));
                return -1;
            }

            --(wd->index[0]);
            wd->path[wd->index[0] + 1] = '\0';
        } else {
            char* tmp_path = strdup(wd->path);
            sprintf(wd->path, "%s/%s", tmp_path, token);
            free(tmp_path);

            ++(wd->index[0]);
            wd->index[wd->index[0]] = strlen(token) + 1;  // +1 for '/'
        }
    }
    free(tmp);

    // 发送给客户端
    if (wd->index[0] == 1) {
        // 家目录，没有 '/' 为了美观
        strcpy(msg, "~");
        sendn(task->fd, msg, strlen(msg));
    } else {
        sprintf(msg, "~/%s",
                &wd->path[wd->index[0] + 2]);  // +2 for path start index
        sendn(task->fd, msg, strlen(msg));
    }

    return 0;
}
