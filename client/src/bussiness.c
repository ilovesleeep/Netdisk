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

void sendFile(int sockfd, const char* path) {
    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        error(1, errno, "open %s", path);
    }

    // 先发文件名
    DataBlock block;
    strcpy(block.data, path);
    block.length = strlen(path);
    sendn(sockfd, &block, sizeof(int) + block.length);

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

void cdCmd(Task* ptask, char* buf) {
    char path[MAXLINE] = "~";
    if (ptask->args[1] != NULL) {
        // TODO: path checking
        sprintf(path, "~/%s", ptask->args[1]);
        send(ptask->fd, path, strlen(path), 0);
    } else {
        send(ptask->fd, path, strlen(path), 0);
    }
    // TODO: error checking

    return;
}

void lsCmd(int sockfd) {
    //参数校验
    int recv_stat = 0;
    recv(sockfd, &recv_stat, sizeof(int), MSG_WAITALL);
    //错误处理
    if(recv_stat == 1){
        int info_len = 0;
        recv(sockfd, &info_len, sizeof(int), MSG_WAITALL);
        char error_info[1000] = {0};
        recv(sockfd, error_info, info_len, MSG_WAITALL);
        puts(error_info);
        return;
    }


    int name_len = 0;
    while(recv(sockfd, &name_len, sizeof(int), MSG_WAITALL)){
        if(name_len == 0){
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

void mkdirCmd(int sockfd, char* buf) {
    recv(sockfd, buf, MAXLINE, 0);
    if (strcmp(buf,"0") != 0) {
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

