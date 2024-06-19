#ifndef __K_POOL_H
#define __K_POOL_H

#include "../include/hashtable.h"
#include <func.h>
#include <mysql/field_types.h>
#include <mysql/mysql.h>

typedef struct ConnNode ConnNode;

typedef struct ConnNode {
    MYSQL* pconn;
    struct ConnNode* pre;
    struct ConnNode* next;
} ConnNode;

typedef struct {
    const char* host;
    const char* user;
    const char* passwd;
    const char* db;

    ConnNode* head;
    ConnNode* tail;
    int curr_size;   // 当前连接数
    int init_size;   // 初始化容量，用于协助监视线程增加连接和删除连接
    int max_size;    // 队列容量
    int min_size;    // 最小连接数，小于这个数就会扩展容量

    pthread_mutex_t lock; // 用一把锁去使用队列
    pthread_cond_t cond;

}DBConnectionPool;


    
DBConnectionPool* initDBPool(int init_size, int max_size, int min_size, HashTable* ht);

MYSQL* getDBConnection(DBConnectionPool* dbpool);
void releaseDBConnection(DBConnectionPool* dbpool, MYSQL* pconn);
void expandDBPool(DBConnectionPool* dbpool, int add_size);
void shrinkDBPool(DBConnectionPool* dbpool);
void* monitorPool(void* arg);

void destroyDBPool(DBConnectionPool* dbpool);


#endif


