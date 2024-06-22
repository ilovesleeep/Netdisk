#include "../include/task.h"

void freeTask(Task* task) {
    freeStringArray(task->args);
    free(task->token);
    free(task->host);
    free(task->port);
    free(task);
}
