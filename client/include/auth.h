#ifndef __NB_AUTH_H
#define __NB_AUTH_H

#include "bussiness.h"
#include "head.h"

void getSetting(char* salt, char* passwd);

int userRegister(int sockfd);
int userLogin(int sockfd, char* name, char* cwd);

#endif
