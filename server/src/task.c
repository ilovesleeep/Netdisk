#include "../include/task.h"

void taskFree(Task* task) {
    for (int i = 0; task->args[i] != NULL; ++i) {
        free(task->args[i]);
    }
    free(task->args);
    free(task);
}
