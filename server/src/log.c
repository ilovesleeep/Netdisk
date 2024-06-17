#include "log.h"
#define MAX_CALLBACKS 32
#define LOG_USE_COLOR 1
#define MAXLINE 1024

pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    log_LogFn fn;
    void* udata;
    int level;
} CallBack;

static struct {
    void* udata;
    log_LockFn lock;
    int level;
    bool quiet;                         // 静默，则log不输出到stdout
    CallBack callbacks[MAX_CALLBACKS];
} L;

static const char *level_strings[] = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};

#ifdef LOG_USE_COLOR
static const char *level_colors[] = {"\x1b[94m", "\x1b[36m", "\x1b[32m", "\x1b[33m", "\x1b[31m", "\x1b[35m"};
#endif

// 输出到标准输出,不用考虑
static void stdout_callback(Log_Event *ev) {
    char buf[16];
    buf[strftime(buf, sizeof(buf), "%H:%M:%S", ev->time)] = '\0';
    #ifdef LOG_USE_COLOR
        fprintf(ev->udata, "%s %s%-5s\x1b[0m \x1b[90m%s:%d:\x1b[0m ", buf, level_colors[ev->level], level_strings[ev->level], ev->file, ev->line);
    #else
        fprintf(ev->udata, "%s %-5s %s:%d: ", buf, level_strings[ev->level], ev->file, ev->line);
    #endif
    vfprintf(ev->udata, ev->fmt, ev->ap);
    fprintf(ev->udata, "\n");
    fflush(ev->udata);
}

