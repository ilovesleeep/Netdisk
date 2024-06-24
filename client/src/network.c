#include "../include/network.h"

int tcpConnect(const char* host, const char* port) {
    struct addrinfo hints, *res, *cur;
    bzero(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int err;
    if ((err = getaddrinfo(host, port, &hints, &res)) == -1) {
        error(1, 0, "getaddrinfo: %s", gai_strerror(err));
    }

    int sockfd;
    for (cur = res; cur != NULL; cur = cur->ai_next) {
        if ((sockfd = socket(cur->ai_family, cur->ai_socktype,
                             cur->ai_protocol)) == -1) {
            error(0, errno, "socket");
            continue;
        }

        if (connect(sockfd, cur->ai_addr, cur->ai_addrlen) == -1) {
            error(0, errno, "connect");
            continue;
        }

        break;
    }
    freeaddrinfo(res);

    if (cur == NULL) {
        error(1, 0, "[ERROR] Connect to %s:%s failed", host, port);
    }

    return sockfd;
}

void epollAdd(int epfd, int fd) {
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = fd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event) == -1) {
        error(1, errno, "epoll_ctl_add");
    }
}

void epollDel(int epfd, int fd) { epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL); }

void epollMod(int epfd, int fd, enum EPOLL_EVENTS epoll_events) {
    struct epoll_event event;
    event.events = epoll_events;
    event.data.fd = fd;
    if (epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &event) == -1) {
        error(1, errno, "epoll_ctl");
    }
}
