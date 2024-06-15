#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

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
            perror("Error opening log file");
            exit(EXIT_FAILURE);
        }
    }
}

// 日志输出函数，使用write系统调用来写入日志
void log_output(LogLevel level, const char* format, ...) {
    if (level >= current_log_level && log_fd != -1) {
        va_list args;
        va_start(args, format);

        // 构建完整的日志消息，包括前缀和实际内容
        char prefix[20];
        switch (level) {
            case LOG_DEBUG:
                snprintf(prefix, sizeof(prefix), "DEBUG: ");
                break;
            case LOG_INFO:
                snprintf(prefix, sizeof(prefix), "INFO: ");
                break;
            case LOG_WARNING:
                snprintf(prefix, sizeof(prefix), "WARNING: ");
                break;
            case LOG_ERROR:
                snprintf(prefix, sizeof(prefix), "ERROR: ");
                break;
        }

        // 将前缀写入日志文件
        write(log_fd, prefix, strlen(prefix));

        // 将格式化后的日志信息写入日志文件
        char buffer[4096];  // 假设最大日志长度不超过4096字节
        vsnprintf(buffer, sizeof(buffer), format, args);
        write(log_fd, buffer, strlen(buffer));

        va_end(args);
    }
}

// 在程序结束时关闭日志文件
void close_log_file() {
    if (log_fd != -1) {
        close(log_fd);
        log_fd = -1;
    }
}
