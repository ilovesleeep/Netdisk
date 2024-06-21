#define OPENSSL_SUPPRESS_DEPRECATED
#include <openssl/md5.h>
#include "../include/bussiness.h"

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
    int bytes = 0;
    while (bytes < length) {
        int n = recv(fd, (char*)buf + bytes, length - bytes, 0);
        if (n < 0) {
            return -1;
        }

        bytes += n;
    }  // bytes == length

    return 0;
}

void sendFile(int sockfd, int fd) {
    // 发送文件大小
    struct stat statbuf;
    fstat(fd, &statbuf);
    off_t fsize = statbuf.st_size;
    sendn(sockfd, &fsize, sizeof(fsize));
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"

    //计算自己的哈希值(不可能有文件空洞)
    unsigned char md5sum_client[16];
    MD5_CTX ctx;
    MD5_Init(&ctx);
    for(off_t curr = 0; curr < statbuf.st_size; curr += MMAPSIZE){
        if(curr + MMAPSIZE <= statbuf.st_size){
            char* p = mmap(NULL, MMAPSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, curr);
            MD5_Update(&ctx, p, MMAPSIZE);
            munmap(p, MMAPSIZE);
        }
        else{
            int surplus = statbuf.st_size - curr;
            char* p = mmap(NULL, surplus, PROT_READ | PROT_WRITE, MAP_SHARED, fd, curr);
            MD5_Update(&ctx, p, surplus);
            munmap(p, surplus);
            break;
        }
    }
    //生成MD5值
    unsigned char md5sum[16];
    MD5_Final(md5sum, &ctx);

    //发送哈希值
    send(sockfd, md5sum, sizeof(md5sum), MSG_NOSIGNAL);
    #pragma GCC diagnostic pop

    //服务端是否允许发送
    int recv_stat = 0;
    recv(sockfd, &recv_stat, sizeof(int), MSG_NOSIGNAL);
    if(recv_stat == 1){
        //不允许发送,接收错误原因
        int info_len = 0;
        recv(sockfd, &info_len, sizeof(int), MSG_WAITALL);
        char recv_info[100] = {0};
        recv(sockfd, recv_info, sizeof(recv_info), MSG_WAITALL);
        puts(recv_info);
    }

    //接收是否已完成秒传
    recv(sockfd, &recv_stat, sizeof(int), MSG_NOSIGNAL);
    if(recv_stat == 0){
        //已存在该文件,可以秒传
        return 0;
    }

    //接收服务端希望从哪里开始发
    off_t send_bytes = 0;
    recv(sockfd, &send_bytes, sizeof(off_t), MSG_WAITALL);
    //此时send_bytes对应正确的开始发送位置

    // 发送文件内容
    if (fsize >= BIGFILE_SIZE) {
        // 大文件
        while (send_bytes < fsize) {
            off_t length =
                fsize - send_bytes >= MMAPSIZE ? MMAPSIZE : fsize - send_bytes;

            void* addr =
                mmap(NULL, length, PROT_READ, MAP_SHARED, fd, send_bytes);
            sendn(sockfd, addr, length);
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
            sendn(sockfd, buf, length);

            send_bytes += length;
        }
    }
    close(fd);
}

