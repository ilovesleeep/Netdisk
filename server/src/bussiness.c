#include "../include/bussiness.h"

#include <stdlib.h>

#define BUFSIZE 4096
#define MAXLINE 1024
#define BIGFILE_SIZE (100 * 1024 * 1024)
#define MMAPSIZE (1024 * 1024)

typedef struct {
    int length;
    char data[BUFSIZE];
} DataBlock;

int sendn(int fd, void* buf, int length) {
    int bytes = 0;
    while (bytes < length) {
        int n = send(fd, (char*)buf + bytes, length - bytes, MSG_NOSIGNAL);
        if (n < 0) {
            return -1;
        }

        bytes += n;
    }  // bytes == length

    return 0;
}

int recvn(int fd, void* buf, int length) {
    int i = 0;

    int bytes = 0;
    while (bytes < length) {
        int n = recv(fd, (char*)buf + bytes, length - bytes, 0);
        if (n < 0) {
            return -1;
        }
        if (n == 0) {
            if (++i ==
                1000) {  // 如果接收了1000次0长度的信息，那对端一定是关闭了，要么就是网太差，直接踢出去
                return -1;
            }
        }

        bytes += n;
    }  // bytes == length

    return 0;
}

int sendFile(int sockfd, int fd) {
    // 发送文件大小
    struct stat statbuf;
    fstat(fd, &statbuf);
    off_t fsize = statbuf.st_size;
    sendn(sockfd, &fsize, sizeof(fsize));

    // 接收客户端本文件的大小及哈希值

    // 发送文件内容
    off_t sent_bytes = 0;
    if (fsize >= BIGFILE_SIZE) {
        // 大文件
        while (sent_bytes < fsize) {
            off_t length =
                fsize - sent_bytes >= MMAPSIZE ? MMAPSIZE : fsize - sent_bytes;

            void* addr =
                mmap(NULL, length, PROT_READ, MAP_SHARED, fd, sent_bytes);
            if (sendn(sockfd, addr, length) == -1) {
                close(fd);
                return 1;
            }
            munmap(addr, length);

            sent_bytes += length;
        }
    } else {
        // 小文件
        char buf[BUFSIZE];
        while (sent_bytes < fsize) {
            off_t length =
                fsize - sent_bytes >= BUFSIZE ? BUFSIZE : fsize - sent_bytes;

            read(fd, buf, length);
            if (sendn(sockfd, buf, length) == -1) {
                close(fd);
                return 1;
            }

            sent_bytes += length;
        }
    }
    close(fd);
    return 0;
}

int recvFile(int sockfd, char* path) {
    // 接收文件名
    DataBlock block;
    bzero(&block, sizeof(block));
    recvn(sockfd, &block.length, sizeof(int));
    if (recvn(sockfd, block.data, block.length) == -1) {
        return 1;
    }

    // 拼接出path_file
    char path_file[1000] = {0};
    sprintf(path_file, "%s/%s", path, block.data);
    printf("%s\n", block.data);

    // 打开文件
    int fd = open(path_file, O_RDWR | O_TRUNC | O_CREAT, 0666);
    if (fd == -1) {
        error(1, errno, "open");
    }

    // 接收文件的大小
    off_t fsize;
    recvn(sockfd, &fsize, sizeof(fsize));

    // 接收文件内容
    off_t recv_bytes = 0;
    if (fsize >= BIGFILE_SIZE) {
        ftruncate(fd, fsize);
        // 大文件
        while (recv_bytes < fsize) {
            off_t length = (fsize - recv_bytes >= MMAPSIZE)
                               ? MMAPSIZE
                               : fsize - recv_bytes;
            void* addr = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED,
                              fd, recv_bytes);
            if (recvn(sockfd, addr, length) == -1) {
                close(fd);
                return 1;
            }
            munmap(addr, length);

            recv_bytes += length;

            printf("[INFO] downloading %5.2lf%%\r", 100.0 * recv_bytes / fsize);
            fflush(stdout);
        }
    } else {
        char buf[BUFSIZE];
        while (recv_bytes < fsize) {
            off_t length =
                (fsize - recv_bytes >= BUFSIZE) ? BUFSIZE : fsize - recv_bytes;
            if (recvn(sockfd, buf, length) == -1) {
                close(fd);
                return 1;
            }
            write(fd, buf, length);

            recv_bytes += length;

            printf("[INFO] downloading %5.2lf%%\r", 100.0 * recv_bytes / fsize);
            fflush(stdout);
        }
    }
    printf("[INFO] downloading %5.2lf%%\n", 100.0);
    close(fd);
    return 0;
}

