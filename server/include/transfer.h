#ifndef __NB_TRANSFER_H
#define __NB_TRANSFER_H

#include "head.h"

int sendn(int sockfd, void* buf, int length);
int recvn(int sockfd, void* buf, int length);

#endif
