#ifndef __K_PARSER_H
#define __K_PARSER_H

#include <func.h>

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

void parseConfig(ServerConfig* conf);

char** parseRequest(const char* req);
Command getCommand(const char* cmd);

#endif
