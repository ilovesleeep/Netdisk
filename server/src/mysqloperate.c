
#include "../include/mysqloperate.h"
#define MAX_CHILD_CHARACTER 512



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

// 根据当前路径和传入参数找到目标路径,传入当前路径索引和**文件名**,传出目标路径索引,传出若路径不存在则返回0,若已被删除则返回id的负数
// type为传出参数,传出目标索引对应的文件类型'd'f',若传参为NULL则不返回
int goToRelativeDir(MYSQL* mysql, int pwd, char* path, char* type) {
    int retval = 0;
    if (strcmp(path, "..") == 0) {

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
            "select id, type, exist from nb_vftable where p_id = ? and name = ?";
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
        bind[1].buffer_type = MYSQL_TYPE_VAR_STRING;
        bind[1].buffer = path;
        int buf_len = strlen(sql);
        bind[1].length = &buf_len;
        bind[1].is_null = 0;
        mysql_stmt_bind_param(stmt, bind);

        mysql_stmt_execute(stmt);
        MYSQL_RES* res = mysql_stmt_result_metadata(stmt);
        if (res == NULL) {
            return 0;
        }

        // 初始化结果绑定参数
        MYSQL_BIND res_bind[3];

        bind[0].buffer_type = MYSQL_TYPE_LONG;
        bind[0].buffer = &retval;
        bind[0].buffer_length = sizeof(int);

        char res_type = '\0';
        bind[1].buffer_type = MYSQL_TYPE_STRING;
        bind[1].buffer = &res_type;
        bind[1].buffer_length = sizeof(res_type);

        char res_exist = '\0';
        bind[2].buffer_type = MYSQL_TYPE_STRING;
        bind[2].buffer = &res_exist;
        bind[2].buffer_length = sizeof(res_exist);
        ret = mysql_stmt_bind_result(stmt, res_bind);

        ret = mysql_stmt_store_result(stmt);

        // 由于用户id,路径名,文件名是联合唯一约束,因此查询结果一定不超过一个
        ret = mysql_stmt_fetch(stmt);
        if (ret == 1 || ret == MYSQL_NO_DATA) {
            fprintf(stderr, "%s", mysql_error(mysql));
            retval = 0;
        }
        else if(res_exist == '0'){
            retval = -retval;
        }

        if (type != NULL) {
            *type = res_type;
        }
        // type为'D'时retval已完成赋值,直接返回即可
    }
    return retval;
}

char* getPwd(MYSQL* mysql, int pwdid) {}


char** findchild(MYSQL* mysql, int pwdid){
    int idx = 0;
    char** family = (char**)calloc(MAX_CHILD_CHARACTER * 4, sizeof(char*));
    if (family == NULL) {
        log_error("malloc: %s", strerror(errno));
        error(1, errno, "malloc");
    }

    const char* qurey_str = "select name from nb_vftable where p_id = ";
    char str[16] = {0};
    sprintf(str, "%d", pwdid);
    char query[1024] = {0};
    sprintf(query,"%s%s", qurey_str, str);

    int ret = mysql_query(mysql, query);
    if (ret != 0) {
        log_error("mysql_query: %s", strerror(errno));
        error(1, errno, "findchild: mysql_query");  
    } 

    MYSQL_RES* result = mysql_store_result(mysql);
    if (result) {
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(result)) != NULL) {
            family[idx] = (char*)calloc(MAX_CHILD_CHARACTER * 2, sizeof(char));
            if (family[idx] == NULL) {
                while (idx > 0) {
                    free(family[--idx]);
                }
                free(family);
                mysql_free_result(result);
                log_error("findchild malloc: %s", strerror(errno));
                error(1, errno, "findchild malloc");
            }
            strcpy(family[idx], *row);
            //printf("%s\n", *row);
            row++;
            idx++;
        }
    }
    family[idx] = NULL;
    mysql_free_result(result);
    return family;
}

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

int getFileInfo(MYSQL* mysql, int pwdid, char* f_hash, off_t* f_size, off_t* c_size){
    char sql[100] = {0};
    sprintf(sql, "%s", "SELECT f_hash, f_size, c_size FROM nb_vftable WHERE id = pwdid");
    mysql_query(mysql, sql);
    MYSQL_RES* res = mysql_store_result(mysql);
    MYSQL_ROW row = mysql_fetch_row(res);
    strcpy(f_hash, row[0]);
    if(f_size){
        *f_size = atol(row[1]);
    }
    if(c_size){
        *c_size = atol(row[2]);
    }
    mysql_free_result(res);
    return 0;
}