void recvFile(int sockfd) {
    // 先接收哈希值和文件大小
    char f_hash[16] = {0};
    recvn(sockfd, f_hash, 32);
    off_t f_size = 0;
    recvn(sockfd, f_size, sizeof(off_t));
    // 接收文件名
    DataBlock block;
    bzero(&block, sizeof(block));
    recvn(sockfd, &block.length, sizeof(int));
    recvn(sockfd, block.data, block.length);

    // 打开文件
    int fd = open(block.data, O_RDWR | O_CREAT, 0666);
    if (fd == -1) {
        error(1, errno, "open");
    }

    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"

    //检查0为没有存在过,1为存在过
    struct stat statbuf;
    fstat(fd, &statbuf);
    off_t recv_bytes = 0;
    if(statbuf.st_size < MMAPSIZE || statbuf.st_size > f_size){
        //没有存在过(小于MMAPSIZE都当没存在过处理,不差那1M流量,懒得再算哈希值)
        int send_stat = 1;
        sendn(sockfd, &send_stat, sizeof(int));
        sendn(sockfd, &recv_bytes, sizeof(off_t));
    }
    else{
        //存在过,检查哈希值,检查哈希值全部以MMAPSIZE为单位来查找
        int send_stat = 1;
        sendn(sockfd, &send_stat, sizeof(int));
        //计算哈希值
        char empty[MMAPSIZE] = {0};
        MD5_CTX ctx;
        MD5_Init(&ctx);
        
        //prev是后面即将要用的数据,每次计算确认当前数据可用时才为其赋值
        off_t prev_bytes = 0;
        MD5_CTX prev_ctx;
        for(recv_bytes = 0; recv_bytes < statbuf.st_size; recv_bytes += MMAPSIZE){
            if(recv_bytes + MMAPSIZE <= statbuf.st_size){
                //当前大小小于文件大小,计算
                char* p = mmap(NULL, MMAPSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, recv_bytes);
                if(memcmp(p, empty, MMAPSIZE) == 0){
                    //文件空洞,计算到此为止,就用上一次的哈希值和recv_bytes
                    memcpy(&ctx, &prev_ctx, sizeof(ctx));
                    recv_bytes = prev_bytes;
                    munmap(p, MMAPSIZE);
                    break;
                }
                else{
                    //非文件空洞,继续计算
                    memcpy(&prev_ctx, &ctx, sizeof(ctx));
                    prev_bytes = recv_bytes;

                    MD5_Update(&ctx, p, MMAPSIZE);
                    munmap(p, MMAPSIZE);
                }
            }
            else{
                //继续mmap这个大小就要超啦,看看最后一点一不一样
                int surplus = statbuf.st_size - recv_bytes;
                char* p = mmap(NULL, surplus, PROT_READ | PROT_WRITE, MAP_SHARED, fd, recv_bytes);
                char* empty = calloc(surplus, sizeof(char));
                if(memcmp(p, empty, surplus) == 0){
                    free(empty);
                    //文件空洞,计算到此为止,就用上一次的哈希值和recv_bytes
                    memcpy(&ctx, &prev_ctx, sizeof(ctx));
                    recv_bytes = prev_bytes;
                    munmap(p, surplus);
                    break;
                }
                else{
                    //非文件空洞,全部都是有效信息,计算所有的哈希值,offset移动到末尾
                    free(empty);

                    recv_bytes += surplus;
                    MD5_Update(&ctx, p, surplus);

                    munmap(p, surplus);
                    break;
                }
            }
        }
        //生成哈希值
        unsigned char md5sum[16];
        MD5_Final(md5sum, &ctx);
        //发送文件实际大小及哈希值
        //若哈希值相等
        if(memcmp(md5sum, f_hash, 16) == 0){
            int send_stat = 0;
            sendn(sockfd, &send_stat, sizeof(int));
            close(fd);
            return 0;
        }
        //若不等,则发送哈希值
        else{
            int send_stat = 1;
            sendn(sockfd, &send_stat, sizeof(int));
            sendn(sockfd, &recv_bytes, sizeof(recv_bytes));
            sendn(sockfd, md5sum, sizeof(md5sum));
        }
    }
    #pragma GCC diagnostic pop
    //接收从哪里开始发
    recvn(sockfd, &recv_bytes, sizeof(off_t));
    //此时recv_bytes对应正确的开始接收位置
    // 接收文件内容
    if (f_size >= BIGFILE_SIZE) {
        ftruncate(fd, f_size);
        // 大文件
        while (recv_bytes < f_size) {
            off_t length = (f_size - recv_bytes >= MMAPSIZE)
                               ? MMAPSIZE
                               : f_size - recv_bytes;
            void* addr = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED,
                              fd, recv_bytes);
            recvn(sockfd, addr, length);
            munmap(addr, length);

            recv_bytes += length;

            printf("[INFO] downloading %5.2lf%%\r", 100.0 * recv_bytes / f_size);
            fflush(stdout);
        }
    } else {
        char buf[BUFSIZE];
        while (recv_bytes < f_size) {
            off_t length =
                (f_size - recv_bytes >= BUFSIZE) ? BUFSIZE : f_size - recv_bytes;
            recvn(sockfd, buf, length);
            write(fd, buf, length);

            recv_bytes += length;

            printf("[INFO] downloading %5.2lf%%\r", 100.0 * recv_bytes / f_size);
            fflush(stdout);
        }
    }
    printf("[INFO] downloading %5.2lf%%\n", 100.0);
    close(fd);
}

int cdCmd(int sockfd, char* buf, char* cwd) {
    int recv_stat = 0;
    recvn(sockfd, &recv_stat, sizeof(int));
    if (recv_stat) {
        recv(sockfd, buf, MAXLINE, 0);
        printf("Error: %s\n", buf);
        return -1;
    }

    bzero(cwd, MAXLINE);
    recv(sockfd, cwd, MAXLINE, 0);
    return 0;
}

