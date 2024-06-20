#define OPENSSL_SUPPRESS_DEPRECATED

#include "../include/bussiness.h"

#include <openssl/md5.h>
#include <stdlib.h>

#include "../include/dbpool.h"
#include "../include/mysqloperate.h"

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
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

    // 接收客户端对本文件是否存在过的确认及哈希值
    // 收到客户端是否存在过的确认，若存在则检查哈希值，若不存在则直接发送
    int recv_stat = 0;
    recv(sockfd, &recv_stat, sizeof(int), MSG_WAITALL);

    off_t send_bytes = 0;
    if (recv_stat == 1) {
        // 文件存在过,检查哈希值
        // 先看看他有多大的文件
        recvn(sockfd, &send_bytes, sizeof(send_bytes));
        unsigned char md5sum_client[16];
        recvn(sockfd, md5sum_client, sizeof(md5sum_client));
        if (send_bytes > statbuf.st_size) {
            // 我服务器的文件都没那么大,你哪来那么大,我给你重发一个
            send_bytes = 0;
        } else {
            // 先根据收到的文件大小计算自己的哈希值(服务器的文件不可能有文件空洞)
            MD5_CTX ctx;
            MD5_Init(&ctx);
            for (off_t curr = 0; curr < send_bytes; curr += MMAPSIZE) {
                if (curr + MMAPSIZE <= send_bytes) {
                    char* p = mmap(NULL, MMAPSIZE, PROT_READ | PROT_WRITE,
                                   MAP_SHARED, fd, curr);
                    MD5_Update(&ctx, p, MMAPSIZE);
                    munmap(p, MMAPSIZE);
                } else {
                    int surplus = send_bytes - curr;
                    char* p = mmap(NULL, surplus, PROT_READ | PROT_WRITE,
                                   MAP_SHARED, fd, curr);
                    MD5_Update(&ctx, p, surplus);
                    munmap(p, surplus);
                    break;
                }
            }
            // 生成MD5值
            unsigned char md5sum[16];
            MD5_Final(md5sum, &ctx);

            // 比较
            if (memcmp(md5sum_client, md5sum, sizeof(md5sum)) == 0) {
                // 是一个文件(＾－＾),继续发送叭
                int send_stat = 0;
                sendn(sockfd, &send_stat, sizeof(int));
            } else {
                // 不是一个文件,重新来过吧
                int send_stat = 1;
                sendn(sockfd, &send_stat, sizeof(int));
                send_bytes = 0;
            }
        }
    }
