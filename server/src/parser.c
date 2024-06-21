#include "../include/parser.h"

#define MAXLINE 1024
#define MAXARGS 32

void readConfig(const char* filename, HashTable* ht) {
    FILE* fp = fopen(filename, "r");
    if (fp == NULL) {
        error(1, errno, "fopen %s", filename);
    }

    char buf[256] = {0};
    while (fgets(buf, sizeof(buf), fp) != NULL) {
        char key[128] = {0};

        char* token = strtok(buf, "=\n");
        strcpy(key, token);
        token = strtok(NULL, "=\n");

        char* value = (char*)calloc(1, strlen(token) + 1);  // +1 for '\0'
        strcpy(value, token);
        insert(ht, key, value);

        // for safe
        token = NULL;
    }

    fclose(fp);
}

char** getArgs(const char* req) {
    char** args = (char**)calloc(MAXARGS, sizeof(char*));
    if (args == NULL) {
        log_error("malloc: %s", strerror(errno));
        error(1, errno, "malloc");
    }

    char* tmp = strdup(req);
    char* token = strtok(tmp, " \r\n\t");
    for (int i = 0; i < MAXARGS && token != NULL; ++i) {
        int token_len = strlen(token);
        args[i] = (char*)calloc((token_len + 1), sizeof(char));  // +1 for '\0'
        if (args[i] == NULL) {
            freeStringArray(args);
            free(args);
            free(tmp);
            log_error("malloc: %s", strerror(errno));
            error(1, errno, "malloc");
        }

        strcpy(args[i], token);
        token = strtok(NULL, " \r\n\t");
    }
    free(tmp);

    // 安全考虑，防止越界
    args[MAXARGS - 1] = NULL;
    return args;
}

void freeStringArray(char** array) {
    if (array == NULL) return;

    for (int i = 0; array[i] != NULL; ++i) {
        free(array[i]);
    }
    free(array);
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
    } else if (strcmp(cmd, "mkdir") == 0) {
        return CMD_MKDIR;
    } else if (strcmp(cmd, "gets") == 0) {
        return CMD_GETS;
    } else if (strcmp(cmd, "puts") == 0) {
        return CMD_PUTS;
    } else if (strcmp(cmd, "reg1") == 0) {
        return CMD_REG1;
    } else if (strcmp(cmd, "reg2") == 0) {
        return CMD_REG2;
    } else if (strcmp(cmd, "login1") == 0) {
        return CMD_LOGIN1;
    } else if (strcmp(cmd, "login2") == 0) {
        return CMD_LOGIN2;
    } else {
        return CMD_UNKNOWN;
    }
}
