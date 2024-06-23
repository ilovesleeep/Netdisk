#define OPENSSL_SUPPRESS_DEPRECATED

#include "../include/bussiness.h"

#include <openssl/md5.h>
#include <stdlib.h>

#include "../include/dbpool.h"
#include "../include/mysqloperate.h"

#define BUFSIZE 4096
#define MAXLINE 1024
#define BIGFILE_SIZE (100 * 1024 * 1024)
#define MMAPSIZE (1024 * 1024 * 10)
#define HASH_SIZE 32

int sendFile(int sockfd, int fd, off_t f_size) {
    // 接收客户端想从哪里开始发
    int recv_stat = 0;
    recvn(sockfd, &recv_stat, sizeof(int));
    if (recv_stat == 0) {
        close(fd);
        return 0;
    }

    off_t send_bytes = 0;
    recv(sockfd, &send_bytes, sizeof(off_t), MSG_WAITALL);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    if (send_bytes != 0) {
        char md5sum_client[HASH_SIZE + 1];
        recvn(sockfd, md5sum_client, HASH_SIZE);
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

        char md5_hex[HASH_SIZE + 1];
        for (int i = 0; i < 16; i++) {
            sprintf(md5_hex + (i * 2), "%02x", md5sum[i]);
        }

        // 接收用户计算的md5值
        // 比较
        if (memcmp(md5sum_client, md5_hex, HASH_SIZE) != 0) {
            // 不是一个文件,重新来过吧
            int send_stat = 1;
            sendn(sockfd, &send_stat, sizeof(int));
            send_bytes = 0;
        }
    }

#pragma GCC diagnostic pop
    // 此时send_bytes对应正确的开始发送位置
    // 告诉客户端正确发送位置
    sendn(sockfd, &send_bytes, sizeof(off_t));
    //  发送文件内容
    if (f_size >= BIGFILE_SIZE) {
        // 大文件
        while (send_bytes < f_size) {
            off_t length = f_size - send_bytes >= MMAPSIZE
                               ? MMAPSIZE
                               : f_size - send_bytes;

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
        while (send_bytes < f_size) {
            off_t length =
                f_size - send_bytes >= BUFSIZE ? BUFSIZE : f_size - send_bytes;

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

int recvFile(int sockfd, MYSQL* mysql, int u_id) {
    int p_id = getPwdId(mysql, u_id);
    char path[1024] = {0};
    getPwd(mysql, p_id, path, 1024);
    // 接收文件名
    DataBlock block;
    bzero(&block, sizeof(block));
    recvn(sockfd, &block.length, sizeof(int));
    if (recvn(sockfd, block.data, block.length) == -1) {
        return 1;
    }
    // 接收文件的大小
    off_t fsize;
    recvn(sockfd, &fsize, sizeof(fsize));

    // 接收文件哈希值
    char recv_hash[HASH_SIZE + 1] = {0};
    recvn(sockfd, recv_hash, HASH_SIZE);

    // 查看是否有同名文件
    char type = '\0';
    int file_id = goToRelativeDir(mysql, p_id, block.data, &type);
    if ((file_id != 0 && type == 'd') || file_id > 0) {
        // 已存在目录 || 文件已存在
        int send_stat = 1;
        send(sockfd, &send_stat, sizeof(int), MSG_NOSIGNAL);
        char send_info[] = "illegal file name";
        int info_len = strlen(send_info);
        send(sockfd, &info_len, sizeof(int), MSG_NOSIGNAL);
        send(sockfd, send_info, info_len, MSG_NOSIGNAL);
        return 0;
    }
    if (file_id < 0 && type == 'f') {
        // 修改目录项
        file_id = -file_id;
        updateRecord(mysql, file_id, NULL, NULL, (char*)recv_hash, NULL, NULL,
                     NULL, NULL);
        int send_stat = 0;
        send(sockfd, &send_stat, sizeof(int), MSG_NOSIGNAL);
    } else {
        off_t c_size = 0;
        file_id = insertRecord(mysql, p_id, u_id, (char*)recv_hash, block.data,
                               path, 'f', &fsize, &c_size, '0');
        int send_stat = 0;
        send(sockfd, &send_stat, sizeof(int), MSG_NOSIGNAL);
    }

    // 查表查看是否文件存在(f_hash)(是否可以续传)
    off_t f_size = 0, c_size = 0;
    localFile(mysql, (char*)recv_hash, &f_size, &c_size);
    if (c_size == f_size && f_size != 0) {
        // 文件已存在且完整
        printf("*****秒了\n");
        int send_stat = 0;
        send(sockfd, &send_stat, sizeof(int), MSG_NOSIGNAL);

        updateRecord(mysql, file_id, NULL, NULL, (char*)recv_hash, NULL,
                     &f_size, &c_size, "1");
        return 0;
    } else {
        // 文件存在但不完整或文件不存在
        int send_stat = 1;
        if (send(sockfd, &send_stat, sizeof(int), MSG_NOSIGNAL) == -1) {
            return 1;
        }
    }
    // 发送服务器准备从哪里开始接收
    if (send(sockfd, &c_size, sizeof(off_t), MSG_NOSIGNAL) == -1) {
        return 1;
    }
    // 打开文件
    int fd = open((char*)recv_hash, O_RDWR | O_CREAT, 0666);
    if (fd == -1) {
        error(1, errno, "open");
    }

    off_t recv_bytes = c_size;
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
                munmap(addr, length);
                updateRecord(mysql, file_id, NULL, NULL, NULL, NULL, NULL,
                             &recv_bytes, NULL);
                return 1;
            }
            munmap(addr, length);

            recv_bytes += length;

            log_info("downloading %5.2lf%%", 100.0 * recv_bytes / fsize);
            // fflush(stdout);
        }
    } else {
        char buf[BUFSIZE];
        while (recv_bytes < fsize) {
            off_t length =
                (fsize - recv_bytes >= BUFSIZE) ? BUFSIZE : fsize - recv_bytes;
            if (recvn(sockfd, buf, length) == -1) {
                close(fd);
                updateRecord(mysql, file_id, NULL, NULL, NULL, NULL, NULL,
                             &recv_bytes, NULL);
                return 1;
            }
            write(fd, buf, length);

            recv_bytes += length;

            log_info("downloading %5.2lf%%", 100.0 * recv_bytes / fsize);
            // fflush(stdout);
        }
    }
    updateRecord(mysql, file_id, NULL, NULL, NULL, NULL, NULL, &recv_bytes,
                 "1");
    log_info("downloading %5.2lf%%", 100.0);
    close(fd);
    return 0;
}

static int touchClient(Task* task) {
    char data[2] = "1";
    int res_len = sizeof(Command) + 1;
    sendn(task->fd, &res_len, sizeof(int));
    sendn(task->fd, &task->cmd, sizeof(Command));
    sendn(task->fd, data, 1);

    return 0;
}

int cdCmd(Task* task) {
    touchClient(task);
    char** parameter = task->args;
    if (parameter[2] != NULL) {
        // ls不允许多余参数,直接报错
        int send_stat = 1;
        send(task->fd, &send_stat, sizeof(int), MSG_NOSIGNAL);
        char send_info[] = "parameter error";
        int info_len = strlen(send_info);
        send(task->fd, &info_len, sizeof(int), MSG_NOSIGNAL);
        send(task->fd, send_info, info_len, MSG_NOSIGNAL);
        return 0;
    }
    MYSQL* mysql = getDBConnection(task->dbpool);
    int pwdid = getPwdId(mysql, task->uid);
    if (parameter[1] == NULL) {
        // 回家
        pwdid = goToRelativeDir(mysql, pwdid, "~", NULL);
        if (pwdid == 0) {
            int send_stat = 1;
            send(task->fd, &send_stat, sizeof(int), MSG_NOSIGNAL);
            char send_info[] = "you already at home";
            int info_len = strlen(send_info);
            send(task->fd, &info_len, sizeof(int), MSG_NOSIGNAL);
            send(task->fd, send_info, info_len, MSG_NOSIGNAL);
            releaseDBConnection(task->dbpool, mysql);
            return 0;
        }

        char t[10] = {0};
        sprintf(t, "%d", pwdid);
        userUpdate(mysql, task->uid, "pwdid", t);

        int send_stat = 0;
        send(task->fd, &send_stat, sizeof(int), MSG_NOSIGNAL);
        char pwd[10] = {0};
        getPwd(mysql, pwdid, pwd, sizeof(pwd));
        int pwd_len = strlen(pwd);
        send(task->fd, &pwd_len, sizeof(int), MSG_NOSIGNAL);
        send(task->fd, pwd, pwd_len, MSG_NOSIGNAL);

        releaseDBConnection(task->dbpool, mysql);
        return 0;
    }
    for (char* p = parameter[1]; *p != '\0'; p++) {
        for (char* start = p; *p != '\0' && *p != '/'; p++) {
            if (*(p + 1) == '/') {
                char type = '\0';
                char name[256] = {0};
                strncpy(name, start, p - start + 1);
                pwdid = goToRelativeDir(mysql, pwdid, name, &type);
                if (pwdid <= 0 || type == 'f') {
                    int send_stat = 1;
                    send(task->fd, &send_stat, sizeof(int), MSG_NOSIGNAL);
                    char send_info[] = "path error";
                    int info_len = strlen(send_info);
                    send(task->fd, &info_len, sizeof(int), MSG_NOSIGNAL);
                    send(task->fd, send_info, info_len, MSG_NOSIGNAL);
                    releaseDBConnection(task->dbpool, mysql);
                    return 0;
                }
            }
            if (*(p + 1) == '\0') {
                char type = '\0';
                char name[256] = {0};
                strncpy(name, start, p - start + 1);
                pwdid = goToRelativeDir(mysql, pwdid, name, &type);
                if (pwdid <= 0 || type == 'f') {
                    int send_stat = 1;
                    send(task->fd, &send_stat, sizeof(int), MSG_NOSIGNAL);
                    char send_info[] = "path error";
                    int info_len = strlen(send_info);
                    send(task->fd, &info_len, sizeof(int), MSG_NOSIGNAL);
                    send(task->fd, send_info, info_len, MSG_NOSIGNAL);
                    releaseDBConnection(task->dbpool, mysql);
                    return 0;
                } else {
                    char t[10] = {0};
                    sprintf(t, "%d", pwdid);
                    if (userUpdate(mysql, task->uid, "pwdid", t) != 0) {
                        int send_stat = 1;
                        send(task->fd, &send_stat, sizeof(int), MSG_NOSIGNAL);
                        char send_info[] = "path error";
                        int info_len = strlen(send_info);
                        send(task->fd, &info_len, sizeof(int), MSG_NOSIGNAL);
                        send(task->fd, send_info, info_len, MSG_NOSIGNAL);
                    } else {
                        int send_stat = 0;
                        send(task->fd, &send_stat, sizeof(int), MSG_NOSIGNAL);

                        char pwd[1024] = {0};
                        getPwd(mysql, pwdid, pwd, sizeof(pwd));
                        int pwd_len = strlen(pwd);
                        send(task->fd, &pwd_len, sizeof(int), MSG_NOSIGNAL);
                        send(task->fd, pwd, pwd_len, MSG_NOSIGNAL);
                    }
                    releaseDBConnection(task->dbpool, mysql);
                    return 0;
                }
            }
        }
    }

    return 0;
}

int lsCmd(Task* task) {
    // 告知客户端，接收当前命令的响应
    touchClient(task);
    char** parameter = task->args;
    if (parameter[2] != NULL ||
        (parameter[1] && strcmp(parameter[1], "-l") != 0)) {
        int send_stat = 1;
        send(task->fd, &send_stat, sizeof(int), MSG_NOSIGNAL);
        char error_info[] = "no such parameter";
        int info_len = strlen(error_info);
        send(task->fd, &info_len, sizeof(int), MSG_NOSIGNAL);
        send(task->fd, error_info, info_len, MSG_NOSIGNAL);
        return 0;
    }
    int send_stat = 0;
    send(task->fd, &send_stat, sizeof(int), MSG_NOSIGNAL);

    MYSQL* mysql = getDBConnection(task->dbpool);
    int pwdid = getPwdId(mysql, task->uid);
    if (parameter[1] == NULL) {
        int send_stat = 0;  // 无参数接收
        send(task->fd, &send_stat, sizeof(int), MSG_NOSIGNAL);
        char sql[100] = {0};
        sprintf(
            sql,
            "SELECT type, name FROM nb_vftable WHERE p_id = %d AND exist = '1'",
            pwdid);
        mysql_query(mysql, sql);
        MYSQL_RES* res = mysql_store_result(mysql);
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res))) {
            send(task->fd, row[0], 1, MSG_NOSIGNAL);
            int send_len = strlen(row[1]);
            send(task->fd, &send_len, sizeof(int), MSG_NOSIGNAL);
            send(task->fd, row[1], send_len, MSG_NOSIGNAL);
        }
        send(task->fd, "\0", 1, MSG_NOSIGNAL);
        mysql_free_result(res);
    } else {
        int send_stat = 1;  //-l
        send(task->fd, &send_stat, sizeof(int), MSG_NOSIGNAL);
        char sql[100] = {0};
        sprintf(sql,
                "SELECT type, name, f_size FROM nb_vftable WHERE p_id = %d AND "
                "exist = '1'",
                pwdid);
        mysql_query(mysql, sql);
        MYSQL_RES* res = mysql_store_result(mysql);
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res))) {
            send(task->fd, row[0], 1, MSG_NOSIGNAL);
            int send_len = strlen(row[1]);
            send(task->fd, &send_len, sizeof(int), MSG_NOSIGNAL);
            send(task->fd, row[1], send_len, MSG_NOSIGNAL);
            if (row[0][0] == 'f') {
                off_t f_size = 0;
                f_size = atoi(row[2]);
                send(task->fd, &f_size, sizeof(off_t), MSG_NOSIGNAL);
            }
        }
        send(task->fd, "\0", 1, MSG_NOSIGNAL);
        mysql_free_result(res);
    }
    releaseDBConnection(task->dbpool, mysql);
    return 0;
}

