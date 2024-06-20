#include "../include/auth.h"

#define STR_LEN 8

char* generateSalt(void) {
    int salt_len = 3 + STR_LEN;                // 3 for "$6$"
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
    salt[salt_len] = '\0';

    return salt;
}

// char* getSalt();

char* getSaltByUID(MYSQL* pconn, int uid) {
    char query[256];
    snprintf(query, sizeof(query),
             "SELECT salt FROM nb_usertable WHERE id = %d", uid);

    int err = mysql_query(pconn, query);
    if (err) {
        error(1, 0, "[ERROR] mysql_query() failed: %s\n", mysql_error(pconn));
    }

    MYSQL_RES* res = mysql_store_result(pconn);
    if (res == NULL) {
        error(1, 0, "[ERROR] mysql_store_result() failed: %s\n",
              mysql_error(pconn));
    }

    MYSQL_ROW row;
    char* result_salt = NULL;

    if ((row = mysql_fetch_row(res))) {
        unsigned long* lengths = mysql_fetch_lengths(res);
        if (lengths == NULL) {
            error(1, 0, "[ERROR] mysql_fetch_lengths() failed: %s\n",
                  mysql_error(pconn));
        }

        result_salt = (char*)malloc(lengths[0] + 1);
        if (result_salt == NULL) {
            error(1, 0, "[ERROR] Memory allocation failed\n");
        }

        strncpy(result_salt, row[0], lengths[0]);
        result_salt[lengths[0]] = '\0';  // 添加 null 终止符
    }

    mysql_free_result(res);

    return result_salt;
}

char* getCryptpasswdByUID(MYSQL* pconn, int uid) {
    char query[256];
    snprintf(query, sizeof(query),
             "SELECT cryptpasswd FROM nb_usertable WHERE id = %d", uid);

    int err = mysql_query(pconn, query);
    if (err) {
        error(1, 0, "[ERROR] mysql_query() failed: %s\n", mysql_error(pconn));
    }

    MYSQL_RES* res = mysql_store_result(pconn);
    if (res == NULL) {
        error(1, 0, "[ERROR] mysql_store_result() failed: %s\n",
              mysql_error(pconn));
    }

    MYSQL_ROW row;
    char* cryptpasswd = NULL;

    if ((row = mysql_fetch_row(res))) {
        unsigned long* lengths = mysql_fetch_lengths(res);
        if (lengths == NULL) {
            error(1, 0, "[ERROR] mysql_fetch_lengths() failed: %s\n",
                  mysql_error(pconn));
        }

        cryptpasswd = (char*)malloc(lengths[0] + 1);
        if (cryptpasswd == NULL) {
            error(1, 0, "[ERROR] Memory allocation failed\n");
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
        error(1, 0, "[ERROR] mysql_stmt_init() failed\n");
    }

    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        log_error("[ERROR] mysql_stmt_prepare() failed: %s\n",
                  mysql_stmt_error(stmt));
        error(1, 0, "[ERROR] mysql_stmt_prepare() failed: %s\n",
              mysql_stmt_error(stmt));
    }

    MYSQL_BIND bind[1];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_VAR_STRING;
    bind[0].buffer = (char*)username;
    bind[0].buffer_length = strlen(username);

    if (mysql_stmt_bind_param(stmt, bind)) {
        log_error("[ERROR] mysql_stmt_bind_param() failed: %s\n",
                  mysql_stmt_error(stmt));
        error(1, 0, "[ERROR] mysql_stmt_bind_param() failed: %s\n",
              mysql_stmt_error(stmt));
    }

    if (mysql_stmt_execute(stmt)) {
        log_error("[ERROR] mysql_stmt_execute() failed: %s\n",
                  mysql_stmt_error(stmt));
        error(1, 0, "[ERROR] mysql_stmt_execute() failed: %s\n",
              mysql_stmt_error(stmt));
    }

    MYSQL_RES* res = mysql_stmt_result_metadata(stmt);
    if (res == NULL) {
        mysql_stmt_close(stmt);
        return false;
    }

    int user_id = 0;
    MYSQL_BIND res_bind[1];
    memset(res_bind, 0, sizeof(res_bind));

    res_bind[0].buffer_type = MYSQL_TYPE_LONG;
    res_bind[0].buffer = &user_id;
    res_bind[0].buffer_length = sizeof(int);

    if (mysql_stmt_bind_result(stmt, res_bind)) {
        log_error("[ERROR] mysql_stmt_bind_result() failed: %s\n",
                  mysql_stmt_error(stmt));
        error(1, 0, "[ERROR] mysql_stmt_bind_result() failed: %s\n",
              mysql_stmt_error(stmt));
    }

    if (mysql_stmt_fetch(stmt) != 0 &&
        mysql_stmt_fetch(stmt) != MYSQL_NO_DATA) {
        log_error("[ERROR] mysql_stmt_fetch() failed: %s\n",
                  mysql_stmt_error(stmt));
        error(1, 0, "[ERROR] mysql_stmt_fetch() failed: %s\n",
              mysql_stmt_error(stmt));
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
        error(1, 0, "[ERROR] mysql_stmt_init() failed\n");
    }

    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        error(1, 0, "[ERROR] mysql_stmt_prepare() failed: %s\n",
              mysql_stmt_error(stmt));
    }

    MYSQL_BIND bind[1];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_VAR_STRING;
    bind[0].buffer = (char*)username;
    bind[0].buffer_length = strlen(username);

    if (mysql_stmt_bind_param(stmt, bind)) {
        error(1, 0, "[ERROR] mysql_stmt_bind_param() failed: %s\n",
              mysql_stmt_error(stmt));
    }

    if (mysql_stmt_execute(stmt)) {
        error(1, 0, "[ERROR] mysql_stmt_execute() failed: %s\n",
              mysql_stmt_error(stmt));
    }

    MYSQL_RES* res = mysql_stmt_result_metadata(stmt);
    if (res == NULL) {
        mysql_stmt_close(stmt);
        return false;
    }

    int exists = 0;
    MYSQL_BIND res_bind[1];
    memset(res_bind, 0, sizeof(res_bind));

    res_bind[0].buffer_type = MYSQL_TYPE_LONG;
    res_bind[0].buffer = &exists;
    res_bind[0].buffer_length = sizeof(int);

    if (mysql_stmt_bind_result(stmt, res_bind)) {
        error(1, 0, "[ERROR] mysql_stmt_bind_result() failed: %s\n",
              mysql_stmt_error(stmt));
    }

    if (mysql_stmt_fetch(stmt) != 0 &&
        mysql_stmt_fetch(stmt) != MYSQL_NO_DATA) {
        error(1, 0, "[ERROR] mysql_stmt_fetch() failed: %s\n",
              mysql_stmt_error(stmt));
    }

    mysql_free_result(res);
    mysql_stmt_close(stmt);

    return exists > 0;
}