#pragma GCC diagnostic pop
    // 此时send_bytes对应正确的开始发送位置
    //  发送文件内容
    if (fsize >= BIGFILE_SIZE) {
        // 大文件
        while (send_bytes < fsize) {
            off_t length =
                fsize - send_bytes >= MMAPSIZE ? MMAPSIZE : fsize - send_bytes;

            void* addr =
                mmap(NULL, length, PROT_READ, MAP_SHARED, fd, send_bytes);
            if (sendn(sockfd, addr, length) == -1) {
                munmap(addr, length);
                close(fd);
                return 1;
            }
            munmap(addr, length);

            send_bytes += length;
        }
    } else {
        // 小文件
        char buf[BUFSIZE];
        while (send_bytes < fsize) {
            off_t length =
                fsize - send_bytes >= BUFSIZE ? BUFSIZE : fsize - send_bytes;

            read(fd, buf, length);
            if (sendn(sockfd, buf, length) == -1) {
                close(fd);
                return 1;
            }

            send_bytes += length;
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
    int fd = open(path_file, O_RDWR | O_CREAT, 0666);
    if (fd == -1) {
        error(1, errno, "open");
    }

    // 接收文件的大小
    off_t fsize;
    recvn(sockfd, &fsize, sizeof(fsize));
    off_t recv_bytes = 0;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

    // 检查0为没有存在过,1为存在过
    struct stat statbuf;
    fstat(fd, &statbuf);
    if (statbuf.st_size < MMAPSIZE) {
        // 没有存在过(小于MMAPSIZE都当没存在过处理,不差那1M流量,懒得再算哈希值)
        int send_stat = 0;
        send(sockfd, &send_stat, sizeof(int), MSG_NOSIGNAL);
    } else {
        // 存在过,检查哈希值,检查哈希值全部以MMAPSIZE为单位来查找
        int send_stat = 1;
        sendn(sockfd, &send_stat, sizeof(int));
        // 计算哈希值
        char empty[MMAPSIZE] = {0};
        MD5_CTX ctx;
        MD5_Init(&ctx);

        // prev是后面即将要用的数据,每次计算确认当前数据可用时才为其赋值
        off_t prev_bytes = 0;
        MD5_CTX prev_ctx;
        for (recv_bytes = 0; recv_bytes < statbuf.st_size;
             recv_bytes += MMAPSIZE) {
            if (recv_bytes + MMAPSIZE <= statbuf.st_size) {
                // 当前大小小于文件大小,计算
                char* p = mmap(NULL, MMAPSIZE, PROT_READ | PROT_WRITE,
                               MAP_SHARED, fd, recv_bytes);
                if (memcmp(p, empty, MMAPSIZE) == 0) {
                    // 文件空洞,计算到此为止,就用上一次的哈希值和recv_bytes
                    memcpy(&ctx, &prev_ctx, sizeof(ctx));
                    recv_bytes = prev_bytes;
                    munmap(p, MMAPSIZE);
                    break;
                } else {
                    // 非文件空洞,继续计算
                    memcpy(&prev_ctx, &ctx, sizeof(ctx));
                    prev_bytes = recv_bytes;

                    MD5_Update(&ctx, p, MMAPSIZE);
                    munmap(p, MMAPSIZE);
                }
            } else {
                // 继续mmap这个大小就要超啦,看看最后一点一不一样
                int surplus = statbuf.st_size - recv_bytes;
                char* p = mmap(NULL, surplus, PROT_READ | PROT_WRITE,
                               MAP_SHARED, fd, recv_bytes);
                char* empty = calloc(surplus, sizeof(char));
                if (memcmp(p, empty, surplus) == 0) {
                    free(empty);
                    // 文件空洞,计算到此为止,就用上一次的哈希值和recv_bytes
                    memcpy(&ctx, &prev_ctx, sizeof(ctx));
                    recv_bytes = prev_bytes;
                    munmap(p, surplus);
                    break;
                } else {
                    // 非文件空洞,全部都是有效信息,计算所有的哈希值,offset移动到末尾
                    free(empty);

                    recv_bytes += surplus;
                    MD5_Update(&ctx, p, surplus);

                    munmap(p, surplus);
                    break;
                }
            }
        }
        // 生成哈希值
        unsigned char md5sum[16];
        MD5_Final(md5sum, &ctx);
        // 发送文件实际大小及哈希值
        sendn(sockfd, &recv_bytes, sizeof(recv_bytes));
        sendn(sockfd, md5sum, sizeof(md5sum));

        // 看看文件是不是一样的呀
        int recv_stat = 0;
        recvn(sockfd, &recv_stat, sizeof(int));
        if (recv_stat == 1) {
            // 糟糕!文件不一样
            recv_bytes = 0;
        }
        // 文件一样,recv_bytes指向的是开始接收的位置
    }
#pragma GCC diagnostic pop
    // 此时recv_bytes对应正确的开始接收位置

    // 接收文件内容
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

    // 备份，越权时回退
    char* path_bak = strdup(wd->path);
    int index_bak[MAXLINE] = {0};
    memcpy(&index_bak, wd->index, MAXLINE * sizeof(int));

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

                log_warn("[%s] want to escape", wd->name);

                // 回退
                strcpy(wd->path, path_bak);
                free(path_bak);
                memcpy(wd->index, &index_bak, MAXLINE * sizeof(int));

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
        char* error_info = "parameter number error";
        int info_len = strlen(error_info);
        send(task->fd, &info_len, sizeof(int), MSG_NOSIGNAL);
        send(task->fd, error_info, info_len, MSG_NOSIGNAL);
        log_error("lsCmd: parameter number error");
        return;
    } else {
        int sendstat = 0;
        send(task->fd, &sendstat, sizeof(int), MSG_NOSIGNAL);
    }

    // 获取当前路径
    MYSQL* mysql = getDBConnection(task->dbpool); 
    // int pwdid = getPwdId(mysql, task->uid);
    int pwdid = 1;
    char** family = findchild(mysql, pwdid);
    releaseDBConnection(task->dbpool, mysql);

    // bufsize = 4096
    char result[BUFSIZE] = {0};

    while (*family != NULL) {
        strncat(result, *family, sizeof(result) - strlen(*family) - 1);
        strncat(result, "\t", sizeof(result) - strlen("\t"));
        family++;
    }

    for (int i = 0; family[i] != NULL; ++i) {
        free(family);
    }
    free(family);
    
    // 发送（大火车）
    int info_len = strlen(result);
    send(task->fd, &info_len, sizeof(int), MSG_NOSIGNAL);
    send(task->fd, result, info_len, MSG_NOSIGNAL);

    return;
}

