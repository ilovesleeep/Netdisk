#include "../include/auth.h"

#define STR_LEN 8

char* generateSalt(void) {
    int salt_len = 4 + STR_LEN;                // 4 for "$6$...$"
    char* salt = (char*)malloc(salt_len + 1);  // +1 for '\0'
    if (salt == NULL) {
        log_error("Memory allocation failed");
        error(1, 0, "Memory allocation failed");
    }

    int i, flag;
    srand(time(NULL));
    strcpy(salt, "$6$");
    for (i = 3; i < salt_len; ++i) {
        flag = rand() % 3;
        switch (flag) {
            case 0:
                salt[i] = rand() % 26 + 'a';
                break;
            case 1:
                salt[i] = rand() % 26 + 'A';
                break;
            case 2:
                salt[i] = rand() % 10 + '0';
                break;
        }
    }
    salt[salt_len - 1] = '$';
    salt[salt_len] = '\0';

    return salt;
}

void getSaltByCryptPasswd(char* salt, char* cryptpasswd) {
    int i, j;
    // 取出salt,i 记录密码字符下标，j记录$出现次数
    for (i = 0, j = 0; cryptpasswd[i] && j != 3; ++i) {
        if (cryptpasswd[i] == '$') ++j;
    }
    strncpy(salt, cryptpasswd, i);
}

char* getCryptpasswdByUID(MYSQL* pconn, int uid) {
    char query[256];
    snprintf(query, sizeof(query),
             "SELECT cryptpasswd FROM nb_usertable WHERE id = %d", uid);

    // 开始事务
    mysql_query(pconn, "START TRANSACTION");
    int err = mysql_query(pconn, query);
    if (err) {
        // 出错，回滚事务
        mysql_query(pconn, "ROLLBACK");
        log_error("[ERROR] mysql_query() failed: %s", mysql_error(pconn));
        exit(EXIT_FAILURE);
    }

    MYSQL_RES* res = mysql_store_result(pconn);
    if (res == NULL) {
        log_error("[ERROR] mysql_store_result() failed: %s",
                  mysql_error(pconn));
        exit(EXIT_FAILURE);
    }
    // 提交事务
    mysql_query(pconn, "COMMIT");

    MYSQL_ROW row;
    char* cryptpasswd = NULL;

    if ((row = mysql_fetch_row(res))) {
        unsigned long* lengths = mysql_fetch_lengths(res);
        if (lengths == NULL) {
            log_error("[ERROR] mysql_fetch_lengths() failed: %s",
                      mysql_error(pconn));
            exit(EXIT_FAILURE);
        }

        cryptpasswd = (char*)malloc(lengths[0] + 1);
        if (cryptpasswd == NULL) {
            log_error("[ERROR] Memory allocation failed");
            exit(EXIT_FAILURE);
        }

        strncpy(cryptpasswd, row[0], lengths[0]);
        cryptpasswd[lengths[0]] = '\0';  // 添加 null 终止符
    }

    mysql_free_result(res);

    return cryptpasswd;
}

int getUserIDByUsername(MYSQL* pconn, const char* username) {
    const char* query =
        "SELECT id FROM nb_usertable WHERE username = ? LIMIT 1";
    MYSQL_STMT* stmt = mysql_stmt_init(pconn);

    if (stmt == NULL) {
        log_error("mysql_stmt_init() failed");
        exit(EXIT_FAILURE);
    }

    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        log_error("[ERROR] mysql_stmt_prepare() failed: %s",
                  mysql_stmt_error(stmt));
        exit(EXIT_FAILURE);
    }

    MYSQL_BIND bind[1];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_VAR_STRING;
    bind[0].buffer = (char*)username;
    bind[0].buffer_length = strlen(username);

    if (mysql_stmt_bind_param(stmt, bind)) {
        log_error("[ERROR] mysql_stmt_bind_param() failed: %s",
                  mysql_stmt_error(stmt));
        exit(EXIT_FAILURE);
    }

    // 开始事务
    mysql_query(pconn, "START TRANSACTION");
    if (mysql_stmt_execute(stmt)) {
        // 出错，回滚事务
        mysql_query(pconn, "ROLLBACK");
        log_error("[ERROR] mysql_stmt_execute() failed: %s",
                  mysql_stmt_error(stmt));
        exit(EXIT_FAILURE);
    }

    MYSQL_RES* res = mysql_stmt_result_metadata(stmt);
    if (res == NULL) {
        log_error("stmt result");
        mysql_stmt_close(stmt);
        exit(EXIT_FAILURE);
    }
    // 提交事务
    mysql_query(pconn, "COMMIT");

    int user_id = 0;
    MYSQL_BIND res_bind[1];
    memset(res_bind, 0, sizeof(res_bind));

    res_bind[0].buffer_type = MYSQL_TYPE_LONG;
    res_bind[0].buffer = &user_id;
    res_bind[0].buffer_length = sizeof(int);

    if (mysql_stmt_bind_result(stmt, res_bind)) {
        log_error("[ERROR] mysql_stmt_bind_result() failed: %s",
                  mysql_stmt_error(stmt));
        exit(EXIT_FAILURE);
    }

    if (mysql_stmt_fetch(stmt) != 0 &&
        mysql_stmt_fetch(stmt) != MYSQL_NO_DATA) {
        log_error("[ERROR] mysql_stmt_fetch() failed: %s",
                  mysql_stmt_error(stmt));
        exit(EXIT_FAILURE);
    }

    mysql_free_result(res);
    mysql_stmt_close(stmt);

    return user_id;
}

