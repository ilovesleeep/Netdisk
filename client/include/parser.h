#ifndef __NB_PARSER_H
#define __NB_PARSER_H

#include "head.h"

typedef enum {
    CMD_CD,
    CMD_LS,
    CMD_RM,
    CMD_PWD,
    CMD_GETS,
    CMD_PUTS,
    CMD_MKDIR,
    CMD_REG1,
    CMD_REG2,
    CMD_LOGIN1,
    CMD_LOGIN2,
    CMD_EXIT,
    CMD_UNKNOWN,
    CMD_STOP,
} Command;

char** parseRequest(const char* req);
void freeStringArray(char** array);

Command getCommand(const char* cmd);

#endif
