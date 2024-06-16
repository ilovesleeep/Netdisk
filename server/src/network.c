#include "../include/network.h"

#define BACKLOG 10

int tcpListen(int port) {
    char port_str[6];
    sprintf(port_str, "%d", port);

    struct addrinfo hints, *res;
    bzero(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int err;
    if ((err = getaddrinfo(NULL, port_str, &hints, &res)) == -1) {
        error(1, 0, "getaddrinfo: %s", gai_strerror(err));
    }

    int sockfd = -1;
    struct addrinfo* cur;
    for (cur = res; cur != NULL; cur = cur->ai_next) {
        if (cur->ai_family == AF_INET) {
            // printf("[INFO] Skip IPv4 Hug IPv6\n");
            continue;
        }

        if ((sockfd = socket(cur->ai_family, cur->ai_socktype,
                             cur->ai_protocol)) == -1) {
            error(0, errno, "socket");
            continue;
        }

        int opt_on = 1, opt_off = 0;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt_on,
                       sizeof(opt_on)) == -1) {
            error(1, errno, "setsockopt");
        }
        if (setsockopt(sockfd, IPPROTO_IPV6, IPV6_V6ONLY, &opt_off,
                       sizeof(opt_off)) == -1) {
            error(1, errno, "setsockopt");
        }

        if (bind(sockfd, cur->ai_addr, cur->ai_addrlen) == -1) {
            error(0, errno, "bind");
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
    struct addrinfo hints, *res;
    bzero(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int err;
    if ((err = getaddrinfo(host, port, &hints, &res)) == -1) {
        error(1, 0, "getaddrinfo: %s", gai_strerror(err));
    }

    int sockfd;
    struct addrinfo* cur;
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

void epollDel(int epfd, int fd) { epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL); }

void epollMod(int epfd, int fd, enum EPOLL_EVENTS mode) {
    struct epoll_event event;
    event.events = mode;
    event.data.fd = fd;
    if (epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &event) == -1) {
        error(1, errno, "epoll_ctl");
    }
}
