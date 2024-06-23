#include "../include/auth.h"

#define STR_LEN 8
#define MAX_TOKEN_SIZE 512

static const char KEY[] = "YoUR sUpEr S3krEt 1337 HMAC kEy HeRE";

int makeToken(char* token, int uid) {
    char* jwt;
    size_t jwt_length;

    struct l8w8jwt_encoding_params params;
    l8w8jwt_encoding_params_init(&params);

    params.alg = L8W8JWT_ALG_HS512;

    char user[16] = {0};
    sprintf(user, "nbuser %d", uid);
    params.iss = "NewBee Netdisk";  // 该 jwt 签发者
    params.sub = user;              // 该 jwt 面向的用户
    params.aud = "NewBee Client";   // 接收该jwt的一方

    /* Set to expire after 10 minutes (600 seconds). */
    // unix 时间戳
    params.iat = l8w8jwt_time(NULL);  // 什么时候签发的
    // 设置为 30 分钟过期
    params.exp = l8w8jwt_time(NULL) + 1800;  // 什么时候过期

    params.secret_key = (unsigned char*)KEY;
    params.secret_key_length = strlen((const char*)params.secret_key);

    params.out = &jwt;
    params.out_length = &jwt_length;

    int r = l8w8jwt_encode(&params);

    // printf("\n l8w8jwt HS512 token: %s \n",
    //        r == L8W8JWT_SUCCESS ? jwt : " (encoding failure) ");

    bzero(token, MAX_TOKEN_SIZE);
    strcpy(token, jwt);

    /* Always free the output jwt string! */
    l8w8jwt_free(jwt);

    return 0;
}

// 成功返回 0， 失败返回 1
int checkToken(char* token, int uid) {
    log_info("Token Checking ... \n%s", token);

    char* jwt = token;

    struct l8w8jwt_decoding_params params;
    l8w8jwt_decoding_params_init(&params);

    params.alg = L8W8JWT_ALG_HS512;

    params.jwt = jwt;
    params.jwt_length = strlen(jwt);

    params.verification_key = (unsigned char*)KEY;
    params.verification_key_length = strlen(KEY);

    /*
     * Not providing params.validate_iss_length makes it use strlen()
     * Only do this when using properly NUL-terminated C-strings!
     */
    char user[16] = {0};
    sprintf(user, "nbuser %d", uid);
    params.validate_iss = "NewBee Netdisk";
    params.validate_sub = user;

    params.validate_exp = 1;
    params.exp_tolerance_seconds = 60;

    params.validate_iat = 1;
    params.iat_tolerance_seconds = 60;

    enum l8w8jwt_validation_result validation_result;

    int decode_result = l8w8jwt_decode(&params, &validation_result, NULL, NULL);

    if (decode_result == L8W8JWT_SUCCESS &&
        validation_result == L8W8JWT_VALID) {
        log_info("NewBee HS512 token validation successful!");
        return 0;
    } else {
        log_warn("NewBee HS512 token validation failed!");
        return 1;
    }
}

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
    // 取出 salt, i 记录密码字符下标，j记录$出现次数
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
        cryptpasswd[lengths[0]] = '\0';  //  NULL for safe
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

void loginCheck1(Task* task) {
    log_debug("loginCheck1 start");
    log_info("user to login: [%s]", task->args[1]);

    char* username = task->args[1];

    // 0：成功，1：失败
    int status_code = 0;
    MYSQL* pconn = getDBConnection(task->dbpool);
    int exist = userExist(pconn, username);
    log_info("[%s] exist = [%d]", task->args[1], exist);

    if (exist == 0) {
        releaseDBConnection(task->dbpool, pconn);
        // 用户不存在
        status_code = 1;
        sendn(task->fd, &status_code, sizeof(int));
        return;
    }

    // 用户存在
    sendn(task->fd, &status_code, sizeof(int));

    // 获取 uid
    int uid = getUserIDByUsername(pconn, username);

    // 查询 cryptpasswd
    char* cryptpasswd = getCryptpasswdByUID(pconn, uid);
    releaseDBConnection(task->dbpool, pconn);

    // 提取 salt
    char salt[16] = {0};
    getSaltByCryptPasswd(salt, cryptpasswd);
    free(cryptpasswd);

    // 发送 salt
    int salt_len = strlen(salt);
    sendn(task->fd, &salt_len, sizeof(int));
    sendn(task->fd, salt, salt_len);

    // 更新本地 user_table
    // 如果用户没到 check2，会在 say goodbye 时处理
    task->u_table[task->fd] = uid;

    log_debug("loginCheck1 end");
    return;
}

