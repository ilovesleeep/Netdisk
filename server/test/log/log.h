#ifndef __K_LOG_H
#define __K_LOG_H

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

typedef enum {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR,
    LOG_NONE,  // 不记录任何日志
} LogLevel;

void logPrint(LogLevel level, const char* format, ...);
void logToFile(LogLevel level, const char* format, ...);

#endif
