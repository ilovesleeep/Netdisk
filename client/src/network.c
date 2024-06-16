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
        error(1, 0, "connect to %s:%s failed", host, port);
    }
    printf("[INFO] established with %s:%s\n", host, port);

    return sockfd;
}
