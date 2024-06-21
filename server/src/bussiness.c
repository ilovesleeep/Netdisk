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
    // 告知客户端，接受当前命令的响应
    char data[2] = "1";
    int res_len = sizeof(Command) + 1;
    sendn(task->fd, &res_len, sizeof(int));
    sendn(task->fd, &task->cmd, sizeof(Command));
    sendn(task->fd, data, 1);

    return 0;
}

void lsCmd(Task* task) {
    // 告知客户端，接受当前命令的响应
    char data[2] = "1";
    int res_len = sizeof(Command) + 1;
    sendn(task->fd, &res_len, sizeof(int));
    sendn(task->fd, &task->cmd, sizeof(Command));
    sendn(task->fd, data, 1);

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

int deleteDir(int id, char* type) { return 0; }

void rmCmd(Task* task) {
    // 告知客户端，接受当前命令的响应
    char data[2] = "1";
    int res_len = sizeof(Command) + 1;
    sendn(task->fd, &res_len, sizeof(int));
    sendn(task->fd, &task->cmd, sizeof(Command));
    sendn(task->fd, data, 1);

    // TODO:
    // 删除文件及目录。
    // 如果删除的是文件，则直接将它的exist设为“0”。
    // 如果删除的是目录，则需要查看它是否存在子目录。需要遍历父目录id，找到和本目录id相等的行。
    // 并且递归查询下去，直到找到一个目录项不是当前目录id为止，并将它们的exist类型都设置为“0”。
    MYSQL* mysql;
    int pwdid;
    pwdid = getPwdId(mysql, task->uid);
    // char pwd = getPwd(mysql, pwdid);

    char type;
    // type = getTypeById(mysql, pwdid);
}

void pwdCmd(Task* task) {
    // 告知客户端，接受当前命令的响应
    char data[2] = "1";
    int res_len = sizeof(Command) + 1;
    sendn(task->fd, &res_len, sizeof(int));
    sendn(task->fd, &task->cmd, sizeof(Command));
    sendn(task->fd, data, 1);

    MYSQL* mysql = getDBConnection(task->dbpool);
    int pwdid = getPwdId(mysql, task->uid);

    char path[MAXLINE] = {0};
    int path_size = MAXLINE;

    getPwd(mysql, pwdid, path, path_size);

    sendn(task->fd, path, path_size);

    log_debug("pwd: %s", path);

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
    for (int i = 1; parameter[i]; i++) {
        char file_name[512] = {0};
        int target_pwdid = pwdid;
        for (char* p = parameter[i]; *p != '\0'; p++) {
            for (char* start = p; *p != '\0' && *p != '/'; p++) {
                if (*(p + 1) == '/') {
                    bzero(file_name, sizeof(file_name));
                    strncpy(file_name, start, p - start + 1);
                    // target_pwdid =
                    //     goToRelativeDir(mysql, target_pwdid, file_name);
                    if (target_pwdid == -1) {
                        // 路径错误
                        //***消息对接***
                        int send_stat = 1;
                        sendn(task->fd, &send_stat, sizeof(int));
                        char send_info[] = "path not exist";
                        int info_len = strlen(send_info);
                        sendn(task->fd, &info_len, sizeof(int));
                        sendn(task->fd, send_info, info_len);
                        // 资源释放
                        releaseDBConnection(task->dbpool, mysql);
                        return 0;
                    }
                } else if (*(p + 1) == '\0') {
                    // 此时start是文件名,target_pwdid为待插入项的父目录id
                    char type;
                    target_pwdid =
                        goToRelativeDir(mysql, target_pwdid, file_name, &type);
                    if (type == 'D') {
                        // 最后文件名对应的是一个路径,本网盘暂不支持传输文件夹功能
                        //***消息对接***
                        int send_stat = 1;
                        sendn(task->fd, &send_stat, sizeof(int));
                        char send_info[] =
                            "Don't support transmiting directory";
                        int info_len = strlen(send_info);
                        sendn(task->fd, &info_len, sizeof(int));
                        sendn(task->fd, send_info, info_len);
                        // 资源释放
                        releaseDBConnection(task->dbpool, mysql);
                        return 0;
                    }
                    strcpy(file_name, start);
                    break;
                }
            }
        }
        // 此时file_name即文件名,target_pwdid为待插入项的id
        // 检查文件是否完整(不用检查了,我只会将完整的文件目录项设为1)

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
    // 告知客户端，接受当前命令的响应
    char data[2] = "1";
    int res_len = sizeof(Command) + 1;
    sendn(task->fd, &res_len, sizeof(int));
    sendn(task->fd, &task->cmd, sizeof(Command));
    sendn(task->fd, data, 1);

    if (task->args[0] == NULL || task->args[1] != NULL) {  // missing operand
        char errmsg[MAXLINE] = "mkdir: missing operand";
        int res_len = strlen(errmsg);

        send(task->fd, errmsg, strlen(errmsg), 0);
        log_error("mkdirCmd: missing operand");
        error(0, errno, "%d mkdir:", task->fd);
        return;
    }
    int pwdid = 0;
    char* mkdir_path = task->args[0];

    // MAX_PATH_LENGTH = 2048
    char absolute_path[2048] = {0};
    if (mkdir_path[0] == '/') {
        // 返回错误信息
        // TODO:

    } else if (mkdir_path[0] == '~') {
        // mkdir 绝对路径
        strcpy(absolute_path, mkdir_path);

        MYSQL* mysql = getDBConnection(task->dbpool);
        pwdid = goToRelativeDir(mysql, getPwdId(mysql, task->uid), "~", NULL);

    } else {
        // mkdir 相对路径
    }

    // char filename[512] = {0};
    //     for (char* p = absolute_path; *p != '\0'; ++p) {
    //         for (char* start = p; *p != '\0' && *p != '/'; ++p) {
    //             if (*(p + 1) == '/') {
    //                 bzero(filename, sizeof(filename));
    //                 strncpy(filename, start, p - start - 1);

    //             }
    //         }
    //     }

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

    long long pwdid = 0;
    int uid = userInsert(pconn, username, cryptpasswd, pwdid);

    // 插入用户目录记录到 nb_vftable
    pwdid =
        insertRecord(pconn, -1, uid, NULL, "home", "/", 'd', NULL, NULL, '1');
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
            // rmCmd(task);
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
