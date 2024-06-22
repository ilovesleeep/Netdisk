#ifndef __NB_PARSER_H
#define __NB_PARSER_H

#include "hashtable.h"
#include "head.h"
#include "log.h"

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
    CMD_UNKNOWN,
} Command;

void readConfig(const char* filename, HashTable* ht);

char** getArgs(const char* req);

Command getCommand(const char* cmd);

void freeStringArray(char** array);

#endif
