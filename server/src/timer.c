#include "../include/timer.h"

// 初始化时间轮
HashedWheelTimer* hwtCreate(int size) { // size = WHEEL_SIZE + 1
    HashedWheelTimer* timer = (HashedWheelTimer*)malloc(sizeof(HashedWheelTimer));
    timer->size = size;
    timer->curr_idx = 0;
    timer->slots = (HashSet**)malloc(sizeof(HashSet*) * size);

    for (int i = 0; i < size; ++i) {
        timer->slots[i] = hashsetCreate();
    }

    return timer;    
}

void hwtDestroy(HashedWheelTimer* timer) {
    for (int i = 0; i < timer->size; ++i) {
        hashsetDestroy(timer->slots[i]);
    }
    free(timer->slots);
    free(timer);
}

int hwtUpdate(HashedWheelTimer* timer, int connfd, int old_slot_idx) {
    // old_slot_idx = -1 没找到，否则找到
    if (old_slot_idx != -1) {
        // 在旧槽中删除connfd
        hashsetDelete(timer->slots[old_slot_idx], connfd); 
    }
    int slot_idx = (timer->curr_idx + WHEEL_SIZE) % timer->size;
    hashsetInsert(timer->slots[slot_idx], connfd);
    return slot_idx;
}

void hwtClear(HashedWheelTimer* timer) {
    hashsetClear(timer->slots[timer->curr_idx]);
}