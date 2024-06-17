#ifndef __NB_NETWORK_H
#define __NB_NETWORK_H

#include "head.h"

int tcpListen(int port);

void* getIpAddr(struct sockaddr* sa);

void epollAdd(int epfd, int fd);
void epollDel(int epfd, int fd);
void epollMod(int epfd, int fd, enum EPOLL_EVENTS mode);

#endif