int delFileOrDir(MYSQL* mysql, int pwdid) {
    // 通过set exist='0'删除文件或目录
    char sql[60] = {0};
    sprintf(sql, "update nb_vftable set exist='0' where id=%d", pwdid);
    int res = mysql_query(mysql, sql);
    if (res != 0) {
        log_error("set exist='0' failed.");
        error(1, 0, "[ERROR] set exist='0' failed\n");
    }

    return 0;
}

// 传入uid,pwdid,name,type,判断传入的是文件还是目录,文件直接删除，目录先递归删除子目录，再进行删除
int rmCmdHelper(MYSQL* mysql, int uid, int pwdid, char type) {
    // 获取类型
    if (type == 'f') {
        // 类型为file,直接删除
        int res = delFileOrDir(mysql, pwdid);
        if (res != 0) {
            log_error("del failed.");
            error(1, 0, "[ERROR] del failed\n");
        }
        return 0;
    } else if (type == 'd') {
        // 类型是directory,看看是否存在子目录

        char sql[60] = {0};
        sprintf(sql, "select id from nb_vftable where p_id=%d", pwdid);
        mysql_query(mysql, sql);
        MYSQL_RES* res = mysql_store_result(mysql);
        MYSQL_ROW row;

        while ((row = mysql_fetch_row(res))) {
            int childpwdid = atoi(row[0]);  // 获取目录id

            char type = getTypeById(mysql, childpwdid);
            int res = rmCmdHelper(mysql, uid, childpwdid, type);
            if (res != 0) {
                log_error("del failed.");
                error(1, 0, "[ERROR] del failed\n");
            }
        }
    }

    int res = delFileOrDir(mysql, pwdid);
    if (res != 0) {
        log_error("del failed.");
        error(1, 0, "[ERROR] del failed\n");
    }

    return 0;
}

