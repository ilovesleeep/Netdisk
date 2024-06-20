#include "../include/mysqloperate.h"

// //获取当前路径,传入连接和用户uid，传出当前目录的索引
// int getPwdId(MYSQL* mysql, int uid) {
//     const char sql[60] = "select pwd from nb_usertable where id = ?";
//     MYSQL_STMT* stmt = mysql_stmt_init(mysql);
//     mysql_stmt_prepare(stmt, sql, strlen(sql));
//     //初始化参数
//     MYSQL_BIND bind;
//     bzero(&bind, sizeof(bind));
//     bind.buffer_type = MYSQL_TYPE_LONG;
//     bind.buffer = &uid;
//     bind.length = NULL;
//     bind.is_null = 0;

//     mysql_stmt_bind_param(stmt, &bind);

//     mysql_stmt_execute(stmt);

//     MYSQL_RES* res = mysql_stmt_execute(stmt);
//     //获取结果
//     int res_pwd;
//     MYSQL_BIND res_bind;
//     bzero(&res_bind, sizeof(res_bind));
//     res_bind.buffer_type = MYSQL_TYPE_LONG;
//     res_bind.buffer = &res_pwd;
//     res_bind.buffer_length = sizeof(int);

//     mysql_stmt_bind_result(stmt, &bind);
//     mysql_stmt_store_result(stmt);
//     mysql_stmt_fetch(stmt);

//     mysql_stmt_close(stmt);
//     mysql_free_result(res);

//     return res_pwd;
// }

int getPwdId(MYSQL* mysql, int uid) {
    int pwdid;
    char sql[60] = {};
    sprintf(sql, "select pwdid from nb_usertable where id=%d", uid);
    mysql_query(mysql, sql);
    MYSQL_RES* res = mysql_store_result(mysql);
    MYSQL_ROW row;
    row = mysql_fetch_row(res);
    pwdid = atoi(row[0]);
    mysql_free_result(res);

    return pwdid;
}

// 根据当前路径和传入参数找到目标路径,传入当前路径索引和**文件名**,传出目标路径索引,传出若路径不存在则返回-1,
// type为传出参数,传出目标索引对应的文件类型'd'f',若传参为NULL则不返回
int goToRelativeDir(MYSQL* mysql, int pwd, char* path, char* type) {
    int retval = 0;
    if (strcmp(path, "..") == 0) {
        // 查找上一级目录
        char sql[100] = {0};
        sprintf(sql, "select p_id from nb_vftable where id = %d", pwd);
        mysql_query(mysql, sql);
        MYSQL_RES* res = mysql_store_result(mysql);
        MYSQL_ROW row;
        mysql_fetch_row(res);
        retval = atoi(row[0]);
        mysql_free_result(res);
    } else if (strcmp(path, "~") == 0) {
        // 查找家目录
        while ((pwd = goToRelativeDir(mysql, pwd, "..", NULL)) != -1) {
            retval = pwd;
        }
    } else {
        // 查找指定目录项
        char sql[] =
            "select id, type from nb_vftable where p_id = ? and name = ? and "
            "exist = 1";
        // 初始化stmt语句
        MYSQL_STMT* stmt = mysql_stmt_init(mysql);
        int ret = mysql_stmt_prepare(stmt, sql, strlen(sql));
        if (ret) {
            fprintf(stderr, "%s\n", mysql_error(mysql));
            return -1;
        }
        // 初始化绑定参数
        MYSQL_BIND bind[2];
        bzero(bind, sizeof(bind));
        bind[0].buffer_type = MYSQL_TYPE_LONG;
        bind[0].buffer = &pwd;
        bind[0].length = NULL;
        bind[0].is_null = 0;
        bind[1].buffer_type = MYSQL_TYPE_VARCHAR;
        bind[1].buffer = path;
        bind[1].buffer_length = strlen(sql);
        bind[1].is_null = 0;
        mysql_stmt_bind_param(stmt, bind);

        mysql_stmt_execute(stmt);
        MYSQL_RES* res = mysql_stmt_result_metadata(stmt);
        if (res == NULL) {
            return -1;
        }

        // 初始化结果绑定参数
        MYSQL_BIND res_bind[2];

        bind[0].buffer_type = MYSQL_TYPE_LONG;
        bind[0].buffer = &retval;
        bind[0].buffer_length = sizeof(int);

        char res_type = '\0';
        bind[1].buffer_type = MYSQL_TYPE_STRING;
        bind[1].buffer = &res_type;
        bind[1].buffer_length = sizeof(res_type);

        ret = mysql_stmt_bind_result(stmt, res_bind);

        ret = mysql_stmt_store_result(stmt);

        // 由于用户id,路径名,文件名是联合唯一约束,因此查询结果一定不超过一个
        ret = mysql_stmt_fetch(stmt);
        if (ret == 1 || ret == MYSQL_NO_DATA) {
            fprintf(stderr, "%s", mysql_error(mysql));
            retval = -1;
        }

        if (type != NULL) {
            *type = res_type;
        }
        // type为'D'时retval已完成赋值,直接返回即可
    }
    return retval;
}

char* getPwd(MYSQL* mysql, int pwdid) {}

char** findchild(MYSQL* mysql, int pwdid) {}

// 填入需要插入的值,函数会将值插入MYSQL,返回插入的主键id值,若插入失败则返回-1
int insertRecord(MYSQL* mysql, int p_id, int u_id, char* f_hash, char* name,
                 char* path, char type, off_t* f_size, off_t* c_size, char exist) {
    char sql[1024] = {0};
    if (f_hash == NULL) {
        // 文件没有哈希值,因此是目录
        sprintf(sql,
                "INSERT INTO nb_vftable(p_id, u_id, name, path, type, exist) "
                "VALUES(%d, %d, ?, '%s', '%c', '%c')",
                p_id, u_id, path, type, exist);
    } else {
        // 有哈希值,一定是插入文件
        sprintf(sql,
                "INSERT INTO nb_vftable(p_id, u_id, f_hash, name, path, type, "
                "f_size, c_size) VALUES(%d, %d, '%s', ?, '%s', '%c', %ld, %ld, '%c')",
                p_id, u_id, f_hash, path, type, *f_size, *c_size, exist);
    }

    MYSQL_STMT* stmt = mysql_stmt_init(mysql);

    int ret = mysql_stmt_prepare(stmt, sql, strlen(sql));
    if (ret) {
        fprintf(stderr, "%s\n", mysql_error(mysql));
        return -1;
    }

    MYSQL_BIND bind;
    bzero(&bind, sizeof(bind));
    bind.buffer_type = MYSQL_TYPE_VAR_STRING;
    bind.buffer = name;
    unsigned long buf_len = strlen(name);
    bind.length = &buf_len;
    bind.is_null = 0;

    ret = mysql_stmt_bind_param(stmt, &bind);

    // 执行语句前先开启事务
    ret = mysql_query(mysql, "START TRANSACTION");
    ret = mysql_stmt_execute(stmt);

    int retval = mysql_insert_id(mysql);
    ret = mysql_query(mysql, "COMMIT");

    mysql_stmt_close(stmt);

    return retval;
}
