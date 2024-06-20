#ifndef MYSQL_OPERATE_H
#define MYSQL_OPERATE_H

#include <mysql/field_types.h>
#include <mysql/mysql.h>
int getPwdId(MYSQL* mysql, int uid);
char* getPwd(MYSQL* mysql, int pwdid);
int goToRelativeDir(MYSQL* mysql, int pwd, char* path);
char** findchild(MYSQL*mysql, int pwdid);
#endif;