void rmCmd(Task* task) {
    // 告知客户端，接受当前命令的响应
    touchClient(task);
    char** parameter = task->args;
    // NOTE:
    // 删除文件及目录。
    // 如果删除的是文件，则直接将它的exist设为“0”。
    // 如果删除的是目录，则需要查看它是否存在子目录。需要遍历父目录id，找到和本目录id相等的行。
    // 并且递归查询下去，直到找到一个目录项不是当前目录id为止，并将它们的exist类型都设置为“0”。

    // 参数校验,只接受一个参数。 "usage: rm file/dict."
    if (task->args[1] == NULL) {
        int send_stat = 1;
        send(task->fd, &send_stat, sizeof(int), MSG_NOSIGNAL);
        char error_info[] = "rm: 缺少操作对象";
        int info_len = strlen(error_info);
        send(task->fd, &info_len, sizeof(int), MSG_NOSIGNAL);
        send(task->fd, error_info, info_len, MSG_NOSIGNAL);

        return;
    } else {
        int send_stat = 0;
        send(task->fd, &send_stat, sizeof(int), MSG_NOSIGNAL);
    }

    MYSQL* mysql = getDBConnection(task->dbpool);
    int pwdid = getPwdId(mysql, task->uid);
    char pwd[1024] = {0};
    getPwd(mysql, pwdid, pwd, 1024);
    char type = getTypeById(mysql, pwdid);
    for (int i = 1; parameter[i]; i++) {
        int curr_pwdid = pwdid;
        for (char* p = parameter[i]; *p != '\0'; p++) {
            for (char* start = p; *p != '\0' && *p != '/'; p++) {
                if (*(p + 1) == '/') {
                    char type = '\0';
                    char name[256] = {0};
                    strncpy(name, start, p - start + 1);
                    curr_pwdid =
                        goToRelativeDir(mysql, curr_pwdid, name, &type);
                    if (curr_pwdid <= 0 || type == 'f') {
                        int send_stat = 1;
                        send(task->fd, &send_stat, sizeof(int), MSG_NOSIGNAL);
                        char send_info[128] = {0};
                        sprintf(send_info, "NO.%d file %s", i, "path error");
                        int info_len = strlen(send_info);
                        send(task->fd, &info_len, sizeof(int), MSG_NOSIGNAL);
                        send(task->fd, send_info, info_len, MSG_NOSIGNAL);
                        releaseDBConnection(task->dbpool, mysql);
                        return;
                    }
                }
                if (*(p + 1) == '\0') {
                    char type = '\0';
                    char name[256] = {0};
                    strncpy(name, start, p - start + 1);
                    curr_pwdid =
                        goToRelativeDir(mysql, curr_pwdid, name, &type);
                    if (curr_pwdid <= 0) {
                        int send_stat = 1;
                        send(task->fd, &send_stat, sizeof(int), MSG_NOSIGNAL);
                        char send_info[128] = {0};
                        sprintf(send_info, "NO.%d file %s", i, "path error");
                        int info_len = strlen(send_info);
                        send(task->fd, &info_len, sizeof(int), MSG_NOSIGNAL);
                        send(task->fd, send_info, info_len, MSG_NOSIGNAL);
                        releaseDBConnection(task->dbpool, mysql);
                        return;
                    }
                }
            }
        }
        type = getTypeById(mysql, curr_pwdid);
        int res = rmCmdHelper(mysql, task->uid, curr_pwdid, type);
        if (res != 0) {
            // 错误，未能成功删除
            int send_stat = 1;
            send(task->fd, &send_stat, sizeof(int), MSG_NOSIGNAL);
            char error_info[128] = "rm failed";
            int info_len = strlen(error_info);
            send(task->fd, &info_len, sizeof(int), MSG_NOSIGNAL);
            send(task->fd, error_info, info_len, MSG_NOSIGNAL);
            log_error("rm %s failed.", pwd);
            releaseDBConnection(task->dbpool, mysql);
            return;
        }
    }
    // 成功删除
    int send_stat = 0;
    send(task->fd, &send_stat, sizeof(int), MSG_NOSIGNAL);
    send(task->fd, &send_stat, sizeof(int), MSG_NOSIGNAL);

    releaseDBConnection(task->dbpool, mysql);
    return;
}

