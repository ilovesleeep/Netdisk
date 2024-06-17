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

    // 发送文件内容
    off_t sent_bytes = 0;
    if (fsize >= BIGFILE_SIZE) {
        // 大文件
        while (sent_bytes < fsize) {
            off_t length =
                fsize - sent_bytes >= MMAPSIZE ? MMAPSIZE : fsize - sent_bytes;

            void* addr =
                mmap(NULL, length, PROT_READ, MAP_SHARED, fd, sent_bytes);
            sendn(sockfd, addr, length);
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
            sendn(sockfd, buf, length);

            sent_bytes += length;
        }
    }
    close(fd);
}

void recvFile(int sockfd) {
    // 接收文件名
    DataBlock block;
    bzero(&block, sizeof(block));
    recvn(sockfd, &block.length, sizeof(int));
    recvn(sockfd, block.data, block.length);

    // 打开文件
    int fd = open(block.data, O_RDWR | O_TRUNC | O_CREAT, 0666);
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

int cdCmd(int sockfd, char* buf, char* cwd, int* recv_status) {
    recv(sockfd, recv_status, sizeof(int), 0);
    if (*recv_status) {
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

void putsCmd(int sockfd, char** args){
    //先等服务端就绪
    int recv_stat = 0;
    recv(sockfd, &recv_stat, sizeof(int), MSG_WAITALL);


    //参数错误
    if(args[1] == NULL){
        printf("parameter error\n");
        int send_stat = 1;
        send(sockfd, &send_stat, sizeof(int), MSG_NOSIGNAL);
        return;
    }
    for(int i = 1; args[i]; i++){
        //统一先发送是否要发送，0为要发送，1为不发送
        //有文件则发送0，路径错误或发送完成则发送1
        int fd = open(args[i], O_RDWR);
        if(fd == -1){
            int send_stat = 1;
            send(sockfd, &send_stat, sizeof(int), MSG_NOSIGNAL);
            printf("path error : no find NO.%d file\n", i);
            return;
        }
        
        int send_stat = 0;
        send(sockfd, &send_stat, sizeof(int), MSG_NOSIGNAL);
printf("send stat %d\n", send_stat);
        //解析出文件名
        char filename[1000] = {0};
        for(char* p = args[i]; *p != '\0'; p++){
            for(char* start = p; *p != '/'; p++){
                if(*p == '\0'){
                    strcpy(filename, start);
                    break;
                }
            } //*p == '\0' || *p == '/'
        }

printf("filename == %s\n", filename);
        // 先发文件名
        DataBlock block;
        strcpy(block.data, filename);
        block.length = strlen(filename);
        sendn(sockfd, &block, sizeof(int) + block.length);

        //发送文件
        sendFile(sockfd, fd);
printf("puts no %d\n", i);
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

void exitCmd(char* buf) {
    strcpy(buf, "I will miss you");
    return;
}

void unknownCmd(char* buf) {
    strcpy(buf, "What can I say");
    return;
}
