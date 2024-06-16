#include "../include/bussiness.h"

#define BUFSIZE 4096
#define MAXLINE 1024
#define BIGFILE_SIZE (100 * 1024 * 1024)
#define MMAPSIZE (1024 * 1024)

typedef struct {
    int length;
    char data[BUFSIZE];
} DataBlock;

int sendn(int fd, void* buf, int length) {
    int bytes = 0;
    while (bytes < length) {
        int n = send(fd, (char*)buf + bytes, length - bytes, MSG_NOSIGNAL);
        if (n < 0) {
            return -1;
        }

        bytes += n;
    }  // bytes == length

    return 0;
}

int recvn(int fd, void* buf, int length) {
    int bytes = 0;
    while (bytes < length) {
        int n = recv(fd, (char*)buf + bytes, length - bytes, 0);
        if (n < 0) {
            return -1;
        }

        bytes += n;
    }  // bytes == length

    return 0;
}

void sendFile(int sockfd, const char* path) {
    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        error(1, errno, "open %s", path);
    }

    // 先发文件名
    DataBlock block;
    strcpy(block.data, path);
    block.length = strlen(path);
    sendn(sockfd, &block, sizeof(int) + block.length);

    // 发送文件大小
    struct stat statbuf;
    fstat(fd, &statbuf);
    off_t fsize = statbuf.st_size;
    sendn(sockfd, &fsize, sizeof(fsize));

    // 发送文件内容
    off_t sent_bytes = 0;
    if (fsize >= BIGFILE_SIZE) {
        // 大文件
        while (sent_bytes < fsize) {
            off_t length =
                fsize - sent_bytes >= MMAPSIZE ? MMAPSIZE : fsize - sent_bytes;

            void* addr =
                mmap(NULL, length, PROT_READ, MAP_SHARED, fd, sent_bytes);
            sendn(sockfd, addr, length);
            munmap(addr, length);

            sent_bytes += length;
        }
    } else {
        // 小文件
        char buf[BUFSIZE];
        while (sent_bytes < fsize) {
            off_t length =
                fsize - sent_bytes >= BUFSIZE ? BUFSIZE : fsize - sent_bytes;

            read(fd, buf, length);
            sendn(sockfd, buf, length);

            sent_bytes += length;
        }
    }
    close(fd);
}

void recvFile(int sockfd) {
    // 接收文件名
    DataBlock block;
    bzero(&block, sizeof(block));
    recvn(sockfd, &block.length, sizeof(int));
    recvn(sockfd, block.data, block.length);

    // 打开文件
    int fd = open(block.data, O_RDWR | O_TRUNC | O_CREAT, 0666);
    if (fd == -1) {
        error(1, errno, "open");
    }

    // 接收文件的大小
    off_t fsize;
    recvn(sockfd, &fsize, sizeof(fsize));

    // 接收文件内容
    off_t recv_bytes = 0;
    if (fsize >= BIGFILE_SIZE) {
        ftruncate(fd, fsize);
        // 大文件
        while (recv_bytes < fsize) {
            off_t length = (fsize - recv_bytes >= MMAPSIZE)
                               ? MMAPSIZE
                               : fsize - recv_bytes;
            void* addr = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED,
                              fd, recv_bytes);
            recvn(sockfd, addr, length);
            munmap(addr, length);

            recv_bytes += length;

            printf("[INFO] downloading %5.2lf%%\r", 100.0 * recv_bytes / fsize);
            fflush(stdout);
        }
    } else {
        char buf[BUFSIZE];
        while (recv_bytes < fsize) {
            off_t length =
                (fsize - recv_bytes >= BUFSIZE) ? BUFSIZE : fsize - recv_bytes;
            recvn(sockfd, buf, length);
            write(fd, buf, length);

            recv_bytes += length;

            printf("[INFO] downloading %5.2lf%%\r", 100.0 * recv_bytes / fsize);
            fflush(stdout);
        }
    }
    printf("[INFO] downloading %5.2lf%%\n", 100.0);
    close(fd);
}

void cdCmd(Task* task) {
    // TODO:

    return;
}

