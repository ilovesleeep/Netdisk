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
    CMD_GETS,
    CMD_PUTS,
    CMD_MKDIR,
    CMD_LOGIN1,
    CMD_LOGIN2,
    CMD_REGISTER,
    CMD_UNKNOWN,
} Command;

void readConfig(const char* filename, HashTable* ht);

char** parseRequest(const char* req);
Command getCommand(const char* cmd);

#endif
