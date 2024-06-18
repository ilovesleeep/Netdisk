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

    //接收客户端对本文件是否存在过的确认及哈希值
    //收到客户端是否存在过的确认，若存在则检查哈希值，若不存在则直接发送
    int recv_stat = 0;
    recv(sockfd, &recv_stat, sizeof(int), MSG_WAITALL);

    off_t send_bytes = 0;
    if(recv_stat == 1){
        //文件存在过,检查哈希值
        //先看看他有多大的文件
        recvn(sockfd, &send_bytes, sizeof(send_bytes));
        unsigned char md5sum_client[16];
        recvn(sockfd, md5sum_client, sizeof(md5sum_client));
        if(send_bytes > statbuf.st_size){
            //我服务器的文件都没那么大,你哪来那么大,我给你重发一个
            send_bytes = 0;
        }
        else{
            //先根据收到的文件大小计算自己的哈希值(服务器的文件不可能有文件空洞)
            MD5_CTX ctx;
            MD5_Init(&ctx);
            for(off_t curr = 0; curr < send_bytes; curr += MMAPSIZE){
                if(curr + MMAPSIZE <= send_bytes){
                    char* p = mmap(NULL, MMAPSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, curr);
                    MD5_Update(&ctx, p, MMAPSIZE);
                    munmap(p, MMAPSIZE);
                }
                else{
                    int surplus = send_bytes - curr;
                    char* p = mmap(NULL, surplus, PROT_READ | PROT_WRITE, MAP_SHARED, fd, curr);
                    MD5_Update(&ctx, p, surplus);
                    munmap(p, surplus);
                    break;
                }
            }
            //生成MD5值
            unsigned char md5sum[16];
            MD5_Final(md5sum, &ctx);

            //比较
            if(memcmp(md5sum_client, md5sum, sizeof(md5sum)) == 0){
                //是一个文件(＾－＾),继续发送叭
                int send_stat = 0;
                sendn(sockfd, &send_stat, sizeof(int));
            }
            else{
                //不是一个文件,重新来过吧
                int send_stat = 1;
                sendn(sockfd, &send_stat, sizeof(int));
                send_bytes = 0;
            }

        }
    }
    #pragma GCC diagnostic pop
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

unsigned char md6[16];

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

    // 接收文件的大小
    off_t fsize;
    recvn(sockfd, &fsize, sizeof(fsize));
    off_t recv_bytes = 0;

    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"

    //检查0为没有存在过,1为存在过
    struct stat statbuf;
    fstat(fd, &statbuf);
    if(statbuf.st_size < MMAPSIZE){
        //没有存在过(小于MMAPSIZE都当没存在过处理,不差那1M流量,懒得再算哈希值)
        int send_stat = 0;
        send(sockfd, &send_stat, sizeof(int), MSG_NOSIGNAL);
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
        sendn(sockfd, &recv_bytes, sizeof(recv_bytes));
        sendn(sockfd, md5sum, sizeof(md5sum));

        //看看文件是不是一样的呀
        int recv_stat = 0;
        recvn(sockfd, &recv_stat, sizeof(int));
        if(recv_stat == 1){
            //糟糕!文件不一样
            recv_bytes = 0;
        }
        //文件一样,recv_bytes指向的是开始接收的位置
    }
    #pragma GCC diagnostic pop
    //此时recv_bytes对应正确的开始接收位置
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
            recvn(sockfd, addr, length);
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
            recvn(sockfd, buf, length);
            write(fd, buf, length);

            recv_bytes += length;

            printf("[INFO] downloading %5.2lf%%\r", 100.0 * recv_bytes / fsize);
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

    int name_len = 0;
    while (recv(sockfd, &name_len, sizeof(int), MSG_WAITALL)) {
        if (name_len == 0) {
            printf("\n");
            break;
        }
        char filename[1000] = {0};
        recv(sockfd, filename, name_len, MSG_WAITALL);
        printf("%s\t", filename);
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
        for(char* p = args[i]; *p != '\0'; p++){
            for(char* start = p; *p != '/'; p++){
                if(*(p + 1) == '\0'){
                    strcpy(filename, start);
                    break;
                }
            }  //*p == '\0' || *p == '/'
        }

        // 先发文件名
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

void rmCmd(int sockfd, char* buf) {
    recv(sockfd, buf, MAXLINE, 0);
    if (strcmp(buf, "0") != 0) {
        puts(buf);
    }
    return;
}
