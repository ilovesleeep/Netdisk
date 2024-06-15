#ifndef __K_NETWORK_H
#define __K_NETWORK_H

#include <func.h>

int tcpListen(int port);
int tcpConnect(const char* host, const char* port);
void* getIpAddr(struct sockaddr* sa);

void epollAdd(int epfd, int fd);
void epollDel(int epfd, int fd);

#endif
