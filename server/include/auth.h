#ifndef __NB_AUTH_C
#define __NB_AUTH_C

#include <l8w8jwt/decode.h>
#include <l8w8jwt/encode.h>

#include "head.h"
#include "log.h"
#include "mysqloperate.h"
#include "task.h"
#include "transfer.h"

int makeToken(char* token, int uid);
int checkToken(char* token, int uid);

char* generateSalt(void);

void getSaltByCryptPasswd(char* salt, char* cryptpasswd);
char* getCryptpasswdByUID(MYSQL* pconn, int uid);
int getUserIDByUsername(MYSQL* pconn, const char* username);

bool userExist(MYSQL* pconn, const char* username);
bool userPass(MYSQL* pconn, int uid, const char* cryptpasswd);
int userInsert(MYSQL* pconn, const char* username, const char* cryptpasswd,
               long long pwdid);
int userUpdate(MYSQL* pconn, int uid, const char* fieldname, const char* value);

void regCheck1(Task* task);
void regCheck2(Task* task);
void loginCheck1(Task* task);
void loginCheck2(Task* task);

#endif
