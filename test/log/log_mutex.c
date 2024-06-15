#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>

// 全局锁，用于保护对日志文件的访问
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

// 文件描述符，用于指向日志文件
static int log_fd = -1;

// 初始化日志文件
void init_log_file(const char* filename) {
    // 检查文件描述符是否已经打开
    if (log_fd == -1) {
        // 打开或创建日志文件，追加模式
        log_fd =
            open(filename, O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR);
        if (log_fd == -1) {
            // 处理错误
            // ...
        }
    }
}

// 日志输出函数，使用write系统调用来写入日志
void log_output(const char* message) {
    if (log_fd != -1) {
        pthread_mutex_lock(&log_mutex);           // 加锁
        write(log_fd, message, strlen(message));  // 写入日志
        pthread_mutex_unlock(&log_mutex);         // 解锁
    }
}

// 在程序结束时关闭日志文件
void close_log_file() {
    if (log_fd != -1) {
        close(log_fd);
        log_fd = -1;
    }
}
