#ifndef __NB_LOG_H
#define __NB_LOG_H

#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "head.h"

#define LOG_VERSION "0.1.0"

typedef struct {
    va_list ap;        // 可变参数列表
    const char* fmt;   // 格式化字符串
    const char* file;  // 源文件名
    struct tm* time;   // 日志时间
    void* udata;       // 用户数据
    int line;          // 行号
    int level;         // 日志级别
} Log_Event;

typedef enum {
    LOG_TRACE,
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL,
    LOG_UNKNOW  // 用于判断参数
} LogLevel;

typedef struct {
    char filename[128];
    LogLevel level;
    bool quiet;
} LogConfig;

typedef void (*log_LogFn)(Log_Event* ev);
// 类似
// void log_LogFn(Log_Event* ev) {

// }
typedef void (*log_LockFn)(bool lock, void* udata);
// void log_LockFn(bool lock, void* udata){

// }

#define log_trace(...) log_log(LOG_TRACE, __FILE__, __LINE__, __VA_ARGS__)
#define log_debug(...) log_log(LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define log_info(...) log_log(LOG_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define log_warn(...) log_log(LOG_WARN, __FILE__, __LINE__, __VA_ARGS__)
#define log_error(...) log_log(LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define log_fatal(...) log_log(LOG_FATAL, __FILE__, __LINE__, __VA_ARGS__)

const char* log_level_string(int level);
// 可以让我们在log外面的具体业务中定义自己的锁函数，实现粒度小的锁
void log_set_lock(log_LockFn fn, void* udata);

void log_set_level(int level);
void log_set_quiet(bool enable);
int log_add_callback(log_LogFn fn, void* udata, int level);
int log_add_fp(FILE* fp, int level);

void log_log(int level, const char* file, int line, const char* fmt, ...);

void initLog(void);

#endif