int cdCmd(Task* task) {
    WorkDir* wd = task->wd_table[task->fd];
    char msg[MAXLINE] = {0};
    int send_status = 0;

    // 检查参数个数是否合法
    if (task->args[2] != NULL) {
        // 不合法
        send_status = 1;
        sendn(task->fd, &send_status, sizeof(int));

        strcpy(msg, "Parameter at most 1");
        sendn(task->fd, msg, strlen(msg));
        return -1;
    }

    // 没有参数，回到家目录
    if (task->args[1] == NULL) {
        sendn(task->fd, &send_status, sizeof(int));

        wd->path[wd->index[1] + 1] = '\0';
        wd->index[0] = 1;
        strcpy(msg, "~");
        sendn(task->fd, msg, strlen(msg));
        return 0;
    }

    // 获取当前路径
    char current_path[MAXLINE];
    strcpy(current_path, wd->path);
    current_path[wd->index[wd->index[0]] + 1] = '\0';

    // 检查目标路径是否有效
    char target_path[MAXLINE];
    sprintf(target_path, "%s/%s", current_path, task->args[1]);
    DIR* pdir;
    if ((pdir = opendir(target_path)) == NULL) {
        send_status = 1;
        sendn(task->fd, &send_status, sizeof(int));

        strcpy(msg, strerror(errno));
        sendn(task->fd, msg, strlen(msg));
        return -1;
    }
    closedir(pdir);

    // 更新 WorkDir
    char* tmp = strdup(task->args[1]);
    char* token = strtok(tmp, "/");
    while (token) {
        if (strcmp(token, ".") == 0) {
            ;  // nothing to do
        } else if (strcmp(token, "..") == 0) {
            if (wd->index[0] == 1) {
                // 没有上一级了
                send_status = 1;
                sendn(task->fd, &send_status, sizeof(int));

                strcpy(msg, "Permission denied");
                sendn(task->fd, msg, strlen(msg));
                free(tmp);
                return -1;
            }

            int pre_idx = wd->index[--wd->index[0]];
            wd->path[pre_idx + 1] = '\0';
        } else {
            char* tmp_path = strdup(wd->path);
            sprintf(wd->path, "%s/%s", tmp_path, token);
            free(tmp_path);

            int len = wd->index[wd->index[0]];
            wd->index[++wd->index[0]] = len + strlen(token) + 1;  // +1 for '/'
        }
        token = strtok(NULL, "/");
    }
    free(tmp);

    // 发送给客户端
    sendn(task->fd, &send_status, sizeof(int));
    int offset = wd->index[1] + 1;  // +1 for start position
    sprintf(msg, "~%s", wd->path + offset);
    sendn(task->fd, msg, strlen(msg));

    return 0;
}

void lsCmd(Task* task) {
    // 校验参数,发送校验结果，若为错误则继续发送错误信息
    if (task->args[1] != NULL) {
        int sendstat = 1;
        send(task->fd, &sendstat, sizeof(int), MSG_NOSIGNAL);
        char error_info[] = "parameter number error";
        int info_len = strlen(error_info);
        send(task->fd, &info_len, sizeof(int), MSG_NOSIGNAL);
        send(task->fd, error_info, info_len, MSG_NOSIGNAL);
        return;
    } else {
        int sendstat = 0;
        send(task->fd, &sendstat, sizeof(int), MSG_NOSIGNAL);
    }

    // 获取当前路径
    char path[1000] = {0};
    WorkDir* pathbase = task->wd_table[task->fd];
    strncpy(path, pathbase->path, pathbase->index[pathbase->index[0]] + 1);

    // 打开目录
    DIR* dir = opendir(pathbase->path);

    // 发送文件信息
    struct dirent* p = NULL;
    while ((p = readdir(dir))) {
        if (strcmp(p->d_name, ".") == 0 || strcmp(p->d_name, "..") == 0) {
            continue;
        }
        int info_len = strlen(p->d_name);
        char send_info[1000] = {0};
        strcpy(send_info, p->d_name);
        // 发送文件名长度及文件名
        send(task->fd, &info_len, sizeof(int), MSG_NOSIGNAL);
        send(task->fd, send_info, info_len, MSG_NOSIGNAL);
    }
    // 发送int类型的0(这个文件名长度为0)代表文件已发完
    int info_len = 0;
    send(task->fd, &info_len, sizeof(int), MSG_NOSIGNAL);
    return;
}

// rm -r
void deleteDir(const char* dir) {
    // 打开目录
    DIR* stream = opendir(dir);
    if (stream == NULL) {
        error(1, errno, "opendir %s", dir);
    }

    // 遍历目录流，依次删除每一个目录项
    errno = 0;
    struct dirent* pdirent;
    while ((pdirent = readdir(stream)) != NULL) {
        // 忽略.和..
        char* name = pdirent->d_name;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
            continue;
        }

        // 注意，这里才开始拼接路径
        char subpath[MAXLINE];
        sprintf(subpath, "%s/%s", dir, name);
        if (pdirent->d_type == DT_DIR) {
            // 拼接路径
            deleteDir(subpath);
        } else if (pdirent->d_type == DT_REG) {
            unlink(subpath);
        }
    }

    // 关闭目录流
    closedir(stream);

    if (errno) {
        error(1, errno, "readdir");
    }
    // 再删除该目录
    rmdir(dir);
}

