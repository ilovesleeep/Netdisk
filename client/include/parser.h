#ifndef __NB_PARSER_H
#define __NB_PARSER_H

#include "head.h"

typedef struct {
    int port;
    int num_threads;
} ServerConfig;

typedef enum {
    CMD_CD,
    CMD_LS,
    CMD_RM,
    CMD_PWD,
    CMD_GETS,
    CMD_PUTS,
    CMD_MKDIR,
    CMD_EXIT,
    CMD_UNKNOWN,
} Command;

char** parseRequest(const char* req);
void argsFree(char** args);

Command getCommand(const char* cmd);

#endif
