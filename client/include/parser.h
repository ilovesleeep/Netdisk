#ifndef __NB_PARSER_H
#define __NB_PARSER_H

#include "head.h"

typedef enum {
    CMD_CD,
    CMD_LS,
    CMD_RM,
    CMD_PWD,
    CMD_MKDIR,
    CMD_GETS1,
    CMD_GETS2,
    CMD_PUTS1,
    CMD_PUTS2,
    CMD_REG1,
    CMD_REG2,
    CMD_LOGIN1,
    CMD_LOGIN2,
    CMD_INFO_TOKEN,
    CMD_EXIT,
    CMD_STOP,
    CMD_UNKNOWN,
} Command;

char** parseRequest(const char* req);

void freeStringArray(char** array);

Command getCommand(const char* cmd);

#endif