bool userExist(MYSQL* pconn, const char* username) {
    const char* query = "SELECT 1 FROM nb_usertable WHERE username = ? LIMIT 1";
    MYSQL_STMT* stmt = mysql_stmt_init(pconn);

    if (stmt == NULL) {
        log_error("mysql_stmt_init() failed");
        exit(EXIT_FAILURE);
    }

    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        log_error("[ERROR] mysql_stmt_prepare() failed: %s",
                  mysql_stmt_error(stmt));
        exit(EXIT_FAILURE);
    }

    MYSQL_BIND bind[1];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_VAR_STRING;
    bind[0].buffer = (char*)username;
    bind[0].buffer_length = strlen(username);

    if (mysql_stmt_bind_param(stmt, bind)) {
        log_error("[ERROR] mysql_stmt_bind_param() failed: %s",
                  mysql_stmt_error(stmt));
        exit(EXIT_FAILURE);
    }

    // 开始事务
    mysql_query(pconn, "START TRANSACTION");
    if (mysql_stmt_execute(stmt)) {
        // 出错，回滚事务
        mysql_query(pconn, "ROLLBACK");
        log_error("[ERROR] mysql_stmt_execute() failed: %s",
                  mysql_stmt_error(stmt));
        exit(EXIT_FAILURE);
    }

    MYSQL_RES* res = mysql_stmt_result_metadata(stmt);
    if (res == NULL) {
        log_error("stmt result");
        mysql_stmt_close(stmt);
        exit(EXIT_FAILURE);
    }
    // 提交事务
    mysql_query(pconn, "COMMIT");

    int exists = 0;
    MYSQL_BIND res_bind[1];
    memset(res_bind, 0, sizeof(res_bind));

    res_bind[0].buffer_type = MYSQL_TYPE_LONG;
    res_bind[0].buffer = &exists;
    res_bind[0].buffer_length = sizeof(int);

    if (mysql_stmt_bind_result(stmt, res_bind)) {
        log_error("[ERROR] mysql_stmt_bind_result() failed: %s",
                  mysql_stmt_error(stmt));
        exit(EXIT_FAILURE);
    }

    if (mysql_stmt_fetch(stmt) != 0 &&
        mysql_stmt_fetch(stmt) != MYSQL_NO_DATA) {
        log_error("[ERROR] mysql_stmt_fetch() failed: %s",
                  mysql_stmt_error(stmt));
        exit(EXIT_FAILURE);
    }

    mysql_free_result(res);
    mysql_stmt_close(stmt);

    return exists > 0;
}

int userInsert(MYSQL* pconn, const char* username, const char* cryptpasswd,
               long long pwdid) {
    const char* query =
        "INSERT INTO nb_usertable (username, cryptpasswd, pwdid) VALUES "
        "(?, ?, ?)";
    MYSQL_STMT* stmt = mysql_stmt_init(pconn);

    if (stmt == NULL) {
        log_error("mysql_stmt_init() failed");
        exit(EXIT_FAILURE);
    }

    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        log_error("[ERROR] mysql_stmt_prepare() failed: %s",
                  mysql_stmt_error(stmt));
        exit(EXIT_FAILURE);
    }

    MYSQL_BIND bind[3];
    bzero(bind, sizeof(bind));

    // 绑定参数
    bind[0].buffer_type = MYSQL_TYPE_VAR_STRING;
    bind[0].buffer = (char*)username;
    bind[0].is_null = 0;
    bind[0].buffer_length = strlen(username);

    bind[1].buffer_type = MYSQL_TYPE_VAR_STRING;
    bind[1].buffer = (char*)cryptpasswd;
    bind[1].is_null = 0;
    bind[1].buffer_length = strlen(cryptpasswd);

    bind[2].buffer_type = MYSQL_TYPE_LONG;
    bind[2].buffer = &pwdid;
    bind[2].is_null = 0;
    bind[2].length = NULL;

    if (mysql_stmt_bind_param(stmt, bind)) {
        log_error("[ERROR] mysql_stmt_bind_param() failed: %s",
                  mysql_stmt_error(stmt));
        exit(EXIT_FAILURE);
    }

    // 开始事务
    mysql_query(pconn, "START TRANSACTION");
    if (mysql_stmt_execute(stmt)) {
        // 出错，回滚事务
        mysql_query(pconn, "ROLLBACK");
        log_error("[ERROR] mysql_stmt_execute() failed: %s",
                  mysql_stmt_error(stmt));
        exit(EXIT_FAILURE);
    }
    int uid = mysql_insert_id(pconn);
    // 提交事务
    mysql_query(pconn, "COMMIT");

    log_info("User [%s] inserted successfully.", username);
    mysql_stmt_close(stmt);

    return uid;
}

int userUpdate(MYSQL* pconn, int uid, const char* fieldname,
               const char* value) {
    char query[256];
    snprintf(query, sizeof(query),
             "UPDATE nb_usertable SET %s = '%s' WHERE id = %d", fieldname,
             value, uid);

    // 开始事务
    mysql_query(pconn, "START TRANSACTION");
    int err = mysql_query(pconn, query);
    if (err) {
        // 出错，回滚事务
        mysql_query(pconn, "ROLLBACK");
        log_error(mysql_error(pconn));
        return -1;
    }

    // 检查是否成功更新
    if (mysql_affected_rows(pconn) == 0) {
        // 出错，回滚事务
        mysql_query(pconn, "ROLLBACK");
        log_warn("No rows updated when update uid[%d] at field[%s]", uid,
                 fieldname);
        return 1;
    }
    // 提交事务
    mysql_query(pconn, "COMMIT");

    return 0;
}
