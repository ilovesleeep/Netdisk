#include "../include/transfer.h"

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
            if (++i == 1000) {
                // 如果接收了1000次0长度的信息，那对端一定是关闭了，要么就是网太差，直接踢出去
                return -1;
            }
        }

        bytes += n;
    }  // bytes == length

    return 0;
}
