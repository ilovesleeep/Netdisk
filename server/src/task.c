#include "../include/task.h"

void freeTask(Task* task) {
    free(task->token);
    for (int i = 0; task->args[i] != NULL; ++i) {
        free(task->args[i]);
    }
    free(task->args);
    free(task);
}