void lsCmd(Task* task) {
    //校验参数,发送校验结果，若为错误则继续发送错误信息
    if(task->args[1] != NULL){
        int sendstat = 1;
        send(task->fd, &sendstat, sizeof(int), MSG_NOSIGNAL);
        char error_info[] = "parameter number error";
        int info_len = strlen(error_info);
        send(task->fd, &info_len, sizeof(int), MSG_NOSIGNAL);
        send(task->fd, error_info, info_len, MSG_NOSIGNAL);
        return;
    }
    else{
        int sendstat = 0;
        send(task->fd, &sendstat, sizeof(int), MSG_NOSIGNAL);
    }

    //获取当前路径
    char path[1000] = {0};
    WorkDir* pathbase = task->wd_table[task->fd];
    strncpy(path, pathbase->path, pathbase->index[pathbase->index[0]]);

    printf("path: %s\n", path);
    //打开目录
    DIR* dir = opendir(pathbase->path);

    //发送文件信息
    struct dirent* p = NULL;
    while((p = readdir(dir))){
        if(strcmp(p->d_name, ".") == 0 || strcmp(p->d_name, "..") == 0){
            continue;
        }
        int info_len = strlen(p->d_name);
        char send_info[1000] = {0};
        strcpy(send_info, p->d_name);
        //发送文件名长度及文件名
        send(task->fd, &info_len, sizeof(int), MSG_NOSIGNAL);
        send(task->fd, send_info, info_len, MSG_NOSIGNAL); 
    }
    //发送int类型的0(这个文件名长度为0)代表文件已发完
    int info_len = 0;
    send(task->fd, &info_len, sizeof(int), MSG_NOSIGNAL);
    return;
}

void rmCmd(Task* task) {
    // TODO:

    return;
}

void pwdCmd(Task* task) {
    // TODO:

    return;
}

void getsCmd(Task* task) {
    // TODO:

    return;
}

void putsCmd(Task* task) {
    // TODO:

    return;
}

void mkdirCmd(Task* task) {
    // if (sizeof(task ->args[1]) >= 1000) {
    //     error(1, 0, "mkdir_dirlen too long!");
    // }

    // 找到当前目录
    // bug: 如果在极端情况下，path有1k长度，mkdir_dirlen也有1k长度，会溢出
    char curr_path[MAXLINE] = {0};
    WorkDir* wd = task->wd_table[task->fd];
    strncpy(curr_path, wd->path, strlen(wd->path));

    int index = wd->index[wd->index[0]];
    curr_path[index + 1] = '\0';
    
    // 将当前目录和args[1]拼接在一起
    char dir[2 * MAXLINE] = {0};
    sprintf(dir, "%s/%s", curr_path, task->args[1]);
    

    // debug
    // printf("wd->index[1] = %d, wd->index[0] = %d\n", wd->index[wd->index[0]], wd->index[0]);
    // printf("wdpath = %s\n", wd->path);
    // printf("curpath = %s\n", curr_path);
    // printf("dir = %s\n",dir);
    

    // 根据dir递归创建目录
    // 找每个'/',将其替换成'\0'
    for (char* p = dir + 1; *p; ++p) {
        if (*p == '/') {
            // 这里没有跳过多余的'/'，但没有出bug，大概率是mkdir背后做了这个事情
            *p = '\0';

            if (mkdir(dir, 0777) && errno != EEXIST) {
                char errmsg[MAXLINE] = "mkdir";
                strncat(errmsg, strerror(errno), sizeof(errmsg) - strlen(strerror(errno)) - 1);
                send(task->fd, errmsg, strlen(errmsg), 0);
                // 后面补日志
                error(0, errno, "%d mkdir:", task->fd);
            }

            *p = '/';
        }        
    }

    if (mkdir(dir, 0777) && errno != EEXIST) {
        char errmsg[MAXLINE] = "mkdir";
        strncat(errmsg, strerror(errno), sizeof(errmsg) - strlen(strerror(errno)) - 1);
        send(task->fd, errmsg, strlen(errmsg), 0);
        // 后面补日志
        error(0, errno, "%d mkdir:", task->fd);
    }

    // 成功了给客户端发一个0
    send(task->fd, "0", sizeof("0"), 0);
    
    return;
}

void exitCmd(Task* task) {
    // TODO:

    return;
}

void unknownCmd(Task* task) {
    // TODO:

    return;
}

void taskHandler(Task* task) {
    switch (getCommand(task->args[0])) {
        case CMD_CD:
            cdCmd(task);
            break;
        case CMD_LS:
            lsCmd(task);
            break;
        case CMD_RM:
            break;
        case CMD_PWD:
            pwdCmd(task);
            break;
        case CMD_MKDIR:
            mkdirCmd(task);
            break;
        case CMD_GETS:
            getsCmd(task);
            break;
        case CMD_PUTS:
            putsCmd(task);
            break;
        case CMD_EXIT:
            exitCmd(task);
            break;
        default:
            unknownCmd(task);
            break;
    }
}

void workdirInit(WorkDir** workdir_table, int connfd, char* username) {
    // TODO: error checking
    workdir_table[connfd] = (WorkDir*)malloc(sizeof(WorkDir));
    workdir_table[connfd]->path = (char*)malloc(MAXLINE * sizeof(char));
    workdir_table[connfd]->index = (int*)malloc(MAXLINE * sizeof(int));

    strcpy(workdir_table[connfd]->path, username);
    workdir_table[connfd]->index[0] = 1;
    workdir_table[connfd]->index[1] = strlen(username) - 1;
}
