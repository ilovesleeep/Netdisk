#include <func.h>

int main(void) {
    time_t now;
    struct tm* local_time;

    time(&now);
    local_time = localtime(&now);

    char filename[64];
    strftime(filename, SIZE(filename), "server_log_%Y_%m_%d_%H_%M_%S",
             local_time);

    int fd = open(filename, O_WRONLY | O_TRUNC | O_CREAT, 0666);
    if (fd == -1) {
        error(1, errno, "open %s", filename);
    }

    close(fd);

    return 0;
}
