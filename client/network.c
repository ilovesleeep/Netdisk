#include "network.h"

#define BACKLOG 10

int tcpListen(char* port) {
    struct addrinfo hints, *res;
    bzero(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, port, &hints, &res) == -1) {
        error(1, errno, "getaddrinfo");
    }

    int sockfd = -1;
    struct addrinfo* cur = res;
    while (cur) {
        if (cur->ai_family == AF_INET) {
            printf("[INFO] skip IPv4\n");
            cur = cur->ai_next;
            continue;
        }

        if ((sockfd = socket(cur->ai_family, cur->ai_socktype, cur->ai_protocol)) == -1) {
            error(0, errno, "socket");
            cur = cur->ai_next;
            continue;
        }

        int opt_on = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 
                       &opt_on, sizeof(opt_on)) == -1) {
            error(1, errno, "setsockopt");
        }
        int opt_off = 0;
        if (setsockopt(sockfd, IPPROTO_IPV6, IPV6_V6ONLY, 
                       &opt_off, sizeof(opt_off)) == -1) {
            error(1, errno, "setsockopt");
        }

        if (bind(sockfd, cur->ai_addr, cur->ai_addrlen) == -1) {
            error(0, errno, "bind");
            cur = cur->ai_next;
            continue;
        }

        break;
    }
    freeaddrinfo(res);
    
    if (cur == NULL) {
        error(1, errno, "failed to bind");
    }
    
    if (listen(sockfd, BACKLOG) == -1) {
        error(1, errno, "listen");
    }

    return sockfd;
}

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
        if ((sockfd = socket(cur->ai_family, cur->ai_socktype, cur->ai_protocol)) == -1) {
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
        error(1, 0, "connect to %s:%s failed", host, port);
    }
    printf("[INFO] established with %s:%s\n", host, port);

    return sockfd;
}

void* getIpAddr(struct sockaddr* sa) {
    if (sa->sa_family == AF_INET) {
        return &((struct sockaddr_in*)sa)->sin_addr;
    }
    return &((struct sockaddr_in6*)sa)->sin6_addr;
}

void epollAdd(int epfd, int fd) {
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = fd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event) == -1) {
        error(1, errno, "epoll_ctl");
    }
}

void epollDel(int epfd, int fd) {
    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
}