void rmCmd(Task* task) {
    // TODO:
    // 删除每一个目录项
    // 校验参数，发送校验结果，若为错误则发送错误信息
    if (task->args[2] != NULL) {
        int sendstat = 1;  // 错误
        send(task->fd, &sendstat, sizeof(int), MSG_NOSIGNAL);
        char error_info[] = "parameter number error";
        int info_len = strlen(error_info);
        send(task->fd, &info_len, sizeof(int), MSG_NOSIGNAL);
        send(task->fd, error_info, info_len, MSG_NOSIGNAL);
        return;
    } else {
        int sendstat = 0;  // 正确
        send(task->fd, &sendstat, sizeof(int), MSG_NOSIGNAL);
    }

    // 获取当前路径
    char curr_path[MAXLINE] = {0};
    WorkDir* wd = task->wd_table[task->fd];
    strncpy(curr_path, wd->path, strlen(wd->path));

    // 拼接路径
    char dir[2 * MAXLINE] = {0};
    sprintf(dir, "%s/%s", curr_path, task->args[1]);

    // 使用deleteDir函数删除文件

    if (remove(dir) == 0) {
        printf("Successfully deleted %s\n", dir);
    } else if (remove(dir) == -1 || rmdir(dir) == -1) {
        // 检查是否是因为目录不存在导致的错误
        if (errno == ENOENT) {
            fprintf(stderr, "Error: The directory '%s' doesn't exist.\n", dir);
        } else {
            // 打印其他错误
            fprintf(stderr, "Error: Unable to remove directory '%s':%s\n", dir,
                    strerror(errno));
            return;
        }
    } else {
        deleteDir(dir);
    }
    // send(task->fd,"0",sizeof("0"),0);
    //} else {
    //   perror("Error deleting file");
    //}

    return;
}

void pwdCmd(Task* task) {
    // TODO:
    char path[MAXLINE] = {0};
    WorkDir* wd = task->wd_table[task->fd];
    strncpy(path, wd->path, strlen(wd->path));

    sendn(task->fd, path, sizeof(path));

    return;
}

void getsCmd(Task* task) {
    // 获取当前路径
    char path[1000] = {0};
    WorkDir* pathbase = task->wd_table[task->fd];
    strncpy(path, pathbase->path, pathbase->index[pathbase->index[0]] + 1);

    // 确认参数数量是否正确
    if (task->args[1] == NULL) {
        int send_stat = 1;
        send(task->fd, &send_stat, sizeof(int), MSG_NOSIGNAL);
        char error_info[] = "no such parameter";
        int info_len = strlen(error_info);
        send(task->fd, &info_len, sizeof(int), MSG_NOSIGNAL);
        send(task->fd, error_info, info_len, MSG_NOSIGNAL);
        return;
    } else {
        int send_stat = 0;
        send(task->fd, &send_stat, sizeof(int), MSG_NOSIGNAL);
    }
    // 发送文件
    char** parameter = task->args;
    while (*(++parameter)) {
        static int i = 0;  // 第一个文件
        i++;
        char path_file[1000] = {0};
        sprintf(path_file, "%s/%s", path, *parameter);

        int fd = open(path_file, O_RDWR);
        // 检查文件是否存在
        // 不存在
        if (fd == -1) {
            int send_stat = 1;
            send(task->fd, &send_stat, sizeof(int), MSG_NOSIGNAL);

            char error_info[1000] = {0};
            sprintf(error_info, "%s%d", "no such file : file number : ", i);
            int info_len = strlen(error_info);
            send(task->fd, &info_len, sizeof(int), MSG_NOSIGNAL);
            send(task->fd, error_info, info_len, MSG_NOSIGNAL);
            return;
        }
        // 存在
        int send_stat = 0;
        send(task->fd, &send_stat, sizeof(int), MSG_NOSIGNAL);
        // 先发文件名
        DataBlock block;
        strcpy(block.data, *parameter);
        block.length = strlen(*parameter);
        sendn(task->fd, &block, sizeof(int) + block.length);
        if (sendFile(task->fd, fd) ==
            1) {  // sendfile中close了fd,若返回值为1证明连接中断,则不进行剩余发送任务
            return;
        }
    }
    // 所有文件发送完毕
    int send_stat = 1;
    send(task->fd, &send_stat, sizeof(int), MSG_NOSIGNAL);
    char send_info[] = "SUCCESS";
    int info_len = strlen(send_info);
    send(task->fd, &info_len, sizeof(int), MSG_NOSIGNAL);
    send(task->fd, send_info, info_len, MSG_NOSIGNAL);
    return;
}