int userInsert(MYSQL* pconn, const char* username, const char* salt,
               const char* cryptpasswd, int pwdid) {
    const char* query =
        "INSERT INTO nb_usertable (username, salt, cryptpasswd, pwdid) VALUES "
        "(?, ?, ?, ?)";
    MYSQL_STMT* stmt = mysql_stmt_init(pconn);

    if (stmt == NULL) {
        log_error("mysql_stmt_init() failed");
    }

    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        log_error("[ERROR] mysql_stmt_prepare() failed: %s",
                  mysql_stmt_error(stmt));
    }

    MYSQL_BIND bind[4];
    bzero(bind, sizeof(bind));

    // 绑定参数
    bind[0].buffer_type = MYSQL_TYPE_VAR_STRING;
    bind[0].buffer = (char*)username;
    bind[0].is_null = 0;
    bind[0].buffer_length = strlen(username);

    bind[1].buffer_type = MYSQL_TYPE_VAR_STRING;
    bind[1].buffer = (char*)salt;
    bind[1].is_null = 0;
    bind[1].buffer_length = strlen(salt);

    bind[2].buffer_type = MYSQL_TYPE_VAR_STRING;
    bind[2].buffer = (char*)cryptpasswd;
    bind[2].is_null = 0;
    bind[2].buffer_length = strlen(cryptpasswd);

    bind[3].buffer_type = MYSQL_TYPE_LONG;
    bind[3].buffer = &pwdid;
    bind[3].is_null = 0;
    bind[3].length = NULL;

    if (mysql_stmt_bind_param(stmt, bind)) {
        error(1, 0, "[ERROR] mysql_stmt_bind_param() failed: %s\n",
              mysql_stmt_error(stmt));
    }

    if (mysql_stmt_execute(stmt)) {
        error(1, 0, "[ERROR] mysql_stmt_execute() failed: %s\n",
              mysql_stmt_error(stmt));
    }

    log_info("User '%s' inserted successfully.", username);
    mysql_stmt_close(stmt);

    int uid = mysql_insert_id(pconn);
    return uid;
}