// 使用单独的函数来实现命令的功能
int deleteDir(const char* dir) {}

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
    int errnum;
    if (remove(dir) == 0) {
        printf("Successfully deleted %s\n", dir);
        int send_stat = 0;
        send(task->fd, &send_stat, sizeof(int), MSG_NOSIGNAL);
    } else if ((errnum = deleteDir(dir)) == 0) {
        printf("Successfully deleted %s\n", dir);
        int send_stat = 0;
        send(task->fd, &send_stat, sizeof(int), MSG_NOSIGNAL);
    }

    // 如果删除不存在的目录，则返回报错信息
    int send_status = 1;
    char msg[MAXLINE] = {0};
    if (errnum == ENOENT) {
        send(task->fd, &send_status, sizeof(int), MSG_NOSIGNAL);
        strcpy(msg, strerror(errno));
        int info_len = strlen(msg);
        send(task->fd, &info_len, sizeof(int), MSG_NOSIGNAL);
        send(task->fd, msg, info_len, MSG_NOSIGNAL);
        return;
    }

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

int getsCmd(Task* task) {
    // 确认参数数量是否正确
    if (task->args[1] == NULL) {
        int send_stat = 1;
        send(task->fd, &send_stat, sizeof(int), MSG_NOSIGNAL);
        char error_info[] = "no such parameter";
        int info_len = strlen(error_info);
        send(task->fd, &info_len, sizeof(int), MSG_NOSIGNAL);
        send(task->fd, error_info, info_len, MSG_NOSIGNAL);
        return 0;
    } else {
        int send_stat = 0;
        send(task->fd, &send_stat, sizeof(int), MSG_NOSIGNAL);
    }

    // 获取一个mysql连接
    MYSQL* mysql = getDBConnection(task->dbpool);
    // 获取当前路径
    int pwdid = getPwdId(mysql, task->uid);

    // 发送文件
    char** parameter = task->args;
    while (*(++parameter)) {
        static int i = 0;  // 第一个文件
        i++;
        char file_name[512] = {0};
        for (char* p = parameter; *p; p++) {
            int target_pwdid = pwdid;
            for (char* start = p; *p != '\0' && *p != '/'; p++) {
                if (*(p + 1) == '/') {
                    bzero(file_name, sizeof(file_name));
                    strncpy(file_name, start, p - start + 1);
                    // user2 change here, add 4th parameter as NULL
                    target_pwdid =
                        goToRelativeDir(mysql, target_pwdid, file_name, NULL);
                    if (target_pwdid == -1) {
                        // 路径错误
                        //***消息对接***
                        return 0;
                    }
                } else if (*(p + 1) == '\0') {
                    // 此时start是文件名,
                    strcpy(file_name, start);
                    break;
                }
            }
        }
        // 此时file_name即文件名

        char* path_file = NULL;
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
            return 0;
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
            return 1;
        }
    }
    // 所有文件发送完毕
    int send_stat = 1;
    send(task->fd, &send_stat, sizeof(int), MSG_NOSIGNAL);
    char send_info[] = "SUCCESS";
    int info_len = strlen(send_info);
    send(task->fd, &info_len, sizeof(int), MSG_NOSIGNAL);
    send(task->fd, send_info, info_len, MSG_NOSIGNAL);
    return 0;
}

int putsCmd(Task* task) {
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
            return 1;
        }
    }

    return 0;
}

void mkdirCmd(Task* task) {
    if (task->args[1] == NULL) {  // missing operand
        char errmsg[MAXLINE] = "mkdir: missing operand";
        send(task->fd, errmsg, strlen(errmsg), 0);
        log_error("mkdirCmd: missing operand");
        error(0, errno, "%d mkdir:", task->fd);
        return;
    }







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
                return;
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
        return;
    }

    // 成功了给客户端发一个0
    send(task->fd, "0", sizeof("0"), 0);

    return;
}

void loginCheck1(Task* task) {
    log_debug("loginCheck1 start");
    log_info("user to login: [%s]", task->args[1]);

    char* username = task->args[1];

    // 0：成功，1：失败
    int status_code = 0;
    MYSQL* pconn = getDBConnection(task->dbpool);
    int exist = userExist(pconn, username);
    log_info("[%s] exist = [%d]", task->args[1], exist);

    if (exist == 0) {
        releaseDBConnection(task->dbpool, pconn);
        // 用户不存在
        status_code = 1;
        sendn(task->fd, &status_code, sizeof(int));
        return;
    }

    // 用户存在
    sendn(task->fd, &status_code, sizeof(int));

    // 获取 uid
    int uid = getUserIDByUsername(pconn, username);

    // 查询 cryptpasswd
    char* cryptpasswd = getCryptpasswdByUID(pconn, uid);
    releaseDBConnection(task->dbpool, pconn);

    // 提取 salt
    char salt[16] = {0};
    getSaltByCryptPasswd(salt, cryptpasswd);
    free(cryptpasswd);

    // 发送 salt
    int salt_len = strlen(salt);
    sendn(task->fd, &salt_len, sizeof(int));
    sendn(task->fd, salt, salt_len);

    // 更新本地 user_table
    // 如果用户没到 check2，会在 say goodbye 时处理
    task->u_table[task->fd] = uid;

    log_debug("loginCheck1 end");
    return;
}