void putsCmd(Task* task) {
    // 默认存放在当前目录
    char path[1000] = {0};
    WorkDir* pathbase = task->wd_table[task->fd];
    strncpy(path, pathbase->path, pathbase->index[pathbase->index[0]] + 1);

    // 告诉客户端已就绪
    int recv_stat = 0;
    send(task->fd, &recv_stat, sizeof(int), MSG_NOSIGNAL);

    for (int i = 0; true; i++) {
        // 先接收是否要发送
        int recv_stat = 0;
        if (recv(task->fd, &recv_stat, sizeof(int), MSG_WAITALL) == -1) {
            break;
        }

        // 不发送
        if (recv_stat != 0) {
            break;
        }

        if (recvFile(task->fd, path) == 1) {
            break;
        }
    }

    return;
}

void mkdirCmd(Task* task) {
    // if (sizeof(task ->args[1]) >= 1000) {
    //     error(1, 0, "mkdir_dirlen too long!");
    // }

    // 找到当前目录
    // bug: 如果在极端情况下，path有1k长度，mkdir_dirlen也有1k长度，会溢出
    char curr_path[MAXLINE] = {0};
    WorkDir* wd = task->wd_table[task->fd];
    strncpy(curr_path, wd->path, strlen(wd->path));

    int index = wd->index[wd->index[0]];
    curr_path[index + 1] = '\0';

    // 将当前目录和args[1]拼接在一起
    char dir[2 * MAXLINE] = {0};
    sprintf(dir, "%s/%s", curr_path, task->args[1]);

    // debug
    // printf("wd->index[1] = %d, wd->index[0] = %d\n", wd->index[wd->index[0]],
    // wd->index[0]); printf("wdpath = %s\n", wd->path); printf("curpath =
    // %s\n", curr_path); printf("dir = %s\n",dir);

    // 根据dir递归创建目录
    // 找每个'/',将其替换成'\0'
    for (char* p = dir + 1; *p; ++p) {
        if (*p == '/') {
            // 这里没有跳过多余的'/'，但没有出bug，大概率是mkdir背后做了这个事情
            *p = '\0';

            if (mkdir(dir, 0777) && errno != EEXIST) {
                char errmsg[MAXLINE] = "mkdir";
                strncat(errmsg, strerror(errno),
                        sizeof(errmsg) - strlen(strerror(errno)) - 1);
                send(task->fd, errmsg, strlen(errmsg), 0);
                // 后面补日志
                error(0, errno, "%d mkdir:", task->fd);
            }

            *p = '/';
        }
    }

    if (mkdir(dir, 0777) && errno != EEXIST) {
        char errmsg[MAXLINE] = "mkdir";
        strncat(errmsg, strerror(errno),
                sizeof(errmsg) - strlen(strerror(errno)) - 1);
        send(task->fd, errmsg, strlen(errmsg), 0);
        // 后面补日志
        error(0, errno, "%d mkdir:", task->fd);
    }

    // 成功了给客户端发一个0
    send(task->fd, "0", sizeof("0"), 0);

    return;
}

void unknownCmd(void) {
    printf("[WARN] unknownCmd\n");
    return;
}

void taskHandler(Task* task) {
    switch (getCommand(task->args[0])) {
        case CMD_CD:
            cdCmd(task);
            break;
        case CMD_LS:
            lsCmd(task);
            break;
        case CMD_RM:
            rmCmd(task);
            break;
        case CMD_PWD:
            pwdCmd(task);
            break;
        case CMD_MKDIR:
            mkdirCmd(task);
            break;
        case CMD_GETS:
            getsCmd(task);
            break;
        case CMD_PUTS:
            putsCmd(task);
            break;
        default:
            unknownCmd();
            break;
    }
}

void taskFree(Task* task) {
    for (int i = 0; task->args[i] != NULL; ++i) {
        free(task->args[i]);
    }
    free(task->args);
    free(task);
    // task->wd_table 在客户端断开时 free
}

void workdirInit(WorkDir** workdir_table, int connfd, char* username) {
    // TODO: error checking
    workdir_table[connfd] = (WorkDir*)malloc(sizeof(WorkDir));
    workdir_table[connfd]->path = (char*)malloc(MAXLINE * sizeof(char));
    workdir_table[connfd]->index = (int*)malloc(MAXLINE * sizeof(int));

    strcpy(workdir_table[connfd]->path, username);
    workdir_table[connfd]->index[0] = 1;
    workdir_table[connfd]->index[1] = strlen(username) - 1;
}

void workdirFree(WorkDir* workdir) {
    free(workdir->path);
    free(workdir->index);
    free(workdir);
}
