#ifndef __NB_AUTH_H
#define __NB_AUTH_H

#include "head.h"

int userRegister(char* name, char* passwd);
int userLogin(char* name, char* passwd);

#endif