// 输出到文件
static void file_callback(Log_Event *ev) {
      char buf[64];
      buf[strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", ev->time)] = '\0';
      fprintf(ev->udata, "%s %-5s %s:%d: ", buf, level_strings[ev->level], ev->file, ev->line);
        
      vfprintf(ev->udata, ev->fmt, ev->ap);
      fprintf(ev->udata, "\n"); // 这里做了换行
      fflush(ev->udata);
}

// static void lock(void)   { 
//     if (L.lock) { 
//         // lock不为空，则说明已经为它赋值了一个函数地址
//         // lock为空，表示没有设置锁定函数
//         L.lock(true, L.udata); 
//     }
// }

// static void unlock(void) {
//     if (L.lock) { 
//         L.lock(false, L.udata); 
//     }
// }

// 返回日志级别的字符串表示。
const char* log_level_string(int level) { 
    return level_strings[level];
}

// 设置锁函数
void log_set_lock(log_LockFn fn, void *udata) {
    L.lock = fn;
    L.udata = udata;
}

// 自定义锁函数
void lock_function(bool lock, void *udata) {
    if (lock) {
        pthread_mutex_lock(&log_mutex);
    } else {
        pthread_mutex_unlock(&log_mutex);
    }
}

// 设置日志级别
void log_set_level(int level) {
    L.level = level;
}

// 设置静默模式
void log_set_quiet(bool enable) {
    L.quiet = enable;
}

// 添加回调函数
int log_add_callback(log_LogFn fn, void *udata, int level) { // 添加回调函数
    for (int i = 0; i < MAX_CALLBACKS; i++) {
        if (!L.callbacks[i].fn) { // 为空执行 
            L.callbacks[i] = (CallBack) { fn, udata, level };
            return 0;
        }
    }
    return -1;
}

// 添加文件回调
int log_add_fp(FILE *fp, int level) {
    return log_add_callback(file_callback, fp, level);
}

// 初始化日志事件的时间戳和用户数据
static void init_event(Log_Event *ev, void *udata) {
    if (!ev->time) { // ev时间没初始化
        time_t t = time(NULL);
        ev->time = localtime(&t);
    }
    ev->udata = udata;
}

// 创建 log_Event 结构体。
// 锁定资源。
// 如果不在静默模式且日志级别足够高，调用 stdout_callback 输出日志到标准输出。
// 遍历回调函数数组，调用每个回调函数。
// 解锁资源。
void log_log(int level, const char *file, int line, const char *fmt, ...) {
    Log_Event ev = {
        .fmt   = fmt,
        .file  = file,
        .line  = line,
        .level = level,
    };

    // lock();
    pthread_mutex_lock(&log_mutex);

    // 若静默则stdout没用了
    if (!L.quiet && level >= L.level) { // 不为静默, 且输出大于等于level
        init_event(&ev, stderr);
        va_start(ev.ap, fmt);
        stdout_callback(&ev);
        va_end(ev.ap);
    }

    for (int i = 0; i < MAX_CALLBACKS && L.callbacks[i].fn; i++) {
        CallBack *cb = &L.callbacks[i];
        if (level >= cb->level) { // 调用大于
            init_event(&ev, cb->udata);
            va_start(ev.ap, fmt);
            cb->fn(&ev);
            va_end(ev.ap);
        }
    }

    //unlock();
    pthread_mutex_unlock(&log_mutex);
}

// 将字符串映射到枚举类型
static LogLevel getLogLevelFromString(const char* token) {
    if (strcmp(token, "LOG_TRACE") == 0) {
        return LOG_TRACE;
    } else if (strcmp(token, "LOG_DEBUG") == 0) {
        return LOG_DEBUG;
    } else if (strcmp(token, "LOG_INFO") == 0) {
        return LOG_INFO;
    } else if (strcmp(token, "LOG_WARN") == 0) {
        return LOG_WARN;
    } else if (strcmp(token, "LOG_ERROR") == 0) {
        return LOG_ERROR;
    } else if (strcmp(token, "LOG_FATAL") == 0) {
        return LOG_FATAL;
    } else {
        return LOG_UNKNOW;
    }
}

static void parseConfig(LogConfig* conf) {
    int fd = open("../config/log.conf", O_RDONLY);
    if (fd == -1) {
        error(1, errno, "open log.conf");
    }

    char buf[MAXLINE];
    read(fd, buf, MAXLINE);

    char* token = strtok(buf, "= \r\n\t");
    while (token != NULL) {
        if (strcmp(token, "dirname") == 0) {
            token = strtok(NULL, "= \r\n\t"); // dirname

            // 以日期作为日志名
            time_t now = time(NULL);
            struct tm* local_time = localtime(&now);
            char filename[64];
            strftime(filename, SIZE(filename), "server_log_%Y_%m_%d.txt", local_time);
            
            // 初始化日志文件名
            char fullpath[128];
            bzero(fullpath, sizeof(fullpath));

            // 拼接
            strcpy(fullpath, token);
            strncat(fullpath, filename, sizeof(fullpath) - strlen(fullpath) - 1);

            printf("[INFO] Set filename = %s\n", fullpath);

            conf->filename = fullpath;
        } else if (strcmp(token, "quiet") == 0) {
            token = strtok(NULL, "= \r\n\t");

            int tof = atoi(token); // tof:true or false
            if (tof == 1) {
                printf("[INFO] Set quiet = true\n");
            } else if (tof == 0) {
                printf("[INFO] Set quiet = false\n");
            } else { // 出错
                error(1, 0, "quiet number is not 1 or 0");
            }

            conf->quiet = tof;
        } else if (strcmp(token, "level") == 0) {
            token = strtok(NULL, "= \r\n\t");

            conf->level = getLogLevelFromString(token);
            if (conf->level == LOG_UNKNOW) {
                error(1, 0, "LogLevel Unknow");
            }
            printf("[INFO] Set level = %s\n", token);
        }

        token = strtok(NULL, "= \r\n\t");
    }

    close(fd);
}

void initLog() {
    LogConfig conf = {"log.txt", LOG_UNKNOW, 1};
    parseConfig(&conf);
    // 打开文件用于追加，若文件不存在则创建
    FILE* fp = fopen(conf.filename, "a");
    if (fp == NULL) {
        error(1, 0, "create log file failed.");
    }
    log_add_fp(fp, conf.level);
    log_set_quiet(conf.quiet);
}