void loginCheck2(Task* task) {
    log_debug("loginCheck2 start");

    // args[1] = u_cryptpasswd
    char* u_cryptpasswd = task->args[1];
    int uid = task->u_table[task->fd];

    // 查询数据库中的 cryptpasswd
    MYSQL* pconn = getDBConnection(task->dbpool);
    char* cryptpasswd = getCryptpasswdByUID(pconn, uid);
    releaseDBConnection(task->dbpool, pconn);

    int status_code = 0;
    if (strcmp(u_cryptpasswd, cryptpasswd) == 0) {
        // 登录成功
        sendn(task->fd, &status_code, sizeof(int));
        // 获取用户上一次的工作目录
        char cwd[1024] = {0};
        MYSQL* pconn = getDBConnection(task->dbpool);
        int pwdid = getPwdId(pconn, uid);
        getPwd(pconn, pwdid, cwd, sizeof(cwd));
        releaseDBConnection(task->dbpool, pconn);
        // 发送给客户端
        int cwd_len = strlen(cwd);
        sendn(task->fd, &cwd_len, sizeof(int));
        sendn(task->fd, cwd, cwd_len);

        log_info("[uid=%d] login successfully", uid);
    } else {
        // 登录失败，密码错误
        status_code = 1;
        sendn(task->fd, &status_code, sizeof(int));
        log_warn("[%d] login failed", uid);
    }

    log_debug("loginCheck2 end");
    return;
}

void regCheck1(Task* task) {
    log_debug("regCheck1 start");
    log_info("user to register: [%s]", task->args[1]);

    char* username = task->args[1];
    // 查数据库，用户名是否可用
    // 0: 用户名可用, 1: 用户名已存在
    int status_code = 0;
    MYSQL* pconn = getDBConnection(task->dbpool);
    if (userExist(pconn, username)) {
        releaseDBConnection(task->dbpool, pconn);
        status_code = 1;
        sendn(task->fd, &status_code, sizeof(int));
        return;
    }
    releaseDBConnection(task->dbpool, pconn);

    // 可以注册
    sendn(task->fd, &status_code, sizeof(int));
    // 生成 salt
    char* salt = generateSalt();
    // 发送 salt
    int salt_len = strlen(salt);
    sendn(task->fd, &salt_len, sizeof(int));
    sendn(task->fd, salt, salt_len);
    free(salt);

    log_debug("regCheck1 end");

    return;
}

void regCheck2(Task* task) {
    log_debug("regCheck2 start");

    // args[1] = username
    // args[2] = cryptpasswd

    char* username = task->args[1];
    char* cryptpasswd = task->args[2];

    MYSQL* pconn = getDBConnection(task->dbpool);

    // 插入用户记录到 nb_usertable
    long long pwdid = 0;
    int uid = userInsert(pconn, username, cryptpasswd, pwdid);

    // 插入用户目录记录到 nb_vftable
    pwdid = insertRecord(pconn, -1, uid, NULL, "home", "/home", 'd', NULL, NULL,
                         '1');
    if (pwdid == -1) {
        log_error("insertRecord failed");
        exit(EXIT_FAILURE);
    }
    char pwdid_str[64] = {0};
    sprintf(pwdid_str, "%lld", pwdid);

    // 更新用户的 pwdid
    int err = userUpdate(pconn, uid, "pwdid", pwdid_str);
    if (err) {
        log_error("userUpdate failed");
        exit(EXIT_FAILURE);
    }

    releaseDBConnection(task->dbpool, pconn);

    // 0: 注册成功
    int status_code = 0;
    sendn(task->fd, &status_code, sizeof(int));
    log_info("[%s] register successfully", username);

    log_debug("regCheck2 end");
    return;
}
