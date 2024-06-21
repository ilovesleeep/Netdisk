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

int sendFile(int sockfd, int fd, off_t f_size) {

    // 接收客户端想从哪里开始发
    int recv_stat = 0;
    recvn(sockfd, &recv_stat, sizeof(int));
    if(recv_stat == 0){
        close(fd);
        return 0;
    }

    off_t send_bytes = 0;
    recv(sockfd, &send_bytes, sizeof(off_t), MSG_WAITALL);
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"

    unsigned char md5sum_client[16];
    recvn(sockfd, md5sum_client, sizeof(md5sum_client));
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

    // 接收用户计算的md5值
    // 比较
    if (memcmp(md5sum_client, md5sum, sizeof(md5sum)) != 0) {
        // 不是一个文件,重新来过吧
        int send_stat = 1;
        sendn(sockfd, &send_stat, sizeof(int));
        send_bytes = 0;
    }

    #pragma GCC diagnostic pop
    // 此时send_bytes对应正确的开始发送位置
    // 告诉客户端正确发送位置
    sendn(sockfd, &send_bytes, sizeof(off_t));
    //  发送文件内容
    if (f_size >= BIGFILE_SIZE) {
        // 大文件
        while (send_bytes < f_size) {
            off_t length =
                f_size - send_bytes >= MMAPSIZE ? MMAPSIZE : f_size - send_bytes;

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
    getPwd(mysql, p_id, path);
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
    unsigned char recv_hash[17] = {0};
    recvn(sockfd, recv_hash, sizeof(recv_hash));

    //查看是否有同名文件
    char type = '\0';
    int file_id = goToRelativeDir(mysql, p_id, block.data, &type);
    if(file_id != 0 && type == 'd' || file_id > 0){
        //已存在目录 || 文件已存在
        int send_stat = 1;
        send(sockfd, &send_stat, sizeof(int), MSG_NOSIGNAL);
        char send_info[] = "illegal file name";
        int info_len = strlen(send_info);
        send(sockfd, &info_len, sizeof(int), MSG_NOSIGNAL);
        send(sockfd, send_info, info_len, MSG_NOSIGNAL);
        return 0;
    }
    if(file_id < 0 && type == 'f'){
        //修改目录项
        file_id = -file_id;
        updateRecord(mysql, file_id, NULL, NULL, recv_hash, NULL, NULL, NULL, NULL);
        int send_stat = 1;
        send(sockfd, &send_stat, sizeof(int), MSG_NOSIGNAL);
    }
    else{
        off_t c_size = 0;
        file_id = insertRecord(mysql, p_id, u_id, recv_hash, block.data, path, 'f', &fsize, &c_size, '0');
        int send_stat = 1;
        send(sockfd, &send_stat, sizeof(int), MSG_NOSIGNAL);
    }



    // 查表查看是否文件存在(f_hash)(是否可以续传)
    off_t f_size, c_size;
    localFile(mysql, recv_hash, &f_size, &c_size);
    if(c_size == f_size && f_size != 0){
        //文件已存在且完整
        int send_stat = 0;
        send(sockfd, &send_stat, sizeof(int), MSG_NOSIGNAL);
        
        updateRecord(mysql, file_id, NULL, NULL, recv_hash, NULL, &f_size, &c_size, "1");
        return 0;
    }
    else{
        //文件存在但不完整或文件不存在
        int send_stat = 1;
        if(send(sockfd, &send_stat, sizeof(int), MSG_NOSIGNAL) == -1){
            return 1;
        }
    }
    //发送服务器准备从哪里开始接收
    if(send(sockfd, &c_size, sizeof(off_t), MSG_NOSIGNAL) == -1){
        return 1;
    }
    // 打开文件
    int fd = open(recv_hash, O_RDWR | O_CREAT, 0666);
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
                updateRecord(mysql, file_id, NULL, NULL, NULL, NULL, NULL, &recv_bytes, NULL);
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
                updateRecord(mysql, file_id, NULL, NULL, NULL, NULL, NULL, &recv_bytes, NULL);
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
    // char data[2] = "1";
    // int res_len = sizeof(Command) + 1;
    // sendn(task->fd, &res_len, sizeof(int));
    // sendn(task->fd, &task->cmd, sizeof(Command));
    // sendn(task->fd, data, 1);


    // bufsize = 4096
    char result[BUFSIZE] = {0};

    // 校验参数,发送校验结果，若为错误则继续发送错误信息
    if (task->args[0] != NULL) {
        // int sendstat = 1;
        // send(task->fd, &sendstat, sizeof(int), MSG_NOSIGNAL);

        char* error_info = "parameter number error";
        int info_len = strlen(error_info);
        send(task->fd, &info_len, sizeof(int), MSG_NOSIGNAL);
        send(task->fd, error_info, info_len, MSG_NOSIGNAL);
        
        log_error("lsCmd: parameter number error");
    } else {
        // int sendstat = 0;
        // send(task->fd, &sendstat, sizeof(int), MSG_NOSIGNAL);
        // 获取当前路径
        MYSQL* mysql = getDBConnection(task->dbpool); 
        // int pwdid = getPwdId(mysql, task->uid);
        int pwdid = 1;
        char** family = findchild(mysql, pwdid);
        releaseDBConnection(task->dbpool, mysql);

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
    }


    return;
}

int delFileOrDir(int pwdid) {
    MYSQL* mysql;
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

// 传入uid,pwdid,name,type,判断传入的是文件还是目录
int rmCmdHelper(int uid, int pwdid, char* name, char type) {
    MYSQL* mysql;
    // 获取类型
    if (strcmp(type, 'f') == 0) {
        // 类型为file,直接删除
        int res = delFileOrDir(pwdid);
        if (res != 0) {
            log_error("del failed.");
            error(1, 0, "[ERROR] del failed\n");
        }
        return 0;
    } else if (strcmp(type, 'd') == 0) {
        // 类型是directory,看看是否存在子目录
        char** child = findchild(mysql, pwdid);

        while (*child != NULL) {
            int childpwdid = getPwdId(mysql, uid);
            char type = getTypeById(mysql, childpwdid);
            int res = rmCmdHelper(uid, childpwdid, *child, type);
            if (res != 0) {
                log_error("del failed.");
                error(1, 0, "[ERROR] del failed\n");
            }
            child++;
        }
    }

    int res = delFileOrDir(pwdid);
    if (res != 0) {
        log_error("del failed.");
        error(1, 0, "[ERROR] del failed\n");
    }

    return 0;
}

void rmCmd(Task* task) {
    // TODO:
    // 删除文件及目录。
    // 如果删除的是文件，则直接将它的exist设为“0”。
    // 如果删除的是目录，则需要查看它是否存在子目录。需要遍历父目录id，找到和本目录id相等的行。
    // 并且递归查询下去，直到找到一个目录项不是当前目录id为止，并将它们的exist类型都设置为“0”。

    // 告知客户端接收当前命令的响应
    // char data[2] = "1";
    // int res_len = sizeof(Command) + 1;
    // sendn(task->fd, &res_len, sizeof(int));
    // sendn(task->fd, &task->cmd, sizeof(Command));
    // sendn(task->fd, data, 1);

    // 参数校验,只接受一个参数。 "usage: rm file/dict."
    if (task->args[1] == NULL) {
        int send_stat = 1;
        send(task->fd, &send_stat, sizeof(int), MSG_NOSIGNAL);
        char error_info[] = "rm: 缺少操作对象";
        int info_len = strlen(error_info);
        send(task->fd, &info_len, sizeof(int), MSG_NOSIGNAL);
        send(task->fd, error_info, info_len, MSG_NOSIGNAL);

        return;
    } else if (task->args[2] != NULL) {
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

    MYSQL* mysql;
    int pwdid = getPwdId(mysql, task->uid);
    char *pwd;
    getPwd(mysql, pwdid,*pwd,1024);
    char type = getTypeById(mysql, pwdid);

    int res = rmCmdHelper(task->uid, pwdid, pwd, type);
    if (res != 0) {
        // 错误，未能成功删除
        int send_stat = 1;
        send(task->fd,&send_stat,sizeof(int),MSG_NOSIGNAL);
        char error_info[] = "rm failed";
        int info_len = strlen(error_info);
        send(task->fd,&info_len,sizeof(int),MSG_NOSIGNAL);
        send(task->fd,error_info,info_len,MSG_NOSIGNAL);
        log_error("rm %s failed.",pwd);
        error(1, 0, "[ERROR] rm failed\n");
    }else{
        // 成功删除
        int send_stat = 0;
        send(task->fd,&send_stat,sizeof(int),MSG_NOSIGNAL);
    }

    return;
}

void pwdCmd(Task* task) {
    
    //char data[2]="1";
    //int res_len =sizeof(Command) + 1;
    //sendn(task->fd,&res_len, sizeof(int));
    //sendn(task->fd,&task->cmd,sizeof(Command));
    //sendn(task->fd, data, 1);

    MYSQL* mysql = getDBConnection(task->dbpool);
    int pwdid = getPwdId(mysql, task->uid);
    
    char path[MAXLINE] = {0};
    int path_size = MAXLINE;

    getPwd(mysql, pwdid, path, path_size);

    sendn(task->fd, path, path_size);
    
    log_debug("pwd: %s",path);

    return;
}

int getsCmd(Task* task) {
    // 确认参数数量是否正确
    if (task->args[1] == NULL) {
        //参数错误
        int send_stat = 1;
        send(task->fd, &send_stat, sizeof(int), MSG_NOSIGNAL);
        char error_info[] = "no such parameter";
        int info_len = strlen(error_info);
        send(task->fd, &info_len, sizeof(int), MSG_NOSIGNAL);
        send(task->fd, error_info, info_len, MSG_NOSIGNAL);
        return 0;
    } else {
        //参数正确
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
                    if (type == 'd') {
                        // 最后文件名对应的是一个路径,本网盘暂不支持传输文件夹功能
                        //***消息对接***
                        int send_stat = 1;
                        sendn(task->fd, &send_stat, sizeof(int));
                        char send_info[] = "Don't support transmiting directory";
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
        // 此时file_name即文件名,target_pwdid为待发送的id
        // 检查文件是否完整(不用检查了,我只会将完整的文件目录项设为1)
        off_t f_size;
        off_t c_size;
        char f_hash[17] = {0};
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
        if (sendFile(task->fd, fd, f_size) == 1) {  
            // sendfile中close了fd,若返回值为1证明连接中断,则不进行剩余发送任务
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
    char data[2] = "1";
    int res_len = sizeof(Command) + 1;
    sendn(task->fd, &res_len, sizeof(int));
    sendn(task->fd, &task->cmd, sizeof(Command));
    sendn(task->fd, data, 1);


    if (task->args[0] == NULL || task->args[1] != NULL) {  // missing operand
        char errmsg[MAXLINE] = "mkdir: missing operand";
        res_len = strlen(errmsg);
        send(task->fd, &res_len, sizeof(int), MSG_NOSIGNAL);
        
        send(task->fd, errmsg, strlen(errmsg), MSG_NOSIGNAL);
        log_error("mkdirCmd: missing operand");
        error(0, errno, "%d mkdir:", task->fd);
        return;
    }

    MYSQL* mysql = getDBConnection(task->dbpool);
    int pwdid = getPwdId(mysql, task->uid);            // pwdid
    char* mkdir_name = task->args[0];                  // name

    char pwd[1024] = {0};                              // 当前目录的绝对路径
    if (getPwd(mysql, pwdid, pwd, 1024) == -1) {
        releaseDBConnection(task->dbpool, mysql);
        char* errmsg = "mkdir: getPwd failed";
        res_len = strlen(errmsg);
        send(task->fd, &res_len, sizeof(int), MSG_NOSIGNAL);
        send(task->fd, errmsg, res_len, MSG_NOSIGNAL);
        log_error("%s",errmsg);
        return;
    }

    char* absolute_path[1024] = {0};              // 绝对路径
    sprintf(absolute_path, "%s/%s", pwd, mkdir_name);
    char* type = "d";  // 类型

    int ret = goToRelativeDir(mysql, pwdid, mkdir_name, type);
    if (ret == 0) {
        // 不存在, insert
        int insert_ret = insertRecord(mysql, pwdid, task->uid, NULL, mkdir_name, absolute_path, 
                    "d", NULL, NULL, '1');
        releaseDBConnection(task->dbpool, mysql);
        if (insert_ret == -1) {
            char* errmsg = "mkdir: insert failed";
            res_len = strlen(errmsg);
            send(task->fd, &res_len, sizeof(int), MSG_NOSIGNAL);
            send(task->fd, errmsg, res_len, MSG_NOSIGNAL);
            log_error("%s",errmsg);
        } else {
            log_info("mkdir: insert succeed");
        }
        
    } else if (ret > 0) {
        releaseDBConnection(task->dbpool, mysql);
        // 存在，不能创建
        char* errmsg = "mkdir: dir already exists";
        res_len = strlen(errmsg);
        send(task->fd, &res_len, sizeof(int), MSG_NOSIGNAL);
        send(task->fd, errmsg, res_len, MSG_NOSIGNAL);
        log_error("%s",errmsg);
    } else {
        // ret < 0 
        // 目录项存在，exist = 0
        if (strcmp(type, "d") == 0) { // 是目录，exist改为1
            const char* query_str = "update nb_vftable set exist = 1 where id = ";
            char str[16] = {0};
            sprintf(str, "%d", -ret);
            char query[1024] = {0};
            sprintf(query, "%s%s", query_str, str);

            int update_ret = mysql_query(mysql, query);
            if (update_ret != 0) {
                log_error("mkdir: mysql_query: %s", strerror(errno));
                error(1, errno, "mkdir: mysql_query");
            }else {
                log_info("mkdir: mysql_query succeed");
            }
        } else { // 是文件，不能创建
            char* errmsg = "mkdir: dirname wrong";
            res_len = strlen(errmsg);
            send(task->fd, &res_len, sizeof(int), MSG_NOSIGNAL);
            send(task->fd, errmsg, res_len, MSG_NOSIGNAL);
            log_error("%s",errmsg);
        }
        releaseDBConnection(task->dbpool, mysql);
    } 
    
    

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
    pwdid = insertRecord(pconn, -1, uid, NULL, "home", "/", 'd', NULL, NULL, '1');
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
