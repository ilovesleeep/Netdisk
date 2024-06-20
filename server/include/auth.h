#ifndef __NB_AUTH_C
#define __NB_AUTH_C

#include "head.h"
#include "log.h"

char* generateSalt(void);
void getSaltByCryptPasswd(char* salt, char* cryptpasswd);

char* getCryptpasswdByUID(MYSQL* pconn, int uid);
int getUserIDByUsername(MYSQL* pconn, const char* username);

bool userExist(MYSQL* pconn, const char* username);
bool userPass(MYSQL* pconn, int uid, const char* cryptpasswd);
int userInsert(MYSQL* pconn, const char* username, const char* cryptpasswd,
               long long pwdid);
int userUpdate(MYSQL* pconn, int uid, const char* fieldname, const char* value);

#endif
