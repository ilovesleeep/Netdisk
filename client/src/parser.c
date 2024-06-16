#include "../include/parser.h"

#define MAXARGS 32

char** parseRequest(const char* req) {
    // TODO: error checking
    char** args = (char**)calloc(MAXARGS, sizeof(char*));

    char* tmp = strdup(req);
    char* token = strtok(tmp, " \r\n\t");
    for (int i = 0; i < MAXARGS && token != NULL; ++i) {
        int token_len = strlen(token);
        args[i] = (char*)malloc((token_len + 1) * sizeof(char));  // +1 for '\0'
        strcpy(args[i], token);
        token = strtok(NULL, " \r\n\t");
    }
    free(tmp);

    // 安全考虑，防止越界
    args[MAXARGS - 1] = NULL;
    return args;
}

void argsFree(char** args) {
    char* cur = *args;
    while (cur) {
        char* tmp = cur;
        ++cur;
        free(tmp);
    }
    free(args);
}

Command getCommand(const char* cmd) {
    if (strcmp(cmd, "cd") == 0) {
        return CMD_CD;
    } else if (strcmp(cmd, "ls") == 0) {
        return CMD_LS;
    } else if (strcmp(cmd, "rm") == 0) {
        return CMD_RM;
    } else if (strcmp(cmd, "pwd") == 0) {
        return CMD_PWD;
    } else if (strcmp(cmd, "gets") == 0) {
        return CMD_GETS;
    } else if (strcmp(cmd, "puts") == 0) {
        return CMD_PUTS;
    } else if (strcmp(cmd, "exit") == 0) {
        return CMD_EXIT;
    } else if (strcmp(cmd, "mkdir") == 0) {
        return CMD_MKDIR;
    } else {
        return CMD_UNKNOWN;
    }
}
