#ifndef __MYSQL_OPERATE_H
#define __MYSQL_OPERATE_H

#include <mysql/field_types.h>
#include <mysql/mysql.h>

#include "dbpool.h"
#include "head.h"

int getPwdId(MYSQL* mysql, int uid);

int getPwd(MYSQL* mysql, int pwdid, char* path, int path_size);

char getTypeById(MYSQL* mysql, int id);

int goToRelativeDir(MYSQL* mysql, int pwdid, char* name, char* type);

char** findchild(MYSQL* mysql, int pwdid);

int insertRecord(MYSQL* mysql, int p_id, int u_id, char* f_hash, char* name,
                 char* path, char type, off_t* f_size, off_t* c_size,
                 char exist);

int getFileInfo(MYSQL* mysql, int pwdid, char* f_hash, off_t* f_size,
                off_t* c_size);

int localFile(MYSQL* mysql, char* f_hash, off_t* f_size, off_t* c_size);
int updateRecord(MYSQL* mysql, int pwdid, const int* p_id, const int* u_id, const char* f_hash, const char* type, const off_t* f_size, const off_t* c_size, const char* exist);
#endif
