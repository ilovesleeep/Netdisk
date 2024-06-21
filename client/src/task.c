#include "../include/task.h"

void freeTask(Task* task) {
    free(task->token);
    free(task->host);
    free(task->port);
    free(task);
}
