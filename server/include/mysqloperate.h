#ifndef __MYSQL_OPERATE_H
#define __MYSQL_OPERATE_H

#include <mysql/field_types.h>
#include <mysql/mysql.h>

#include "dbpool.h"
#include "head.h"

int getPwdId(MYSQL* mysql, int uid);
char* getPwd(MYSQL* mysql, int pwdid);
int goToRelativeDir(MYSQL* mysql, int pwdid, char* name,char *type);
char** findchild(MYSQL* mysql, int pwdid);
int insertRecord(MYSQL* mysql, int p_id, int u_id, char* f_hash, char* name,
                 char* path, char type, off_t* f_size, off_t* c_size, char exist);

#endif
