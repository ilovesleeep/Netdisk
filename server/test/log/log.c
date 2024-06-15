#include "log.h"

LogLevel g_log_level = LOG_INFO;

void logPrint(LogLevel level, const char* format, ...) {
    if (level >= g_log_level) {  // 只在日志级别满足条件时输出
        va_list args;
        va_start(args, format);

        // 输出日志级别前缀
        switch (level) {
            case LOG_DEBUG:
                fprintf(stderr, "DEBUG: ");
                break;
            case LOG_INFO:
                fprintf(stderr, "INFO: ");
                break;
            case LOG_WARNING:
                fprintf(stderr, "WARNING: ");
                break;
            case LOG_ERROR:
                fprintf(stderr, "ERROR: ");
                break;
            default:
                break;
        }

        // 输出日志信息
        vfprintf(stderr, format, args);
        va_end(args);
    }
}

// 定义LOG宏
#define LOG(level, ...) logPrint(level, __VA_ARGS__)

int main(int argc, char* argv[]) {
    // 从命令行参数读取日志级别
    if (argc > 1) {
        char* arg = argv[1];
        if (!strcmp(arg, "debug"))
            g_log_level = LOG_DEBUG;
        else if (!strcmp(arg, "info"))
            g_log_level = LOG_INFO;
        else if (!strcmp(arg, "warning"))
            g_log_level = LOG_WARNING;
        else if (!strcmp(arg, "error"))
            g_log_level = LOG_ERROR;
        else if (!strcmp(arg, "none"))
            g_log_level = LOG_NONE;
        else
            g_log_level = LOG_INFO;  // 如果参数无效，默认为INFO
    }

    // 测试LOG宏
    LOG(LOG_DEBUG, "This is a debug message.\n");
    LOG(LOG_INFO, "This is an info message.\n");
    LOG(LOG_WARNING, "This is a warning message.\n");
    LOG(LOG_ERROR, "This is an error message.\n");

    return 0;
}
