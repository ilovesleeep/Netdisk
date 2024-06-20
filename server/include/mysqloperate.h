#ifndef MYSQL_OPERATE_H
#define MYSQL_OPERATE_H

#include <mysql/field_types.h>
#include <mysql/mysql.h>
int getPwdId(MYSQL* mysql, int uid);
int getPwd(MYSQL* mysql, int pwdid, char* path, int path_size);
int goToRelativeDir(MYSQL* mysql, int pwd, char* path);
char** findchild(MYSQL*mysql, int pwdid);
#endif;