void pwdCmd(Task* task) {
    // 告知客户端，接受当前命令的响应
    touchClient(task);

    char path[MAXLINE] = {0};
    int path_size = MAXLINE;

    MYSQL* mysql = getDBConnection(task->dbpool);

    int pwdid = getPwdId(mysql, task->uid);
    getPwd(mysql, pwdid, path, path_size);

    releaseDBConnection(task->dbpool, mysql);

    sendn(task->fd, path, path_size);

    log_debug("pwd: %s", path);

    return;
}

int getsCmd(Task* task) {
    // 确认参数数量是否正确
    if (task->args[1] == NULL) {
        // 参数错误
        int send_stat = 1;
        send(task->fd, &send_stat, sizeof(int), MSG_NOSIGNAL);
        char error_info[] = "no such parameter";
        int info_len = strlen(error_info);
        send(task->fd, &info_len, sizeof(int), MSG_NOSIGNAL);
        send(task->fd, error_info, info_len, MSG_NOSIGNAL);
        return 0;
    } else {
        // 参数正确
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
        printf("******************%s\n", parameter[i]);
    }
    for (int i = 1; parameter[i]; i++) {
        char file_name[512] = {0};
        int target_pwdid = pwdid;
        for (char* p = parameter[i]; *p != '\0'; p++) {
            for (char* start = p; *p != '\0' && *p != '/'; p++) {
                if (*(p + 1) == '/') {
                    bzero(file_name, sizeof(file_name));
                    strncpy(file_name, start, p - start + 1);
                    char type = '\0';
                    target_pwdid =
                        goToRelativeDir(mysql, target_pwdid, file_name, &type);
                    // target_pwdid =
                    //     goToRelativeDir(mysql, target_pwdid, file_name);
                    if (target_pwdid <= 0 || type == 'f') {
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
                    strcpy(file_name, start);
                    char type;
                    target_pwdid =
                        goToRelativeDir(mysql, target_pwdid, file_name, &type);
                    if (type == 'd') {
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
                    if (target_pwdid == 0) {
                        // 目录不存在
                        //***消息对接***
                        int send_stat = 1;
                        sendn(task->fd, &send_stat, sizeof(int));
                        char send_info[] = "file don't exist";
                        int info_len = strlen(send_info);
                        sendn(task->fd, &info_len, sizeof(int));
                        sendn(task->fd, send_info, info_len);
                        // 资源释放
                        releaseDBConnection(task->dbpool, mysql);
                        return 0;
                    }
                    break;
                }
            }
        }
        // 此时file_name即文件名,target_pwdid为待发送的id
        // 检查文件是否完整(不用检查了,我只会将完整的文件目录项设为1)
        off_t f_size;
        off_t c_size;
        char f_hash[HASH_SIZE + 1] = {0};
        getFileInfo(mysql, target_pwdid, f_hash, &f_size, &c_size);
        int fd = open(f_hash, O_RDWR);
        // 存在
        int send_stat = 0;
        send(task->fd, &send_stat, sizeof(int), MSG_NOSIGNAL);
        // 先发文件名
        DataBlock block;
        strcpy(block.data, file_name);
        block.length = strlen(file_name);
        sendn(task->fd, &block, sizeof(int) + block.length);
        // 发送哈希值和文件大小
        sendn(task->fd, f_hash, HASH_SIZE);
        sendn(task->fd, &f_size, sizeof(off_t));
        if (sendFile(task->fd, fd, f_size) == 1) {
            // sendfile中close了fd,若返回值为1证明连接中断,则不进行剩余发送任务
            releaseDBConnection(task->dbpool, mysql);
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
    releaseDBConnection(task->dbpool, mysql);
    return 0;
}

int putsCmd(Task* task) {
    MYSQL* mysql = getDBConnection(task->dbpool);
    // 默认存放在当前目录

    // 告诉客户端已就绪
    int recv_stat = 0;
    send(task->fd, &recv_stat, sizeof(int), MSG_NOSIGNAL);

    int retval = 0;
    for (int i = 0; true; i++) {
        // 先接收是否要发送
        int recv_stat = 0;
        if (recv(task->fd, &recv_stat, sizeof(int), MSG_WAITALL) == -1) {
            retval = 1;
            break;
        }

        // 不发送
        if (recv_stat != 0) {
            break;
        }

        if (recvFile(task->fd, mysql, task->uid) == 1) {
            retval = 1;
            break;
        }
    }
    releaseDBConnection(task->dbpool, mysql);
    return retval;
}

void mkdirCmd(Task* task) {
    // 告知客户端，接受当前命令的响应
    touchClient(task);

    int res_len = 0;
    if (task->args[1] == NULL || task->args[2] != NULL) {  // missing operand
        char errmsg[MAXLINE] = "mkdir: missing operand";
        res_len = strlen(errmsg);
        send(task->fd, &res_len, sizeof(int), MSG_NOSIGNAL);

        send(task->fd, errmsg, strlen(errmsg), MSG_NOSIGNAL);
        log_error("mkdirCmd: missing operand");
        error(0, errno, "%d mkdir:", task->fd);
        return;
    }

    MYSQL* mysql = getDBConnection(task->dbpool);
    int pwdid = getPwdId(mysql, task->uid);  // pwdid
    char* mkdir_name = task->args[1];        // name

    char pwd[1024] = {0};  // 当前目录的绝对路径
    if (getPwd(mysql, pwdid, pwd, 1024) == -1) {
        releaseDBConnection(task->dbpool, mysql);
        char* errmsg = "mkdir: getPwd failed";
        res_len = strlen(errmsg);
        send(task->fd, &res_len, sizeof(int), MSG_NOSIGNAL);
        send(task->fd, errmsg, res_len, MSG_NOSIGNAL);
        log_error("%s", errmsg);
        return;
    }

    char absolute_path[1024] = {0};  // 绝对路径
    sprintf(absolute_path, "%s/%s", pwd, mkdir_name);
    char type[] = "d";  // 类型

    int ret = goToRelativeDir(mysql, pwdid, mkdir_name, type);
    if (ret == 0) {
        // 不存在, insert
        int insert_ret = insertRecord(mysql, pwdid, task->uid, NULL, mkdir_name,
                                      absolute_path, 'd', NULL, NULL, '1');
        releaseDBConnection(task->dbpool, mysql);
        if (insert_ret == -1) {
            char* errmsg = "mkdir: insert failed";
            res_len = strlen(errmsg);
            send(task->fd, &res_len, sizeof(int), MSG_NOSIGNAL);
            send(task->fd, errmsg, res_len, MSG_NOSIGNAL);
            log_error("%s", errmsg);
        } else {
            res_len = 0;
            send(task->fd, &res_len, sizeof(int), MSG_NOSIGNAL);
            log_info("mkdir: insert succeed");
        }

    } else if (ret > 0) {
        releaseDBConnection(task->dbpool, mysql);
        // 存在，不能创建
        char* errmsg = "mkdir: dir already exists";
        res_len = strlen(errmsg);
        send(task->fd, &res_len, sizeof(int), MSG_NOSIGNAL);
        send(task->fd, errmsg, res_len, MSG_NOSIGNAL);
        log_error("%s", errmsg);
    } else {
        // ret < 0
        // 目录项存在，exist = 0
        if (type[0] == 'd') {  // 是目录，exist改为1
            const char* query_str =
                "update nb_vftable set exist = 1 where id = ";
            char str[16] = {0};
            sprintf(str, "%d", -ret);
            char query[1024] = {0};
            sprintf(query, "%s%s", query_str, str);

            int update_ret = mysql_query(mysql, query);
            if (update_ret != 0) {
                char* errmsg = "mkdir: mysql_query update failed";
                res_len = strlen(errmsg);
                send(task->fd, &res_len, sizeof(int), MSG_NOSIGNAL);
                send(task->fd, errmsg, res_len, MSG_NOSIGNAL);

                error(0, errno, "mkdir: mysql_query");
                log_error("mkdir: mysql_query: %s", strerror(errno));
            } else {
                res_len = 0;
                send(task->fd, &res_len, sizeof(int), MSG_NOSIGNAL);
                log_info("mkdir: mysql_query succeed");
            }
        } else {  // 是文件，不能创建
            char* errmsg = "mkdir: dirname wrong";
            res_len = strlen(errmsg);
            send(task->fd, &res_len, sizeof(int), MSG_NOSIGNAL);
            send(task->fd, errmsg, res_len, MSG_NOSIGNAL);
            log_error("%s", errmsg);
        }
        releaseDBConnection(task->dbpool, mysql);
    }
    return;
}

void unknownCmd(void) { return; }

int taskHandler(Task* task) {
    int retval = 0;
    switch (task->cmd) {
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
        case CMD_GETS2:
            retval = getsCmd(task);
            break;
        case CMD_PUTS2:
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
