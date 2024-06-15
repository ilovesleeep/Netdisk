#include "thread_pool.h"
#define FILENAME "bigfile.avi"


int transferFile(int sockfd)
{
    //进行文件的发送
    //1. 先发送文件名
    //1.1 设置文件名的长度
    train_t t;
    memset(&t, 0, sizeof(t));
    t.len = strlen(FILENAME);
    strcpy(t.buff, FILENAME);
    send(sockfd, &t, 4 + t.len, 0);

    //2. 读取文件内容( 相对路径 )
    int fd = open(FILENAME, O_RDWR);
    ERROR_CHECK(fd, -1, "open");
    memset(&t, 0, sizeof(t));

    //2.1 获取文件的长度
    struct stat fileInfo;
    memset(&fileInfo, 0, sizeof(fileInfo));
    fstat(fd, &fileInfo);
    off_t length = fileInfo.st_size;
    printf("file length: %ld\n", length);

    //发送文件的长度
    sendn(sockfd, &length, sizeof(length));

    //借助于一条管道来搬运数据
    int fds[2];
    int ret = -1;
    int curSize = 0;
    pipe(fds);

    //发送文件内容
    while(curSize < length) {
        ret = splice(fd, NULL, fds[1], NULL, 4096, SPLICE_F_MORE);
        ret = splice(fds[0], NULL, sockfd, NULL, ret, SPLICE_F_MORE);
        curSize += ret;
    }
    printf("curSize:%d\n", curSize);
    close(fd);
    return 0;
}

