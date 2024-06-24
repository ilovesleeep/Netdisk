#include "../include/server.h"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        error(1, 0, "Usage: sudo %s [path/to/conf]\n", argv[0]);
    }

    // 初始化哈希表，用于存储配置
    HashTable ht;
    initHashTable(&ht);

    // 读取配置
    readConfig(argv[1], &ht);

    // 初始化日志
    initLog(&ht);

    // 初始化服务器配置
    ServerConfig conf = {"8080", 2};
    serverInit(&conf, &ht);

    // 启动
    serverMain(&conf, &ht);

    return 0;
}
