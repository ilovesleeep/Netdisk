#ifndef __NB_CLIENT_H
#define __NB_CLIENT_H

#include <ctype.h>

#include "auth.h"
#include "bussiness.h"
#include "head.h"
#include "network.h"
#include "parser.h"

int clientMain(int argc, char* argv[]);

void printMenu(void);

void welcome(int sockfd, char* username);

int sessionHandler(int sockfd, char* host, char* user);

#endif
