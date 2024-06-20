#include "../include/dbpool.h"
#include "../include/head.h"
//获取当前路径,传入连接和用户uid，传出当前目录的索引
int getpwd(MYSQL* mysql, int uid){
    const char sql[60] = "select pwd from nb_usertable where id = ?";
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    mysql_stmt_prepare(stmt, sql, strlen(sql));
    //初始化参数
    MYSQL_BIND bind;
    bzero(&bind, sizeof(bind));
    bind.buffer_type = MYSQL_TYPE_LONG; 
    bind.buffer = &uid;
    bind.length = NULL;
    bind.is_null = 0;

    mysql_stmt_bind_param(stmt, &bind);

    mysql_stmt_execute(stmt);

    MYSQL_RES* res = mysql_stmt_execute(stmt);
    //获取结果
    int res_pwd;
    MYSQL_BIND res_bind;
    bzero(&res_bind, sizeof(res_bind));
    res_bind.buffer_type = MYSQL_TYPE_LONG;
    res_bind.buffer = &res_pwd;
    res_bind.buffer_length = sizeof(int);

    mysql_stmt_bind_result(stmt, &bind);
    mysql_stmt_store_result(stmt);
    mysql_stmt_fetch(stmt);
    
    mysql_stmt_close(stmt);
    mysql_free_result(res);

    return res_pwd;
}