#include "thread_pool.h"

int tcpInit(const char * ip, const char * port)
{
    //1. 创建TCP的监听套接字
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    printf("listenfd: %d\n", listenfd);
    if(listenfd < 0) {
        perror("socket");
        return -1;
    }
    int on = 1;
    int ret = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    if(ret < 0) {
        perror("setsockopt");
        close(listenfd);
        return -1;
    }

    //2. 绑定网络地址
    struct sockaddr_in serveraddr;
    //初始化操作,防止内部有脏数据
    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;//指定IPv4
    serveraddr.sin_port = htons(atoi(port));//指定端口号
    //指定IP地址
    serveraddr.sin_addr.s_addr = inet_addr(ip);
    ret = bind(listenfd, (const struct sockaddr *)&serveraddr, sizeof(serveraddr));
    if(ret < 0) {
        perror("bind");
        close(listenfd);//关闭套接字
        return -1;
    }
    //3. 进行监听
    ret = listen(listenfd, 10);
    if(ret < 0) {
        perror("listen");
        close(listenfd);
        return -1;
    }
    printf("server start listening.\n");
    return listenfd;
}

int addEpollReadfd(int epfd, int fd)
{
    struct epoll_event evt;
    memset(&evt, 0, sizeof(evt));
    evt.data.fd = fd;
    evt.events = EPOLLIN;
    int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &evt);
    ERROR_CHECK(ret, -1, "epoll_ctl");
    return 0;
}

int delEpollReadfd(int epfd, int fd)
{
    struct epoll_event evt;
    memset(&evt, 0, sizeof(evt));
    evt.data.fd = fd;
    evt.events = EPOLLIN;
    int ret = epoll_ctl(epfd, EPOLL_CTL_DEL, fd, &evt);
    ERROR_CHECK(ret, -1, "epoll_ctl");
    return 0;
}

//其作用：确定发送len字节的数据
int sendn(int sockfd, const void * buff, int len)
{
    int left = len;
    const char * pbuf = buff;
    int ret = -1;
    while(left > 0) {
        ret = send(sockfd, pbuf, left, 0);
        if(ret < 0) {
            perror("send");
            return -1;
        }

        left -= ret;
        pbuf += ret;
    }
    return len - left;
}

//其作用：确定接收len字节的数据
int recvn(int sockfd, void * buff, int len)
{
    int left = len;//还剩下多少个字节需要接收
    char * pbuf = buff;
    int ret = -1;
    while(left > 0) {
        ret = recv(sockfd, pbuf, left, 0);
        if(ret == 0) {
            break;
        } else if(ret < 0) {
            perror("recv");
            return -1;
        }

        left -= ret;
        pbuf += ret;
    }
    //当退出while循环时，left的值等于0
    return len - left;
}