//传入文件哈希值,通过传参返回文件大小和现存最大文件大小
int localFile(MYSQL* mysql, char* f_hash, off_t* f_size, off_t* c_size) {
    char sql[] = "SELECT f_size, c_szie FROM nb_vftable WHERE f_hash = ?";
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    int ret = mysql_stmt_prepare(stmt, sql, strlen(sql));
    MYSQL_BIND bind;
    bzero(&bind, sizeof(bind));
    bind.buffer_type = MYSQL_TYPE_STRING;
    bind.buffer = f_hash;
    int buf_len = 16;
    bind.length = &buf_len;
    bind.is_null = 0;
    ret = mysql_stmt_bind_param(stmt, &bind);
    ret = mysql_stmt_execute(stmt);
    MYSQL_RES* res = mysql_stmt_result_metadata(stmt);

    MYSQL_BIND res_bind[2];
    res_bind[0].buffer_type = MYSQL_TYPE_LONGLONG;
    res_bind[0].buffer = f_size;
    res_bind[0].buffer_length = sizeof(off_t);

    off_t max_size = 0;
    res_bind[1].buffer_type = MYSQL_TYPE_LONGLONG;
    res_bind[1].buffer = max_size;
    res_bind[1].buffer_length = sizeof(off_t);

    ret = mysql_stmt_bind_result(stmt, res_bind);

    ret = mysql_stmt_store_result(stmt);

    *f_size = 0;
    *c_size = 0;
    for(;;){
        ret = mysql_stmt_fetch(stmt);
        if (ret == 1 || ret == MYSQL_NO_DATA) {
            break;
        }
        *c_size = max_size > *c_size ? max_size : *c_size;
    }
    mysql_free_result(res);
    mysql_stmt_free_result(stmt);
    return 0;
}

//想修改的就传入指针,不想更改的就传NULL
int updateRecord(MYSQL* mysql, int pwdid, const int* p_id, const int* u_id, const char* f_hash, const char* type, const off_t* f_size, const off_t* c_size, const char* exist){
    int i = 0;
    char sql[256] = "UPDATE nb_vftable SET ";
    if(p_id){
        char addition[100] = "p_id = ";
        if(i++ == 0){
            sprintf(sql, "%s%s%d ", sql, addition, *p_id);
        }
        else{
            sprintf(sql, "%s, %s%d ", sql, addition, *p_id);
        }
    }
    if(u_id){
        char addition[100] = "u_id = ";
        if(i++ == 0){
            sprintf(sql, "%s%s%d ", sql, addition, *u_id);
        }
        else{
            sprintf(sql, "%s, %s%d ", sql, addition, *u_id);
        }
    }
    if(f_hash){
        char addition[100] = "f_hash = ";
        if(i++ == 0){
            sprintf(sql, "%s%s'%s' ", sql, addition, f_hash);
        }
        else{
            sprintf(sql, "%s, %s'%s' ", sql, addition, f_hash);
        }
    }
    if(type){
        char addition[100] = "type = ";
        if(i++ == 0){
            sprintf(sql, "%s%s'%c' ", sql, addition, *type);
        }
        else{
            sprintf(sql, "%s, %s'%c' ", sql, addition, *type);
        }
    }
    if(f_size){
        char addition[100] = "f_size = ";
        if(i++ == 0){
            sprintf(sql, "%s%s%ld ", sql, addition, *f_size);
        }
        else{
            sprintf(sql, "%s, %s%ld ", sql, addition, *f_size);
        }
    }
    if(c_size){
        char addition[100] = "c_size = ";
        if(i++ == 0){
            sprintf(sql, "%s%s%ld ", sql, addition, *c_size);
        }
        else{
            sprintf(sql, "%s, %s%ld ", sql, addition, *c_size);
        }
    }
    if(exist){
        char addition[100] = "exist = ";
        if(i++ == 0){
            sprintf(sql, "%s%s'%c' ", sql, addition, *exist);
        }
        else{
            sprintf(sql, "%s, %s'%c' ", sql, addition, *exist);
        }
    }
    sprintf(sql, "%s%s%d", sql, "WHERE id = ", pwdid);
}