void lsCmd(int sockfd) {
    // 参数校验
    int recv_stat = 0;
    recv(sockfd, &recv_stat, sizeof(int), MSG_WAITALL);
    // 错误处理
    if (recv_stat == 1) {
        int info_len = 0;
        recv(sockfd, &info_len, sizeof(int), MSG_WAITALL);
        char error_info[1000] = {0};
        recv(sockfd, error_info, info_len, MSG_WAITALL);
        puts(error_info);
        return;
    }

    // 接收函数，大火车
    int name_len = 0;
    // bufsize = 4096;
    char filename[BUFSIZE] = {0};
    recv(sockfd, &name_len, sizeof(int), MSG_WAITALL);
    recv(sockfd, filename, name_len, MSG_WAITALL);
    printf("%s\n", filename);

    return;
}

void rmCmd(int sockfd, char* buf) {
    int recv_stat = 0;
    recvn(sockfd, &recv_stat, sizeof(int));

    // 错误处理
    if (recv_stat == 1) {
        int info_len = 0;
        recv(sockfd, &info_len, sizeof(int), MSG_WAITALL);
        char error_info[MAXLINE] = {0};
        recv(sockfd, error_info, info_len, MSG_WAITALL);
        puts(error_info);
        return;
    }

    // 参数正确
    recv_stat = 0;
    recv(sockfd, &recv_stat, sizeof(int), MSG_NOSIGNAL);
    // 错误处理
    if (recv_stat != 0) {
        int info_len = 0;
        char recv_info[MAXLINE] = {0};
        recv(sockfd, &info_len, sizeof(int), MSG_WAITALL);
        recv(sockfd, recv_info, info_len, MSG_WAITALL);
        puts(recv_info);
        return;
    }

    return;
}

void pwdCmd(char* buf) {
    getcwd(buf, MAXLINE);
    return;
}

void getsCmd(int sockfd) {
    // 先检查参数数量是否正确
    int recv_stat = 0;
    recv(sockfd, &recv_stat, sizeof(int), MSG_WAITALL);
    // 若错误则接收错误信息，否则直接开始下一步
    if (recv_stat != 0) {
        int info_len = 0;
        recv(sockfd, &info_len, sizeof(int), MSG_WAITALL);
        char error_info[1000] = {0};
        recv(sockfd, error_info, info_len, MSG_WAITALL);
        puts(error_info);
        return;
    }

    // 参数正确
    for (;;) {
        // 检查文件是否存在,或是否发送完成
        int recv_stat = 0;
        recv(sockfd, &recv_stat, sizeof(int), MSG_WAITALL);
        if (recv_stat != 0) {
            int info_len = 0;
            char recv_info[1000] = {0};
            recv(sockfd, &info_len, sizeof(int), MSG_WAITALL);
            recv(sockfd, recv_info, info_len, MSG_WAITALL);
            puts(recv_info);
            break;
        }
        // 文件存在则接收
        recvFile(sockfd);
    }
    return;
}

void putsCmd(int sockfd, char** args) {
    // 先等服务端就绪
    int recv_stat = 0;
    recv(sockfd, &recv_stat, sizeof(int), MSG_WAITALL);

    // 参数错误
    if (args[1] == NULL) {
        printf("parameter error\n");
        int send_stat = 1;
        send(sockfd, &send_stat, sizeof(int), MSG_NOSIGNAL);
        return;
    }
    for (int i = 1; args[i]; i++) {
        // 统一先发送是否要发送，0为要发送，1为不发送
        // 有文件则发送0，路径错误或发送完成则发送1
        int fd = open(args[i], O_RDWR);
        if (fd == -1) {
            int send_stat = 1;
            send(sockfd, &send_stat, sizeof(int), MSG_NOSIGNAL);
            printf("path error : no find NO.%d file\n", i);
            return;
        }

        int send_stat = 0;
        send(sockfd, &send_stat, sizeof(int), MSG_NOSIGNAL);

        // 解析出文件名
        char filename[1000] = {0};
        for (char* p = args[i]; *p != '\0'; p++) {
            for (char* start = p; *p != '/'; p++) {
                if (*(p + 1) == '\0') {
                    strcpy(filename, start);
                    break;
                }
            }  //*p == '\0' || *p == '/'
        }

        // 文件名
        DataBlock block;
        strcpy(block.data, filename);
        block.length = strlen(filename);
        sendn(sockfd, &block, sizeof(int) + block.length);

        // 发送文件
        sendFile(sockfd, fd);
    }
    int send_stat = 1;
    send(sockfd, &send_stat, sizeof(int), MSG_NOSIGNAL);
    printf("puts success\n");
    return;
}

void mkdirCmd(int sockfd, char* buf) {
    recv(sockfd, buf, MAXLINE, 0);
    if (strcmp(buf, "0") != 0) {
        puts(buf);
    }
    return;
}