void loginCheck2(Task* task) {
    log_debug("loginCheck2 start");

    // args[1] = u_cryptpasswd
    char* u_cryptpasswd = task->args[1];
    int uid = task->u_table[task->fd];

    // 查询数据库中的 cryptpasswd
    MYSQL* pconn = getDBConnection(task->dbpool);
    char* cryptpasswd = getCryptpasswdByUID(pconn, uid);
    releaseDBConnection(task->dbpool, pconn);

    int status_code = 0;
    if (strcmp(u_cryptpasswd, cryptpasswd) == 0) {
        // 登录成功
        sendn(task->fd, &status_code, sizeof(int));
        log_info("[uid=%d] login successfully", uid);
    } else {
        // 登录失败，密码错误
        status_code = 1;
        sendn(task->fd, &status_code, sizeof(int));
        log_warn("[%d] login failed", uid);
    }

    log_debug("loginCheck2 end");
    return;
}

void regCheck1(Task* task) {
    log_debug("regCheck1 start");
    log_info("user to register: [%s]", task->args[1]);

    char* username = task->args[1];
    // 查数据库，用户名是否可用
    // 0: 用户名可用, 1: 用户名已存在
    int status_code = 0;
    MYSQL* pconn = getDBConnection(task->dbpool);
    if (userExist(pconn, username)) {
        releaseDBConnection(task->dbpool, pconn);
        status_code = 1;
        sendn(task->fd, &status_code, sizeof(int));
        return;
    }
    releaseDBConnection(task->dbpool, pconn);

    // 可以注册
    sendn(task->fd, &status_code, sizeof(int));
    // 生成 salt
    char* salt = generateSalt();
    // 发送 salt
    int salt_len = strlen(salt);
    sendn(task->fd, &salt_len, sizeof(int));
    sendn(task->fd, salt, salt_len);
    free(salt);

    log_debug("regCheck1 end");

    return;
}

void regCheck2(Task* task) {
    log_debug("regCheck2 start");

    // args[1] = username
    // args[2] = cryptpasswd

    char* username = task->args[1];
    char* cryptpasswd = task->args[2];

    MYSQL* pconn = getDBConnection(task->dbpool);

    // 插入用户记录到 nb_usertable

    long long pwdid = 0;
    int uid = userInsert(pconn, username, cryptpasswd, pwdid);

    // 插入用户目录记录到 nb_vftable
    pwdid = insertRecord(pconn, -1, uid, NULL, "home", "/", 'd', NULL, NULL);
    if (pwdid == -1) {
        log_error("insertRecord failed");
        exit(EXIT_FAILURE);
    }
    char pwdid_str[64] = {0};
    sprintf(pwdid_str, "%lld", pwdid);

    // 更新用户的 pwdid
    int err = userUpdate(pconn, uid, "pwdid", pwdid_str);
    if (err) {
        log_error("userUpdate failed");
        exit(EXIT_FAILURE);
    }


    releaseDBConnection(task->dbpool, pconn);

    // 0: 注册成功
    int status_code = 0;
    sendn(task->fd, &status_code, sizeof(int));
    log_info("[%s] register successfully", username);

    log_debug("regCheck2 end");
    return;
}

void unknownCmd(void) {
    // TODO:

    return;
}

int taskHandler(Task* task) {
    int retval = 0;
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
            retval = getsCmd(task);
            break;
        case CMD_PUTS:
            retval = putsCmd(task);
            break;
        case CMD_LOGIN1:
            loginCheck1(task);
            break;
        case CMD_LOGIN2:
            loginCheck2(task);
            break;
        case CMD_REG1:
            regCheck1(task);
            break;
        case CMD_REG2:
            regCheck2(task);
            break;
        default:
            unknownCmd();
            break;
    }
    return retval;
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

    strcpy(workdir_table[connfd]->name, "");
    strcpy(workdir_table[connfd]->encrypted, "");
}

void workdirFree(WorkDir* workdir) {
    free(workdir->path);
    free(workdir->index);
    free(workdir);
}
