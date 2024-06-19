#include "../include/dbpool.h"

static void initDBPoolConfig(DBConnectionPool* dbpool, HashTable* ht) {
    dbpool->host = (const char*)find(ht, "host");
    dbpool->user = (const char*)find(ht, "user");
    dbpool->passwd = (const char*)find(ht, "passwd");
    dbpool->db = (const char*)find(ht, "db");
}

DBConnectionPool* initDBPool(int init_size, int max_size, int min_size, HashTable* ht) {
    DBConnectionPool* dbpool = (DBConnectionPool*)malloc(sizeof(DBConnectionPool));
    if (!dbpool) {
        perror("Failed to allocate memory for connection pool");
        exit(EXIT_FAILURE);
    }
    initDBPoolConfig(dbpool, ht);
    
    dbpool->head = NULL;
    dbpool->tail = NULL;
    dbpool->curr_size = 0; 
    dbpool->max_size = max_size;
    dbpool->min_size = min_size;
    dbpool->init_size = init_size;

    pthread_mutex_init(&dbpool->lock, NULL);
    pthread_cond_init(&dbpool->cond, NULL);

    // add init_size ConnNode
    for (int i = 0; i < init_size; ++i) {
        
        // node->pconn
        MYSQL* pconn = mysql_init(NULL);
        if (pconn == NULL) {
            fprintf(stderr, "mysql_init failed\n");
            exit(EXIT_FAILURE);
        }
        pconn = mysql_real_connect(pconn, dbpool->host, dbpool->user, dbpool->passwd, dbpool->db, 0, NULL, 0); 
        if (pconn == NULL) {
            fprintf(stderr, "mysql_real_connect failed\n");
            exit(EXIT_FAILURE);
        }


        // test
        // test


        ConnNode* node = (ConnNode*)malloc(sizeof(ConnNode));
        node->pconn = pconn;

        // node->tail,node->pre,dbpool->head,dbpool->tail
        node->next = NULL;
        node->pre = dbpool->tail;
        if (dbpool->tail) {
            dbpool->tail->next = node;
        } else { 
            // tail == NULL 说明dbpool没有元素 
            dbpool->head = node;
        }
        dbpool->tail = node;
        dbpool->curr_size++;

    }
    
    return dbpool;
}

MYSQL* getDBConnection(DBConnectionPool* dbpool) {
    // 出ConnNode
    pthread_mutex_lock(&dbpool->lock);

    while (dbpool->head == NULL) {
        pthread_cond_wait(&dbpool->cond, &dbpool->lock);
    } // 非空，有锁

    ConnNode* node = dbpool->head;
    MYSQL* pconn = node->pconn;

    dbpool->head = node->next;
    if (dbpool->head) {
        dbpool->head->pre = NULL;
    } else {
        // only one node
        dbpool->tail = NULL;
    }
    free(node);
    dbpool->curr_size--;
    pthread_mutex_unlock(&dbpool->lock);
    return pconn;
}

void releaseDBConnection(DBConnectionPool* dbpool, MYSQL* pconn) { 
    // 在动态增长中判断了增加的结点数量，即curr_size 永远不会大于 max_size
    pthread_mutex_lock(&dbpool->lock);

    ConnNode* node = (ConnNode*)malloc(sizeof(ConnNode));
    node->pconn = pconn;
    node->next = NULL;
    node->pre = dbpool->tail;
    if (dbpool->tail) {
        dbpool->tail->next = node;
    } else {
        dbpool->head = node;
    }

    dbpool->tail = node;
    dbpool->curr_size++;
    pthread_cond_signal(&dbpool->cond); // 池中有数据
    pthread_mutex_unlock(&dbpool->lock);
}

void expandDBPool(DBConnectionPool* dbpool, int add_size) {
    if (add_size + dbpool->init_size > dbpool->max_size) return;
    for (int i = 0; i < add_size; ++i) {
        /* 为什么不能改为动态增长，在连接池场景下需要大量的pop_front()
            势必会导致大量的数据从后往前移动，这样性能反而降低了
        */
        

        // node->pconn
        MYSQL* pconn = mysql_init(NULL);
        if (pconn == NULL) {
            fprintf(stderr, "mysql_init failed\n");
            exit(EXIT_FAILURE);
        }
        pconn = mysql_real_connect(pconn, dbpool->host, dbpool->user, dbpool->passwd, dbpool->db, 0, NULL, 0); 
        if (pconn == NULL) {
            fprintf(stderr, "mysql_real_connect failed\n");
            exit(EXIT_FAILURE);
        }
        ConnNode* node = (ConnNode*)malloc(sizeof(ConnNode));
        node->pconn = pconn;

        node->next = NULL;
        node->pre = dbpool->tail;
        if (dbpool->tail) {
            dbpool->tail->next = node;
        } else {
            dbpool->head = node;
        }
        dbpool->tail = node;
        dbpool->curr_size++;
    }
    dbpool->init_size += add_size;
}

void shrinkDBPool(DBConnectionPool* dbpool) {
    // 减到上界
    int upper_bound = dbpool->max_size - dbpool->max_size / 4;
    int diff = dbpool->curr_size - upper_bound;
    if (dbpool->init_size - diff <= 0) return;


    while (dbpool->curr_size >= upper_bound) { 
        ConnNode* node = dbpool->tail;
        dbpool->tail = node->pre;
        if (dbpool->tail) { // 一般都在这里
            dbpool->tail->next = NULL;
        } else {
            dbpool->head = NULL;
        }
        mysql_close(node->pconn);
        free(node);
        dbpool->curr_size--;
    } // 减到maxsize的3/4

    dbpool->init_size -= diff;
}

void* monitorPool(void* arg) {
    DBConnectionPool* dbpool = (DBConnectionPool*)arg;
    const int TIMEOUT = 30;
    const int upper_bound = dbpool->max_size - dbpool->max_size / 4; // 3/4 
    const int lower_bound = dbpool->min_size; 
    int greater_count = 0; // 计算大于上界的时间
    int lesser_count = 0; // 计算小于下界的时间

    while (1) {
        sleep(1);
        if (dbpool->curr_size > upper_bound) {
            greater_count++;
            lesser_count = 0;
        } else if (dbpool->curr_size < lower_bound) {
            lesser_count++;
            greater_count = 0;
        } else {
            greater_count = 0;
            lesser_count = 0;
        }


        if (greater_count >= TIMEOUT) {
            pthread_mutex_lock(&dbpool->lock);
            shrinkDBPool(dbpool); // 连接太多，要减少一点
            pthread_mutex_unlock(&dbpool->lock);
            greater_count = 0;
        } 

        if (lesser_count >= TIMEOUT) {
            pthread_mutex_lock(&dbpool->lock);
            // 连接太少，要加一点
            expandDBPool(dbpool, 2);
            pthread_mutex_unlock(&dbpool->lock);
            lesser_count = 0;
        }

    }
    return NULL;
}

void destroyDBPool(DBConnectionPool* dbpool) {
    pthread_mutex_lock(&dbpool->lock);

    while (dbpool->head) {
        ConnNode* node = dbpool->head;
        dbpool->head = node->next;
        mysql_close(node->pconn);
        free(node);
    }

    pthread_mutex_unlock(&dbpool->lock);
    pthread_mutex_destroy(&dbpool->lock);
    pthread_cond_destroy(&dbpool->cond);
    free(dbpool);
}




